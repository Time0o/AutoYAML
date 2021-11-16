#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

static char const *AUTO_YAML_MATCHER_ID = "AutoYAML";
static char const *AUTO_YAML_ANNOTATION = "AutoYAML";

// Command line options.
static llvm::cl::OptionCategory AutoYAMLToolCategory { "autoyaml options" };

static llvm::cl::extrahelp CommonHelp { clang::tooling::CommonOptionsParser::HelpMessage };

static llvm::cl::opt<bool> GenCompOps {
  "gen-comp-ops",
  llvm::cl::desc("Generate comparison operators"),
  llvm::cl::cat(AutoYAMLToolCategory) };

static llvm::cl::opt<std::string> OutDir {
  "out-dir",
  llvm::cl::desc("Output directory"),
  llvm::cl::cat(AutoYAMLToolCategory) };

// Convenience wrapper around llvm::raw_fd_ostream that handles indentation levels nicely.
class AutoYAMLOS
{
  template<typename T>
  friend AutoYAMLOS &operator<<(AutoYAMLOS &, T const &);

  enum { TABSTOP = 2 };

public:
  static struct EndLine {} const EndL; // End of line.
  static struct EndBlock {} const EndB; // End of block.

  AutoYAMLOS(llvm::StringRef File)
  {
    std::error_code EC;
    OS_ = std::make_unique<llvm::raw_fd_ostream>(File, EC);
    OSValid_ = !EC;
  }

  operator bool() const
  { return OSValid_; }

  // Increase indentation level.
  void incIndLvl()
  { ++IndLvl_; }

  // Decrease indentation level.
  void decIndLvl()
  {
    assert(IndLvl_ > 0);
    --IndLvl_;
  }

private:
  std::unique_ptr<llvm::raw_fd_ostream> OS_;
  bool OSValid_ {false};

  unsigned Ind_ {0};
  unsigned IndLvl_ {0};
  bool IndActive_ {true};
};

AutoYAMLOS::EndLine const AutoYAMLOS::EndL;
AutoYAMLOS::EndBlock const AutoYAMLOS::EndB;

template<typename T>
AutoYAMLOS &operator<<(AutoYAMLOS &OS, T const &Val)
{
  assert(OS);

  if (OS.IndActive_) {
    for (unsigned i = 0; i < AutoYAMLOS::TABSTOP * OS.IndLvl_; ++i)
      *OS.OS_ << ' ';

    OS.IndActive_ = false;
  }

  *OS.OS_ << Val;

  return OS;
}

template<>
AutoYAMLOS &operator<<(AutoYAMLOS &OS, AutoYAMLOS::EndLine const &)
{
  assert(OS);

  *OS.OS_ << '\n';
  OS.IndActive_ = true;

  return OS;
}

template<>
AutoYAMLOS &operator<<(AutoYAMLOS &OS, AutoYAMLOS::EndBlock const &)
{
  assert(OS);

  *OS.OS_ << "\n\n";
  OS.IndActive_ = true;

  return OS;
}

class AutoYAMLMatchCallback : public clang::ast_matchers::MatchFinder::MatchCallback
{
public:
  AutoYAMLMatchCallback(AutoYAMLOS &OS,
                        clang::ASTContext &ASTContext,
                        clang::ast_matchers::MatchFinder &MatchFinder)
  : OS_(OS),
    ASTContext_(ASTContext),
    MatchFinder_(MatchFinder)
  {}

