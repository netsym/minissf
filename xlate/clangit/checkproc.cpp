#include <stdio.h>
#include <assert.h>
#include <iostream>
#include <set>
#include <list>
#include <map>
#include <fstream>
#include <string.h>
#include <sstream>

using namespace std;

class Record {
public:
  string qualifiedname;
  set<string> bases;

  string getBasesAsString() {
    string b = "";
    set<string>::iterator iter;
    for (iter = bases.begin(); iter != bases.end(); iter++) {
      b += (*iter) + ",";
    }
    if (b != "") b.erase(b.end() - 1); // remove the last comma

    return b;
  }
};

class Method {
public:
  string qualifiedname;       // method qualified name (namespace::class_name::function_name)
  list<string> params;        // paramters
  string rtype;               // return type of the method
  set<string> callexprs;      // all function invocations inside the function body

  string parentclass;         // parent class name (namespace::class_name)

  bool hasAttri;              // use __attribute__((deprecated)) mark to mark starting procedure

  bool isProcedure;           // set to be true if this function is detected as procedure
  bool candidate;             // all the parents of function call wait of semwait should be candidate procedure
  bool visit;                 // used for avoid infinate loop

  set<Method*> parents;       // all functions that calling this function
  set<Method*> childs;        // all functions that called by this function

  Method() {
    hasAttri = false;
    isProcedure = false;
    candidate = false;
    visit = false;

  }

  // return the list of parameters as a string
  string getParamsAsString() {
    string p = "";
    p += "(";
    list<string>::iterator iter;
    for (iter = params.begin(); iter != params.end(); iter++) {
      string tmp = *iter;
      p += tmp + ",";
    }

    if (p != "(") p.erase(p.end() - 1); // remove the last comma
      p += ")";
    return p;
  }

  // return the method as a string
  string getMethodAsString() {
    string method = "";
    method += rtype + " " + qualifiedname + getParamsAsString();
    return method;
  }

  // return the call expressions as a string
  string getCallExprsAsString() {
    string callexpr = "";
    set<string>::iterator iter;
    for (iter = callexprs.begin(); iter != callexprs.end(); iter++) {
      string tmp = *iter;
      callexpr += tmp + "\n";
    }
    return callexpr;
  }

  //check if a method is derived from a class(class name should be qualified name: namespace::class_name)
  bool isDerivedFrom(string classname, map<string, Record*> records) {
    if (parentclass == classname)
      return true;

    map<string, Record*>::iterator iter;
    iter = records.find(parentclass);
    if (iter == records.end()) return false;
    // find the parent class record
    Record *r = iter->second;

    // check if the parent class bases set contains the classname
    set<string>::iterator iter2 = r->bases.find(classname);
    if (iter2 != r->bases.end())
      return true;
    else
      return false;
  }

  bool isAction(map<string, Record*> records) {
    // XXX: we should check whether this function is a method of the
    // class of minissf::Process or its derived class
    // fixed
    size_t found = qualifiedname.find_last_of("::");
    string fname = qualifiedname.substr(found + 1);
    if (rtype == "void" &&
    	fname ==  "action" &&
        getParamsAsString() == "()" &&
        isDerivedFrom("minissf::Process", records))
      return true;
    else return false;
  }

  // where this function is a starting procedure
  bool isStartingProcedure(map<string, Record*> records) {
    //case1 action function
    if (isAction(records))
      return true;

    //case2 user marks starting procedure
    if (hasAttri &&
        rtype == "void" && //check the format
        params.size() == 1 &&
        (*(params.begin())) ==  "class minissf::Process *" &&
        isDerivedFrom("minissf::ProcedureContainer", records)
       )
      return true;

    return false;
  }
};

static map<string, Method*> methods;
static map<string, Record*> records;

