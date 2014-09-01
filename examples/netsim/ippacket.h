#ifndef __NETSIM_IPPACKET_H__
#define __NETSIM_IPPACKET_H__

#include "netsim.h"

// each ip packet is modeled as an ssf event
class IPPacket : public Event {
 public:
  int src;   // source host id
  int dest;  // destination host id
  int sess;  // session id
  int seqno; // sequence number in a session
  int ackno; // acknowledge number (if seqno=0, then it's the total packets for a session)

  // the constructor
  IPPacket(int srcid, int dstid, int sessid, int seq, int ack) :
    src(srcid), dest(dstid), sess(sessid), seqno(seq), ackno(ack) {}

  // the copy constructor
  IPPacket(const IPPacket& pkt) : 
    src(pkt.src), dest(pkt.dest), sess(pkt.sess), 
    seqno(pkt.seqno), ackno(pkt.ackno) {}

  // return a clone of the packet
  virtual Event* clone() { return new IPPacket(*this); }

  // serialize the packet
  virtual int pack(char* buf, int bufsiz) {
    /*
    CompactDataType* dp = new CompactDataType;
    dp->add_int(src);
    dp->add_int(dest);
    dp->add_int(sess);
    dp->add_int(seqno);
    dp->add_int(ackno);
    int ret = dp->pack(buf, bufsiz);
    delete dp;
    return ret;
    */
    int pos = 0;
    CompactDataType::serialize(src, buf, bufsiz, &pos);
    CompactDataType::serialize(dest, buf, bufsiz, &pos);
    CompactDataType::serialize(sess, buf, bufsiz, &pos);
    CompactDataType::serialize(seqno, buf, bufsiz, &pos);
    CompactDataType::serialize(ackno, buf, bufsiz, &pos);
    return pos;
  }

  // the factory method for creating a new packet as deserialized from compact data type
  static Event* createIPPacket(char* buf, int bufsiz) {
    /*
    CompactDataType* dp = new CompactDataType;
    dp->unpack(buf, bufsiz);
    int srcid; dp->get_int(&srcid);
    int dstid; dp->get_int(&dstid);
    int sessid; dp->get_int(&sessid);
    int seq; dp->get_int(&seq);
    int ack; dp->get_int(&ack);
    delete dp;
    return new IPPacket(srcid, dstid, sessid, seq, ack);
    */
    int pos = 0;
    int srcid; CompactDataType::deserialize(srcid, buf, bufsiz, &pos);
    int dstid; CompactDataType::deserialize(dstid, buf, bufsiz, &pos);
    int sessid; CompactDataType::deserialize(sessid, buf, bufsiz, &pos);
    int seq; CompactDataType::deserialize(seq, buf, bufsiz, &pos);
    int ack; CompactDataType::deserialize(ack, buf, bufsiz, &pos);
    return new IPPacket(srcid, dstid, sessid, seq, ack);
  }

  // all ssf events must be declared as such
  SSF_DECLARE_EVENT(IPPacket);
};

#endif /*__NETSIM_IPPACKET_H__*/

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