  void run(clang::ast_matchers::MatchFinder::MatchResult const &Result) override
  {
    // Check whether we've already matched this node.
    auto Decl { getNode<clang::Decl>(Result) };

    if (!MatchedNodes_.insert(Decl->getID()).second)
      return;

    // If node is a record declaration...
    auto Record { getNode<clang::RecordDecl>(Result) };

    if (Record) {
      // Match nested declarations first.
      auto RecordContext { static_cast<clang::DeclContext const *>(Record) };

      for (auto const *InnerDecl : RecordContext->decls())
        MatchFinder_.match(*InnerDecl, ASTContext_);

      // Generate a `YAML::convert` specialization.
      run(Record, &AutoYAMLMatchCallback::emitConvert);

      // Optionally generator an `operator==` definition.
      if (GenCompOps)
        run(Record, &AutoYAMLMatchCallback::emitCompare);

      return;
    }

    // If node is an enum declaration...
    auto Enum { getNode<clang::EnumDecl>(Result) };

    if (Enum)
      // Generate a `YAML::convert` specialization.
      run(Enum, &AutoYAMLMatchCallback::emitConvert);
  }

private:
  template<typename T>
  void run(T const *Node, void(AutoYAMLMatchCallback::*Action)(T const *))
  {
    assert(Node);
    assert(Node->hasAttrs());

    auto Attr { Node->getAttrs()[0] };

    auto AnnotateAttr { llvm::dyn_cast<clang::AnnotateAttr>(Attr) };
    assert(AnnotateAttr);

    if (AnnotateAttr->getAnnotation() != AUTO_YAML_ANNOTATION)
      return;

    (this->*Action)(Node);
  }

  template<typename T>
  void emitConvert(T const *Node)
  {
    auto NodeType { getTypeAsString(Node->getTypeForDecl()) };

    OS_ << "namespace YAML {" << OS_.EndB;

    OS_ << "template<> struct convert<" << NodeType << "> {" << OS_.EndB;

    OS_.incIndLvl();

    emitEncode(Node);

    emitDecode(Node);

    OS_.decIndLvl();

    OS_ << "};" << OS_.EndB;

    OS_ << "} // end namespace YAML" << OS_.EndB;
  }

  template<typename T>
  void emitEncode(T const *Node)
  {
    auto NodeType { getTypeAsString(Node->getTypeForDecl()) };

    OS_ << "static Node encode(" << NodeType << " const &obj) {" << OS_.EndL;

    OS_.incIndLvl();

    OS_ << "Node node;" << OS_.EndL;

    emitEncode_(Node);

    OS_ << "return node;" << OS_.EndL;

    OS_.decIndLvl();

    OS_ << "}" << OS_.EndB;
  }

  void emitEncode_(clang::RecordDecl const *Record)
  {
    for (auto Field : getPublicFields(Record))
      OS_ << "node[\"" << Field.Name << "\"] = obj." << Field.Name << ";" << OS_.EndL;
  }

  void emitEncode_(clang::EnumDecl const *Enum)
  {
    OS_ << "switch (obj) {" << OS_.EndL;

    for (auto Constant : Enum->enumerators()) {
      OS_ << "case " << Constant->getQualifiedNameAsString() << ":" << OS_.EndL;

      OS_.incIndLvl();

      OS_ << "node = \"" << Constant->getNameAsString() << "\";" << OS_.EndL;
      OS_ << "break;" << OS_.EndL;

      OS_.decIndLvl();
    }

    OS_ << "}" << OS_.EndL;
  }

  template<typename T>
  void emitDecode(T const *Node)
  {
    auto NodeType { getTypeAsString(Node->getTypeForDecl()) };

    OS_ << "static bool decode(Node const &node, " << NodeType << " &obj) {" << OS_.EndL;

    OS_.incIndLvl();

    emitDecode_(Node);

    OS_ << "return true;" << OS_.EndL;

    OS_.decIndLvl();

    OS_ << "}" << OS_.EndB;
  }

  void emitDecode_(clang::RecordDecl const *Record)
  {
    std::vector<std::string> FieldNames;

    // Sanity checks.
    OS_ << "check_node(node);" << OS_.EndL;

    OS_ << "check_node_properties(node, {" << OS_.EndL;

    for (auto Field : getPublicFields(Record)) {
      if (!Field.HasDefaultValue)
        OS_ << "  \"" << Field.Name << "\"," << OS_.EndL;
    }

    OS_ << "});" << OS_.EndL;

    // Set fields.
    for (auto Field : getPublicFields(Record)) {
      char const *set = Field.HasDefaultValue ? "set_optional_field" : "set_field";

      OS_ << set << "<" << Field.Type << ">"
          << "(obj." << Field.Name << ", node, \"" << Field.Name << "\");" << OS_.EndL;
    }
  }

