#include <set>
#include <sstream>
#include <fstream>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include "clang/Analysis/ProgramPoint.h"

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Frontend/CompilerInstance.h"

#include "clang/Basic/SourceManagerInternals.h"
#include "clang/Basic/SourceManager.h"

#include "clang/Basic/Version.inc"

#include "llvm/Support/raw_ostream.h"

using namespace clang;

namespace minissf{

class Record {
public:
  std::string qualifiedname;
  std::set<std::string> bases;

  std::string getBasesAsString() {
    std::string b = "";
    std::set<std::string>::iterator iter;
    for (iter = bases.begin(); iter != bases.end(); iter++) {
      b += (*iter) + ",";
    }
    if (b != "") b.erase(b.end() - 1); // remove the last comma

    return b;
  }
};

class Method {
public:
  std::string qualifiedname;       // method qualified name (namespace::class_name::function_name)
  //std::string name;                // method name
  std::list<std::string> params;   // paramters
  std::string rtype;               // return type of the method
  std::set<std::string> callexprs; // all function invocations inside the function body

  std::string parentclass;         // parent class name (namespace::class_name)
  std::set<std::string> bases;     // all base classes

  std::string filename;            // method source file name

  bool hasAttri;                   // use __attribute__((deprecated)) mark to mark starting procedure

  std::set<Method*> parents;       // all functions that calling this function
  std::set<Method*> childs;        // all functions that called by this function

  Method() {
    hasAttri = false;
  }

  // return the list of parameters as a string
  std::string getParamsAsString() {
    std::string p = "";
    p += "(";
    std::list<std::string>::iterator iter;
    for (iter = params.begin(); iter != params.end(); iter++) {
      std::string tmp = *iter;
      p += tmp + ",";
    }

    if (p != "(") p.erase(p.end() - 1); // remove the last comma
      p += ")";
    return p;
  }

  // return the method as a string
  std::string getMethodAsString() {
    std::string method = "";
    method += rtype + " " + qualifiedname + getParamsAsString();
    return method;
  }

