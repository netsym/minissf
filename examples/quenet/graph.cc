#include <stdio.h>
#include <strings.h>
#include "quenet.h"

#define METIS_USE_RECURSIVE
extern "C" {
#include "metis.h"
}

// return an edge weight for the graph; 0 if all the same
static idxtype fx(double min_delay, double max_delay, double delay) {
  idxtype ret;
  if(max_delay == min_delay) ret = 0;
  else {
    double x = (max_delay-delay)/(max_delay-min_delay); x += 1.0;
    x = pow(x, 0.25); // make it favorable to large delays
    ret = (idxtype)(x*16384);
  }
  return ret;
}

Node::Node(int id, int policy, double mst) :
  node_id(id), queue_policy(policy), 
  mean_service_time(mst), machine(0), processor(0) {}

Node::Node(int id) : 
  node_id(id), queue_policy(QueueNode::NONE),
  machine(0), processor(0) {}

Node::~Node() {}

void Node::visit(int policy, double mst) {
  queue_policy = policy;
  mean_service_time = mst;
}

Graph::Graph(STRING filename) : 
  error(NO_ERROR), nlinks(0), min_delay(1e38), max_delay(0)
{
  FILE* fptr = fopen(filename.c_str(), "r");
  if(!fptr) { error = CANNOT_OPENFILE; return; }

  // get number of queues and average number of init jobs per queue
  int nnodes;
  if(fscanf(fptr, "%d %d\n", &nnodes, &init_jobs) != 2) 
    { error = INVALID_FORMAT; return; }
  if(nnodes <= 0) { error = NONPOSITIVE_NNODES; return; }
  if(init_jobs < 0) { error = NEGATIVE_INITJOBS; return; }

  // each queue occupies a separate line
  for(int i=0; i<nnodes; i++) {
    int id, fanout; char policy[16]; double mst;
    if(fscanf(fptr, "%d %10s %lf %d", &id, policy, &mst, &fanout) != 4) 
      { error = INVALID_FORMAT; return; }
    policy[9] = 0; // make sure string is null-terminated

    int mypolicy;
    if(!strcasecmp(policy, "FIFO")) mypolicy = QueueNode::FIFO;
    else if(!strcasecmp(policy, "LIFO")) mypolicy = QueueNode::LIFO;
    else if(!strcasecmp(policy, "RANDOM")) mypolicy = QueueNode::RANDOM;
    else { error = UNKNOWN_POLICY; return; }

    if(mst <= 0.0) { error = NONPOSITIVE_MST; return; }
    if(fanout < 0) { error = NEGATIVE_FANOUT; return; }

    Node* node;
    MAP(int,Node*)::iterator iter = nodes.find(id);
    if(iter != nodes.end()) {
      if((*iter).second->queue_policy != QueueNode::NONE) { error = DUPLICATE_ID; return; }
      else { node = (*iter).second; node->visit(mypolicy, mst); }
    } else {
      node = new Node(id, mypolicy, mst);
      nodes.insert(MAKE_PAIR(id, node));
    }

    // for each output branch
    double acc_prob = 0;
    for(int k=0; k<fanout; k++) {
      int tid; double prob, delay;
      if(fscanf(fptr, "%d %lf %lf", &tid, &prob, &delay) != 3) 
	{ error = INVALID_FORMAT; return; }

      if(prob < 0) { error = NEGATIVE_PROBABILITY; return; }
      else acc_prob += prob;
      if(delay <= 0) { error = NONPOSITIVE_DELAY; return; }

      if(delay < min_delay) min_delay = delay;
      if(delay > max_delay) max_delay = delay;
      
      Node* tonode;
      iter = nodes.find(tid);
      if(iter != nodes.end()) tonode = (*iter).second;
      else { tonode = new Node(tid); nodes.insert(MAKE_PAIR(tid, tonode)); }

      node->fan_out.push_back(MAKE_PAIR(MAKE_PAIR(acc_prob,delay),tonode));
      if(fscanf(fptr, "\n") < 0) 
	{ error = INVALID_FORMAT; return; }
    }
    nlinks += fanout;
    if(fanout > 0) {
      if(acc_prob <= 0) { error = ZERO_FANOUT_PROBABILITY; return; }
      else if(acc_prob != 1.0) {
	for(VECTOR(PAIR(PAIR(double,double),Node*))::iterator f_iter = node->fan_out.begin();
	    f_iter != node->fan_out.end(); f_iter++) 
	  (*f_iter).first.first /= acc_prob;
      }
    }
  }

  fclose(fptr);
}

