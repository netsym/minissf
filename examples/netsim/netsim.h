#ifndef __NETSIM_H__
#define __NETSIM_H__

#include "ssf.h"
using namespace minissf;

// comment out to disable debugging information
#define NETSIM_DEBUG

class IPPacket;
class Host;
class TCPTimer;
class TCPSession;
class TCPSendSession;
class TCPRecvSession;
class Router;
class RoutingEntry;

#define NIC_BUFSIZ 16 // the size of the buffer (in packets) at each nic
#define MAX_TCP_SESSIONS 10 // maximum number of current tcp sessions
#define HOST_MEAN_OFFTIME VirtualTime(25, VirtualTime::SECOND) // tcp session mean interarrival time
#define TCP_WINDOW_SIZE 16 // tcp window size (it's a constant; we don't have congestion control)
#define TCP_MAX_REXMIT_TIMES 3 // max number of times tcp can retransmit a packet before abort
#define TCP_SEND_TIMEOUT VirtualTime(1.0, VirtualTime::SECOND) // tcp timeout for re-transmission
#define TCP_RECV_TIMEOUT VirtualTime((int(TCP_SEND_TIMEOUT+0.5)<<(TCP_MAX_REXMIT_TIMES+1)), VirtualTime::SECOND)
#define TCP_PKTSIZ_IN_BITS 8e3 // we assume tcp packet is 1 kilo-bytes
#define TCP_ACKSIZ_IN_BITS 320 // we assume tcp packet is 40 bytes

#endif /*__NETSIM_H__*/

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
