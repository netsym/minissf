#ifndef __NETSIM_TCPSESS_H__
#define __NETSIM_TCPSESS_H__

#include "netsim.h"
#include "host.h"

// this is the base class for a tcp session; we designate a tcp
// session either as a sending tcp session or a receiving tcp session
class TCPSession {
public:
  Host* host;        // host machine of the session
  int session_id;    // session id
  int peer_id;       // peer host id
  bool is_receiving; // marked whether this is a receiving session (or a sending session)

  // the constructor
  TCPSession(Host* h, int sess, int peer, bool receiving) : 
    host(h), session_id(sess), peer_id(peer), is_receiving(receiving) {}

  // the destructor; we don't do anything here
  virtual ~TCPSession() {}

  // when a tcp timer expires, this method will be invoked
  virtual void timeout(TCPTimer* tmr) = 0;
};

// a sending tcp session
class TCPSendSession : public TCPSession {
public:
  int pkts_to_send;  // the total number of packets to be sent is determined at the creation
  int last_sent;    // the sequence number of the last packet that has been sent
  int first_unack;  // the sequence number of the first packet not yet acknowledged
  TCPTimer* send_window[TCP_WINDOW_SIZE]; // sender-side sliding window

  // the constructor
  TCPSendSession(Host* h, int sess, int peer, int npkts);

  // the destructor
  virtual ~TCPSendSession();

  // when the tcp timer expires, it means a packet has been sent but
  // not acknowledged for a given period of time
  virtual void timeout(TCPTimer* tmr);

  // the host received an ack packet of this session with the given
  // sequence number; this method is called by the host's
  // packet_receiver() process
  void acknowledge(int seqno);
};

// a receiving tcp session
class TCPRecvSession : public TCPSession {
public:
  int pkts_to_recv; // total number of packets to be received (the sender informs the receiver in the first packet)
  int next_recv; // sequence number of the next packet expected to be received
  int recv_window[TCP_WINDOW_SIZE]; // receiver-side sliding window (1 indicates the packet is acknowledged)
  TCPTimer* abort_timer; // timer to abort the session when it's considered dead

  // the constructor
  TCPRecvSession(Host* host, int session_id, int peer, int npkts);

  // the destructor
  virtual ~TCPRecvSession();

  // when the tcp timer expires, the session has not received any
  // packet for a given time; the session should therefore be aborted
  virtual void timeout(TCPTimer* tmr);

  // called by the host's packet_receiver() process when a data packet
  // of the given sequence number has arrived
  void packet_arrive(int seqno);
};

// a tcp timer is used by both the sender and the receiver
class TCPTimer : public Timer {
public:
  TCPSession* session; // the (sending or receiving) tcp session that uses this timer
  int seqno; // the packet sequence number (used by sending tcp session)
  int rexmit; // total number of retransmissions already (used by sending tcp session)

  // the constructor
  TCPTimer(TCPSession* sess, int seq = 0, int rx = 0) :
    Timer(sess->host), session(sess), seqno(seq), rexmit(rx) {}

  // the destructor; we do nothing here
  virtual ~TCPTimer() {}

  // call the timeout method of the corresponding tcp session
  virtual void callback() { session->timeout(this); }
};

#endif /*__NETSIM_TCPSESS_H__*/

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