Graph::~Graph()
{
  for(MAP(int,Node*)::iterator iter = nodes.begin();
      iter != nodes.end(); iter++) 
    delete (*iter).second;
  nodes.clear();
}

const char* Graph::error_message() const
{
  switch(error) {
  case NO_ERROR: return "no error";
  case CANNOT_OPENFILE: return "cannot open file";
  case INVALID_FORMAT: return "unrecognized file format";
  case NONPOSITIVE_NNODES: return "number of queues must be positve";
  case NEGATIVE_INITJOBS: return "number of intial jobs must be non-negative";
  case UNKNOWN_POLICY: return "unknown queuing policy";
  case NONPOSITIVE_MST: return "mean service time should be positive";
  case NEGATIVE_FANOUT: return "fanout should be non-negative";
  case DUPLICATE_ID: return "duplicate queue id";
  case NEGATIVE_PROBABILITY: return "branching probability must be non-negative";
  case NONPOSITIVE_DELAY: return "delay between queues must be positive";
  case ZERO_FANOUT_PROBABILITY: return "total branching probability must be positive";
  default: assert(0);
  }
}

void Graph::print()
{
  printf("%d %d\n", (int)nodes.size(), init_jobs);
  for(MAP(int,Node*)::iterator iter = nodes.begin();
      iter != nodes.end(); iter++) {
    Node* node = (*iter).second;
    printf("%d %s %g %d", node->node_id, 
	   ((node->queue_policy==QueueNode::FIFO)?"FIFO":
	    ((node->queue_policy==QueueNode::LIFO)?"LIFO":
	     ((node->queue_policy==QueueNode::RANDOM)?"RANDOM":"NONE"))),
	   node->mean_service_time, (int)node->fan_out.size());
    for(VECTOR(PAIR(PAIR(double,double),Node*))::iterator f_iter = node->fan_out.begin();
	f_iter != node->fan_out.end(); f_iter++) {
      printf(" %d %lg %lg", (*f_iter).second->node_id, 
	     (*f_iter).first.first, (*f_iter).first.second);
    }
    printf(" [mach=%d,proc=%d]\n", node->machine, node->processor);
  }
}

