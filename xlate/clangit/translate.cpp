#include <set>
#include <sstream>
#include <fstream>
#include <iostream>

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/AST.h"
#include "clang/AST/ParentMap.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Support/MemoryBuffer.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include "clang/Analysis/ProgramPoint.h"
#include "clang/Basic/SourceManagerInternals.h"

#include "clang/Basic/Version.inc"

#include "clang/Frontend/FrontendOptions.h"

#if (CLANG_VERSION_MAJOR == 3 && CLANG_VERSION_MINOR == 2)
#include "clang/Rewrite/Core/Rewriter.h"
#else
#include "clang/Rewrite/Rewriter.h"
#endif

using namespace clang;

namespace minissf{

class Record;

class Function {
public:
  FunctionDecl *FD; // function declaration
  std::set<ParmVarDecl*> params; // a list of parameters of this function
  std::set<CallExpr*> callexprs; // all function invocations inside the function body
  std::map<VarDecl*,int> vars; // std::mapping from the variable declarations to their scoping index

  std::set<Function*> parents; // all functions that calling this function
  std::set<Function*> childs; // all functions that called by this function

  bool isProcedure; // std::set to be true if this function is detected as procedure
  bool candidate; //all the parents of function call wait of semwait should be candidate procedure
  bool visit; //used for avoid infinate loop

  // constructor
  Function() : FD(0), isProcedure(false) {}

  // return function  name
  std::string getName();

  // return full qualified name (class_name::function_name)
  std::string getFullName();

  // return qualified name (namespace::class_name::function_name)
  std::string getQualifiedName();

  // if this is a member function, return the name of the class;
  // otherwise, return an empty string
  std::string getClassName();

  // whether the function is virtual
  bool isVirtual();

  //check if a method is derived from a class(class name should be qualified name)
  bool isDerivedFrom(std::string classname, std::map<CXXRecordDecl*,Record*> records);

  // where this function is a starting procedure
  // start procedrue:
  // 1)virtual void action() method of a class being derived from minissf::Process
  // 2)void foo(Process* p) __attribute__((ssf_starting_procedure))
  //   user marked starting procedure
  //   I will check: __attribute__((ssf_starting_procedure))
  //                 format void funtion_name (Process* )
  //   XXX do I need to check base class and if this function call procedure??
  bool isStartingProcedure(std::map<CXXRecordDecl*,Record*> records);

  // whether this function is a process action method
  bool isAction(std::map<CXXRecordDecl*,Record*> records);

  // return the return type of the function as a string
  std::string getReturnTypeAsString();

  // return the list of parameters as a string
  std::string getParamsAsString();

  // return all the function invocations as a string
  std::string getCallExprsAsString(SourceManager *SM);

  // return all variable declarations as a string
  std::string getVarsAsString();

  // return function as a string
  std::string getFunctionAsString();
};

class Record {
public:
  CXXRecordDecl *RD; // record declaration
  std::set<CXXBaseSpecifier*> bases; // a list of all base classes of this record
  std::set<FieldDecl*> attributes; // a list of all attributes within the record
  std::set<FunctionDecl*> methods; // a list of all member functions of this record

  // return the name of the record
  std::string getName();

  // return the qualified name (namespace::class_name)
  std::string getQualifiedName();

  // return base class of the record
  std::string getBaseClassAsString();

  // return all the attributes as a string
  std::string getAttributesAsString();

