#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

static llvm::cl::OptionCategory AutoYAMLToolCategory { "autoyaml options" };

static llvm::cl::extrahelp CommonHelp { clang::tooling::CommonOptionsParser::HelpMessage };

int main(int argc, char const **argv)
{
  clang::tooling::CommonOptionsParser Parser { argc, argv, AutoYAMLToolCategory };

  clang::tooling::ClangTool Tool { Parser.getCompilations(), Parser.getSourcePathList() };
}
