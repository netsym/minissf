#include <math.h>
#include <string.h>

#include "host.h"
#include "tcpsess.h"
#include "ippacket.h"

class GenProcess : public Process {
public:
  GenProcess(Entity* ent) : Process(ent) {}
  virtual void action() {
    ((Host*)owner())->packet_generator(this);
  }
};
class RcvProcess : public Process {
public:
  RcvProcess(Entity* ent) : Process(ent) {}
  virtual void action() {
    ((Host*)owner())->packet_receiver(this);
  }
};

Host::Host(int id, int start, int end, VirtualTime d, double b) :
  host_id(id), start_host_id(start), end_host_id(end), delay(d), bandwidth(b),
  rng(id*17), next_session_id(0), flush_time(0)
{
  assert(start_host_id <= id && id <= end_host_id);
#ifdef NETSIM_DEBUG
  printf("create host[%d,%d-%d,%.09lf,%lg]\n", host_id,
	 start_host_id, end_host_id, delay.second(), bandwidth);
#endif

  char icname[128];
  sprintf(icname, "IN_%d", id);
  ic = new inChannel(this, icname); assert(ic);
  oc = new outChannel(this, delay); assert(oc);

  Process* p;
  p = new GenProcess(this);
  p = new RcvProcess(this);
  p->waitsOn(ic);

  memset(sessions, 0, MAX_TCP_SESSIONS*sizeof(TCPSession*));

  send_sess_started = send_sess_ended = send_sess_aborted = 0;
  recv_sess_started = recv_sess_ended = recv_sess_aborted = 0;
  pkt_rcvd = pkt_sent = 0;
}

Host::~Host()
{
  for(int i=0; i<MAX_TCP_SESSIONS; i++)
    if(sessions[i]) delete sessions[i];
}

void Host::wrapup()
{
#ifdef NETSIM_DEBUG
  printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", host_id, 
	 send_sess_started, send_sess_ended, send_sess_aborted,
	 recv_sess_started, recv_sess_ended, recv_sess_aborted,
	 pkt_rcvd, pkt_sent);
#endif
}

void Host::packet_generator(Process* p)
{
  for(;;) {
    // wait for the exponentially distributed inter-arrival time
    VirtualTime t = rng.exponential(1.0/HOST_MEAN_OFFTIME);
    p->waitFor(t);

    // find an empty session slot, if there is
    for(int i=0; i<MAX_TCP_SESSIONS; i++) {
      if(!sessions[i]) {
	int sess_id = next_session_id++;

	// randomly select the target host id; we prefer closer hosts
	// than more distant hosts
#define LAMBDA 0.5
#define FX(x) (1.0-exp(-LAMBDA*x))
#define TOTAL_HOSTS (end_host_id-start_host_id+1)
	int direction = rng.equilikely(1, 2) - 1;
	double radius = -1.0/LAMBDA*log(1.0*FX(TOTAL_HOSTS/2)*rng.uniform());
	int peer_id = host_id+direction*(int)radius;
	if(peer_id > end_host_id) peer_id -= TOTAL_HOSTS;
	if(peer_id < start_host_id) peer_id += TOTAL_HOSTS;
	assert(start_host_id <= peer_id && peer_id <= end_host_id);
#undef TOTAL_HOSTS
#undef FX
#undef LAMBDA

	// the number of packets are drawn from a pareto distribution
	int npkts = int(500*rng.pareto(0.5, 2.0));

	send_sess_started++;
	sessions[i] = new TCPSendSession(this, sess_id, peer_id, npkts);
	assert(sessions[i]);
	break;
      } // else we ignore and wait for another time
    }
  }
}

void Host::packet_receiver(Process* p)
{
  int i;

  for(;;) {
    // wait for a packet to arrive
    p->waitOn();
    assert(ic == p->activeChannel());
    IPPacket* pkt = (IPPacket*)ic->activeEvent();
    assert(pkt && pkt->dest == host_id);
    assert(start_host_id <= pkt->src && pkt->src <= end_host_id);
    
    // we check if this is an ack packet; when seqno is zero, we use
    // 'ack' to inform the receiver how many packets can be expected
    // for the new session
    if(pkt->ackno && pkt->seqno > 0) {
      // find the sending session, if it's still there; note we could
      // use map here rather than list to speed things up!
      for(i=0; i<MAX_TCP_SESSIONS; i++) {
	if(sessions[i] && !sessions[i]->is_receiving &&
	   sessions[i]->session_id == pkt->sess &&
	   sessions[i]->peer_id == pkt->src) {
	  ((TCPSendSession*)sessions[i])->acknowledge(pkt->seqno);
	  break;
	}
      }  // if no such session, the ack packet is ignored
    } else { // if this packet is not an ack packet
      // find the corresponding receiving session
      for(i=0; i<MAX_TCP_SESSIONS; i++) {
	if(sessions[i] && sessions[i]->is_receiving &&
	   sessions[i]->session_id == pkt->sess &&
	   sessions[i]->peer_id == pkt->src) {
	  ((TCPRecvSession*)sessions[i])->packet_arrive(pkt->seqno);
	  break;
	}
      }

      if(i == MAX_TCP_SESSIONS) { // if the receiving session is not found
	if(pkt->seqno == 0) {
	  // it's a new session, start a receiving session if there's room
	  for(i=0; i<MAX_TCP_SESSIONS; i++) {
	    if(!sessions[i]) { 
	      // if there is available slot, we create a new receiving
	      // session, pkt->ackno stores the number of packets to be
	      // transmitted by this sending tcp session
	      recv_sess_started++;
	      sessions[i] = new TCPRecvSession(this, pkt->sess, pkt->src, pkt->ackno);
	      assert(sessions[i]);
	      break;
	    }
	  }
	  // ignore if no room for a new receiving session
	}
	// ignore if the receiving session has died in the middle
      }
    }
    pkt_rcvd++;
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