  // return all member functions as a tring
  std::string getMethodsAsString(std::map<FunctionDecl*,Function*>functions);
};

// this class contains all the information related to a function declaration

std::string Function::getName() {
  assert(FD && "Function missing in getName()");
  return FD->getNameAsString();
}

// return full qualified name (class_name::function_name)
std::string Function::getFullName() {
  if(getClassName() != "") return getClassName()+"::"+getName();
  else return getName();
}

// return qualified name (namespace::class_name::function_name)
std::string Function::getQualifiedName() {
  return FD->getQualifiedNameAsString();
}

// if this is a member function, return the name of the class;
// otherwise, return an empty string
std::string Function::getClassName() {
  std::string classname = "";
  if (CXXMethodDecl *MD = dyn_cast<CXXMethodDecl>(FD)) {
    CXXRecordDecl *RD = MD->getParent();
    classname = RD->getNameAsString();
  }
  return classname;
}

// whether the function is virtual
bool Function::isVirtual() {
  assert(FD && "Function missing in isVirtual()");
  return FD->isVirtualAsWritten();
}

//check if a method is derived from a class(class name should be qualified name and not the system class)
bool Function::isDerivedFrom(std::string classname, std::map<CXXRecordDecl*,Record*> records) {
  assert(FD && "Function missing in isDerivedFrom");

  //check if this function is a method
  if (CXXMethodDecl *MD = dyn_cast<CXXMethodDecl>(FD)) {
    CXXRecordDecl *rd1 = MD->getParent();
    //we skip all the template classes or there will be Assertion failed
    if (!rd1 || rd1->getDescribedClassTemplate ()) return false;

    CXXRecordDecl *rd2 = NULL;
    for(std::map<CXXRecordDecl*,Record*>::iterator iter = records.begin();
      iter != records.end(); iter++) {
      Record *r = (*iter).second;
        if (r->getQualifiedName() == classname) {
          rd2 = r->RD;
          break;
        }
    }

    if (rd2) return rd1->isDerivedFrom(rd2);
    else return false;
  }
  else return false;
}

// where this function is a starting procedure
bool Function::isStartingProcedure(std::map<CXXRecordDecl*,Record*> records) {
  //case1 action function
  if (isAction(records))
    return true;
  //case2 user marks start procedure
  ////check __attribute__ ((annotate("ssf_starting_procedure")))
  if (FD->getAttr<AnnotateAttr>() &&
      getReturnTypeAsString() == "void" && //check the format
      params.size() == 1 &&
      (*(params.begin()))->getType().getAsString() ==  "class minissf::Process *" &&
      isDerivedFrom("minissf::ProcedureContainer", records)
     )
    return true;

  return false;
}

// whether this function is a process action method
bool Function::isAction(std::map<CXXRecordDecl*,Record*> records) {
  // XXX: we should check whether this function is a method of the
  // class of minissf::Process or its derived class
  // fixed
  if (getReturnTypeAsString() == "void" &&
      getName() == "action" &&
      getParamsAsString() == "()" &&
      isDerivedFrom("minissf::Process", records))
    return true;
  else return false;
}

// return the return type of the function as a string
std::string Function::getReturnTypeAsString() {
  assert(FD && "Function missing in getReturnTypeAsString()");
  QualType rType = FD->getResultType();
  return rType.getAsString();
}

// return the list of parameters as a string
std::string Function::getParamsAsString() {
  std::string p = "";
  p += "(";
  std::set<ParmVarDecl*>::iterator iter;
  for (iter = params.begin(); iter != params.end(); iter++) {
    ParmVarDecl* PD = *iter;
    if (PD) {
      QualType type = PD->getType();
      std::string name = PD->getName();
      p += type.getAsString() + ",";
    } else assert(0); // XXX: how could it be null?
  }

  if (p != "(") p.erase(p.end() - 1); // remove the last comma
    p += ")";
  return p;
}

// return all the function invocations as a string
std::string Function::getCallExprsAsString(SourceManager *SM) {
  std::string call = "";
  std::set<CallExpr*>::iterator iter;
  for (iter = callexprs.begin(); iter != callexprs.end(); iter++) {
    FunctionDecl * fd = (*iter)->getDirectCallee();
    if (fd != 0)
    call += "\t\tCALLEE: " + fd->getNameAsString() + "\n";
  }
  return call;
}

// return all variable declarations as a string
std::string Function::getVarsAsString() {
  std::string v = "";
  std::map<VarDecl*, int>::const_iterator iter;
  for (iter=vars.begin(); iter != vars.end(); ++iter) {
    std::string type = iter->first->getType().getAsString();
    std::string name = iter->first->getNameAsString();
    int lv = iter->second;
    std::stringstream out;
    out << "\t\t" << type << " " << name << " scoping:" << lv << "\n";
    v += out.str();
  }
  return v;
}

// return function as a string
std::string Function::getFunctionAsString() {
  std::string func = "";
  func += getReturnTypeAsString() + " " + getQualifiedName() + getParamsAsString();
  return func;
}

// return the name of the record
std::string Record::getName() {
  return RD->getNameAsString();
}

// return the qualified name (namespace::class_name)
std::string Record::getQualifiedName() {
  return RD->getQualifiedNameAsString();
}


// return base class of the record
std::string Record::getBaseClassAsString() {
  std::string b = "";
  std::set<CXXBaseSpecifier*>::iterator iter;
  for (iter = bases.begin(); iter != bases.end(); iter++) {
    QualType type = (*iter)->getType();
    b += type.getAsString() + ",";
  }
  if (b != "") b.erase(b.end() - 1); // remove the last comma
   return b;
}

// return all the attributes as a string
std::string Record::getAttributesAsString() {
  std::string s = "";
  std::set<FieldDecl*>::iterator iter;
  for (iter = attributes.begin(); iter != attributes.end(); iter++) {
    std::string type = (*iter)->getType().getAsString();
    std::string name = (*iter)->getNameAsString();
    s += "\t" + type + " " + name + "\n";
  }
  return s;
}

// return all member functions as a tring
std::string Record::getMethodsAsString(std::map<FunctionDecl*,Function*>functions) {
  std::string s = "";
  std::set<FunctionDecl*>::iterator iter1;
  for (iter1 = methods.begin(); iter1 != methods.end(); iter1++) {
    FunctionDecl *fd = *iter1;
    std::map<FunctionDecl*,Function*>::iterator iter2 = functions.find(fd);
    if(iter2 != functions.end()) {
      // the member function must be in the list of all functions we know already
      s += "METHOD:\n\t" +
      (*iter2).second->getReturnTypeAsString() + " " +
      (*iter2).second->getName() + " " +
      (*iter2).second->getParamsAsString() + "\n";
    } //else assert(0);
  }
  return s;
}

std::set<std::string> procedure;

class SourceCodexlateConsumer : public ASTConsumer,
                                public RecursiveASTVisitor<SourceCodexlateConsumer> {
public:
  std::map<FunctionDecl*,Function*> functions; // std::map from function declaration handle to the function
  std::map<CXXRecordDecl*,Record*> records; // std::map from record declaration handle to the record

  Rewriter Rewrite; // for rewriting source code

#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
  DiagnosticsEngine &Diags; // for reporting errors
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
  Diagnostic &Diags; // for reporting errors
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
#error  "clang version not supported!"
#else
#error  "clang version not supported!"
#endif

  const LangOptions &LangOpts; // for language options
  unsigned RewriteFailedDiag; // ???

  std::string InFileName; // source file name
  std::string OutFile; // the output file name

  ASTContext *Context; // points to the AST context
  SourceManager *SM; // contains information about source files
  TranslationUnitDecl *TUDecl; // The top declaration context
  FileID MainFileID; // the main file we are modifying

  const char *MainFileStart, *MainFileEnd; // points to the buffer containing the main file

  // This std::maps an original source AST to it's rewritten form. This
  // allows us to avoid rewriting the same node twice (which is very
  // uncommon).  This is needed to support some of the exotic property
  // rewriting.
  llvm::DenseMap<Stmt*, Stmt*> ReplacedNodes;

  bool SilenceRewriteMacroWarning; // true if we want to print out rewrite errors
  // array of wait functions name
  std::string waitfunctions[7];

  bool prettyprint;

  //map to storage all modification in macro
  typedef std::map<unsigned, std::pair<std::string, std::string> > RewriteInfoMap;
  typedef std::map<unsigned, RewriteInfoMap> MacroInfoMap;
  MacroInfoMap macro;

public:
  // the constructor
  SourceCodexlateConsumer(std::string inFile, std::string outFile,
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
		  	  	  	  	  DiagnosticsEngine &D, const LangOptions &LOpts, bool silenceMacroWarn)
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
  	  	  	  	  	  	  Diagnostic &D, const LangOptions &LOpts, bool silenceMacroWarn)
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
#error  "clang version not supported!"
#else
#error  "clang version not supported!"
#endif
     : Diags(D), LangOpts(LOpts), InFileName(inFile), OutFile(outFile),
     SilenceRewriteMacroWarning(silenceMacroWarn) {
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
	RewriteFailedDiag = Diags.getCustomDiagID(DiagnosticsEngine::Warning,
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
    RewriteFailedDiag = Diags.getCustomDiagID(Diagnostic::Warning,
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
#error  "clang version not supported!"
#else
#error  "clang version not supported!"
#endif
                            "rewriting sub-expression within a macro (may not be correct)");

    waitfunctions[0] = "minissf::Process::waitOn";
    waitfunctions[1] = "minissf::Process::waitOnFor";
    waitfunctions[2] = "minissf::Process::waitFor";
    waitfunctions[3] = "minissf::Process::waitUntil";
    waitfunctions[4] = "minissf::Process::waitForever";
    waitfunctions[5] = "minissf::Process::suspendForever";
    waitfunctions[6] = "minissf::Process::waitOnUntil";

    prettyprint = false;
  }

  // the destructor: write the stuff out
  ~SourceCodexlateConsumer() {
    // Get the buffer corresponding to MainFileID.  If we haven't
    // changed it, then we are done.
      //XXX: supposed to write out to 'OutFile'
    if (OutFile == "") {
          size_t found = InFileName.find(".");
          std::string s = InFileName;
          s.replace(found, 1, "_xlate.");
          OutFile = s;
    }


    if (const RewriteBuffer *RewriteBuf = Rewrite.getRewriteBufferFor(MainFileID)) {
      std::string S(RewriteBuf->begin(), RewriteBuf->end());
      std::fstream file;

      file.open(OutFile.c_str(), std::ios::out);
      if (file.is_open())
        file << S.c_str();
      file.close();

      RewriteMacro(OutFile.c_str());
      /*
      MacroInfoMap::iterator iter1;
      RewriteInfoMap::iterator iter2;
      for (iter1 = macro.begin(); iter1 != macro.end(); iter1++) {
        llvm::errs() << "line: " << iter1->first << "\n";
        for (iter2 = iter1->second.begin(); iter2 != iter1->second.end(); iter2++) {
          llvm::errs() << "colum: " << iter2->first
                       << " old:" << (iter2->second).first
                       << " new:" << (iter2->second).second << "\n";
        }
      }*/
    } else {
      printf("No changes\n");
      //XXX: copy from main file to the out file

      std::ifstream f1(InFileName.c_str(), std::fstream::binary);
      std::ofstream f2(OutFile.c_str(), std::fstream::trunc | std::fstream::binary);
      f2 << f1.rdbuf();
    }
  }

  void Initialize(ASTContext &context) {
    //llvm::errs() << "initializing consumer\n";
    Context = &context;
    SM = &Context->getSourceManager();
    TUDecl = Context->getTranslationUnitDecl();

    // Get the ID and start/end of the main file.
    MainFileID = SM->getMainFileID();

    const llvm::MemoryBuffer *MainBuf = SM->getBuffer(MainFileID);
    MainFileStart = MainBuf->getBufferStart();
    MainFileEnd = MainBuf->getBufferEnd();
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
    Rewrite.setSourceMgr(Context->getSourceManager(), Context->getLangOpts());
#else
    Rewrite.setSourceMgr(Context->getSourceManager(), Context->getLangOptions());
#endif
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
        records.insert(std::make_pair(r->RD, r));
      }
    } //else llvm::errs() << "uninteresting record: " << D->getNameAsString() << "\n";
    return true;
  }

  Record* HandleRecordDecl(CXXRecordDecl *D) {
    assert(D && "Class missing in HandleRecordDecl");
    if (!D->hasDefinition())
      return NULL;

    // skip duplication
    if(records.find(D) != records.end()) return 0;

    Record *r = new Record;
    r->RD = D;
    //find all base classes
    //we skip all the template classes or there will be Assertion failed
    if (!D->getDescribedClassTemplate ()) {
      for (CXXRecordDecl::base_class_iterator BI = D->bases_begin(),
        E = D->bases_end(); BI != E; ++BI) {
        if (BI) {
          r->bases.insert(BI);
        }
      }
    }

    // find all member variables
    for (RecordDecl::field_iterator FI = D->field_begin(),
      E = D->field_end(); FI != E; ++FI) {
      FieldDecl *FD = dyn_cast<FieldDecl>(*FI);
      if (FD) {
        r->attributes.insert(FD);
      }
    }

    // find all member functions
    for (CXXRecordDecl::method_iterator  MI = D->method_begin(),
      E = D->method_end(); MI != E; ++MI) {
      FunctionDecl *MD = dyn_cast<FunctionDecl>(*MI);
      if(MD) {
        r->methods.insert(MD);
      }

      //llvm::errs() << "method: " << MD->getResultType().getAsString() << " " << MD->getNameAsString() << "\n";
      Function *f = HandleFunctionDecl(MD);

      if(f) functions.insert(std::make_pair(f->FD,f));
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
      Function *f = HandleFunctionDecl(D);
      if (f) functions.insert(std::make_pair(f->FD,f));
    } //else llvm::errs() << "uninteresting function declation: " << D->getNameAsString() << "\n";

    //if (SM->isFromMainFile(Loc)) {
      //if (D->hasAttrs())
        //llvm::errs() << "Function: " << D->getNameAsString() << " has attributes\n";
    //}
    return true;
  }

  Function* HandleFunctionDecl(FunctionDecl *FD) {
    assert(FD && "Function missing in HandleFunctionDecl");
    if(functions.find(FD) != functions.end()) return 0;

    Function *f = new Function();
    f->FD = FD;

    // get function paramters
    for (FunctionDecl::param_iterator I = FD->param_begin(),
      E = FD->param_end(); I != E; ++I) {
      //llvm::errs() << "Function: " + FD->getNameAsString() <<" params number: " << FD->param_size () << "\"\n";
      if(ParmVarDecl *PD = dyn_cast<ParmVarDecl>(*I))
        f->params.insert(PD);
      else assert(0); // case of interest!
    }

    //find all function calls and variables declaration in function body
    if (FD->hasBody()) {
      Stmt* body = FD->getBody();
      int index = 0;
      TraverseFunctionBody(body, f, index);
    }
    return f;
  }

  void TraverseFunctionBody(Stmt *S, Function *f, int &lv) {
    // perform depth first traversal of all children
    for (Stmt::child_iterator CI = S->child_begin(),
      E = S->child_end(); CI != E; ++CI) {
      if (*CI) TraverseFunctionBody(*CI, f, lv);
    }

    // if it's a function call, register it
    if (CallExpr *CE = dyn_cast<CallExpr>(S))
      f->callexprs.insert(CE);
    else if (DeclStmt *DS = dyn_cast<DeclStmt>(S)) {
      // look for all variable declarations
      for (DeclStmt::decl_iterator DI = DS->decl_begin(),
        DE = DS->decl_end(); DI != DE; ++DI) {
        Decl *SD = *DI;
        if (VarDecl *VD = dyn_cast<VarDecl>(SD)) {
          //llvm::errs() << "Variables: " << VD->getType().getAsString() << " " << VD->getNameAsString() << " level: " << lv << "\n";
          f->vars[VD] = lv++;
        }
      }
    }
  }

  /*-------------------------------------------------------------------------*/
  /* Rewrite functions                                                       */
  /*-------------------------------------------------------------------------*/
  // replace a statement
  void ReplaceStmt(Stmt *Old, Stmt *New) {
    Stmt *ReplacingStmt = ReplacedNodes[Old];
    if (ReplacingStmt) return; // We can't rewrite the same node twice

    // If replacement succeeded or warning disabled return with no warning.
    if (!Rewrite.ReplaceStmt(Old, New)) { // zero means success
      ReplacedNodes[Old] = New; // add to the list already rewritten
      return;
    }

    if (!SilenceRewriteMacroWarning)
      Diags.Report(Context->getFullLoc(Old->getLocStart()), RewriteFailedDiag)
            << Old->getSourceRange();
  }

  // replace a statement within a range
  void ReplaceStmtWithRange(Stmt *Old, Stmt *New, SourceRange SrcRange) {
    // Measaure the old text.
    int Size = Rewrite.getRangeSize(SrcRange);
    if (Size == -1) {
      Diags.Report(Context->getFullLoc(Old->getLocStart()), RewriteFailedDiag)
            << Old->getSourceRange();
      return;
    }
    // Get the new text.
    std::string SStr;
    llvm::raw_string_ostream S(SStr);
#if (CLANG_VERSION_MAJOR == 3 && CLANG_VERSION_MINOR == 2)
    New->printPretty(S, 0,  PrintingPolicy(LangOpts));
#else
    New->printPretty(S, *Context, 0, PrintingPolicy(LangOpts));
#endif
    const std::string &Str = S.str();

    // If replacement succeeded or warning disabled return with no warning.
    if (!Rewrite.ReplaceText(SrcRange.getBegin(), Size, Str)) {
      ReplacedNodes[Old] = New;
      return;
    }
    if (!SilenceRewriteMacroWarning)
      Diags.Report(Context->getFullLoc(Old->getLocStart()), RewriteFailedDiag)
            << Old->getSourceRange();
  }

  // insert text at source location
  void InsertText(SourceLocation Loc, const char *StrData, unsigned StrLen, bool InsertAfter = true) {
    // If insertion succeeded or warning disabled return with no warning.
    if (!Rewrite.InsertText(Loc, llvm::StringRef(StrData, StrLen),InsertAfter) || SilenceRewriteMacroWarning)
      return;
      Diags.Report(Context->getFullLoc(Loc), RewriteFailedDiag);
  }

  // remove text at source location
  void RemoveText(SourceLocation Loc, unsigned StrLen) {
    // If removal succeeded or warning disabled return with no warning.
    if (!Rewrite.RemoveText(Loc, StrLen) || SilenceRewriteMacroWarning)
      return;

    Diags.Report(Context->getFullLoc(Loc), RewriteFailedDiag);
  }

  // replace text at specified location
  void ReplaceText(SourceLocation Start, unsigned OrigLength, const char *NewStr, unsigned NewLength) {
    // If removal succeeded or warning disabled return with no warning.
    if (!Rewrite.ReplaceText(Start, OrigLength, llvm::StringRef(NewStr, NewLength)) ||
        SilenceRewriteMacroWarning)
      return;

    Diags.Report(Context->getFullLoc(Start), RewriteFailedDiag);
  }

  //after inserting use this function to add line number
  void InsertLineNum(SourceLocation loc) {
    PresumedLoc presumeloc= SM->getPresumedLoc(loc);
    std::stringstream out;
    out << "\n#line " << presumeloc.getLine () << " " << '"' << presumeloc.getFilename ()  << '"' << "\n";
    //out << "\n#line " << linenum << "\n";
    std::string buf = out.str();
    InsertText(loc, buf.c_str(), buf.size());
  }

  // rewrite the declaration of the procedure
  void RewriteProcedureDeclare(Function *f) {
    assert(f && "Function missing in RewriteProcedureDeclare");
    assert(f->FD && "Function missing in RewriteProcedureDeclare");
    if (!f->isProcedure ) return;

    FunctionDecl *FD = f->FD;
    if (FD->isThisDeclarationADefinition()) return; // do nothing if this is a defintion

    /*
     * Translate the declare of the function (a procedure)
     *   void serve(list of parameters);
     *  translate to -------------------------->
     *   minissf::Procedure* _ssf_create_procedure_serve(list of parameters);
     *   void serve(minissf::Process* p);
     */
    SourceLocation LocDeclStart = FD->getLocStart();
    SourceLocation LocDeclEnd = FD->getLocEnd();
    const char *startBuf = SM->getCharacterData(LocDeclStart);

    // find the beginning of the line that contains this functiond declaration
    const char *sbuf = startBuf;
    while (*sbuf && *sbuf != '\n') sbuf--;

    // get the source location of the beginning of the line
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
    SourceLocation LocFucDeclStart = LocDeclStart.getLocWithOffset(sbuf - startBuf + 1);
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
    SourceLocation LocFucDeclStart = LocDeclStart.getFileLocWithOffset(sbuf - startBuf + 1);
#else
#error  "clang version not supported!"
#endif
    std::string buff = "";
    if (prettyprint)
      buff += "\n  //------------------------ EMBEDDED CODE STARTS (Procedure Declaration: " + f->getName() + ")----------------------------//\n";
    buff += "  ";
    if (f->isVirtual()) buff += "virtual ";
    buff +=  "minissf::Procedure* _ssf_create_procedure_" + f->getName() + "(";
    //check starting procedure
    //contain only one argument and the argument
    //must be a pointer to the Process class.
    if (f->isAction(records) ||
        (f->params.size() == 1 &&
        (*f->params.begin())->getType().getAsString() == "class minissf::Process")) buff += ");"; // XXX: how to make sure this is the starting procedure??? fixed
    else {
      for (std::set<ParmVarDecl*>::iterator iter = f->params.begin(); iter != f->params.end();iter++) {
        ParmVarDecl* PD = *iter;
        std::string arg_type = PD->getType().getAsString();
        std::string arg_name = PD->getName();
        buff += arg_type + " " + arg_name + ", ";
      }
      buff += "void* retaddr = 0);";
    }
    InsertText(LocFucDeclStart, buff.c_str(), buff.size());
    InsertLineNum(LocFucDeclStart);

    //output the function declaration
    if (f->isAction(records)) buff = "  void action();";
    else buff = "  void " + f->getName() + "(minissf::Process*);";

    if (prettyprint)
      buff += "\n  //------------------------ EMBEDDED CODE ENDS (Procedure Declaration: " + f->getName() + ")----------------------------//\n";

    const char *lbuf = startBuf;
    while (*lbuf && *lbuf != '\n') lbuf--;
    lbuf++;
    const char *rbuf = startBuf;
    while (*rbuf && *rbuf != ';') rbuf++;
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
    SourceLocation LocFucParamDeclStart = LocDeclStart.getLocWithOffset(lbuf - startBuf);
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
    SourceLocation LocFucParamDeclStart = LocDeclStart.getFileLocWithOffset(lbuf - startBuf);
#else
#error  "clang version not supported!"
#endif
    ReplaceText(LocFucParamDeclStart, rbuf-lbuf+1, buff.c_str(), buff.size());

  }

  // rewrite the defination of the procedure
  void RewriteProcedureDefine(Function *f) {
    assert(f && "Function missing in RewriteProcedureDefine");
    assert(f->FD && "Function missing in RewriteProcedureDefine");
    if(!f->isProcedure) return;

    FunctionDecl *FD = f->FD;
    if (!FD->isThisDeclarationADefinition()) return; // do nothing if this is not a definition

    SourceLocation LocDeclStart = FD->getLocStart();
    SourceLocation LocDeclEnd = FD->getLocEnd();

    std::string buff = "";
    if (prettyprint)
      buff += "\n//------------------------ EMBEDDED CODE STARTS (ProcedureDefine: " +
               f->getFullName() + ") ----------------------------//\n";
    buff += "class _ssf_procedure_" + f->getClassName() + "_" + f->getName() + " : public minissf::Procedure { ";
    if (prettyprint)
      buff += "\n";
    buff += "public: ";
    if (prettyprint)
      buff +="\n";

    // all parameters are procedure variables
    for (std::set<ParmVarDecl*>::iterator iter = f->params.begin(); iter != f->params.end(); iter++) {
      ParmVarDecl* PD = *iter;
      std::string arg_type = PD->getType().getAsString();
      std::string arg_name = PD->getName();
      buff += "  " + arg_type + " " + arg_name + "; ";
      if (prettyprint)
       buff += "// function parameter\n";
    }

    // local variables are procedure variables too
    for (std::map<VarDecl*, int>::const_iterator iter = f->vars.begin(); iter != f->vars.end(); ++iter) {
      std::string type = iter->first->getType().getAsString();
      std::string name = iter->first->getNameAsString();
      int level = iter->second;
      std::stringstream out;

      // need to handle array variable declaration here
      // int a[10] => type = int [10], name = a
      size_t begin, end;
      begin = type.find_first_of("[");
      end = type.find_last_of("]");

      // need to handle struct varibable declaration here
      // struct timeval t1 => type = struct struct timeval, name = t1;
      if (type.find("struct") != std::string::npos) {
    	 size_t it = type.find("struct");
    	 type.erase(it, 6);
      }

      if (begin != std::string::npos &&
          end != std::string::npos) {
          std::string array = type.substr(begin, end - begin + 1);
          type = type.substr(0, begin);

          out << "  " << type << " " << name << "_" << level << array << "; ";
      }
      else
    	  out << "  " << type << " " << name << "_" << level << "; ";

      if (prettyprint)
        out << "// local variable\n";
      buff += out.str();
    }

    //temp variables for procedure call return
    int i = 0;
    for (std::set<CallExpr*>::iterator iter = f->callexprs.begin(); iter != f->callexprs.end(); ++iter) {
      CallExpr *CE = (*iter);
      if (isWaitFunctionCall(CE) || isProcedureCall(CE) || isSemWaitCall(CE)) {
        FunctionDecl *FD = CE->getDirectCallee();
        if (FD) {
          QualType rtype = FD->getResultType();
          if (rtype.getAsString() != "void") {
              std::stringstream out;
              out << "  " << rtype.getAsString() << " " << "tmp" << i << "; ";
              if (prettyprint)
                out << "// tmp variable\n";
              buff += out.str();
              i++;
          }
        }
      }
    }

    // the constructor for the procedure class
    if (f->isAction(records)) {
      //buff += "  _ssf_procedure_" + f->getClassName() + "_" + f->getName() + "(minissf::Process* _ssf_focus) :";
      buff += "  _ssf_procedure_" + f->getClassName() + "_" + f->getName() + "() :";
      if (prettyprint)
        buff += "\n";
      //buff += "    minissf::Procedure(_ssf_focus) {};";
      buff += "  minissf::Procedure(0, 0) {};";
      if (prettyprint)
        buff += "\n";
    }
    else {
      buff += "  _ssf_procedure_" + f->getClassName() + "_" + f-> getName() + "(minissf::ProcedureContainer* _ssf_focus, ";
      for (std::set<ParmVarDecl*>::iterator iter = f->params.begin(); iter != f->params.end(); iter++) {
        ParmVarDecl* PD = *iter;
        std::string arg_type = PD->getType().getAsString();
        std::string arg_name = PD->getName();
        buff += arg_type + " _ssf_local_" + arg_name + ", ";
      }
      buff += "void* _ssf_retaddr) : ";
        if (prettyprint)
          buff += "\n";
      buff += "    minissf::Procedure((ProcedureFunction)";
      buff += "&" + f->getFullName() + ", ";
      if (f->getReturnTypeAsString() != "void")
        buff += "_ssf_focus, _ssf_retaddr, sizeof(" + f->getReturnTypeAsString() + "))";
      else buff += "_ssf_focus)";
      for (std::set<ParmVarDecl*>::iterator iter = f->params.begin(); iter != f->params.end(); iter++) {
        ParmVarDecl* PD = *iter;
        std::string arg_name = PD->getName();
        buff += ", " + arg_name + "(_ssf_local_" + arg_name + ")";
      }
      buff += "{}";
      if (prettyprint)
        buff += "\n";
    }
    buff += "}; ";
    if (prettyprint)
      buff += "\n";

    // _ssf_create_procedure_??? function
    if (f->getClassName() != "" && !FD->isInlined())
      buff += "minissf::Procedure* " + f->getClassName() + "::_ssf_create_procedure_" + f->getName() + "(";
    else
      buff += "minissf::Procedure* _ssf_create_procedure_" + f->getName() + "(";
    if (f->isAction(records)) {
      buff += ") {";
      if (prettyprint)
        buff += "\n";
      //buff += "  return new _ssf_procedure_" + f->getClassName() + "_action(this); ";
      buff += "  return new _ssf_procedure_" + f->getClassName() + "_action(); ";
    }
    else {
      for (std::set<ParmVarDecl*>::iterator iter = f->params.begin(); iter != f->params.end(); iter++) {
        ParmVarDecl *PD = *iter;
        std::string arg_type = PD->getType().getAsString();
        std::string arg_name = PD->getName();

        buff += arg_type + " " + arg_name + ", ";
      }
      buff += "void* retaddr) {";
      if (prettyprint)
        buff += "\n";

      buff += "  return new _ssf_procedure_" + f->getClassName() + "_" + f->getName() + "(this, ";
      for (std::set<ParmVarDecl*>::iterator iter = f->params.begin(); iter != f->params.end(); iter++) {
        ParmVarDecl* PD = *iter;
        std::string arg_type = PD->getType().getAsString();
        std::string arg_name = PD->getName();
        buff += arg_name + ", ";
      }
      buff += "retaddr); ";
    }
    if (prettyprint)
      buff += "\n";
    buff += "}";
    if (prettyprint)
      buff += "\n";

    // find the start source location of the function definition
    const char *startBuf = SM->getCharacterData(LocDeclStart);
    const char *sbuf = startBuf;
    while (*sbuf && *sbuf != '\n') sbuf--;
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
    SourceLocation LocFucDeclStart = LocDeclStart.getLocWithOffset(sbuf - startBuf + 1);
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
    SourceLocation LocFucDeclStart = LocDeclStart.getFileLocWithOffset(sbuf - startBuf + 1);
#else
#error  "clang version not supported!"
#endif
    InsertText(LocFucDeclStart, buff.c_str(), buff.size());
    InsertLineNum(LocFucDeclStart);

    // translate the body of the function
    // rewrite function body
    if(FD->hasBody()) {
      Stmt* body = FD->getBody();
      //llvm::errs() << "FUNCTION: " << f->getName() << "\n";
      //body->dump();
      int i = 1;
      int j = 0;

      ParentMap *parentmap = new ParentMap(body);
      //we do 2 step:
      //first step modify varibale declaration reference and return statement
      //second step modify call reference
      RewriteVar(body, f, macro);
      RewriteCallExpr(body, parentmap, i, j);
      delete parentmap;
    }

    if (f->isAction(records)) {
    	if (FD->isInlined())
    		buff =  "void " + f->getName() + "() { ";
    	else
    		buff = "void " + f->getFullName() + "() { ";
      if (prettyprint)
        buff += "\n";
      buff += "  minissf::Process* _ssf_proc = this;";
      if (prettyprint)
        buff += "\n";
    }
    else if (FD->isInlined())
    	buff =  "void " + f->getName() + "(minissf::Process* _ssf_proc) { ";
    else
    	buff = "void " + f->getFullName() + "(minissf::Process* _ssf_proc) { ";

    /*
     *  Check whether this function qualifies for being a starting
     *  procedure, which must contain only one argument and the argument
     *  must be a pointer to the Process class. A starting procedure is
     *  responsible for putting the first procedure frame onto the stack.
     */
    if (f->isStartingProcedure(records)) {
      if (prettyprint)
        buff += "\n";
      buff += "  if(!_ssf_proc->top_stack()) { ";
      if (prettyprint)
        buff += "\n";
      buff += "    _ssf_proc->push_stack(_ssf_create_procedure_" + f->getName();
      if (f->isAction(records)) {
        buff += "()); ";
        if (prettyprint)
          buff += "\n";
      }
      else {
        buff += "(_ssf_proc, 0)); ";
        if (prettyprint)
          buff += "\n";
      }
    }
    else {
      buff += "  if(!_ssf_proc->top_stack()) { ";
      if (prettyprint)
        buff += "\n";
      buff += "    { minissf::ThrowableStream(__FILE__,__LINE__,__func__) << \"procedure ";
      buff += f->getFullName();
      buff += " is not a starting procedure\"; }";
      if (prettyprint)
        buff += "\n";
    }
    buff += "  }";
    if (prettyprint)
      buff += "\n";
    buff += "  if(!_ssf_proc || !_ssf_proc->in_procedure_context()) { minissf::ThrowableStream(__FILE__,__LINE__,__func__) << \"improper procedure call\"; }";
    if (prettyprint)
      buff += "\n";
    // _ssf_pframe is the current procedure frame on stack
    buff += "  _ssf_procedure_" + f->getClassName() + "_" + f->getName() + "* _ssf_pframe = ";
    buff += "(_ssf_procedure_" + f->getClassName() + "_" + f->getName() + "*)_ssf_proc->top_stack();";
    if (prettyprint)
      buff += "\n";

    // jump table
    buff += "  switch(_ssf_pframe->entry_point) { ";
    if (prettyprint)
      buff += "\n";
    unsigned int bptsNum = getBreakPtNum(f);
    for (unsigned i = 1; i <= bptsNum; i++) {
      std::stringstream out;
      out << "    case " << i << ": goto _ssf_sync" << i << "; ";
      if (prettyprint)
        out << "\n";
      buff += out.str();
    }
    buff += "  }";
    if (prettyprint)
      buff += "\n";

    const char *lbuf = startBuf;
    while (*lbuf && *lbuf != '\n') lbuf--;
    lbuf++;
    const char *rbuf = startBuf;
    while (*rbuf && *rbuf != '{') rbuf++;
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
    SourceLocation LocFucParamDeclStart = LocDeclStart.getLocWithOffset(lbuf - startBuf);
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
    SourceLocation LocFucParamDeclStart = LocDeclStart.getFileLocWithOffset(lbuf - startBuf);
#else
#error  "clang version not supported!"
#endif
    ReplaceText(LocFucParamDeclStart, rbuf - lbuf + 1, buff.c_str(), buff.size());

    // implicit return from procedure
    buff = "";
    if (f->isStartingProcedure(records) || f->getReturnTypeAsString() == "void") {
      if (prettyprint)
        buff += "\n";
      buff += "  { _ssf_pframe->call_return(); return; } ";
      if (prettyprint)
        buff += "\n//------------------------ EMBEDDED CODE ENDS (ProcedureDefine: " +
                  f->getClassName() + ":" + f->getName() + ") ----------------------------//\n";
    }
    else
      if (prettyprint)
        buff = "\n//------------------------ EMBEDDED CODE ENDS (ProcedureDefine: " +
              f->getClassName() + ":" + f->getName() + ") ----------------------------//\n";
    if (buff != "") {
      InsertText(LocDeclEnd, buff.c_str(), buff.size());
      InsertLineNum(LocDeclEnd);
    }
  }

  // get the number of break points of the given function; break
  // points are precedure calls or wait statements
  unsigned int getBreakPtNum(Function *f) {
    unsigned int bpts_num = 0;
    std::set<CallExpr*>::iterator iter;
    for (iter = f->callexprs.begin(); iter != f->callexprs.end(); iter++) {
      // for every function invocation within this function
      if (isWaitFunctionCall(*iter) ||
          isSemWaitCall(*iter) ||
          isProcedureCall(*iter))
        bpts_num++;
    }
    return bpts_num;
  }

  // rewriting the function body by enumerating all statements
  void RewriteVar(Stmt *S, Function *f, MacroInfoMap &macro) {
    // perform a depth first traversal of the function body
    for (Stmt::child_iterator CI = S->child_begin(), E = S->child_end();
         CI != E; ++CI) {
      if (*CI) // if the child is not null?
        RewriteVar(*CI, f, macro);
    }

    /* deal with local variable declarations
     *1 remove stmt: int a;
     *2 remove stmt and add to constructor: MyClass x(1,2,3); <------  (XXX: initialization should be done at the procedure's constructor)
     *3 change stmt's variable name: int a = 3;
     */
    if (DeclStmt *DS = dyn_cast<DeclStmt>(S)) {
      //llvm::errs() << "DECLSTMT: " << getStmtAsString(DS) <<  "\n";
      if (DS->isSingleDecl()) {
        if (VarDecl *VarD = dyn_cast<VarDecl>(DS->getSingleDecl()) ) {
          SourceLocation LocStart = DS->getLocStart();
          SourceLocation LocEnd = DS->getLocEnd();
          const char *startBuf = SM->getCharacterData(LocStart);
          const char *endBuf = SM->getCharacterData(LocEnd);

          const char *fbuf = startBuf;
          while(fbuf != endBuf && *fbuf != '=' && *fbuf != '(')
            fbuf++;
          if (fbuf != endBuf)  {
	        std::stringstream out;
            std::string name = VarD->getNameAsString();
            out << "_ssf_pframe->" << name;
            std::map<VarDecl*, int>::iterator it = f->vars.find(VarD);
            if (it != f->vars.end())
              out << "_" << it->second;
            
            //fix bug to correctly translate VirtualTime t(k, VirtualTime::SECOND);
            if (*fbuf == '(') {
              std::string s = VarD->getType().getAsString();
              //remove class at beginning of string
              s = s.substr(6);
              out << "=" << s;
            }
            fbuf--;
            while(*fbuf == ' ' || *fbuf == '\t')
             fbuf--;

            std::string buff = out.str();
            ReplaceText(LocStart, fbuf - startBuf + 1, buff.c_str(), buff.size());
          }
          else
            RemoveText(LocStart, endBuf - startBuf + 1);
        }
      }
      //multiple declstmt example: int a, b = 2, c;
      else {
        DeclGroupRef g = DS->getDeclGroup();
        DeclGroupRef::iterator iter;
        SourceLocation LocStart, LocEnd;
        bool first = true;
        const char *start = NULL, *end = NULL, *tmp = NULL;
        for (iter = g.begin(); iter != g.end(); iter++) {
          if (VarDecl *VarD = dyn_cast<VarDecl>(*iter) ) {
            if (first) {
              LocStart = DS->getLocStart();
              start = SM->getCharacterData(LocStart);

              tmp = start;
              while(tmp && *tmp != ',') tmp++;
              end = tmp;
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
              LocEnd = LocStart.getLocWithOffset(end - start);
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
              LocEnd = LocStart.getFileLocWithOffset(end - start);
#else
#error  "clang version not supported!"
#endif
              first = false;
            }
            else {
              tmp = end + 1;
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
              LocStart = LocStart.getLocWithOffset(tmp - start);
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
              LocStart = LocStart.getFileLocWithOffset(tmp - start);
#else
#error  "clang version not supported!"
#endif
              start = SM->getCharacterData(LocStart);

              while(tmp && *tmp != ',' && *tmp != ';') tmp++;
              end = tmp;
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
              LocEnd = LocStart.getLocWithOffset(end - start);
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
              LocEnd = LocStart.getFileLocWithOffset(end - start);
#else
#error  "clang version not supported!"
#endif
            }

            std::string s = std::string(start, end - start + 1);
            llvm::errs() << s << "\n";

            tmp = start;
            //find "=" in the variable declaration
            while(tmp != end && *tmp != '=') tmp++;
            if (tmp != end)  {
              tmp--;
              while(*tmp == ' ' || *tmp == '\t') tmp--;
              std::stringstream out;
              std::string name = VarD->getNameAsString();
              out << "_ssf_pframe->" << name;
              std::map<VarDecl*, int>::iterator it = f->vars.find(VarD);
              if (it != f->vars.end())
                out << "_" << it->second;
              std::string buff = out.str();
              ReplaceText(LocStart, tmp - start + 1, buff.c_str(), buff.size());
              if (*end != ';')
                ReplaceText(LocEnd, 1, ";", 1);
            }
            else
              RemoveText(LocStart, end - start + 1);
          }
        }
      }
    }

    /* replace local variables
     *   variablename
     *            translate to -------------------------->
     *   _ssf_pframe->variablename_level
     */
    if (DeclRefExpr *RE = dyn_cast<DeclRefExpr>(S)) {
      std::string name = "";
      int level = -1;

      if (ParmVarDecl *ParD = dyn_cast<ParmVarDecl>(RE->getDecl())) {
        std::set<ParmVarDecl *>::iterator it = f->params.find(ParD);
        if (it != f->params.end())
          name = ParD->getName();
      }
      else if (VarDecl *VarD = dyn_cast<VarDecl>(RE->getDecl())) {
        //XXX clang did not do good work to separete function reference and variable reference
        //clang mark cout as varible reference which is wrong we need to avoid it.
        //llvm::errs() << "DeclRefExpr: " << VarD->getNameAsString() <<  "\n";

        if (VarD->getNameAsString() != "cout")
        {
          std::map<VarDecl*, int>::iterator it = f->vars.find(VarD);
          if (it != f->vars.end()) {
            name = it->first->getNameAsString();
            level = it->second;
          }
        }
      }

      if (name != "") {
        std::stringstream out;
        out << "_ssf_pframe->" << name;
        if (level != -1)
          out << "_" << level;
        std::string buff = out.str();

        SourceLocation LocStart = RE->getLocStart();
        ReplaceText(LocStart, name.size(), buff.c_str(), buff.size());

        if (LocStart.isMacroID()) {
          unsigned line = SM->getSpellingLineNumber(LocStart);
          unsigned column = SM->getSpellingColumnNumber(LocStart);
          std::string oldstr = name;
          std::string newstr = buff;
          MacroInfoMap::iterator it;
          it = macro.find(line);
          if (it != macro.end())
            it->second.insert(std::make_pair(column, std::make_pair(oldstr, newstr)));
          else {
            std::map<unsigned, std::pair<std::string, std::string> > temp;
            temp.insert(std::make_pair(column, std::make_pair(oldstr, newstr)));
            macro.insert(std::make_pair(line, temp));
          }
        }
        /*
        const char *start;
        const char *end;
        start = SM->getCharacterData(LocStart);
        end = SM->getCharacterData(LocStart);
        while (*start && *start != '\n') start--;
        start++;
        while (*end && *end != '\n') end++;

        std::string str = std::string(start, end - start);
        llvm::errs() << "LineNumber: " << SM->getInstantiationLineNumber(LocStart)
                     << " ColumnNumber:" << SM->getSpellingColumnNumber(LocStart)
                     << " stmt: " << str << "\n";
        //xxx have to add function to translate Marco
        //llvm::errs() << " rewriting: " << VarD->getNameAsString() << " to " << buff << "\n";
        llvm::errs() << "isMacroID: " << LocStart.isMacroID()
                     << " isValid: " << RE->getLocation().isValid()
                     << " isInvalid: "<< RE->getLocation().isInvalid()
                     <<"\nsrc dump:"<< "\n";
        SourceRange sr = RE->getSourceRange();
        RE->dump(*SM);
        llvm::errs() << "\n";
        */
      }
    }

    /* transform return statement
     *   return
     *      translate to -------------------------->
     *   { ${rtntyp} _ssf_tmp = ${ret}; _ssf_pframe->call_return(&_ssf_tmp); return; }
     *   { _ssf_pframe->call_return(); return; }
     */
    if (ReturnStmt *RS = dyn_cast<ReturnStmt>(S)) {
      SourceLocation LocStart = RS->getLocStart();
      //llvm::errs() << "return stmt: " << getStmtAsString(RS) << "\n";
      Expr* rExpr = RS->getRetValue();
      if (rExpr) {
        //llvm::errs() << "return Expr: " << getStmtAsString(rExpr) << "\n";
        std::string buff = "{ " + f->getReturnTypeAsString() + " _ssf_tmp =";
        ReplaceText(LocStart, 6, buff.c_str(), buff.size());

        buff = " _ssf_pframe->call_return(&_ssf_tmp); return; }";
        const char *startBuf = SM->getCharacterData(LocStart);
        const char *endBuf = SM->getCharacterData(LocStart);
        while(*endBuf++ && *endBuf != ';') {;}
        endBuf++;
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
        SourceLocation LocEnd = LocStart.getLocWithOffset(endBuf -startBuf);
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
        SourceLocation LocEnd = LocStart.getFileLocWithOffset(endBuf -startBuf);
#else
#error  "clang version not supported!"
#endif
        InsertText(LocEnd, buff.c_str(), buff.size());
      }
      else {
        const char *startBuf = SM->getCharacterData(LocStart);
        const char *endBuf = SM->getCharacterData(LocStart);
        //find ";" before return
        while(*endBuf++ && *endBuf != ';') {;}
        endBuf++;
        std::string buff = "{ _ssf_pframe->call_return(); return; }";
        ReplaceText(LocStart, endBuf - startBuf, buff.c_str(), buff.size());
      }
    }
  }

  void RewriteCallExpr(Stmt *S, ParentMap* parentmap, int &i, int &j) {
    // perform a depth first traversal of the function body
    for (Stmt::child_iterator CI = S->child_begin(), E = S->child_end();
         CI != E; ++CI) {
      if (*CI) // if the child is not null?
        RewriteCallExpr(*CI, parentmap, i, j);
    }

    // transform procedure calls and wait statements
    if (CallExpr *CE = dyn_cast<CallExpr>(S)) {
      SourceLocation LocStart = CE->getLocStart();
      SourceLocation LocEnd;

      const char *start = SM->getCharacterData(LocStart);
      const char *cptr;
      const char *end;

      // find ";" after the call
      cptr = start;
      while(*cptr && *cptr != ';') cptr++;

      std::string callexpr = "";
      std::string callexpr_m = "";
      std::string prefix = "";
      std::string param = "";

      // XXX: we need check if function has return variable fixed
      FunctionDecl *FD = CE->getDirectCallee();

      if (FD) { //XXX what if the callee is not a function?
        std::string fucname = FD->getNameAsString();
        std::string tmp = std::string(start, cptr - start + 1);

        size_t found = tmp.find(fucname);
        end = start + found;

        int level = 0;
        while(end != cptr && !(*end == ')' &&  level == 0)) {
          end++;
          if (*end == '(')
            level++;
          if (*end == ')')
            level--;
        }

        //llvm::errs() << "fuccall " <<  FD->getNameAsString() << "\n";
        callexpr = std::string(start, end - start + 1);
        //llvm::errs() << "callexpr: " << callexpr <<"\n";
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
        LocEnd = LocStart.getLocWithOffset(end - start);
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
        LocEnd = LocStart.getFileLocWithOffset(end - start);
#else
#error  "clang version not supported!"
#endif
        callexpr_m = Rewrite.getRewrittenText(SourceRange(LocStart, LocEnd));
        found = callexpr_m.find(fucname);
        if (found != std::string::npos)
          prefix = callexpr_m.substr(0, found);
        //llvm::errs() << "modified string: " << Rewrite.getRewrittenText(SourceRange(LocStart, LocEnd)) << "\n";
        //llvm::errs() << "prefix: " << prefix << "\n";

        //find the param
        const char *cptr1;
        const char *cptr2;
        found = callexpr_m.find(fucname);
        cptr1 = callexpr_m.c_str() + found;
        cptr2 = cptr1;

        while(*cptr1 && *cptr1 != '(') cptr1++;
        level = 0;
        while(*cptr2 && !(*cptr2 == ')' && level == 0)) {
          cptr2++;
          if (*cptr2 == '(')
            level++;
          if (*cptr2 == ')')
            level--;
        }
        if (cptr2 - cptr1 - 1 > 0)
          param = std::string(cptr1 + 1, cptr2 - cptr1 - 1);
        //llvm::errs() << "param: " << param << "\n";
        bool emptyparam = true;
        for (std::string::iterator it = param.begin(); it != param.end(); it++) {
        	if (*it != ' ' && *it != '\t' && *it != '\n') {
        		emptyparam = false;
        		break;
        	}
        }
        std::string buff;
        QualType rtype = FD->getResultType();

        // transform wait statement
        if (isWaitFunctionCall(CE)) {
          //llvm::errs() << "WaitFunctionCall: " << callexpr << "\n";
          if (rtype.getAsString() == "void") {
            std::stringstream out1;
            out1 << "{ _ssf_pframe->entry_point = " << i << "; ";
            buff = out1.str();
            InsertText(LocStart, buff.c_str(), buff.size());

            std::stringstream out2;
            out2 << " if(_ssf_pframe->call_suspended()) { return; _ssf_sync" << i << ": ; } }";
            buff = out2.str();
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
            SourceLocation Locinsert =  LocStart.getLocWithOffset(cptr - start + 1);
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
            SourceLocation Locinsert =  LocStart.getFileLocWithOffset(cptr - start + 1);
#else
#error  "clang version not supported!"
#endif
            InsertText(Locinsert, buff.c_str(), buff.size());
          }
          else {
        	// XXX bug need to be fixed (if function call return vaule not being used there will be effectless statement warning)
			// check parent node of the expr(should be CompoundStmt if the expression is not used)
            Stmt* parentstmt = parentmap->getParent(CE);
			if (isa<CompoundStmt>(parentstmt)) {
			  //remove the callexpr
              buff = "";
			}
			else {
		      //replace the callexpr with the return value
			  std::stringstream out1;
			  out1 << "_ssf_pframe->tmp" << j;
			  buff = out1.str();
			}
        	ReplaceText(LocStart, callexpr_m.size(), buff.c_str(), buff.size());

            std::stringstream out2;
            out2 << "{ _ssf_pframe->entry_point = " << i << "; "
                 << callexpr_m << ";" << " if(_ssf_pframe->call_suspended()) { return; _ssf_sync" << i << ": ; "
                 << " _ssf_pframe->tmp" << j << " = " << prefix << "timed_out(); } } ";
            if (prettyprint)
              out2 << "\n";
            buff = out2.str();

            // find the '/n' at beginning of the function call
            cptr = start;
            while(*cptr && *cptr != '\n') cptr--;
            cptr++;
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
            SourceLocation Locinsert =  LocStart.getLocWithOffset(cptr - start + 1);
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
            SourceLocation Locinsert =  LocStart.getFileLocWithOffset(cptr - start);
#else
#error  "clang version not supported!"
#endif
            InsertText(Locinsert, buff.c_str(), buff.size());
            InsertLineNum(Locinsert);
            j++;
          }
          i++;
        }

        // transform semWait statement (XXX: need to check it's a member function of the minissf::Sestd::maphore class) fixed
        if (isSemWaitCall(CE)) {
            //llvm::errs() << "semWaitCall: " << callexpr << "\n";
            std::stringstream out;
            out << "{ if(" << prefix << "value()>0) " << prefix << "wait()" << "; ";
            out << "else { _ssf_pframe->entry_point = " << i << "; ";
            out << prefix << "wait()" << "; return; ";
            out << "_ssf_sync" << i <<": ; } }";

            std::string buff = out.str();
            ReplaceText(LocStart, callexpr_m.size(), buff.c_str(), buff.size());
            i++;
        }

        // transform procedure calls
        if (isProcedureCall(CE)) {
            llvm::errs() << "procedure call: " << callexpr << "\n";

            if (rtype.getAsString() == "void") {
              std::stringstream out;
              out << "{ _ssf_pframe->entry_point = " << i << "; _ssf_pframe->call_procedure(" << prefix << "_ssf_create_procedure_"
                  << fucname << "(";
              if (emptyparam)
            	  out << "0)); if(_ssf_pframe->call_suspended()) { return; _ssf_sync" << i << ": ; } }";
              else
            	  out << param << ", 0)); if(_ssf_pframe->call_suspended()) { return; _ssf_sync" << i << ": ; } }";
              buff = out.str();
              ReplaceText(LocStart, callexpr_m.size() ,buff.c_str(), buff.size());
            }
            else {
			  // XXX bug need to be fixed (if function call return vaule not being used there will be effectless statement warning)
			  // check parent node of the expr(should be CompoundStmt if the expression is not used)
              Stmt* parentstmt = parentmap->getParent(CE);
			  if (isa<CompoundStmt>(parentstmt)) {
				//remove the callexpr
                buff = "";
			  }
			  else {
				//replace the callexpr with the return value
				std::stringstream out1;
				out1 << "_ssf_pframe->tmp" << j;
				buff = out1.str();
			  }

              ReplaceText(LocStart, callexpr_m.size(), buff.c_str(), buff.size());

              std::stringstream out2;
              out2 << "{ _ssf_pframe->entry_point = " << i << "; _ssf_pframe->call_procedure(" << prefix << "_ssf_create_procedure_"
                   << fucname << "(";
              if (emptyparam)
            	  out2 << "&_ssf_pframe->tmp" << j << ")); if(_ssf_pframe->call_suspended()) { return; _ssf_sync" << i << ": ; } } ";
              else
            	  out2 << param << ", &_ssf_pframe->tmp" << j << ")); if(_ssf_pframe->call_suspended()) { return; _ssf_sync" << i << ": ; } } ";
              if (prettyprint)
                out2 << "\n";
              buff = out2.str();

              // find the '/n' at beginning of the function call
              cptr = start;
              while(*cptr && *cptr != '\n') cptr--;
              cptr++;
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
              SourceLocation Locinsert =  LocStart.getLocWithOffset(cptr-start);
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
              SourceLocation Locinsert =  LocStart.getFileLocWithOffset(cptr-start);
#else
#error  "clang version not supported!"
#endif
              InsertText(Locinsert, buff.c_str(), buff.size());
              InsertLineNum(Locinsert);
              j++;
            }
            i++;
        }
      }
    }
  }

  //modify macros
  void RewriteMacro(std::string file) {
    //there is no macro need to modify
    if (macro.size() == 0) return;

    std::ifstream in(file.c_str(), std::ios::in);
    std::fstream out("temp.cc", std::ios::out);

    if (!in) {
      llvm::errs() << "Cannot open "<< file << ".\n";
    }
    else {
      std::string line;
      unsigned linenum = 0;
      std::string tmp;

      while (!in.eof()) {
        getline(in, line);
        std::string start = "#line ";
        tmp = line.substr(0,start.size());
        if (tmp == start) {
          size_t f1,f2;
          f1 = line.find_first_of(" ");
          f2 = line.find_last_of(" ");
          tmp = line.substr(f1+1, f2 -1);
          linenum = atoi(tmp.c_str()) - 1;
          //llvm::errs() << line << " " << linenum << "\n";
        }
        else
          linenum++;

        MacroInfoMap::iterator iter = macro.find(linenum);
        if (iter != macro.end()) {
          //llvm::errs() << line << "\n";
          RewriteInfoMap rewritemap;
          rewritemap = iter->second;
          RewriteInfoMap::iterator iter1;

          for (iter1 = rewritemap.begin(); iter1 != rewritemap.end(); iter1++) {
            std::string oldstr = (iter1->second).first;
            std::string newstr = (iter1->second).second;

            size_t found = line.find(oldstr);
            if (found != std::string::npos)
                line.replace(found, oldstr.size(), newstr);

            //llvm::errs() << " old:" << oldstr << " new:" << newstr << "\n";
            //llvm::errs() << line << "\n";
          }
        }

        out << line << "\n";
      }
    }
    in.close();
    out.close();

    remove(file.c_str());
    rename("temp.cc", file.c_str());
  }
  /*-------------------------------------------------------------------------*/
  /*End Rewrite functions                                                    */
  /*-------------------------------------------------------------------------*/

  // creat call graph to identify procedures
  void CreateCallGraph() {
    for(std::map<FunctionDecl*,Function*>::iterator iter = functions.begin();
        iter != functions.end(); iter++) {
      Function *f = (*iter).second;
      std::set<CallExpr*>::iterator iter;
      for (iter = f->callexprs.begin(); iter != f->callexprs.end(); iter++) {
        CallExpr *callexpr = *iter;
        FunctionDecl *callee = callexpr->getDirectCallee();
        if (callee) { // only when the callee is a FunctionDecl
          std::map<FunctionDecl*,Function*>::iterator callee_iter = functions.find(callee);
          if (callee_iter != functions.end()) {
            Function *f1 = (*callee_iter).second;
            f->childs.insert(f1);
            f1->parents.insert(f);
          }
          //else assert(0); //callee can be system function which is not storage in our struct
        }
      }
    }
  }

  // recursively mark the function and all its parent function as candidate precedures
  void MakeAsCandidate(Function *f) {
    f->candidate = true;
    //llvm::errs() << "function: " << f->getName() + f->getParamsAsString() << " markeded as candidate" << "\n";
    f->visit = true;

    for(std::set<Function*>::iterator iter = f->parents.begin();
        iter != f->parents.end(); iter++) {
      Function *parent = *iter;
      if (parent && !parent->visit)
        MakeAsCandidate(*iter);
    }
  }

  //start from starting procedure traverse the call chain
  void TraverseProcedure(Function *f) {
    if (f->candidate) {
      //llvm::errs() << "function: " << f->getName() + f->getParamsAsString() << " markeded as procedure" << "\n";
      f->isProcedure = true;
    }
    f->visit = true;

    for(std::set<Function*>::iterator iter = f->childs.begin();
        iter != f->childs.end(); iter++) {
      Function *child = *iter;
      if (child && !child->visit)
        TraverseProcedure(child);
    }
  }

  // check all the function to find precedures
  void CheckProcedure() {
    for(std::map<FunctionDecl*,Function*>::iterator iter = functions.begin();
        iter != functions.end(); iter++) {
      (*iter).second->visit = false;
      (*iter).second->candidate = false;
    }

    //check all the function if it calls wait for semwait mark it and mark all its parents as candidate
    for(std::map<FunctionDecl*,Function*>::iterator iter = functions.begin();
        iter != functions.end(); iter++) {
      Function *f = (*iter).second;
      if (f->candidate) continue;
      std::set<CallExpr*>::iterator iter;
      for (iter = f->callexprs.begin(); iter != f->callexprs.end(); iter++) {
        CallExpr *ce = *iter;
        if (isWaitFunctionCall(ce) ||  isSemWaitCall(ce)) {
          MakeAsCandidate(f);
          break;
        }
      }
    }

    for(std::map<FunctionDecl*,Function*>::iterator iter = functions.begin();
        iter != functions.end(); iter++) {
      (*iter).second->visit = false;
      (*iter).second->isProcedure = false;
    }

    //start from starting procedure traverse the call chain
    for(std::map<FunctionDecl*,Function*>::iterator iter = functions.begin();
      iter != functions.end(); iter++) {
      Function *f = (*iter).second;
      if (f->isStartingProcedure(records)) {
        TraverseProcedure(f);
      }
    }

    //add warning normal function could not call procedure
    /*
    for(std::map<FunctionDecl*,Function*>::iterator iter = functions.begin();
        iter != functions.end(); iter++) {
      Function *f = (*iter).second;
      if (!f->FD->isThisDeclarationADefinition()) continue;
      if (!f->isProcedure) {
        std::set<CallExpr*>::iterator iter;
        for (iter = f->callexprs.begin(); iter != f->callexprs.end(); iter++) {
          CallExpr *ce = *iter;
          if (isProcedureCall(ce) ||
              isWaitFunctionCall(ce) ||
              isSemWaitCall(ce)){
            std::string buf = "\n//ERR NORMAL FUNCTION COULD NOT CALL PROCEDURE";
            SourceLocation LocStart = ce->getLocStart();
            const char *startBuf = SM->getCharacterData(LocStart);
            const char *sbuf = startBuf;
            while(*sbuf-- && *sbuf != '\n') {;}

            // get the source location of the beginning of the line
            SourceLocation LocFucStart = LocStart.getFileLocWithOffset(sbuf - startBuf);
            InsertText(LocFucStart, buf.c_str(), buf.size());
          }
        }
      }
    }*/
  }

  // for debugging, return a statement as a string; this function
  // does not work well could endlocation is not always right
  std::string getStmtAsString(Stmt *S) {
    SourceRange range = S->getSourceRange();

    const char *begin = SM->getCharacterData(range.getBegin());
    const char *end = SM->getCharacterData(range.getEnd());
    std::string str(begin, end - begin + 1);

    return str;
  }

  // check whether the call expression contains a process wait statement
  bool isWaitFunctionCall(CallExpr *expr) {
    //std::string exprstr = getStmtAsString(expr);
    FunctionDecl * fd = expr->getDirectCallee();
    if (fd != 0) {
      std::string callee = fd->getQualifiedNameAsString();
      size_t found;
      for (unsigned int i = 0; i < 7; i++) {
        found = callee.find(waitfunctions[i]);
        if (found != std::string::npos) return true;
      }
      return false;
    }
    else return false;
  }

  // check whether the call expresssion is a sestd::maphore wait statement
  bool isSemWaitCall(CallExpr *expr) {
    //XXX: need to make sure the class is minissf::Semaphore fixed
    FunctionDecl *fd = expr->getDirectCallee();
    if (fd) { //XXX what if callee is not a function?
      //std::map<FunctionDecl*,Function*>::iterator iter = functions.find(fd);
      //if (iter != functions.end()) {
        //if (!(*iter).second->isDerivedFrom("minissf::Semaphore", records))
          //return false;
      std::string name = fd->getQualifiedNameAsString();
      std::string semwait = "minissf::Semaphore::wait";
      if (name == semwait)
        return true;
      else
        return false;
    }
    else return false;
  }

  // check whether the call expresssion is procedure call
  bool isProcedureCall(CallExpr *expr) {
    FunctionDecl *fd = expr->getDirectCallee ();
    if (fd) {
      std::map<FunctionDecl*,Function*>::iterator f_iter = functions.find(fd);
      if(f_iter != functions.end()) return (*f_iter).second->isProcedure;
      //else assert(0); //callee can be system function which is not storage in our struct
    }
    return false;
  }

  // this method is called to analyze the entire AST
  void HandleTranslationUnit(ASTContext &Ctx) {
    // the visitor functions will be invoked by this
    //llvm::errs() << "handling translation unit\n";
    TraverseDecl(Ctx.getTranslationUnitDecl());
    //llvm::errs() << "DONE VISITING\n";

    if (procedure.size() == 0) {
      CreateCallGraph();
      //llvm::errs() << "DONE GRAPHING\n";
      CheckProcedure();
      //llvm::errs() << "DONE PROCEDURING\n";
    }
    else {
      std::set<std::string>::iterator found;
      for(std::map<FunctionDecl*,Function*>::iterator iter = functions.begin();
          iter != functions.end(); iter++) {
        Function *f = (*iter).second;
        found = procedure.find(f->getFunctionAsString());
        if (found != procedure.end()) {
          //llvm::errs() << f->getFunctionAsString() << "is procedure\n";
          f->isProcedure = true;
        }
      }
    }

    for(std::map<FunctionDecl*,Function*>::iterator iter = functions.begin();
        iter != functions.end(); iter++) {
      Function *f = (*iter).second;
      if (f->isProcedure) {
        if (f->FD->isThisDeclarationADefinition())
          RewriteProcedureDefine(f);
        else
          RewriteProcedureDeclare(f);
      }
    }

    //llvm::errs() << "DONE REWRITING\n";
  }

  // output information about each file and store it in a file, so
  // that we can do global analysis in the next round
  void OutputAnnotation() {
    std::fstream file;
    file.open("result.txt", std::ios::out);

    if (file.is_open()) {
      file << "CLASSES:\n";
      std::map<CXXRecordDecl*,Record*>::const_iterator iter;
      for (iter=records.begin(); iter != records.end(); ++iter) {
        file << "CLASS: "<< iter->second->getName() <<"\n";
        if (iter->second->bases.size() != 0)
          file << "BASECLASSES:" << iter->second->getBaseClassAsString() << "\n";
        if (iter->second->attributes.size() != 0)
          file << "ATTRIBUTES:\n" << iter->second->getAttributesAsString();
        if (iter->second->methods.size() != 0)
          file << iter->second->getMethodsAsString(functions);
        file << "\n";
      }
    }

    file << "FUNCTIONS:\n";
    std::map<FunctionDecl*,Function*>::const_iterator iter;
    for (iter=functions.begin(); iter != functions.end(); ++iter) {
      file << "FUNCTION: " << iter->second->getFunctionAsString() << "\n";
      if (iter->second->candidate)
        file << "\tCANDIDATE\n";
      if (iter->second->isProcedure)
        file << "\tPROCEDURE\n";
      //llvm::errs() << "\tPRECEDURE\n";
      if (iter->second->isStartingProcedure(records))
        file << "\tSTARTING PROCEDURE\n";
      if (iter->second->getCallExprsAsString(SM) != "") {
        file << "\tFUNCTION CALL: " << "\n" <<iter->second->getCallExprsAsString(SM);
        //llvm::errs() << "\tFUNCTION CALL: " << "\n" <<functions[i]->getCallExprsAsString(SM);
      }

      if (iter->second->getVarsAsString() != "") {
        file << "\tVARIABLES: " << "\n" << iter->second->getVarsAsString();
        //llvm::errs() << "\tVARIABLES: " << "\n" << functions[i]->getVarsAsString();
      }
      file << "\n";
      //llvm::errs() << "\n";
    }

    file.close();
  }
};