  void emitDecode_(clang::EnumDecl const *Enum)
  {
    OS_ << "auto str { node.as<std::string>() };" << OS_.EndL;

    for (auto Constant : Enum->enumerators()) {
      if (Constant != *Enum->enumerator_begin())
        OS_ << "else ";

      OS_ << "if (str == \"" << Constant->getNameAsString() << "\") "
          << "obj = " << Constant->getQualifiedNameAsString() << ";" << OS_.EndL;
    }

    OS_ << "else return false;" << OS_.EndL;
  }

  // Emit an implementation of operator== for a given record type. This is
  // useful when C++20's operator==(...) = default is not available.
  void emitCompare(clang::RecordDecl const *Record)
  {
    auto RecordType { getTypeAsString(Record->getTypeForDecl()) };
    auto RecordNamespace { getNamespace(Record) };

    // Strip namespace prexi from type.
    if (RecordNamespace && RecordType.rfind(*RecordNamespace, 0) == 0)
      RecordType = RecordType.substr(RecordNamespace->size() + 2);

    if (RecordNamespace)
      OS_ << "namespace " << *RecordNamespace << " {" << OS_.EndB;

    OS_ << "bool operator==("
        << RecordType << " const &obj, "
        << RecordType << " const &other) {" << OS_.EndL;

    OS_.incIndLvl();

    // XXX Check if fields are actually comparable.
    auto Fields { getPublicFields(Record) };

    if (!Fields.empty()) {
      for (std::size_t i = 0; i < Fields.size(); ++i) {
        auto const &Field { Fields[i] };

        OS_ << (i == 0 ? "return" : "      ");

        OS_ << " obj." << Field.Name << " == other." << Field.Name;

        OS_ << (i == Fields.size() - 1 ? ";" : " &&") << OS_.EndL;
      }
    }

    OS_.decIndLvl();

    OS_ << "}" << OS_.EndB;

    if (RecordNamespace)
      OS_ << "} // end namespace " << *RecordNamespace << OS_.EndB;
  }

  struct RecordField
  {
    std::string Name;
    std::string Type;
    bool HasDefaultValue;
  };

  template<typename T>
  static T const *getNode(clang::ast_matchers::MatchFinder::MatchResult const &Result)
  {
    return Result.Nodes.getNodeAs<T>(AUTO_YAML_MATCHER_ID);
  }

  std::vector<RecordField> getPublicFields(clang::RecordDecl const *Record) const
  {
    std::vector<RecordField> Fields;

    for (auto FieldDecl : Record->fields()) {
      // Skip non-public members.
      if (FieldDecl->getAccess() != clang::AS_public)
        continue;

      RecordField Field { FieldDecl->getNameAsString(),
                          getTypeAsString(FieldDecl->getType()),
                          FieldDecl->getInClassInitStyle() == clang::ICIS_CopyInit };

      Fields.emplace_back(std::move(Field));
    }

    return Fields;
  }

  std::optional<std::string> getNamespace(clang::RecordDecl const *Record) const
  {
    auto const *Context { Record->getDeclContext() };

    while (!Context->isTranslationUnit()) {
      if (Context->isNamespace()) {
        auto NamespaceDecl(llvm::dyn_cast<clang::NamespaceDecl>(Context));

        if (NamespaceDecl->isAnonymousNamespace())
          return std::nullopt;

        return NamespaceDecl->getNameAsString();
      }

      Context = Context->getParent();
    }

    return std::nullopt;
  }

  std::string getTypeAsString(clang::QualType const &Type) const
  {
    return getTypeAsString(Type.getTypePtr());
  }

  std::string getTypeAsString(clang::Type const *Type) const
  {
    clang::PrintingPolicy PP { ASTContext_.getLangOpts() };

    std::string Str;

    auto ElaboratedType { llvm::dyn_cast<clang::ElaboratedType>(Type) };

    if (ElaboratedType) {
      auto QT { ElaboratedType->getNamedType() };
      Str = QT.getAsString(PP);

      // Possibly prepend missing scope qualifiers.
      auto Qualifier { ElaboratedType->getQualifier() };

      std::string QualifierStr;
      llvm::raw_string_ostream OS { QualifierStr };
      Qualifier->print(OS, PP);

      if (Str.rfind(QualifierStr, 0) != 0)
        Str = QualifierStr + Str;

    } else {
      auto QT { clang::QualType(Type, 0) };
      Str = QT.getAsString(PP);
    }

    return Str;
  }

