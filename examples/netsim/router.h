#ifndef __NETSIM_ROUTER_H__
#define __NETSIM_ROUTER_H__

#include "netsim.h"

// there is a routing entry for each network interface; since the
// class contains a method to be used as ssf procedure, it must be
// derived from the ProcedureContainer class
class RoutingEntry : public ProcedureContainer {
public:
  Router* router; // points to the owning router
  int entry_idx; // routing entry index (the router has an array of routing entries)
  int start_host_id; // start of routing range (inclusive)
  int end_host_id; // end of routing range (inclusive)
  VirtualTime delay;  // propagation delay of the link
  double bandwidth; // in bits/second
  outChannel* oc; // outchannel for outgoing packets (it's an alias)
  IPPacket* buffer[NIC_BUFSIZ]; // buffers for queueing packets going out (circular list)
  int buffer_head; // head of the circular buffer
  int buffer_tail; // next to tail of the circular buffer
  Semaphore* sem; // semaphore for connecting arrival and service processes

  // the constructor
  RoutingEntry(Router* router, int entidx, int startid, int endid, VirtualTime d, double bw);

  // the destructor
  ~RoutingEntry();

  // called by the router() process in Router; return true if the
  // packet is indeeded routed by this entry
  bool route(IPPacket* pkt);

  // the process that sends packets queued up in the buffer
  void dispatch(Process*) __attribute__ ((annotate("ssf_starting_procedure")));
};

// each router is an ssf entity; the router is responsible for
// forwarding a packet from the source host to its destination host
class Router : public Entity {
public:
  Router(int id, int start_host_id, int end_host_id, int num_ups,
	 VirtualTime up_delay, double up_bandwidth,
	 int down_start_host_id, int down_end_host_id, int num_downs,
	 VirtualTime down_delay, double down_bandwidth);
  virtual ~Router();
  virtual void wrapup();

  // this is the procedure for the router process, which handles each
  // incoming packet, and find the route to send the packet
  void router(Process*) __attribute__ ((annotate("ssf_starting_procedure")));

public:
  int router_id; // the router id
  int nroutes; // number of routing entries for moving up and down the hierarchy
  RoutingEntry** routings; // routing outgoing packets

  inChannel* ic; // the inchannel for all incoming packets
  outChannel** oc; // one outchannel for each network interface
 
  int stat_nsent; // total number of packets sent
  int stat_nrcvd; // total number of packets received
  int stat_nlost; // total packets lost because of buffer overflow
};

#endif /*__NETSIM_ROUTER_H__*/

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