  // return the call expressions as a string
  std::string getCallExprsAsString() {
    std::string callexpr = "";
    std::set<std::string>::iterator iter;
    for (iter = callexprs.begin(); iter != callexprs.end(); iter++) {
      std::string tmp = *iter;
      callexpr += tmp + "\n";
    }
    return callexpr;
  }
};

std::map<std::string, Method*> methods;
std::map<std::string, Record*> records;

class Annotate             : public ASTConsumer,
                             public RecursiveASTVisitor<Annotate> {
public:
  ASTContext *Context; // points to the AST context
  SourceManager *SM;   // contains information about source files
  FileID MainFileID;   // the main file we are modifying
  std::string inFile;  //input file name
  std::string outFile; //output file name
public:
  Annotate(std::string s, std::string o) : inFile(s), outFile(o) {}

  void Initialize(ASTContext &context) {
    //llvm::errs() << "initializing consumer\n";
    Context = &context;
    SM = &Context->getSourceManager();

    // Get the ID and start/end of the main file.
    MainFileID = SM->getMainFileID();
  }

  // visit a class/struct/enum node
  bool VisitCXXRecordDecl(CXXRecordDecl *D) {
    if(!D) return true;
    //llvm::errs() << "begin to visit record: " << D->getNameAsString() << "\n";
    SourceLocation Loc = D->getLocation();
    Loc = SM->getSpellingLoc(Loc);
    FileID ID = SM->getFileID(Loc);
    //llvm::errs() << "end to visit record: " << D->getNameAsString() << "\n";

    // we do not handle the declaration if it's in the system header file
    if (!SM->isInSystemHeader(Loc) && !SM->isInExternCSystemHeader(Loc)) {
      //llvm::errs() << ">>>>> visit record: " << D->getNameAsString() << "\n";
      Record *r = HandleRecordDecl(D);
      if (r) {
        records.insert(std::make_pair(r->qualifiedname, r));
      }

    } //else llvm::errs() << "uninteresting record: " << D->getNameAsString() << "\n";

    return true;
  }

  Record* HandleRecordDecl(CXXRecordDecl *D) {
    assert(D && "Class missing in HandleRecordDecl");
    if (!D->hasDefinition())
      return NULL;

    // skip duplication
    if(records.find(D->getQualifiedNameAsString()) != records.end()) return 0;

    Record *r = new Record;
    r->qualifiedname = D->getQualifiedNameAsString();
    //find all base classes
    //we skip all the template classes or there will be Assertion failed
    if (!D->getDescribedClassTemplate ()) {
      for (CXXRecordDecl::base_class_iterator iter = D->bases_begin();
           iter != D->bases_end(); ++iter) {
        if (iter) {
          QualType type = iter->getType();
          std::string tmp = type.getAsString();
          //remove "class "
          tmp.erase(0, 6);
          r->bases.insert(tmp);
        }
      }
    }

    return r;
  }
  // visit a function declaration
  bool VisitFunctionDecl(FunctionDecl *D) {
    if(!D) return true;
    //llvm::errs() << "begin to visit function declation: " << D->getNameAsString() << "\n";
    SourceLocation Loc = D->getLocation();
    Loc = SM->getSpellingLoc(Loc);
    //llvm::errs() << "end to visit function declation: " << D->getNameAsString() << "\n";

    if (!SM->isInSystemHeader(Loc) && !SM->isInExternCSystemHeader(Loc)) {
      //llvm::errs() << ">>>>> visit function declaration: " << D->getNameAsString() << "\n";
      if (CXXMethodDecl *MD = dyn_cast<CXXMethodDecl>(D)) {
        Method *m = HandleFunctionDecl(MD);
        if (m) methods.insert(std::make_pair(m->getMethodAsString(), m));
      }
    } //else llvm::errs() << "uninteresting function declation: " << D->getNameAsString() << "\n";

    return true;
  }

  // handle method declaration
  Method* HandleFunctionDecl(CXXMethodDecl *MD) {
    assert(MD && "method handle missing in HandleFunctionDecl");

    if (methods.find(MD->getQualifiedNameAsString()) != methods.end()) return 0;

    Method *m = new Method();

    m->qualifiedname = MD->getQualifiedNameAsString();

    QualType type = MD->getResultType();
    m->rtype = type.getAsString();

    // get function paramters
    for (FunctionDecl::param_iterator I = MD->param_begin(),
         E = MD->param_end(); I != E; ++I) {
      //llvm::errs() << "Function: " + FD->getNameAsString() <<" params number: " << FD->param_size () << "\"\n";
      if(ParmVarDecl *PD = dyn_cast<ParmVarDecl>(*I)) {
        QualType type = PD->getType();
        m->params.push_back(type.getAsString());
      }
      else assert(0); // case of interest!
    }

    //find all function calls and variables declaration in function body
    if (MD->hasBody()) {
      Stmt* body = MD->getBody();
      TraverseFunctionBody(body, m);
    }

    CXXRecordDecl *RD;
    RD = MD->getParent();
    m->parentclass = RD->getQualifiedNameAsString();

    //check __attribute__ ((annotate("ssf_starting_procedure")))
    if (MD->getAttr<AnnotateAttr>()) {
      //llvm::errs() << MD->getName() << " has annotate attribute\n";
      m->hasAttri = true;
    }

    m->filename = inFile;

    return m;
  }

  void TraverseFunctionBody(Stmt *S, Method *m) {
    // perform depth first traversal of all children
    for (Stmt::child_iterator CI = S->child_begin(),
         E = S->child_end(); CI != E; ++CI) {
      if (*CI) TraverseFunctionBody(*CI, m);
    }

    // if it's a function call, register it
    if (CallExpr *CE = dyn_cast<CallExpr>(S)) {
      FunctionDecl *fd = CE->getDirectCallee();
      if (fd != 0) {
        QualType type = fd->getResultType();
        std::string rtype = type.getAsString();
        std::string qname = fd->getQualifiedNameAsString();
        std::string param = "";
        param += "(";

        for (FunctionDecl::param_iterator I = fd->param_begin(),
             E = fd->param_end(); I != E; ++I) {
          if(ParmVarDecl *PD = dyn_cast<ParmVarDecl>(*I)) {
            QualType type = PD->getType();
            param += type.getAsString() + ",";
          }
          else assert(0); // case of interest!
        }

        if (param != "(") param.erase(param.end() - 1); // remove the last comma
        param += ")";
        std::string callee = rtype + " " + qname + param;
        m->callexprs.insert(callee);
      }
    }
  }

  // output information about each file and store it in a file, so
  // that we can do global analysis in the next round
  void OutputAnnotation() {
    std::fstream file;
    if (outFile == "") {
      size_t found = inFile.find(".");
      outFile = inFile.substr(0, found) + ".x";
    }
    file.open(outFile.c_str(), std::ios::out);

    if (file.is_open())	{
      for (std::map<std::string, Record*>::const_iterator iter = records.begin();
           iter != records.end(); ++iter) {
        file << "CLASS:" << iter->second->qualifiedname << "\n";
        if (iter->second->getBasesAsString() != "") {
          for (std::set<std::string>::iterator base_iter = iter->second->bases.begin();
               base_iter != iter->second->bases.end(); base_iter++)
            file << "BASE:" << *base_iter << "\n";
        }
      }

      for (std::map<std::string, Method*>::const_iterator m_iter = methods.begin();
           m_iter != methods.end(); ++m_iter) {
        file << "METHOD:" << m_iter->second->getMethodAsString() << "\n";
        file << "RETURN:" << m_iter->second->rtype << "\n";
        file << "METHODNAME:" << m_iter->second->qualifiedname << "\n";
        if (m_iter->second->getParamsAsString() != "()") {
          for (std::list<std::string>::iterator p_iter = m_iter->second->params.begin();
               p_iter != m_iter->second->params.end(); p_iter++)
            file << "PARAM:" << *p_iter << "\n";
        }
        if (m_iter->second->hasAttri)
          file << "HASATTRI" << "\n";
        file << "PARENT:" << m_iter->second->parentclass << "\n";
        if (m_iter->second->getCallExprsAsString() != "") {
          for (std::set<std::string>::iterator c_iter = m_iter->second->callexprs.begin();
               c_iter != m_iter->second->callexprs.end(); c_iter++)
            file << "FUNCTION CALL:" << *c_iter << "\n";
        }
      }
    }

    file.close();
  }

  // this method is called to analyze the entire AST
  void HandleTranslationUnit(ASTContext &Ctx) {
    // the visitor functions will be invoked by this
    //llvm::errs() << "handling translation unit\n";
    TraverseDecl(Ctx.getTranslationUnitDecl());
    //llvm::errs() << "DONE VISITING\n";

    OutputAnnotation();
    //llvm::errs() << "DONE ANNOTATING\n";
  }
};

class AnnotateAction : public PluginASTAction {
protected:
  ASTConsumer *CreateASTConsumer(CompilerInstance &CI, llvm::StringRef S) {
    //llvm::errs() << "creating consumer for file: " << S <<  "\n";
    // for handle multiple files clear the buff first
    records.clear();
    methods.clear();

    FrontendOptions &FOpts = CI.getFrontendOpts();
    std::string outFile = FOpts.OutputFile;

    return new Annotate(S, outFile);
  }

  // parse the arguments passed to this plugin
  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string>& args) {
    //llvm::errs() << "parsing arguments: size = " << args.size() << "\n";
    for (unsigned i = 0, e = args.size(); i != e; ++i) {
      //llvm::errs() << "PrintFunctionNames arg = " << args[i] << "\n";

      // Example error handling.
      if (args[i] == "-an-error") {
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
    	DiagnosticsEngine &D = CI.getDiagnostics();
    	unsigned DiagID = D.getCustomDiagID(DiagnosticsEngine::Error, "invalid argument '" + args[i] + "'");
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
        Diagnostic &D = CI.getDiagnostics();
        unsigned DiagID = D.getCustomDiagID(Diagnostic::Error, "invalid argument '" + args[i] + "'");
#else
#error  "clang version not supported!"
#endif
        D.Report(DiagID);
        return false;
      }
    }
    if (args.size() && args[0] == "help")
      PrintHelp(llvm::errs());

    return true;
  }
  void PrintHelp(llvm::raw_ostream& ros) {
    ros << "Help for PrintFunctionNames plugin goes here\n";
  }
};

}

static FrontendPluginRegistry::Add<minissf::AnnotateAction>
X("minissf-annotate", "source code annotate for minissf");