// check whether the call expression contains a process wait statement
bool isWaitFunctionCall(string expr) {
  string waitfunctions[7];

  waitfunctions[0] = "minissf::Process::waitOn";
  waitfunctions[1] = "minissf::Process::waitOnFor";
  waitfunctions[2] = "minissf::Process::waitFor";
  waitfunctions[3] = "minissf::Process::waitUntil";
  waitfunctions[4] = "minissf::Process::waitForever";
  waitfunctions[5] = "minissf::Process::suspendForever";
  waitfunctions[6] = "minissf::Process::waitOnUntil";

  for (unsigned int i = 0; i < 7; i++) {
    if (expr.find(waitfunctions[i]) != string::npos)
      return true;
  }

  return false;
}

// check whether the call expresssion is a semaphore wait statement
bool isSemWaitCall(string expr) {
  string semwait = "void minissf::Semaphore::wait()";
  if (semwait == expr)
    return true;
  else
    return false;
}

bool isProcedureCall(string expr) {
  map<string, Method*>::iterator found = methods.find(expr);
  if (found == methods.end())
    return false;
  else
    return found->second->isProcedure;
}

void OutputAnnotation(string name) {
  fstream file;
  if (name == "")
    file.open("procedure.txt", ios::out);
  else
	file.open(name.c_str(), ios::out);
  if (file.is_open()) {
    /*
    file << "CLASSES:\n";
    for (map<string, Record*>::const_iterator iter = records.begin();
         iter != records.end(); ++iter) {
      file << "CLASS: " << iter->second->qualifiedname << "\n";
      if (iter->second->getBasesAsString() != "")
        file << "BASES: " << iter->second->getBasesAsString() << "\n";
    }

    file << "\nMETHODS:\n";
    for (map<string, Method*>::const_iterator iter = methods.begin();
         iter != methods.end(); ++iter) {
      file << "METHOD: " << iter->second->getMethodAsString() << "\n";

      if (iter->second->isStartingProcedure(records))
        file << "\tSTARTING PROCEDURE\n";
      if (iter->second->isAction(records))
        file << "\tACTION\n";
      if (iter->second->isProcedure)
        file << "\tPROCEDURE\n";
      if (iter->second->candidate)
        file << "\tCANDIDATE\n";
      file << "PARENT CLASS: " << iter->second->parentclass << "\n";
      if (iter->second->getCallExprsAsString() != "") {
        file << "FUNCTION CALL: " << "\n";
        for (set<string>::iterator callexpr_iter = iter->second->callexprs.begin();
             callexpr_iter != iter->second->callexprs.end(); callexpr_iter++) {
          string callee = *callexpr_iter;
          if (isWaitFunctionCall(callee))
            file << "WAITFUNCTIONCALL:\n";
          if (isSemWaitCall(callee))
            file << "SEMWAITCALL\n";
          if (isProcedureCall(callee))
            file << "PROCEDURECALL\n";
          file << callee << "\n";
        }
      }
    }

    file << "PROCEDURE\n";
    */
    for (map<string, Method*>::const_iterator iter = methods.begin();
         iter != methods.end(); ++iter) {
      if (iter->second->candidate) {
        //file << iter->second->filename << "\n";
        file << iter->second->getMethodAsString() << "\n";
      }

      if (iter->second->isStartingProcedure(records) &&
          !iter->second->isProcedure &&
          iter->second->qualifiedname != "minissf::Process::action") {
    	  cout << "Warning: Staring procedure "
    		   << iter->second->getMethodAsString()
    		   << " does not define procedure call!\n";
      }
    }
  }

  file.close();
}

//for each record add all it's base classes
void AddBases(Record *r, map<string, Record*> records) {
  for (set<string>::iterator iter1 = r->bases.begin();
       iter1 != r->bases.end(); iter1++) {
    //for each base class name find the class record recursively add base classes
    string basename = *iter1;
    map<string, Record*>::iterator iter2 = records.find(basename);
    if (iter2 != records.end()) {
      Record *base = iter2->second;
      AddBases(base, records);
      for (set<string>::iterator iter3 = base->bases.begin();
           iter3 != base->bases.end(); iter3++)
        r->bases.insert(*iter3);
    }
  }
}