  AutoYAMLOS &OS_;
  clang::ASTContext &ASTContext_;
  clang::ast_matchers::MatchFinder &MatchFinder_;
  std::unordered_set<std::int64_t> MatchedNodes_;
};

class AutoYAMLASTConsumer : public clang::ASTConsumer
{
public:
  AutoYAMLASTConsumer(AutoYAMLOS &OS)
  : OS_(OS)
  {}

  void HandleTranslationUnit(clang::ASTContext &Context) override
  {
    using namespace clang::ast_matchers;

    // Create AST Matcher.
    auto AutoYAMLMatchExpression { tagDecl(hasAttr(clang::attr::Annotate)) };

    clang::ast_matchers::MatchFinder MatchFinder;

    AutoYAMLMatchCallback MatchCallback { OS_, Context, MatchFinder };

    MatchFinder.addMatcher(AutoYAMLMatchExpression.bind(AUTO_YAML_MATCHER_ID), &MatchCallback);

    // Emit conversion code.
    emitPreamble();

    MatchFinder.matchAST(Context);
  }

private:
  void emitPreamble()
  {
    OS_ << "// Automatically generated by AutoYAML, do not modify!" << OS_.EndB;

    OS_ << "#pragma once" << OS_.EndB;
  }

  AutoYAMLOS &OS_;
};

struct AutoYAMLFrontendAction : public clang::ASTFrontendAction
{
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance&,
                                                        llvm::StringRef File) override
  {
    // Create output file stream.
    enum { OUTFILE_PATH_MAX = 4096 };

    llvm::SmallString<OUTFILE_PATH_MAX> OutFile;
    llvm::sys::path::append(OutFile, OutDir, llvm::sys::path::filename(File));
    llvm::sys::path::replace_extension(OutFile, ".AutoYAML.h");

    OS_ = std::make_unique<AutoYAMLOS>(OutFile);

    if (!*OS_) {
      llvm::errs() << "Failed to create output file \"" << OutFile << "\"\n";
      return nullptr;
    }

    // Run AST consumer.
    return std::make_unique<AutoYAMLASTConsumer>(*OS_);
  }

private:
  std::unique_ptr<AutoYAMLOS> OS_;
};

int main(int argc, char const **argv)
{
  // Parse command line arguments.
  auto Parser { clang::tooling::CommonOptionsParser::create(
                  argc, argv, AutoYAMLToolCategory, llvm::cl::OneOrMore) };

  if (!Parser) {
    llvm::errs() << Parser.takeError();
    return EXIT_FAILURE;
  }

  // Create frontend action.
  auto FrontendAction { clang::tooling::newFrontendActionFactory<AutoYAMLFrontendAction>() };

  // Create tool.
  clang::tooling::ClangTool Tool { Parser->getCompilations(), Parser->getSourcePathList() };

  // Append default Clang include paths.
  auto split = [](std::string const &Str) {
    std::istringstream SS(Str);

    std::string Word;
    std::vector<std::string> Words;

    while (SS >> Word)
      Words.push_back(Word);

    return Words;
  };

  auto CppHeaderArgumentAdjuster {
    clang::tooling::getInsertArgumentAdjuster(
      "-xc++-header",
      clang::tooling::ArgumentInsertPosition::BEGIN) };

  Tool.appendArgumentsAdjuster(CppHeaderArgumentAdjuster);

  auto ClangIncludePathsArgumentAdjuster {
    clang::tooling::getInsertArgumentAdjuster(
      split(CLANG_INCLUDE_PATHS),
      clang::tooling::ArgumentInsertPosition::END) };

  Tool.appendArgumentsAdjuster(ClangIncludePathsArgumentAdjuster);

  // Run tool.
  if (Tool.run(FrontendAction.get()))
    return EXIT_FAILURE;
}
