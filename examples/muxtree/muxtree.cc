#include <stdio.h>
#include <stdlib.h>
#include "muxtree.h"

// every user-defined event class must register with ssf
SSF_REGISTER_EVENT(MyMessage, MyMessage::create_my_message);

SourceEntity::SourceEntity(int myid) : 
  id(myid), nsent(0), rng((MUXTREE_FANIN^MUXTREE_LEVELS)+myid), oc(0)
{
#ifdef MUXTREE_DEBUG
  printf("create src[%d] on mach=%d proc=%d\n", id, 
	 ssf_machine_index(), ssf_processor_index());
#endif

  // create an output channel with an appropriate channel delay
  oc = new outChannel(this, TRANSMISSION_DELAY);

  // map the output channel to the input channel of the multiplexer at
  // the next tree level (level 0)
  char icname[128];
  sprintf(icname, "IN_%d_%d", MUXTREE_LEVELS-1, id/MUXTREE_FANIN);
  oc->mapto(icname);

  // create the emit process to generate messages
  new EmitProcess(this);
}

//! SSF PROCEDURE
void EmitProcess::action()
{
  VirtualTime tm; //! SSF STATE
  MyMessage* msg; //! SSF STATE

#ifdef MUXTREE_DEBUG
  printf("%.09lf: emit process (for src[%d]) starts: ",
	 now().second(), owner()->id);
#endif

  for(;;) {
    // wait for an exponentially distributed period of time
    tm = owner()->rng.exponential(1.0/SRC_ENTITY_IAT);
#ifdef MUXTREE_DEBUG
    printf("wait until %.09lf\n", (now()+tm).second());
#endif
    waitFor(tm);

    // create and send out a newly generated message
    msg = new MyMessage(now(), owner()->id);
    owner()->oc->write((Event*&)msg);
    owner()->nsent++;
#ifdef MUXTREE_DEBUG
    printf("%.09lf: emit process (for src[%d]) sends a message, ",
	   now().second(), owner()->id);
#endif
  }
}

MultiplexerEntity::MultiplexerEntity(int mylevel, int myid) :
  level(mylevel), id(myid), rng((MUXTREE_FANIN^mylevel)+myid),
  nrcvd(0), nlost(0), nsent(0), buf(0), inservice(0),
  tail(0), head(0), qlen(0), src_array(0)
{
#ifdef MUXTREE_DEBUG
  printf("create mux[level=%d,id=%d] on mach=%d proc=%d\n",
	 level, id, ssf_machine_index(), ssf_processor_index());
#endif

  // create the buffer to store messages in queue
  buf = new MyMessage*[MUX_ENTITY_BUFSIZ];

  // create the input channel and name it
  char icname[128];
  sprintf(icname, "IN_%d_%d", level, id);
  ic = new inChannel(this, icname);

  // create output channel and map it to input channel of the
  // multiplexer at the next tree level
  oc = new outChannel(this, TRANSMISSION_DELAY);
  if(level > 0) { // if it's not at the tree root
    sprintf(icname, "IN_%d_%d", level-1, id/MUXTREE_FANIN);
    oc->mapto(icname);
  }

  // create the internal input and output channels; map them together
  int_ic = new inChannel(this);
  int_oc = new outChannel(this);
  int_oc->mapto(int_ic);

  // create the process for handling message arrivals
  Process* arrive_proc = new ArriveProcess(this);
  arrive_proc->waitsOn(ic); // set default the input channel

  // create the process for servicing messages at the head of the queue
  Process* serve_proc = new ServeProcess(this);
  serve_proc->waitsOn(int_ic); // set the default input channel
}

//! SSF PROCEDURE
void ArriveProcess::action() 
{
  //! SSF CALL
  ((MultiplexerEntity*)owner())->arrive(this);
}

//! SSF PROCEDURE
void ServeProcess::action() 
{
  //! SSF CALL
  ((MultiplexerEntity*)owner())->serve(this);
}