// creat call graph to identify procedures
void CreateCallGraph() {
  // traverse all method
  for(map<string, Method*>::iterator iter1 = methods.begin();
      iter1 != methods.end(); iter1++) {
    Method *m = iter1->second;
    set<string>::iterator iter2;
    // traverse all the expr call of the method
    for (iter2 = m->callexprs.begin(); iter2 != m->callexprs.end(); iter2++) {
      string callee = *iter2;
      // find the callee
      map<string, Method*>::iterator callee_iter = methods.find(callee);
      if (callee_iter != methods.end()) {
        Method *m1 = callee_iter->second;
        m->childs.insert(m1);
        m1->parents.insert(m);
      }
    }
  }
}

// recursively mark the function and all its parent function as candidate precedures
void MakeAsCandidate(Method *m) {
  m->candidate = true;
  //llvm::errs() << "function: " << f->getName() + f->getParamsAsString() << " markeded as candidate" << "\n";
  m->visit = true;

  for(set<Method*>::iterator iter = m->parents.begin();
      iter != m->parents.end(); iter++) {
    Method *parent = *iter;
    if (parent && !parent->visit)
      MakeAsCandidate(*iter);
  }
}

//start from starting procedure traverse the call chain
void TraverseProcedure(Method *m) {
  if (m->candidate) {
    m->isProcedure = true;
  }
  m->visit = true;

  for(set<Method*>::iterator iter = m->childs.begin();
      iter != m->childs.end(); iter++) {
    Method *child = *iter;
    if (child && !child->visit)
      TraverseProcedure(child);
  }
}

// check all the function to find precedures
void MarkProcedure() {
  //check all the function if it calls wait for semwait mark it and mark all its parents as candidate
  for(map<string, Method*>::iterator iter = methods.begin();
      iter != methods.end(); iter++) {
    Method *m = iter->second;
    if (m->candidate) continue;
    for (set<string>::iterator call_iter = m->callexprs.begin();
         call_iter != m->callexprs.end(); call_iter++) {
      string callee = *call_iter;
      if (isWaitFunctionCall(callee) ||  isSemWaitCall(callee)) {
        MakeAsCandidate(m);
        break;
      }
    }
  }

  for(map<string, Method*>::iterator iter = methods.begin();
      iter != methods.end(); iter++) {
      iter->second->visit = false;
      iter->second->isProcedure = false;
  }

  //start from starting procedure traverse the call chain
  for(map<string, Method*>::iterator iter = methods.begin();
    iter != methods.end(); iter++) {
    Method *m = iter->second;
    if (m->isStartingProcedure(records)) {
      TraverseProcedure(m);
    }
  }
}

bool StartsWith(const std::string& text,const std::string& token)
{
  if(text.length() < token.length() || text.length() == 0)
    return false;
  for(unsigned int i=0; i<token.length(); ++i) {
    if(text[i] != token[i])
    return false;
  }
  return true;
}


