#include <ostream>
using namespace std;

#include <string.h>
#include <stdlib.h>

#include "netsim.h"
#include "host.h"
#include "router.h"

#define BLOCK_LOW(id,p,n)  ((id)*(n)/(p))
#define BLOCK_HIGH(id,p,n) (BLOCK_LOW((id)+1,p,n)-1)
#define BLOCK_SIZE(id,p,n) (BLOCK_HIGH(id,p,n)-BLOCK_LOW(id,p,n)+1)
#define BLOCK_OWNER(j,p,n) (((p)*((j)+1)-1)/(n))

void usage(char* program) {
  cerr << "Usage: " << program << " L A N1 D1 B1 .. NL DL BL\n"
       << "where L is the number of levels of the hierarchical network topology,\n"
       << "      A is the alignment factor (each machine is expecting to have\n"
       << "        A*P number of timelines, where P is the number of processors),\n"
       << "      N1..NL are the branching factors at levels 1..L,\n"
       << "      D1..DL are the link delays at levels 1..L,\n"
       << "      B1..BL are the link bandwidths (in bits/s) at levels 1..L.\n";
  ssf_abort(1);
}

int main(int argc, char** argv)
{
  // must initialize ssf at the very beginning
  ssf_init(argc, argv);

  // processing command line arguments
  if(argc < 3) {
    cerr << "ERROR: incorrect number of command-line arguments!\n";
    usage(argv[0]);
  }
  int nlevels = atoi(argv[1]);
  if(nlevels < 2) {
    cerr << "ERROR: network level (" << argv[1] << ") must be no smaller than 2!\n";
    usage(argv[0]);
  }
  if(argc != 3*nlevels+3) {
    cerr << "ERROR: incorrect number of command arguments!\n";
    usage(argv[0]);
  }
  int alignfactor = atoi(argv[2]);
  if(alignfactor < 1) {
    cerr << "ERROR: alignment factor (" << argv[2] << ") must be positive!\n";
    usage(argv[0]);
  }

  int* branches = new int[nlevels];
  VirtualTime* delays = new VirtualTime[nlevels];
  double* bandwidths = new double[nlevels];
  for(int i=0; i<nlevels; i++) {
    branches[i] = atoi(argv[3+i*3]);
    if(branches[i] < 2) {
      cerr << "ERROR: invalid branch factor (" << argv[3+i*3] 
	   << ") at level " << i << "\n";
      usage(argv[0]);
    }
    delays[i] = VirtualTime(argv[3+i*3+1]);
    bandwidths[i] = atof(argv[3+i*3+2]);
  }

  // the hosts/routers are arranged hierarchically
  int totalents = 0, host_start = 0, host_end = 0;
  for(int i=0, x=1; i<nlevels; i++) {
    if(i == nlevels-1) host_start = totalents;
    x *= branches[i];
    totalents += x;
  }
  host_end = totalents-1;

  int nmachs = ssf_num_machines();
  int machid = ssf_machine_index();
  int nprocs = ssf_num_processors();
  //int procid = ssf_processor_index();

  // we maintain the array of all created entities, so that we can get
  // the statistics afterwards; only those assigned to this machine
  // are enlisted
  Entity** ents = new Entity*[totalents]; 
  memset(ents, 0, totalents*sizeof(Entity*));

  char icname[128];
  int offset = 0, oldoffset = 0; // offset is the starting index of each level 
  for(int i=0, x=1; i<nlevels; i++) { // one level at a time
    x *= branches[i]; // x is the total number of hosts/routers at this level
    int low = BLOCK_LOW(machid,nmachs,x);
    int high = BLOCK_HIGH(machid,nmachs,x);
#ifdef NETSIM_DEBUG
    printf("level=%d, x=%d, low=%d, high=%d, offset=%d\n", i, x, low, high, offset);
#endif
    for(int j=low; j<=high; j++) { 
      // low and high are indices of hosts/routers belonging to this machine
      int id = offset+j;

      if(i<nlevels-1) { // if it's not the last level, they are routers
	int grpsiz = (host_end-host_start+1)/x;
	int mstart = host_start+j*grpsiz, mend = host_start+(j+1)*grpsiz-1;
	if(i > 0) { // if this is not top-level router
	  ents[id] = new Router(id, host_start, host_end, 1, delays[i], bandwidths[i],
				mstart, mend, branches[i+1], delays[i+1], bandwidths[i+1]);
	  assert(ents[id]);

	  // create the uplink
	  sprintf(icname, "IN_%d", oldoffset+int(j/x));
	  ((Router*)ents[id])->oc[branches[i+1]]->mapto(icname);
#ifdef NETSIM_DEBUG
	  printf("  up-link[%d]: %s\n", branches[i+1], icname);
#endif
	} else { // if this is a top-level router
	  ents[id] = new Router(id, host_start, host_end, branches[0], delays[0], bandwidths[0],
				host_start+j*grpsiz, host_start+(j+1)*grpsiz-1, 
				branches[1], delays[1], bandwidths[1]);
	  assert(ents[id]);

	  // create the uplink
	  for(int k=0; k<branches[0]-1; k++) {
	    sprintf(icname, "IN_%d", k>=j?k+1:k);
	    ((Router*)ents[id])->oc[branches[1]+k]->mapto(icname);
#ifdef NETSIM_DEBUG
	    printf("  up-link[%d]: %s\n", branches[1]+k, icname);
#endif
	  }
	}

	// create the downlink
	for(int k=0; k<branches[i+1]; k++) {
	  sprintf(icname, "IN_%d", j*branches[i+1]+offset+x+k);
	  ((Router*)ents[id])->oc[k]->mapto(icname);
#ifdef NETSIM_DEBUG
	  printf("  down-link[%d]: %s\n", k, icname);
#endif
	}
      } else {
	// the last level are hosts
	ents[id] = new Host(id, host_start, host_end, delays[i], bandwidths[i]);
	assert(ents[id]);

	// create the uplink to the routers
	sprintf(icname, "IN_%d", oldoffset+int(j/branches[nlevels-1]));
	((Host*)ents[id])->oc->mapto(icname);
#ifdef NETSIM_DEBUG
	printf("  up-link: %s\n", icname);
#endif
      }

      // make proper alignment of the entities
      if(i>0) { // if it's not the first level, simply align to the parent node
	int toid = oldoffset+j/branches[i];
	Entity* ent1 = ents[toid]; assert(ent1);
	ents[id]->alignto(ent1);
#ifdef NETSIM_DEBUG
	printf("host/router id=%d aligned to router id=%d (hierarchically)\n", id, toid);
#endif
      } else if(alignfactor*nprocs <= j-low) {
	// for entities beyond the first A*P entities at a level, we
	// align them to the first A*P entities
	int toid = offset+low+(j-low)%nprocs;
	Entity* ent1 = ents[toid]; assert(ent1);
	ents[id]->alignto(ent1);
#ifdef NETSIM_DEBUG
	printf("host/router id=%d aligned to router id=%d (due to A factor)\n", id, toid);
#endif
      }
    }
    oldoffset = offset; offset += x;
  }

  delete[] branches;
  delete[] delays;
  delete[] bandwidths;

  // run the simulation for the specified amount of time; the function
  // returns only when the simulation has finished
  ssf_start(VirtualTime("100s"));

  // between ssf_start and ssf_finalize, we can print out statistics;
  // here we can assume wrapup methods for all processes and entities
  // are called already by now
  int sums[11]; memset(sums, 0, 11*sizeof(int));
  for(int i=0; i<totalents; i++) {
    if(ents[i]) {
      if(i < host_start) { // if this entity is a router
	sums[0] += ((Router*)ents[i])->stat_nsent;
	sums[1] += ((Router*)ents[i])->stat_nrcvd;
	sums[2] += ((Router*)ents[i])->stat_nlost;
      } else {
	sums[3] += ((Host*)ents[i])->send_sess_started;
	sums[4] += ((Host*)ents[i])->send_sess_ended;
	sums[5] += ((Host*)ents[i])->send_sess_aborted;
	sums[6] += ((Host*)ents[i])->recv_sess_started;
  	sums[7] += ((Host*)ents[i])->recv_sess_ended;
	sums[8] += ((Host*)ents[i])->recv_sess_aborted;
	sums[9] += ((Host*)ents[i])->pkt_sent;
	sums[10] += ((Host*)ents[i])->pkt_rcvd;
      }
    }
  }
  delete[] ents;

#ifdef HAVE_MPI_H
  // one should be able to use mpi too to communicate between simulator instances
  if(ssf_num_machines() > 1) {
    int rsums[11];
    MPI_Reduce(sums, rsums, 11, MPI_INT, MPI_SUM, 0, ssf_machine_communicator());
    memcpy(sums, rsums, 11*sizeof(int));
  }
#endif

  if(!ssf_machine_index()) {
    cout << "simulation results:\n";
    cout << "routers:\n  nsent=" << sums[0] << "\n  nrcvd=" << sums[1] << "\n  nlost=" << sums[2] << "\n";
    cout << "hosts:\n"
      "  send sessions (" << sums[3] << " started, " << sums[4] << " ended, " << sums[5] << " aborted)\n"
      "  recv sessions (" << sums[6] << " started, " << sums[7] << " ended, " << sums[8] << " aborted)\n"
      "  packets (" << sums[9] << " sent, " << sums[10] << " received)\n";
  }

  // don't forget to wrap up the simulator (noting that there
  // shouldn't be any more work after this call)
  ssf_finalize();
  return 0;
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
