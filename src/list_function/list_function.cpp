//------------------------------------------------------------------------------
// Tooling sample. Demonstrates:
//
// * How to write a simple source tool using libTooling.
// * How to use RecursiveASTVisitor to find interesting AST nodes.
// * How to use the Rewriter API to rewrite the source code.
//
// Eli Bendersky (eliben@gmail.com)
// This code is in the public domain
//------------------------------------------------------------------------------
#include <string>
#include <set>
#include <map>
#include <sstream>
#include <fstream>
#include <iostream>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory MyOptionCategory("Options");
static llvm::cl::opt<std::string> OutputFilename("o", 
        llvm::cl::desc("Specify output filename"), 
        llvm::cl::value_desc("filename"), llvm::cl::cat(MyOptionCategory));
typedef std::map<std::string, std::set<std::string>> CallGraph;
std::string CallerFuncName = "";

// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods.
class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor> {
public:
  MyASTVisitor(CallGraph &cg) : callGraph(cg) {}

  bool VisitFunctionDecl(FunctionDecl *f) {
    // Only function definitions (with bodies), not declarations.
    if (f->hasBody() && f->isThisDeclarationADefinition()) {

      // Function name
	  CallerFuncName = f->getName().data();
	  std::cout << CallerFuncName << std::endl;
    }

    return true;
  }

private:
  CallGraph &callGraph;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class MyASTConsumer : public ASTConsumer {
public:
  MyASTConsumer(CallGraph &callGraph) : Visitor(callGraph) {}

  // Override the method that gets called for each parsed top-level
  // declaration.
  bool HandleTopLevelDecl(DeclGroupRef DR) override {
    for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
      // Traverse the declaration using our AST visitor.
      Visitor.TraverseDecl(*b);
    }
    return true;
  }

private:
  MyASTVisitor Visitor;
};

// For each source file provided to the tool, a new FrontendAction is created.
class MyFrontendAction : public ASTFrontendAction {
public:
  MyFrontendAction() {}
  void EndSourceFileAction() override {
      std::stringstream ss;
      //ss << "digraph callgraph {\n";
      for (auto i = callGraph.begin(); i != callGraph.end(); ++i){
          std::string CallerFuncName = i->first;
          std::set<std::string> CalleeSet = i->second;;
          for (auto j = CalleeSet.begin(); j != CalleeSet.end(); ++j){
              std::string CalleeFuncName = *j;

	      // Format to be graphically displayed at http://www.webgraphviz.com/
              //ss << "\"" << CallerFuncName << "\" -> \"" 
              //    << CalleeFuncName << "\";\n";
              ss << CallerFuncName << "," << CalleeFuncName << "\n";
          }
      }
      //ss << "}";
      std::ofstream Output(OutputFilename.c_str());
      if (Output.good()){
          Output << ss.str();
          Output.close();
      }else{
          llvm::outs() << ss.str();
      }
      callGraph.clear();
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override {
    return llvm::make_unique<MyASTConsumer>(callGraph);
  }

private:
  CallGraph callGraph;
};

int main(int argc, const char **argv) {
  CommonOptionsParser op(argc, argv, MyOptionCategory);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  // ClangTool::run accepts a FrontendActionFactory, which is then used to
  // create new objects implementing the FrontendAction interface. Here we use
  // the helper newFrontendActionFactory to create a default factory that will
  // return a new MyFrontendAction object every time.
  // To further customize this, we could create our own factory class.
  return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
}