void parsefile(string filename) {
  ifstream in(filename.c_str());
  if (!in) {
	cout << "Cannot open input file " << filename << ".\n";
	return;
  }

  static string CLASS = "CLASS:";
  static string BASE = "BASE:";
  static string METHOD = "METHOD:";
  static string RETURN = "RETURN:";
  static string METHODNAME = "METHODNAME:";
  static string PARAM = "PARAM:";
  static string HASATTRI = "HASATTRI";
  static string PARENT = "PARENT:";
  static string FUNCTION_CALL = "FUNCTION CALL:";

  string line, tmp;
  string currecord, curmethod;

  map<string, Record*>::iterator r_iter;
  map<string, Method*>::iterator m_iter;

  bool newm = false;

  while (getline(in, line,'\n')) {
    //cout << "read line:" << line << "\n";
    //found CLASS:qualified_classname
    if (StartsWith(line, CLASS)) {
      currecord = line.substr(CLASS.size());
      r_iter = records.find(currecord);

      if (r_iter == records.end()) {
        Record *r = new Record;
        r->qualifiedname = currecord;
        //cout << "add record: " << currecord << "\n";
    	records.insert(make_pair(currecord, r));
      }

      continue;
    }

    //found BASE:qualified_classname
    if (StartsWith(line, BASE)) {
      tmp = line.substr(BASE.size());
      r_iter = records.find(currecord);

      if (r_iter != records.end()) {
    	Record *r = r_iter->second;
        //cout << "add base:" << tmp << "\n";
    	r->bases.insert(tmp);
      }

      continue;
    }

    //found METHOD:rtype qualified_methodname(params)
    if (StartsWith(line, METHOD)) {
      curmethod = line.substr(METHOD.size());
      m_iter = methods.find(curmethod);

      if (m_iter == methods.end()) {
        Method *m = new Method;
        //cout << "add method:" << curmethod << "\n";
        methods.insert(make_pair(curmethod, m));
        newm = true;
      }
      else newm = false;

      continue;
    }

    if (newm) {
    	//found RETURN:rtype
		if (StartsWith(line, RETURN)) {
		  tmp = line.substr(RETURN.size());
		  m_iter = methods.find(curmethod);
		  if (m_iter != methods.end()) {
			Method *m = m_iter->second;
			m->rtype = tmp;
		  }
		  continue;
		}

		//found METHODNAME:qualified_methodname
		if (StartsWith(line, METHODNAME)) {
		  tmp = line.substr(METHODNAME.size());
		  m_iter = methods.find(curmethod);
		  if (m_iter != methods.end()) {
		    Method *m = m_iter->second;
		    m->qualifiedname = tmp;
		  }
		  continue;
		}

		//found PARAM:type
		if (StartsWith(line, PARAM)) {
		  tmp = line.substr(PARAM.size());
		  m_iter = methods.find(curmethod);
		  if (m_iter != methods.end()) {
		    Method *m = m_iter->second;
		    //cout << "add param:" << tmp << "\n";
		    m->params.push_back(tmp);
		  }
		  continue;
		}

		//found HASATTRI
		if (StartsWith(line, HASATTRI)) {
		  m_iter = methods.find(curmethod);
		  if (m_iter != methods.end()) {
			Method *m = m_iter->second;
		    m->hasAttri = true;
		  }
		  continue;
		}

		//found PARENT CLASS:qualified_classname
		if (StartsWith(line, PARENT)) {
		  tmp = line.substr(PARENT.size());
		  m_iter = methods.find(curmethod);
		  if (m_iter != methods.end()) {
		    Method *m = m_iter->second;
		    m->parentclass = tmp;
		  }
		  continue;
		}
    }

    //found FUNCTION CALL:rtype qualified_methodname(params)
	if (StartsWith(line, FUNCTION_CALL)) {
	  tmp = line.substr(FUNCTION_CALL.size());
	  m_iter = methods.find(curmethod);
	  if (m_iter != methods.end()) {
	    Method *m = m_iter->second;
	    m->callexprs.insert(tmp);
	  }
	  continue;
	}
  }
}

int main(int argc, const char* argv[]) {
  string outfile = "";
  for (int i = 1; i < argc; i++) {
    //printf( "arg %d: %s\n", i, argv[i] );

	if (strcmp(argv[i], "-o") == 0)
	  outfile = argv[++i];
	else {
	  //cout << "PARSE FILE: " << argv[i] << "\n";
	  parsefile(argv[i]);
	  //cout << "DONE PARSE FILE: " << argv[i] << " methods num:" << methods.size() << " classes num:" << records.size() << "\n";
	}
  }

  for (map<string, Record*>::iterator iter = records.begin();
	     iter != records.end(); iter++)
	     AddBases(iter->second, records);
  //cout << "DONE ADDING BASES\n";

  CreateCallGraph();
  //cout << "DONE GRAPHING\n";

  MarkProcedure();
  //cout << "DONE PROCEDURING\n";

  OutputAnnotation(outfile);
  //cout << "DONE ANNOTATING\n";

  return 0;
}

