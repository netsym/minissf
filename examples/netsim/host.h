#ifndef __NETSIM_HOST_H__
#define __NETSIM_HOST_H__

#include "netsim.h"

// a host is an ssf entity with one inchannel for receiving packets
// and one outchannel for sending packets; each host maintains a
// number of tcp sessions (both for sending and receiving data)
class Host : public Entity {
public:
  // the constructor; all hosts are numbered consecutively within a
  // given range (start_host_id to end_host_id); the delay and
  // bandwidth are for the link that connects this host with the
  // adjacent router
  Host(int host_id, int start_host_id, int end_host_id, 
       VirtualTime delay, double bandwidth);

  // the destructor
  virtual ~Host();

 // called when the simulation is finished
  virtual void wrapup();

  // this is the root procedure for the process that generates tcp
  // sessions; the tcp sessions are created with an exponentially
  // distributed inter-arrival time
  void packet_generator(Process*) __attribute__ ((annotate("ssf_starting_procedure")));

   // this is the root procedure for the process that handles incoming
   // packets (which could be either data or ack packets)
  void packet_receiver(Process*) __attribute__ ((annotate("ssf_starting_procedure")));

public:
  int host_id; // we use host/router id as the IP address
  int start_host_id; // the starting id of all hosts
  int end_host_id; // the ending id of all hosts
  VirtualTime delay; // to the routers at the same or previous level
  double bandwidth; // to the routers at the same or previous level (in bits/second)
  LehmerRandom rng; // random stream specific to this host
  inChannel* ic;  // inchannel to receive incoming packets
  outChannel* oc; // outchannel for sending outgoing packets
  int next_session_id; // id of next session initiated by this host
  VirtualTime flush_time; // it's the time when all messages in queue are sent
  TCPSession* sessions[MAX_TCP_SESSIONS]; // list of active tcp sessions

  // statistics collected during runtime
  int send_sess_started;
  int send_sess_ended;
  int send_sess_aborted;
  int recv_sess_started;
  int recv_sess_ended;
  int recv_sess_aborted;
  int pkt_sent;
  int pkt_rcvd;
};

#endif /*__NETSIM_HOST_H__*/

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