MultiplexerEntity::~MultiplexerEntity() 
{
  if(inservice) delete inservice;
  while(qlen-- > 0) delete buf[(head++)%MUX_ENTITY_BUFSIZ];
  delete[] buf;
  if(src_array) delete[] src_array;
}

void MultiplexerEntity::init()
{
  if(level == MUXTREE_LEVELS-1) {
    src_array = new SourceEntity*[MUXTREE_FANIN];
    for(int i=0; i<MUXTREE_FANIN; i++) {
      src_array[i] = new SourceEntity(id*MUXTREE_FANIN+i);
      src_array[i]->alignto(this);
    }
  }
}

void MultiplexerEntity::wrapup()
{
  if(level == MUXTREE_LEVELS-1) {
    // the last level of mux nodes are connected to traffic source; we
    // add the number of messages sent from the source
    assert(src_array);
    for(int i=0; i<MUXTREE_FANIN; i++) {
      nsent += src_array[i]->nsent;
    }
    delete[] src_array;
    src_array = 0;
  }
}

//! SSF PROCEDURE
void MultiplexerEntity::arrive(Process* p)
{
  MyMessage* msg; //! SSF STATE
  Event* evt; //! SSF STATE

#ifdef MUXTREE_DEBUG
    printf("%.09lf: mux[level=%d,id=%d] arrive() starts\n", 
	   now().second(), level, id);
#endif

  for(;;) {
#ifdef MUXTREE_DEBUG
    printf("%.09lf: mux[level=%d,id=%d] arrive() waits\n",
	   now().second(), level, id);
#endif
    p->waitOn(); // wait on the default input channel

    assert(ic == p->activeChannel());
    msg = (MyMessage*)ic->activeEvent();
#ifdef MUXTREE_DEBUG
    printf("%.09lf: mux[level=%d,id=%d] arrive() receives msg from %d at time %.09lf: ",
	   now().second(), level, id, msg->srcid, msg->time.second());
#endif

    nrcvd++;
    if(qlen == MUX_ENTITY_BUFSIZ) { // if the queue is full
      nlost++; delete msg;
#ifdef MUXTREE_DEBUG
      printf("dropped due to overflow\n");
#endif
    } else {
      // save a new reference of this event into the queue
      qlen++; buf[(tail++)%MUX_ENTITY_BUFSIZ] = msg;
#ifdef MUXTREE_DEBUG
      printf("enqueued\n");
#endif

      // notify the server process if it's the first message in the queue
      if(qlen == 1) {
	evt = new Event();
	int_oc->write(evt);
      }
    }
  }
}

//! SSF PROCEDURE
void MultiplexerEntity::serve(Process* p)
{
  VirtualTime d; //! SSF STATE

#ifdef MUXTREE_DEBUG
  printf("%.09lf: mux[level=%d,id=%d] serve() starts\n", 
	 now().second(), level, id);
#endif
  for(;;) {
#ifdef MUXTREE_DEBUG
    printf("%.09lf: mux[level=%d,id=%d] serve() waits\n",
	   now().second(), level, id);
#endif
    p->waitOn();

    while(qlen > 0) {
      inservice = buf[(head++)%MUX_ENTITY_BUFSIZ]; qlen--;
#ifdef MUXTREE_DEBUG
      printf("%.09lf: mux[level=%d,id=%d] serve() wakes up: ",
	     now().second(), level, id);
#endif

      d = rng.exponential(1.0/MUX_ENTITY_MST);
#ifdef MUXTREE_DEBUG
      printf("in service until %.09lf\n", (now()+d).second());
#endif
      p->waitFor(d);

      // send the serviced message
#ifdef MUXTREE_DEBUG
      printf("%.09lf: mux[level=%d,id=%d] serve() sends msg from %d at time %.09lf\n",
	     now().second(), level, id, inservice->srcid, inservice->time.second());
#endif
      oc->write((Event*&)inservice);
      nsent++;
    }
  }
}

