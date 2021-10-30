#include <cstdlib>
#include <memory>
#include <string>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
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

static llvm::cl::OptionCategory AutoYAMLToolCategory { "autoyaml options" };

static llvm::cl::extrahelp CommonHelp { clang::tooling::CommonOptionsParser::HelpMessage };

static llvm::cl::opt<std::string> OutDir { "out-dir",
                                           llvm::cl::desc("Output directory"),
                                           llvm::cl::cat(AutoYAMLToolCategory) };

// Convenience wrapper around llvm::raw_fd_ostream that handles indentation levels nicely.
class AutoYAMLOS
{
  template<typename T>
  friend AutoYAMLOS &operator<<(AutoYAMLOS &, T const &);

  enum { TABSTOP = 2 };

  struct EndLine {};
  struct EndBlock {};

public:
  static inline EndLine const EndL; // End of line.
  static inline EndBlock const EndB; // End of block.

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
  AutoYAMLMatchCallback(AutoYAMLOS &OS)
  : OS_(OS)
  {}

  void run(clang::ast_matchers::MatchFinder::MatchResult const &Result) override
  {
    // Get record declaration node.
    auto Record { Result.Nodes.getNodeAs<clang::RecordDecl>("AutoYAML") };
    assert(Record);
    assert(Record->hasAttrs());

    // Check if it's annotated with __attribute__((annotate("AutoYAML"))).
    auto Attr { Record->getAttrs()[0] };
    auto AnnotateAttr { llvm::dyn_cast<clang::AnnotateAttr>(Attr) };
    assert(AnnotateAttr);

    if (AnnotateAttr->getAnnotation() != "AutoYAML")
      return;

    // Emit conversion code.
    emitConvert(Record);
  }

private:
  void emitConvert(const clang::RecordDecl *Record)
  {
    auto RecordName { Record->getName() };

    OS_ << "template<> struct convert<" << RecordName << "> {" << OS_.EndL;

    OS_.incIndLvl();

    emitEncode(Record);

    emitDecode(Record);

    OS_.decIndLvl();

    OS_ << "};" << OS_.EndB;
  }

  void emitEncode(const clang::RecordDecl *Record)
  {
    auto RecordName { Record->getName() };

    OS_ << "static Node encode(const " << RecordName << " &obj) {" << OS_.EndL;

    OS_.incIndLvl();

    OS_ << "Node node;" << OS_.EndL;

    for (auto Field : Record->fields()) {
      // Skip non-public members.
      if (Field->getAccess() != clang::AS_public)
        continue;

      OS_ << "node.push_back(obj." << Field->getNameAsString() << ");" << OS_.EndL;
    }

    OS_ << "return node;" << OS_.EndL;

    OS_.decIndLvl();

    OS_ << "}" << OS_.EndB;
  }

  void emitDecode(const clang::RecordDecl *Record)
  {
    // XXX
  }

  AutoYAMLOS &OS_;
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
    auto AutoYAMLMatchExpression { recordDecl(hasAttr(clang::attr::Annotate)) };

    AutoYAMLMatchCallback MatchCallback { OS_ };

    clang::ast_matchers::MatchFinder MatchFinder;
    MatchFinder.addMatcher(AutoYAMLMatchExpression.bind("AutoYAML"), &MatchCallback);

    // Emit conversion code.
    emitPreamble();

    OS_.incIndLvl();

    MatchFinder.matchAST(Context);

    OS_.decIndLvl();

    emitEpilogue();
  }

private:
  void emitPreamble()
  {
    OS_ << "// Automatically generated by AutoYAML, do not modify!" << OS_.EndB;

    OS_ << "#pragma once" << OS_.EndB;

    OS_ << "namespace YAML {" << OS_.EndB;
  }

  void emitEpilogue()
  {
    OS_ << "} // end namespace YAML";
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

  // Create and run tool.
  clang::tooling::ClangTool Tool { Parser->getCompilations(), Parser->getSourcePathList() };

  if (Tool.run(FrontendAction.get()))
    return EXIT_FAILURE;
}