void Graph::partition()
{
  int i;
  MAP(int,Node*)::iterator iter;

  int n = nodes.size();
  idxtype *xadj = new idxtype[n+1];
  xadj[0] = 0;
  idxtype* adjncy = nlinks ? new idxtype[nlinks] : 0;
  idxtype* adjwgt = nlinks ? new idxtype[nlinks] : 0;
  int wgtflag = 3;
  int numflag = 0;
  int nparts = ssf_num_machines();
  int options[5]; options[0] = 0;
  int edgecut;
  idxtype* vwgt = new idxtype[n];
  idxtype* part = new idxtype[n];

  float* tpwgts = new float[nparts];
  float sumwgt = (float)ssf_total_num_processors();
  for(i=0; i<ssf_num_machines(); i++)
    tpwgts[i] = (float)ssf_num_processors(i)/sumwgt;

  // partition among the machines
  if(nparts == 1) {
    // we don't do anything since the graph is initialized this way
    /*
    for(iter = nodes.begin(); iter != nodes.end(); iter++) 
      (*iter).second->machine = 0;
    */
  } else {
    int x = 0;
    for(iter = nodes.begin(); iter != nodes.end(); iter++) 
      (*iter).second->serialno = x++;

    x = 0; 
    for(iter = nodes.begin(), i=0; iter != nodes.end(); iter++, i++) {
      Node* node = (*iter).second;
      vwgt[i] = 1; // all nodes are of equal weight
      if(min_delay < max_delay) {
	for(VECTOR(PAIR(PAIR(double,double),Node*))::iterator f_iter = node->fan_out.begin();
	    f_iter != node->fan_out.end(); f_iter++) {
	  idxtype edge = fx(min_delay, max_delay, (*f_iter).first.second);
	  adjncy[x] = (*f_iter).second->serialno;
	  adjwgt[x] = edge;
	  x++;
	}
      }
      xadj[i+1] = x;
    }
    assert(x == 0 || x == nlinks);

    if(x == 0) wgtflag = 2;
#ifdef METIS_USE_RECURSIVE
    if(nparts <= 8) {
      METIS_WPartGraphRecursive(&n, xadj, x>0?adjncy:0, vwgt, x>0?adjwgt:0, &wgtflag,
				&numflag, &nparts, tpwgts, options, &edgecut, part);
    } else {
#endif
      METIS_WPartGraphKway(&n, xadj, x>0?adjncy:0, vwgt, x>0?adjwgt:0, &wgtflag,
			   &numflag, &nparts, tpwgts, options, &edgecut, part);
#ifdef METIS_USE_RECURSIVE
    }
#endif

    i = 0;
    for(iter = nodes.begin(); iter != nodes.end(); iter++) {
      (*iter).second->machine = part[i++];
    }
  }
    
  // partitioning among the processors
  i = ssf_machine_index();
  nparts = ssf_num_processors();
  if(nparts == 1) {
    // actually the graph's initialized this way, nothing needed here
    /*
    for(iter = nodes.begin(); iter != nodes.end(); iter++) 
      if((*iter).second->machine == i) (*iter).second->processor = 0;
    */
  } else {
    int x = 0;
    for(iter = nodes.begin(); iter != nodes.end(); iter++) 
      if((*iter).second->machine == i) (*iter).second->serialno = x++;

    wgtflag = 3;
    numflag = 0;
    options[0] = 0;
    n = 0; xadj[0] = 0; x = 0;
    for(iter = nodes.begin(); iter != nodes.end(); iter++) {
      Node* node = (*iter).second;
      if(node->machine == i) {
	vwgt[n] = 1; // all nodes are of equal weight
	if(min_delay < max_delay) {
	  for(VECTOR(PAIR(PAIR(double,double),Node*))::iterator f_iter = node->fan_out.begin();
	      f_iter != node->fan_out.end(); f_iter++) {
	    if((*f_iter).second->machine != i) continue;
	    idxtype edge = fx(min_delay, max_delay, (*f_iter).first.second);
	    adjncy[x] = (*f_iter).second->serialno;
	    adjwgt[x] = edge;
	    x++;
	  }
	}
	xadj[++n] = x;
      }
    }
    if(n != 0) { // only if the subgraph has links
      if(x == 0) wgtflag = 2;
#ifdef METIS_USE_RECURSIVE
      if(nparts <= 8) {
	METIS_PartGraphRecursive(&n, xadj, x>0?adjncy:0, vwgt, x>0?adjwgt:0, &wgtflag,
				 &numflag, &nparts, options, &edgecut, part);
      } else {
#endif
	METIS_PartGraphKway(&n, xadj, x>0?adjncy:0, vwgt, x>0?adjwgt:0, &wgtflag,
			    &numflag, &nparts, options, &edgecut, part);
#ifdef METIS_USE_RECURSIVE
      }
#endif
      x = 0;
      for(iter = nodes.begin(); iter != nodes.end(); iter++) 
	if((*iter).second->machine == i) 
	  (*iter).second->processor = part[x++];
    }
  }

  delete[] xadj;
  if(adjncy) delete[] adjncy;
  if(adjwgt) delete[] adjwgt;
  delete[] tpwgts;
  delete[] vwgt;
  delete[] part;
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