class SourceCodexlateAction : public PluginASTAction {
protected:
  ASTConsumer *CreateASTConsumer(CompilerInstance &CI, llvm::StringRef S) {
    std::string inFile = S;
    FrontendOptions &FOpts = CI.getFrontendOpts();
    std::string outFile = FOpts.OutputFile;
    //llvm::errs() << "output file: " << outFile << "\n";
#if (CLANG_VERSION_MAJOR == 3 && (CLANG_VERSION_MINOR == 0 || CLANG_VERSION_MINOR == 1 || CLANG_VERSION_MINOR == 2))
    DiagnosticsEngine &D = CI.getDiagnostics();
#elif (CLANG_VERSION_MAJOR == 2 && CLANG_VERSION_MINOR == 9)
    Diagnostic &D = CI.getDiagnostics();
#else
#error  "clang version not supported!"
#endif
    LangOptions &LOpts = CI.getLangOpts();

    //llvm::errs() << "creating consumer for file: " << inFile <<  "\n";
    return new SourceCodexlateConsumer(inFile, outFile, D, LOpts, true);
  }

  // parse the arguments passed to this plugin
  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string>& args) {
    std::string procfile = "procedure.txt";
        if (args.size() == 1)
                procfile = args[0];

    std::ifstream in(procfile.c_str(), std::ios::in);

    if (!in) {
      llvm::errs() << "Cannot open input file " << procfile << "\n";
    }
    else {
      std::string line;

      while (!in.eof()) {
        getline(in, line);
        procedure.insert(line);
      }
      //lvm::errs() << "end reading procedure.txt \n";
    }
    in.close();

    //llvm::errs() << "parsing arguments: size = " << args.size() << "\n";
    for (unsigned i = 0, e = args.size(); i != e; ++i) {
      //llvm::errs() << "arg = " << args[i] << "\n";

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

static FrontendPluginRegistry::Add<minissf::SourceCodexlateAction>
X("minissf-xlate", "source code translation for minissf");

