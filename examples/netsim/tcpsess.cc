#include <string.h>

#include "tcpsess.h"
#include "ippacket.h"

// this macro is required for each (derived) ssf event
SSF_REGISTER_EVENT(IPPacket, IPPacket::createIPPacket);

TCPSendSession::TCPSendSession(Host* h, int s, int p, int npkts) :
  TCPSession(h, s, p, false), pkts_to_send(npkts), last_sent(-1), first_unack(0)
{
#ifdef NETSIM_DEBUG
  printf("%.09lf: host %d creates send session (sess=%d) to %d (%d packets)\n", 
	 host->now().second(), host->host_id, session_id, peer_id, pkts_to_send);
#endif

  memset(send_window, 0, TCP_WINDOW_SIZE*sizeof(TCPTimer*));

  // send the first window-ful of packets
  while(last_sent < pkts_to_send-1 &&
	last_sent-first_unack < TCP_WINDOW_SIZE-1) {
    last_sent++;
    if(host->flush_time < host->now()) host->flush_time = host->now();
    host->pkt_sent++;
    // if it's the first packet we use the ack field to indicate the
    // total number of packets to be sent to the receiver
    IPPacket* ipkt = new IPPacket(host->host_id, peer_id, session_id, last_sent, last_sent?0:pkts_to_send);
    host->oc->write((Event*&)ipkt, host->flush_time-host->now());
#ifdef NETSIM_DEBUG
    printf("%.09lf: host %d (sess=%d) send DAT[%d] to %d (until %.09lf)\n",
	   host->now().second(), host->host_id, session_id, last_sent, 
	   peer_id, host->flush_time.second());
#endif
    send_window[last_sent] = new TCPTimer(this, last_sent);
    assert(send_window[last_sent]);
    send_window[last_sent]->schedule(host->flush_time-host->now()+TCP_SEND_TIMEOUT);
    host->flush_time += VirtualTime(TCP_PKTSIZ_IN_BITS/host->bandwidth, VirtualTime::SECOND);
  }
}

TCPSendSession::~TCPSendSession()
{
  // cancel and delete all the timers upon exit
  for(int i=0; i<TCP_WINDOW_SIZE; i++) {
    if(send_window[i]) {
      send_window[i]->cancel();
      delete send_window[i];
      send_window[i] = 0;
    }
  }
}

void TCPSendSession::timeout(TCPTimer* tmr)
{
  assert(tmr && this == tmr->session);
  assert(first_unack <= tmr->seqno && tmr->seqno <= last_sent);
  assert(tmr == send_window[tmr->seqno%TCP_WINDOW_SIZE]);

  if(tmr->rexmit < TCP_MAX_REXMIT_TIMES) {
    // if the number of retransmissions is still below the threshold,
    // we schedule a new timer and retransmit the packet
    tmr = send_window[tmr->seqno%TCP_WINDOW_SIZE] = new TCPTimer(this, tmr->seqno, tmr->rexmit+1);
    if(host->flush_time < host->now()) host->flush_time = host->now();
    host->pkt_sent++;
    IPPacket* ipkt = new IPPacket(host->host_id, peer_id, session_id, tmr->seqno, tmr->seqno?0:pkts_to_send);
    host->oc->write((Event*&)ipkt, host->flush_time-host->now());
#ifdef NETSIM_DEBUG
    printf("%.09lf: host %d (sess=%d) resend DAT[%d] (rexmit=%d) to %d (until %.09lf)\n", 
	   host->now().second(), host->host_id, session_id, tmr->seqno, 
	   tmr->rexmit, peer_id, host->flush_time.second());
#endif
    VirtualTime t = TCP_SEND_TIMEOUT; for(int k=0; k<tmr->rexmit; k++) t *= 2;
    tmr->schedule(host->flush_time-host->now()+t);
    host->flush_time += VirtualTime(TCP_PKTSIZ_IN_BITS/host->bandwidth, VirtualTime::SECOND);
  } else { // max retransimissions reached, we abort the session
#ifdef NETSIM_DEBUG
    printf("%.09lf: host %d (sess=%d) send session aborts\n", 
	   host->now().second(), host->host_id, session_id);
#endif
    host->send_sess_aborted++;
    for(int i=0; i<MAX_TCP_SESSIONS; i++) {
      if(host->sessions[i] == this) {
	host->sessions[i] = 0;
	delete this;
	return;
      }
    } assert(0);
  }
}

void TCPSendSession::acknowledge(int seqno)
{
#ifdef NETSIM_DEBUG
  printf("%.09lf: host %d (sess=%d) got ACK[%d] from %d\n", 
	 host->now().second(), host->host_id, session_id, seqno, peer_id);
#endif

  // ignore old message
  if(seqno <= first_unack) return;

  // clean up those before the arriving message
  for(int i=first_unack; i<seqno; i++) {
    if(send_window[i%TCP_WINDOW_SIZE]) {
      send_window[i%TCP_WINDOW_SIZE]->cancel();
      delete send_window[i%TCP_WINDOW_SIZE];
      send_window[i%TCP_WINDOW_SIZE] = 0;
    }
  }
  first_unack = seqno;

  // if we get the last ack, the session ends successfully
  if(first_unack == pkts_to_send) {
#ifdef NETSIM_DEBUG
    printf("%.09lf: host %d (sess=%d) send session completes\n", 
	   host->now().second(), host->host_id, session_id);
#endif
    host->send_sess_ended++;
    for(int i=0; i<MAX_TCP_SESSIONS; i++) {
      if(host->sessions[i] == this) {
	host->sessions[i] = 0;
	delete this;
	return;
      }
    } assert(0);
  }

  // if we have more to send, send the next window-ful
  while(last_sent < pkts_to_send-1 &&
	last_sent-first_unack < TCP_WINDOW_SIZE-1) {
    last_sent++;
    if(host->flush_time < host->now()) host->flush_time = host->now();
    host->pkt_sent++;
    IPPacket* ipkt = new IPPacket(host->host_id, peer_id, session_id, last_sent, 0);
    host->oc->write((Event*&)ipkt, host->flush_time-host->now());
#ifdef NETSIM_DEBUG
    printf("%.09lf: host %d (sess=%d) send DAT[%d] to %d (until %.09lf)\n", 
	   host->now().second(), host->host_id, session_id, last_sent,
	   peer_id, host->flush_time.second());
#endif
    assert(send_window[last_sent%TCP_WINDOW_SIZE] == 0);
    TCPTimer* tmr = send_window[last_sent%TCP_WINDOW_SIZE] = 
      new TCPTimer(this, last_sent); assert(tmr);
    tmr->schedule(host->flush_time-host->now()+TCP_SEND_TIMEOUT);
    host->flush_time += VirtualTime(TCP_PKTSIZ_IN_BITS/host->bandwidth, VirtualTime::SECOND);
  }
}

