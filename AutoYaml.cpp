#include <memory>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
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
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

static llvm::cl::OptionCategory AutoYAMLToolCategory { "autoyaml options" };

static llvm::cl::extrahelp CommonHelp { clang::tooling::CommonOptionsParser::HelpMessage };

class AutoYAMLMatchCallback : public clang::ast_matchers::MatchFinder::MatchCallback
{
public:
  AutoYAMLMatchCallback(clang::ASTContext &Context)
  : Context_(Context)
  {}

  void run(clang::ast_matchers::MatchFinder::MatchResult const &Result) override
  {
    auto Record { Result.Nodes.getNodeAs<clang::RecordDecl>("AutoYAML") };

    auto Attr { Record->getAttrs()[0] };

    if (printSource(Attr->getRange()) != R"(annotate("AutoYAML"))")
      return;

    llvm::outs() << Record->getName() << '\n';
  }

private:
  llvm::StringRef printSource(clang::SourceRange const &Range) const
  {
    auto const &SourceManager { Context_.getSourceManager() };
    auto const &LangOpts { Context_.getLangOpts() };

    auto CharRange { clang::CharSourceRange::getTokenRange(Range) };

    return clang::Lexer::getSourceText(CharRange, SourceManager, LangOpts);
  }

  clang::ASTContext &Context_;
};

struct AutoYAMLASTConsumer : public clang::ASTConsumer
{
  void HandleTranslationUnit(clang::ASTContext &Context) override
  {
    using namespace clang::ast_matchers;

    auto AutoYAMLMatchExpression { recordDecl(hasAttr(clang::attr::Annotate)) };

    AutoYAMLMatchCallback MatchCallback { Context };

    clang::ast_matchers::MatchFinder MatchFinder;
    MatchFinder.addMatcher(AutoYAMLMatchExpression.bind("AutoYAML"), &MatchCallback);

    MatchFinder.matchAST(Context);
  }
};

struct AutoYAMLFrontendAction : public clang::ASTFrontendAction
{
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance&,
                                                        llvm::StringRef) override
  {
    return std::make_unique<AutoYAMLASTConsumer>();
  }
};

int main(int argc, char const **argv)
{
  clang::tooling::CommonOptionsParser Parser { argc, argv, AutoYAMLToolCategory };

  clang::tooling::ClangTool Tool { Parser.getCompilations(), Parser.getSourcePathList() };

  auto FrontendAction { clang::tooling::newFrontendActionFactory<AutoYAMLFrontendAction>() };

  Tool.run(FrontendAction.get());
}
