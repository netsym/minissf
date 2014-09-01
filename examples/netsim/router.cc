#include <stdio.h>

#include "router.h"
#include "ippacket.h"

class RouterDispatchProcess : public Process {
public:
  RoutingEntry* entry;
  RouterDispatchProcess(RoutingEntry* rt) : Process(rt->router), entry(rt) {}
  virtual void action() { entry->dispatch(this); }
};

RoutingEntry::RoutingEntry(Router* r, int idx, int startid, int endid, VirtualTime d, double bw) :
  router(r), entry_idx(idx), start_host_id(startid), end_host_id(endid), 
  delay(d), bandwidth(bw) 
{
  oc = new outChannel(router, delay); assert(oc);
  r->oc[entry_idx] = oc;

  buffer_head = buffer_tail = 0;
  sem = new Semaphore(router);

  new RouterDispatchProcess(this);
}

RoutingEntry::~RoutingEntry()
{
  // clean up the circular buffer
  while(buffer_head != buffer_tail) {
    delete buffer[buffer_head];
    buffer_head = (buffer_head+1)%NIC_BUFSIZ;
  }
  delete sem;
}

bool RoutingEntry::route(IPPacket* pkt) 
{
  assert(pkt);

  int dest = pkt->dest;
  // if the destination is within range
  if(start_host_id <= dest && dest <= end_host_id) { 
    int nxt = (buffer_tail+1)%NIC_BUFSIZ;
    if(buffer_head == nxt) { 
      // if the buffer is full, we drop the packet
      delete pkt;
      router->stat_nlost++; 
    } else {
      // otherwise, put the packet into the circular buffer
      buffer[buffer_tail] = pkt;
      buffer_tail = nxt;

      // this is the first packet in buffer, signal the dispatcher process
      if(buffer_head == buffer_tail) sem->signal(); 
    }
    return true;
  }
  return false;
}

void RoutingEntry::dispatch(Process* p)
{
  for(;;) {
    // block until there are packets in the buffer to be sent
    sem->wait(); 

    // send all packets in the buffer one by one
    while(buffer_head != buffer_tail) {
      IPPacket* pkt = buffer[buffer_head];
      buffer_head = (buffer_head+1)%NIC_BUFSIZ;

      // wait for some transmission delay
      if(pkt->ackno && pkt->seqno > 0) 
	p->waitFor(VirtualTime(TCP_ACKSIZ_IN_BITS/bandwidth, VirtualTime::SECOND));
      else
	p->waitFor(VirtualTime(TCP_PKTSIZ_IN_BITS/bandwidth, VirtualTime::SECOND));

      // send the packet out of the outchannel
      oc->write((Event*&)pkt);
      router->stat_nsent++;
    }
  }
}

class RouterProcess : public Process {
public:
  RouterProcess(Entity* ent) : Process(ent) {}
  virtual void action() { ((Router*)owner())->router(this); }
};

Router::Router(int id, int start_host_id, int end_host_id, int num_ups,
	       VirtualTime up_delay, double up_bandwidth,
	       int down_start_host_id, int down_end_host_id, int num_downs,
	       VirtualTime down_delay, double down_bandwidth) :
  router_id(id)
{
#ifdef NETSIM_DEBUG
  printf("create router[id=%d,up_hosts=(%d-%d),nups=%d,up_delay=%.09lf,up_bdw=%lg,"
	 "down_hosts=(%d-%d),ndowns=%d,down_delay=%.09lf,down_bdw=%lg]\n", router_id,
	 start_host_id, end_host_id, num_ups, up_delay.second(), up_bandwidth,
	 down_start_host_id, down_end_host_id, num_downs, down_delay.second(), down_bandwidth);
#endif

  char icname[128];
  sprintf(icname, "IN_%d", id);
  ic = new inChannel(this, icname); 
  assert(ic);
 
  nroutes = num_ups+num_downs; if(num_ups>1) nroutes--;
  oc = new outChannel*[nroutes]; assert(oc);
  routings = new RoutingEntry*[nroutes]; assert(routings);
  memset(oc, 0, nroutes*sizeof(outChannel*));
  memset(routings, 0, nroutes*sizeof(RoutingEntry*));

#ifdef NETSIM_DEBUG
  printf("there are %d routes:\n", nroutes);
#endif
  // settle the routing entries for down links
  int grpsiz = down_end_host_id-down_start_host_id+1; // the number of hosts under control of this router
  int subsiz = grpsiz/num_downs; // the number of hosts managed by each outgoing routing entry
  for(int i=0; i<num_downs; i++) {
    int x = down_start_host_id+i*subsiz;
    int y = x+subsiz-1;
    routings[i] = new RoutingEntry(this, i, x, y, down_delay, down_bandwidth);
    assert(routings[i]);
#ifdef NETSIM_DEBUG
    printf(" insert down routing entry=%d, range=(%d,%d), delay=%.09lf, bdw=%lg\n", 
	   i, x, y, down_delay.second(), down_bandwidth);
#endif
  }

  // settle the routing entries for up links
  if(num_ups > 1) {
    grpsiz = end_host_id-start_host_id+1;
    subsiz = grpsiz/num_ups;
    for(int i=0, j=0; i<num_ups; i++) {
      int x = start_host_id+i*subsiz;
      int y = x+subsiz-1;
      if(down_start_host_id == x) {
	assert(down_end_host_id == y);
	continue;
      }
      routings[j+num_downs] = new RoutingEntry(this, j+num_downs, x, y, up_delay, up_bandwidth);
      assert(routings[j+num_downs]);
#ifdef NETSIM_DEBUG
      printf(" insert up routing entry=%d, range=(%d,%d), delay=%.09lf, bdw=%lg\n", 
	     j+num_downs, x, y, up_delay.second(), up_bandwidth);
#endif
      j++;
    }
  } else {
    routings[num_downs] = new RoutingEntry(this, num_downs, start_host_id, end_host_id, 
					   up_delay, up_bandwidth);
    assert(routings[num_downs]);
#ifdef NETSIM_DEBUG
    printf(" insert up routing entry=%d, range=(%d,%d), delay=%.09lf, bdw=%lg\n", 
	   num_downs, start_host_id, end_host_id, up_delay.second(), up_bandwidth);
#endif
  }
  
  RouterProcess* p = new RouterProcess(this);
  p->waitsOn(ic);
  stat_nsent = stat_nrcvd = stat_nlost = 0;
}

Router::~Router()
{
  for(int i=0; i<nroutes; i++) {
    assert(routings[i]);
    delete routings[i];
  }
  delete[] routings;
  delete[] oc; // delete the array, not outchannel themselves
}


void Router::wrapup()
{ 
#ifdef NETSIM_DEBUG
  printf("%d\t%d\t%d\t%d\n", router_id, stat_nsent, stat_nrcvd, stat_nlost);
#endif
}

void Router::router(Process* p)
{
  for(;;) {
    // wait for the arrival of an incoming packet
    p->waitOn();
    assert(ic == p->activeChannel());
    IPPacket* pkt = (IPPacket*)ic->activeEvent();
    
    // do a linear scan to find the route to forward the packet onward
    for(int i=0; i<nroutes; i++) {
      if(routings[i]->route(pkt)) {
	stat_nrcvd++;
	return;
      }
    } assert(0);
  }
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
