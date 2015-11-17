#include <iostream>
#include <sstream>

#include "../AngelixCommon.h"


class ExpressionHandler : public MatchFinder::MatchCallback {
public:
  ExpressionHandler(Rewriter &Rewrite) : Rewrite(Rewrite) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    if (const Expr *expr = Result.Nodes.getNodeAs<clang::Expr>("repairable")) {
      SourceManager &srcMgr = Rewrite.getSourceMgr();

      SourceRange expandedLoc = getExpandedLoc(expr, srcMgr);

      unsigned beginLine = srcMgr.getSpellingLineNumber(expandedLoc.getBegin());
      unsigned beginColumn = srcMgr.getSpellingColumnNumber(expandedLoc.getBegin());
      unsigned endLine = srcMgr.getSpellingLineNumber(expandedLoc.getEnd());
      unsigned endColumn = srcMgr.getSpellingColumnNumber(expandedLoc.getEnd());

      std::cout << beginLine << " " << beginColumn << " " << endLine << " " << endColumn << "\n"
                << toString(expr) << "\n";

      std::ostringstream stringStream;
      stringStream << "({ angelix_trace("
                   << beginLine << ", "
                   << beginColumn << ", "
                   << endLine << ", "
                   << endColumn
                   << "); "
                   << toString(expr)
                   << "; })";
      std::string replacement = stringStream.str();

      Rewrite.ReplaceText(expandedLoc, replacement);
    }
  }

private:
  Rewriter &Rewrite;
};


class StatementHandler : public MatchFinder::MatchCallback {
public:
  StatementHandler(Rewriter &Rewrite) : Rewrite(Rewrite) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    if (const Stmt *stmt = Result.Nodes.getNodeAs<clang::Stmt>("repairable")) {
      SourceManager &srcMgr = Rewrite.getSourceMgr();

      SourceRange expandedLoc = getExpandedLoc(stmt, srcMgr);

      unsigned beginLine = srcMgr.getSpellingLineNumber(expandedLoc.getBegin());
      unsigned beginColumn = srcMgr.getSpellingColumnNumber(expandedLoc.getBegin());
      unsigned endLine = srcMgr.getSpellingLineNumber(expandedLoc.getEnd());
      unsigned endColumn = srcMgr.getSpellingColumnNumber(expandedLoc.getEnd());

      std::cout << beginLine << " " << beginColumn << " " << endLine << " " << endColumn << "\n"
                << toString(stmt) << "\n";

      std::ostringstream stringStream;
      stringStream << "{ angelix_trace("
                   << beginLine << ", "
                   << beginColumn << ", "
                   << endLine << ", "
                   << endColumn
                   << "); "
                   << toString(stmt)
                   << "; }";
      std::string replacement = stringStream.str();

      Rewrite.ReplaceText(expandedLoc, replacement);
    }
  }

private:
  Rewriter &Rewrite;
};


class MyASTConsumer : public ASTConsumer {
public:
  MyASTConsumer(Rewriter &R) : HandlerForExpressions(R), HandlerForStatements(R) {
    if (getenv("ANGELIX_SEMFIX_MODE")) {
      Matcher.addMatcher(InterestingCondition, &HandlerForExpressions);
      Matcher.addMatcher(InterestingIntegerAssignment, &HandlerForExpressions);
    } else {
      if (getenv("ANGELIX_IGNORE_TRIVIAL")) {
        if (getenv("ANGELIX_IF_CONDITIONS_DEFECT_CLASS"))
          Matcher.addMatcher(NonTrivialRepairableIfCondition, &HandlerForExpressions);

        if (getenv("ANGELIX_LOOP_CONDITIONS_DEFECT_CLASS"))
          Matcher.addMatcher(NonTrivialRepairableLoopCondition, &HandlerForExpressions);

        if (getenv("ANGELIX_ASSIGNMENTS_DEFECT_CLASS"))
          Matcher.addMatcher(NonTrivialRepairableAssignment, &HandlerForExpressions);
      } else {
        if (getenv("ANGELIX_IF_CONDITIONS_DEFECT_CLASS"))
          Matcher.addMatcher(RepairableIfCondition, &HandlerForExpressions);

        if (getenv("ANGELIX_LOOP_CONDITIONS_DEFECT_CLASS"))
          Matcher.addMatcher(RepairableLoopCondition, &HandlerForExpressions);

        if (getenv("ANGELIX_ASSIGNMENTS_DEFECT_CLASS"))
          Matcher.addMatcher(RepairableAssignment, &HandlerForExpressions);
      }
      if (getenv("ANGELIX_GUARDS_DEFECT_CLASS"))
        Matcher.addMatcher(InterestingStatement, &HandlerForStatements);
    }
  }

  void HandleTranslationUnit(ASTContext &Context) override {
    Matcher.matchAST(Context);
  }

private:
  ExpressionHandler HandlerForExpressions;
  StatementHandler HandlerForStatements;
  MatchFinder Matcher;
};


class InstrumentRepairableAction : public ASTFrontendAction {
public:
  InstrumentRepairableAction() {}

  void EndSourceFileAction() override {
    FileID ID = TheRewriter.getSourceMgr().getMainFileID();
    if (INPLACE_MODIFICATION) {
      TheRewriter.overwriteChangedFiles();
    } else {
      TheRewriter.getEditBuffer(ID).write(llvm::outs());
    }
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return llvm::make_unique<MyASTConsumer>(TheRewriter);
  }

private:
  Rewriter TheRewriter;
};


// Apply a custom category to all command-line options so that they are the only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("angelix options");


int main(int argc, const char **argv) {
  // CommonOptionsParser constructor will parse arguments and create a
  // CompilationDatabase.  In case of error it will terminate the program.
  CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);

  // We hand the CompilationDatabase we created and the sources to run over into the tool constructor.
  ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

  return Tool.run(newFrontendActionFactory<InstrumentRepairableAction>().get());
}
