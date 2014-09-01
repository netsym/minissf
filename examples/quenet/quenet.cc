#include <assert.h>
#include <stdio.h>
#include "quenet.h"

SSF_REGISTER_EVENT(Job, Job::create_job);

//! SSF PROCEDURE
void ArriveProcess::action() 
{
  //! SSF CALL
  ((QueueNode*)owner())->arrival(this);
}

//! SSF PROCEDURE
void ServeProcess::action() 
{
  //! SSF CALL
  ((QueueNode*)owner())->service(this);
}

QueueNode::QueueNode(int njobs, Node* node) :
  init_jobs(njobs), queue_id(node->node_id), queuing_policy(node->queue_policy), 
  mean_service_time(node->mean_service_time, VirtualTime::SECOND), inservice(0), 
  rng(node->node_id+12345),  rcvd_jobs(0), thru_jobs(0)
{
  // create the queue (diff data struct for diff queuing policy)
  switch(queuing_policy) {
  case FIFO: 
  case LIFO:
    qdeque = new DEQUE(Job*); 
    break;
  case RANDOM:
    qvector = new VECTOR(Job*);
    break;
  default: assert(0);
  }

  // input channel (named after the queue id)
  char icname[32];
  sprintf(icname, "Q%d", queue_id);
  ic = new inChannel(this, icname);

  // used to sync between arrival and service
  sem = new Semaphore(this, 0);

  fanout = node->fan_out.size();
  if(fanout == 0) { ocs = 0; accu_probs = 0; return; }

  accu_probs = new double[fanout];
  ocs = new outChannel*[fanout];
  VECTOR(PAIR(PAIR(double,double),Node*))::iterator iter; int j;
  for(j = 0, iter = node->fan_out.begin(); 
      iter != node->fan_out.end(); iter++, j++) {
    accu_probs[j] = (*iter).first.first;
    ocs[j] = new outChannel(this, VirtualTime((*iter).first.second, VirtualTime::SECOND));

    // map it to the input channel of the sebsequent queue
    char icname[32];
    sprintf(icname, "Q%d", (*iter).second->node_id);
    ocs[j]->mapto(icname);
#ifdef QUENET_DEBUG
    printf("queue[%d] mapto queue %d\n", queue_id, (*iter).second->node_id);
#endif
  }
}

QueueNode::~QueueNode()
{
  // reclaim events in queue
  switch(queuing_policy) {
  case FIFO:
  case LIFO:
    for(DEQUE(Job*)::iterator iter = qdeque->begin(); 
	iter != qdeque->end(); iter++) delete (*iter);
    delete qdeque;
    break;
  case RANDOM:
    for(VECTOR(Job*)::iterator iter = qvector->begin();
	iter != qvector->end(); iter++) delete (*iter);
    delete qvector;
    break;
  default: assert(0);
  }
  if(inservice) delete inservice;

  if(fanout > 0) {
    delete[] ocs; // delete the array; not the output channels
    delete[] accu_probs;
  }
  delete sem;
}

void QueueNode::init()
{
  // insert the jobs into the queue
  init_jobs = (int)rng.poisson((double)init_jobs);
  for(int i=0; i<init_jobs; i++)
    insert_job(new Job(), now());
#ifdef QUENET_DEBUG
  printf("queue[%d] init with %d jobs\n", queue_id, init_jobs);
#endif

  Process* arrive_proc = new ArriveProcess(this);
  arrive_proc->waitsOn(ic);
  new ServeProcess(this);
}

void QueueNode::wrapup()
{
  /*
  printf("Queue %d:\n", queue_id);
  printf("  number of jobs received: %ld\n", rcvd_jobs);
  printf("  number of jobs serviced: %ld\n", thru_jobs);
  printf("  throughput: %g\n", thrujobs/now().second());
  */
}

//! SSF PROCEDURE
void QueueNode::arrival(Process* p)
{
  VirtualTime t; //! SSF STATE
  Job* job; //! SSF STATE

#ifdef QUENET_DEBUG
  printf("%.09lf: queue[%d] arrival() starts\n", now().second(), queue_id);
#endif
  // if started with some jobs already in the queue, activate the server
  if(init_jobs > 0) sem->signal();

  for(;;) {
    p->waitOn(); // wait on the default input channel
    t = now();
    job = (Job*)ic->activeEvent();
#ifdef QUENET_DEBUG
    printf("%.09lf: queue[%d] received a job\n", t.second(), queue_id);
#endif
    rcvd_jobs++;
    insert_job(job, t);
    if(queue_length() == 1) sem->signal();
  }
}

