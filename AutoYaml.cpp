#include <memory>

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

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

static llvm::cl::OptionCategory AutoYAMLToolCategory { "autoyaml options" };

static llvm::cl::extrahelp CommonHelp { clang::tooling::CommonOptionsParser::HelpMessage };

class AutoYAMLMatchCallback : public clang::ast_matchers::MatchFinder::MatchCallback
{
public:
  AutoYAMLMatchCallback(llvm::raw_fd_ostream &OutStream,
                        clang::ASTContext &Context)
  : OutStream_(OutStream),
    Context_(Context)
  {}

  void run(clang::ast_matchers::MatchFinder::MatchResult const &Result) override
  {
    auto Record { Result.Nodes.getNodeAs<clang::RecordDecl>("AutoYAML") };
    assert(Record);
    assert(Record->hasAttrs());

    auto Attr { Record->getAttrs()[0] };
    auto AnnotateAttr { llvm::dyn_cast<clang::AnnotateAttr>(Attr) };
    assert(AnnotateAttr);

    if (AnnotateAttr->getAnnotation() != "AutoYAML")
      return;

    OutStream_ << Record->getName() << '\n';
  }

private:
  llvm::raw_fd_ostream &OutStream_;
  clang::ASTContext &Context_;
};

class AutoYAMLASTConsumer : public clang::ASTConsumer
{
public:
  AutoYAMLASTConsumer(llvm::raw_fd_ostream &OutStream)
  : OutStream_(OutStream)
  {}

  void HandleTranslationUnit(clang::ASTContext &Context) override
  {
    using namespace clang::ast_matchers;

    auto AutoYAMLMatchExpression { recordDecl(hasAttr(clang::attr::Annotate)) };

    AutoYAMLMatchCallback MatchCallback { OutStream_, Context };

    clang::ast_matchers::MatchFinder MatchFinder;
    MatchFinder.addMatcher(AutoYAMLMatchExpression.bind("AutoYAML"), &MatchCallback);

    MatchFinder.matchAST(Context);
  }

private:
  llvm::raw_fd_ostream &OutStream_;
};

struct AutoYAMLFrontendAction : public clang::ASTFrontendAction
{
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance&,
                                                        llvm::StringRef File) override
  {
    std::string InFileStem { llvm::sys::path::stem(File).str() };

    std::string OutFile { InFileStem + ".AutoYAML.h" };

    std::error_code EC;
    OutStream_ = std::make_unique<llvm::raw_fd_ostream>(OutFile, EC);

    if (EC) {
      llvm::errs() << "Failed to create output file \"" << OutFile << "\"\n";
      return nullptr;
    }

    return std::make_unique<AutoYAMLASTConsumer>(*OutStream_);
  }

private:
  std::unique_ptr<llvm::raw_fd_ostream> OutStream_;
};

int main(int argc, char const **argv)
{
  clang::tooling::CommonOptionsParser Parser { argc, argv, AutoYAMLToolCategory };

  clang::tooling::ClangTool Tool { Parser.getCompilations(), Parser.getSourcePathList() };

  auto FrontendAction { clang::tooling::newFrontendActionFactory<AutoYAMLFrontendAction>() };

  return Tool.run(FrontendAction.get());
}