// The main function starts an ssf simulator instance running on each
// distributed node. SSF uses SPMD (single program multiple data)
// programming model. Here, we make sure that each ssf simulator
// instance has a roughly equal portion of the muxtree tree.
int main(int argc, char** argv)
{
  // must initialize ssf at the very beginning
  ssf_init(argc, argv);

  // make sure we have correct command line arguments
  if(argc != 2) {
    if(!ssf_machine_index()) {
      fprintf(stderr, "Usage: %s <sim_time>\n", argv[0]);
      ssf_print_options(stderr);
    }
    ssf_abort(1);
  }

  // the only argument is the simulation end time
  VirtualTime end_time(argv[1]);
  if(end_time <= 0) {
    if(!ssf_machine_index()) {
      fprintf(stderr, "ERROR: invalid simulation end time: %s\n", argv[1]);
    }
    ssf_abort(2);
  }

  // instantiate only the muxtree nodes belonging to this simulator
  // instance; here we assign the multiplexers among the distributed
  // machines in a round-robin fashion; this is for simplicity, not
  // for performance!
  VECTOR(MultiplexerEntity*) mux_vector;
  for(int level=0, nnodes=1; level<MUXTREE_LEVELS; level++, nnodes*=MUXTREE_FANIN) {
    for(int index=ssf_machine_index(); index<nnodes; index+=ssf_num_machines()) {
      MultiplexerEntity* mux = new MultiplexerEntity(level, index);
      mux_vector.push_back(mux);
    }
  }

  // start the simulation; it returns when the simulation has finished
  ssf_start(end_time);

  // between ssf_start and ssf_finalize, we can print out statistics;
  // here we can assume wrapup methods for all processes and entities
  // are called already by now
  long nsent = 0, nrcvd = 0, nlost = 0;
  for(VECTOR(MultiplexerEntity*)::iterator iter = mux_vector.begin();
      iter != mux_vector.end(); iter++) {
    nsent += (*iter)->nsent;
    nrcvd += (*iter)->nrcvd;
    nlost += (*iter)->nlost;
  }

#ifdef HAVE_MPI_H
  // one should be able to use mpi too to communicate between simulator instances
  if(ssf_num_machines() > 1) {
    long sendbuf[3], recvbuf[3];
    sendbuf[0] = nsent; sendbuf[1] = nrcvd; sendbuf[2] = nlost;
    MPI_Reduce(sendbuf, recvbuf, 3, MPI_LONG, MPI_SUM, 0, ssf_machine_communicator());
    nsent = recvbuf[0]; nrcvd = recvbuf[1]; nlost = recvbuf[2];
  }
#endif

  if(!ssf_machine_index()) {
    printf("simulation results: nrcvd=%ld, nlost=%ld, nsent=%ld\n", 
	   nrcvd, nlost, nsent);
  }

  // don't forget to wrap up the simulator (noting that there
  // shouldn't be any more work after this call)
  ssf_finalize();
  return 0;
}

/*
 * Copyright (c) 2011-2014 Florida International University.
 *
 * Permission is hereby granted, free of charge, to any individual or
 * institution obtaining a copy of this software and associated
 * documentation files (the "software"), to use, copy, modify, and
 * distribute without restriction.
 *
 * The software is provided "as is", without warranty of any kind,
 * express or implied, including but not limited to the warranties of
 * merchantability, fitness for a particular purpose and
 * noninfringement.  In no event shall Florida International
 * University be liable for any claim, damages or other liability,
 * whether in an action of contract, tort or otherwise, arising from,
 * out of or in connection with the software or the use or other
 * dealings in the software.
 *
 * This software is developed and maintained by
 *
 *   Modeling and Networking Systems Research Group
 *   School of Computing and Information Sciences
 *   Florida International University
 *   Miami, Florida 33199, USA
 *
 * You can find our research at http://www.primessf.net/.
 */
