/* This example is explained in the minissf User's Manual, which is
 * located at http://www.primessf.net/minissf/usrman/.
 */

#include <stdio.h>

#include "ssf.h"
using namespace minissf;

class MyEntity : public Entity {
public:
  int id;
  inChannel* ic;
  outChannel* oc;

  MyEntity(int id);
  void printout(Process* p); //! SSF PROCEDURE
};

class MyProcess : public Process {
public:
  MyProcess(Entity* ent) : Process(ent) {}
  virtual void action(); //! SSF PROCEDURE
};

//! SSF PROCEDURE
void MyProcess::action() 
{
  //! SSF CALL
  ((MyEntity*)owner())->printout(this); 
}

MyEntity::MyEntity(int i) : id(i) 
{
  char icname[128]; sprintf(icname, "IN_%d", id);
  ic = new inChannel(this, icname); 
  oc = new outChannel(this);
  new MyProcess(this);

  if(id == 0) {
    Event* evt = new Event();
    oc->write(evt);
  }
}

//! SSF PROCEDURE
void MyEntity::printout(Process* p) 
{
  p->waitOn(ic);
  printf("%.09lf: entity %d says \"Hello, world!\"\n", now().second(), id);
  if(id > 0) {
    Event* evt = new Event();
    oc->write(evt);
  }
}

int main(int argc, char* argv[])
{
  ssf_init(argc, argv);

  int n = ssf_num_machines();
  int p = ssf_machine_index();

  // we instantiate only one entity per machine (mpi process)
  MyEntity* ent = new MyEntity(p);
  char icname[128]; sprintf(icname, "IN_%d", (p+1)%n);

  // we map the output channel of this entity to the input channel of
  // the next entity, where all entities are arranged as a ring
  ent->oc->mapto(icname, 1);

  ssf_start(n+10);

  ssf_finalize();
  return 0;
}
