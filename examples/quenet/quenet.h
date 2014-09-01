#ifndef __QUENET_H__
#define __QUENET_H__

#include "ssf.h"
#include "graph.h"
using namespace minissf;

// commenting out the following to disable verbose debugging info
//#define QUENET_DEBUG

class Job : public Event {
 public:
  int visits; // number of queues visited so far
  VirtualTime qtime; // total time spent in queue (except service)
  VirtualTime stime; // total time spent in service
  VirtualTime marker; // temporarily store the starting time

  // the costructors and the destructor
  Job() : visits(0), qtime(0), stime(0) {}
  Job(int v, VirtualTime q, VirtualTime s) : visits(v), qtime(q), stime(s) {}
  virtual ~Job() {}

  // the copy constructor and the clone method are required
  Job(const Job& job) : Event(job), visits(job.visits), qtime(job.qtime), stime(job.stime) {}
  virtual Event* clone() { return new Job(*this); }

  // serialization
  virtual int pack(char* buf, int bufsiz) {
    /*
    CompactDataType* dat = new CompactDataType;
    dat->add_int(visits);
    dat->add_virtual_time(qtime);
    dat->add_virtual_time(stime);
    int dsize = dat->pack(buf, bufsiz);
    delete dat;
    return dsize;
    */
    int pos = 0;
    CompactDataType::serialize(visits, buf, bufsiz, &pos);
    CompactDataType::serialize(qtime.get_ticks(), buf, bufsiz, &pos);
    CompactDataType::serialize(stime.get_ticks(), buf, bufsiz, &pos);
    return pos;
  }

  // deserialization
  static Event* create_job(char* buf, int bufsiz) {
    /*
    CompactDataType* dat = new CompactDataType;
    dat->unpack(buf, bufsiz);
    int v; VirtualTime q, s;
    if(!dat->get_int(&v) || !dat->get_virtual_time(&q) || 
       !dat->get_virtual_time(&s)) {
      delete dat;
      return 0;
    } else {
      delete dat;
      return new Job(v, q, s);
    }
    */
    int pos = 0;
    int64 qtick, stick; int v;
    CompactDataType::deserialize(v, buf, bufsiz, &pos);
    CompactDataType::deserialize(qtick, buf, bufsiz, &pos);
    CompactDataType::deserialize(stick, buf, bufsiz, &pos);
    return new Job(v, VirtualTime(qtick), VirtualTime(stick));
  }
  
  void enqueue(VirtualTime now) { visits++; marker = now; }
  void dequeue(VirtualTime now) { qtime += (now-marker); }
  void enservice(VirtualTime now) { marker = now; }
  void deservice(VirtualTime now) { stime += (now-marker); }

  // every event must be declared as such
  SSF_DECLARE_EVENT(Job);
};

class QueueNode : public Entity {
 public:
  enum { NONE, FIFO, LIFO, RANDOM };

  QueueNode(int njobs, Node* node); // constructor
  virtual ~QueueNode(); // destructor

  virtual void init(); // initialization
  virtual void wrapup(); // finalization (collecting stats)

  void arrival(Process*); //! SSF PROCEDURE
  void service(Process*); //! SSF PROCEDURE

 public:
  int init_jobs; // average number of initial jobs in the queue; it's poisson distributed
  int queue_id; // the node id
  int queuing_policy; // queuing policy
  int fanout; // number of output channels
  VirtualTime mean_service_time; // mean service time

  union { // store the jobs in the queue
    DEQUE(Job*)* qdeque;   // double ended queue for FIFO and LIFO
    VECTOR(Job*)* qvector; // vector for RANDOM
  };
  Job* inservice; // job now in service

  inChannel* ic; // portal for job arrival
  outChannel** ocs; // an output channel for each branch
  double* accu_probs; // accumulative branching probabilities

  Semaphore* sem; // semaphore used to coordinate processes
  LehmerRandom rng; // random number generator

  long rcvd_jobs; // number of jobs arrived at this queue
  long thru_jobs; // number of jobs processed so far

  int queue_length() const;  // number of jobs in queue
  void insert_job(Job* job, VirtualTime now); // insert a job into the queue
  Job* retrieve_job(VirtualTime now); // get a job from the queue according to the queuing policy
};

class ArriveProcess : public Process {
public:
  ArriveProcess(QueueNode* owner) : Process(owner) {}
  virtual void action(); //! SSF PROCEDURE
};

class ServeProcess : public Process {
public:
  ServeProcess(QueueNode* owner) : Process(owner) {}
  virtual void action(); //! SSF PROCEDURE
};

#endif /*__QUENET_H__*/

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
