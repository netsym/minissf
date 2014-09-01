#ifndef __GRAPH_H__
#define __GRAPH_H__

#include "ssf.h"
using namespace minissf;

class Node {
 public:
  Node(int id, int policy, double mst);
  Node(int id);
  ~Node();

  void visit(int policy, double mst);
 
 public:
  int serialno;
  int node_id;
  int queue_policy;
  double mean_service_time;
  int machine; // assigned machine id
  int processor; // assigned processor id
  VECTOR(PAIR(PAIR(double,double),Node*)) fan_out; // prob, delay, node*
};

class Graph {
 public:
  Graph(STRING filename);
  ~Graph();

  inline bool operator !() const { return error != NO_ERROR; }
  const char* error_message() const; // return a description of the error
  void print(); // print out the graph for debug
  void partition(); // among available machines and processors

 public:
  enum { NO_ERROR, CANNOT_OPENFILE, INVALID_FORMAT, NONPOSITIVE_NNODES, 
	 NEGATIVE_INITJOBS, UNKNOWN_POLICY, NONPOSITIVE_MST, NEGATIVE_FANOUT, 
	 DUPLICATE_ID, NEGATIVE_PROBABILITY, NONPOSITIVE_DELAY, 
	 ZERO_FANOUT_PROBABILITY };

  int error; // error code
  int init_jobs; // the mean number of initial jobs at each node
  int nlinks; // total number of links between the nodes
  MAP(int,Node*) nodes; // map from node id to the node itself
  double min_delay, max_delay; // min and max delay for calculating edge weight
};

#endif /*__GRAPH_H__*/

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