//! SSF PROCEDURE
void QueueNode::service(Process* p)
{
  VirtualTime t; //! SSF STATE
  int k; //! SSF STATE
  double u; //! SSF STATE

  for(;;) {
    sem->wait();

    while(queue_length() > 0) {
      t = now();
      assert(!inservice);
      inservice = retrieve_job(t);
      assert(inservice);

      // job enters service
      inservice->enservice(t);
      t = rng.exponential(1.0/mean_service_time);
#ifdef QUENET_DEBUG
      printf("%.09lf: queue[%d] starts servicing job for %.09lf seconds\n", 
	     now().second(), queue_id, t.second());
#endif
      p->waitFor(t);
      t = now();
      inservice->deservice(t);

      // send the serviced job
      if(fanout > 0) {
	//int k;
	if(fanout > 1) {
	  u = rng.uniform();
	  for(k=0; k<fanout; k++) {
	    if(u <= accu_probs[k]) break;
	  }
	} else k = 0;
#ifdef QUENET_DEBUG
	printf("%.09lf: queue[%d] sends job via branch %d\n", t.second(), queue_id, k);
#endif
	ocs[k]->write((Event*&)inservice);
      }
      thru_jobs++;
    }
  }
}

int QueueNode::queue_length() const
{
  switch(queuing_policy) {
  case FIFO: 
  case LIFO:
    return qdeque->size();
  case RANDOM:
    return qvector->size();
  default: assert(0);
  }
  return 0;
}

void QueueNode::insert_job(Job* job, VirtualTime t)
{
  job->enqueue(t);
  switch(queuing_policy) {
  case FIFO: 
  case LIFO:
    qdeque->push_back(job);
    break;
  case RANDOM:
    qvector->push_back(job);
    break;
  default: assert(0);
  }
}

Job* QueueNode::retrieve_job(VirtualTime t)
{
  Job* job;
  assert(queue_length() > 0);
  switch(queuing_policy) {
  case FIFO:
    job = qdeque->front();
    qdeque->pop_front();
    break;
  case LIFO:
    job = qdeque->back();
    qdeque->pop_back();
    break;
  case RANDOM: {
    int len = queue_length();
    int idx = rng.equilikely(0, len-1);
    job = (*qvector)[idx];
    (*qvector)[idx] = (*qvector)[len-1];
    qvector->pop_back();
    break; 
  }
  default: assert(0);
  }
  job->dequeue(t);
  return job;
}

int main(int argc, char* argv[])
{
  // must initialize ssf at the very beginning
  ssf_init(argc, argv);

  int machid = ssf_machine_index();
  int nmachs = ssf_num_machines();
  int nprocs = ssf_num_processors();

  if(argc != 3) {
    if(!machid) {
      fprintf(stderr, "Usage: %s <sim_time> <topo_file>\n", argv[0]);
      ssf_print_options(stderr);
    }
    ssf_abort(1);
  }

  VirtualTime end_time(argv[1]);
  if(0 >= end_time) {
    if(!machid)
      fprintf(stderr, "ERROR: invalid simulation time: %s\n", argv[1]);
    ssf_abort(2);
  }

  if(!machid) {
    printf("SIMULATION TIME: %g\n", end_time.second());
    printf("INPUT FILE: %s\n", argv[2]);  
  }
  
  // parse the input file that describes the queuing network
  Graph* graph = new Graph(argv[2]); assert(graph);
  if(!(*graph)) {
    if(!machid)
      fprintf(stderr, "ERROR: %s\n", graph->error_message());
    delete graph;
    ssf_abort(3);
  }

  graph->partition();
  //graph->print();

  VECTOR(QueueNode*) qnodes;
  QueueNode** alignment = new QueueNode*[nprocs]; assert(alignment);
  memset(alignment, 0, nprocs*sizeof(QueueNode*));

  for(MAP(int,Node*)::iterator iter = graph->nodes.begin();
      iter != graph->nodes.end(); iter++) {
    Node* node = (*iter).second;
    assert(0 <= node->machine && node->machine < nmachs);
    if(node->machine != machid) continue;

    QueueNode* q = new QueueNode(graph->init_jobs, node); assert(q);
    qnodes.push_back(q);
    
    assert(0 <= node->processor && node->processor < nprocs);
    /*
    if(alignment[node->processor]) q->alignto(alignment[node->processor]);
    else alignment[node->processor] = q;
    */
  }
  delete[] alignment;
  delete graph;

  // start the simulation; it returns when the simulation has finished
  ssf_start(end_time);

  if(!machid) printf("MACH\tARRIVAL\tSERVICE\n");
  long sum_nrcvd = 0;
  long sum_nthru = 0;
  for(VECTOR(QueueNode*)::iterator iter = qnodes.begin();
      iter != qnodes.end(); iter++) {
    sum_nrcvd += (*iter)->rcvd_jobs;
    sum_nthru += (*iter)->thru_jobs;
  }
  printf("%-4d\t%-6ld\t%-7ld\n", machid, sum_nrcvd, sum_nthru);

#ifdef HAVE_MPI_H
  if(nmachs > 0) {
    long sendbuf[2], recvbuf[2];
    sendbuf[0] = sum_nrcvd; sendbuf[1] = sum_nthru;
    MPI_Reduce(sendbuf, recvbuf, 2, MPI_LONG, MPI_SUM, 0, ssf_machine_communicator());
    sum_nrcvd = recvbuf[0]; sum_nthru = recvbuf[1];
  }
#endif

  if(!machid) {
    printf("*   \t%-6ld\t%-7ld\n", sum_nrcvd, sum_nthru);
    printf("Overall throughput: %g\n", sum_nthru/end_time.second());
  }

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