TCPRecvSession::TCPRecvSession(Host* h, int s, int p, int npkts) :
  TCPSession(h, s, p, true), pkts_to_recv(npkts), next_recv(1)
{
#ifdef NETSIM_DEBUG
  printf("%.09lf: host %d starts receive session (sess=%d) with %d (%d packets)\n", 
	 host->now().second(), h->host_id, session_id, peer_id, pkts_to_recv);
#endif

  memset(recv_window, 0, TCP_WINDOW_SIZE*sizeof(int));

  // send the ack back for the first packet
  if(host->flush_time < host->now()) host->flush_time = host->now();
  host->pkt_sent++;
  IPPacket* ipkt = new IPPacket(host->host_id, peer_id, session_id, next_recv, 1);
  host->oc->write((Event*&)ipkt, host->flush_time-host->now());
#ifdef NETSIM_DEBUG
  printf("%.09lf: host %d (sess=%d) send ACK[%d] to %d\n", 
	 host->now().second(), host->host_id, session_id, next_recv, peer_id);
#endif

  // create and schedule the timer for aborting the receiving session
  abort_timer = new TCPTimer(this, 0); assert(abort_timer);
  abort_timer->schedule(host->flush_time-host->now()+TCP_RECV_TIMEOUT);
  host->flush_time += VirtualTime(TCP_ACKSIZ_IN_BITS/host->bandwidth, VirtualTime::SECOND);
}

TCPRecvSession::~TCPRecvSession() 
{
  if(abort_timer) {
    abort_timer->cancel();
    delete abort_timer;
  }
}

void TCPRecvSession::timeout(TCPTimer* tmr)
{
  assert(this == tmr->session);
  assert(abort_timer == tmr);

#ifdef NETSIM_DEBUG
  printf("%.09lf: host %d (sess=%d) receive session aborts\n", 
	 host->now().second(), host->host_id, session_id);
#endif

  abort_timer = 0; // make sure we don't delete the timer twice
  host->recv_sess_aborted++;
  for(int i=0; i<MAX_TCP_SESSIONS; i++) {
    if(host->sessions[i] == this) {
      host->sessions[i] = 0;
      delete this;
      return;
    }
  } assert(0);
}

void TCPRecvSession::packet_arrive(int seqno)
{
#ifdef NETSIM_DEBUG
  printf("%.09lf: host %d (sess=%d) got packet [%d] from %d\n", 
	 host->now().second(), host->host_id, session_id, seqno, peer_id);
#endif

  // cancel and delete the abort timer
  assert(abort_timer);
  abort_timer->cancel();
  abort_timer = 0;

  // mark the packet just arrived (simulate it as such) and dequeue
  // the packets in sequence
  if(next_recv <= seqno) {
    assert(seqno-next_recv < TCP_WINDOW_SIZE);
    recv_window[seqno%TCP_WINDOW_SIZE] = 1;
    while(recv_window[next_recv%TCP_WINDOW_SIZE]) {
      recv_window[next_recv%TCP_WINDOW_SIZE] = 0;
      next_recv++;
    }
  }

  // send the ack message
  host->pkt_sent++;
  if(host->flush_time < host->now()) host->flush_time = host->now();
  IPPacket* ipkt = new IPPacket(host->host_id, peer_id, session_id, next_recv, 1);
  host->oc->write((Event*&)ipkt, host->flush_time-host->now());
#ifdef NETSIM_DEBUG
  printf("%.09lf: host %d (sess=%d) send ACK[%d] to %d\n", 
	 host->now().second(), host->host_id, session_id, next_recv, peer_id);
#endif

  // if there are more packets to be received
  if(next_recv < pkts_to_recv) {
    abort_timer = new TCPTimer(this, 0); assert(abort_timer);
    abort_timer->schedule(host->flush_time-host->now()+TCP_RECV_TIMEOUT);
  }

  host->flush_time += VirtualTime(TCP_ACKSIZ_IN_BITS/host->bandwidth, VirtualTime::SECOND);

  // if receiving session ends successfully, reclaim the session
  if(next_recv == pkts_to_recv) {
#ifdef NETSIM_DEBUG
    printf("%.09lf: host %d (sess=%d) receive session completes\n", 
	   host->now().second(), host->host_id, session_id);
#endif
    host->recv_sess_ended++;
    for(int i=0; i<MAX_TCP_SESSIONS; i++) {
      if(host->sessions[i] == this) {
	host->sessions[i] = 0;
	delete this;
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
