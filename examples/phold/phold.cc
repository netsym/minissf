#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <vector>

#include "ssf.h"
using namespace minissf;

#define BLOCK_LOW(id,p,n)  ((id)*(n)/(p))
#define BLOCK_HIGH(id,p,n) (BLOCK_LOW((id)+1,p,n)-1)
#define BLOCK_SIZE(id,p,n) (BLOCK_HIGH(id,p,n)-BLOCK_LOW(id,p,n)+1)
#define BLOCK_OWNER(j,p,n) (((p)*((j)+1)-1)/(n))

//#define DEBUG_INFO_STATIC
//#define DEBUG_INFO
#define DEBUG_STATS

class Queue : public Entity {
public:
  int id; // a unique identifier
  int branching_factor; // number of outgoing channels
  LehmerRandom rng; // a random stream
  Semaphore sem; // for synchronizing arrival and service processes
  int num_in_buffer; // number of jobs in queue
  VirtualTime mean_service_time; // exponentially distributed service time
  long stats_serviced; // total number of jobs serviced
  inChannel* ic; // the port of arrival
  outChannel** ocs; // ports of departure (number equal branching factor)

  Queue(int id, int branching_factor, VirtualTime mean_service_time, int init_jobs = 0);
  ~Queue(); // destructor

  void arrival(Process* p); //! SSF PROCEDURE
  void service(Process* p); //! SSF PROCEDURE
};

class ArriveProcess : public Process {
public:
  ArriveProcess(Queue* owner) : Process(owner) {}
  virtual void action(); //! SSF PROCEDURE
};

class ServeProcess : public Process {
public:
  ServeProcess(Queue* owner) : Process(owner) {}
  virtual void action(); //! SSF PROCEDURE
};

//! SSF PROCEDURE
void ArriveProcess::action() 
{
  //! SSF CALL
  ((Queue*)owner())->arrival(this);
}

//! SSF PROCEDURE
void ServeProcess::action() 
{
  //! SSF CALL
  ((Queue*)owner())->service(this);
}

Queue::Queue(int i, int b, VirtualTime mst, int j) :
  id(i), // identifier
  branching_factor(b), // branching factor
  rng(12345+i), // init with random seed
  sem(this, j>0?1:0), // depends on whether we start with jobs in queue
  //num_in_buffer(j), // number of jobs at the start
  mean_service_time(mst), // mean service time
  stats_serviced(0) // number of job serviced
{
  num_in_buffer = (int)rng.poisson((double)j);
#ifdef DEBUG_INFO_STATIC
  printf("%d: queue[%d] created with b=%d, mst=%g, j=%d(%d)\n", 
	 ssf_machine_index(), i, b, mst.second(), j, num_in_buffer);
#endif

  char icname[10]; sprintf(icname, "%d", id);
  ic = new inChannel(this, icname); assert(ic);

  assert(branching_factor>=0);
  if(branching_factor > 0) {
    ocs = new outChannel*[branching_factor]; assert(ocs);
    for(int k=0; k<branching_factor; k++) {
      ocs[k] = new outChannel(this); assert(ocs[k]);
    }
  } else ocs = 0;

  Process* p = new ArriveProcess(this);
  p->waitsOn(ic);
  new ServeProcess(this);
}

Queue::~Queue()
{
  if(ocs) delete[] ocs;
}

//! SSF PROCEDURE
void Queue::arrival(Process* p)
{
#ifdef DEBUG_INFO_STATIC
  printf("%d:%d: queue[%d]: start arrival()\n", 
	 ssf_machine_index(), ssf_processor_index(), id);
#endif

  for(;;) {
    p->waitOn(); // wait on the default input channel
#ifdef DEBUG_INFO
    printf("%d:%d: %.09lf: queue[%d] receives a job\n", 
	   ssf_machine_index(), ssf_processor_index(), now().second(), id);
#endif
    /*
    Event* job = ic->activeEvent();
    delete job;
    */

    num_in_buffer++;
    if(num_in_buffer == 1) sem.signal();
  }
}

//! SSF PROCEDURE
void Queue::service(Process* p)
{
  VirtualTime t; //! SSF STATE
  int k; //! SSF STATE

#ifdef DEBUG_INFO_STATIC
  printf("%d:%d: queue[%d]: start service()\n",
	 ssf_machine_index(), ssf_processor_index(), id);
#endif

  for(;;) {
    sem.wait();
    while(num_in_buffer > 0) {
      t = rng.exponential(1.0/mean_service_time);
#ifdef DEBUG_INFO
      printf("%d:%d: %.09lf: queue[%d] services a job (%d in system) for %.09lf (until %.09lf)\n", 
	     ssf_machine_index(), ssf_processor_index(), now().second(), 
	     id, num_in_buffer, t.second(), (now()+t).second());
#endif
      p->waitFor(t);

      num_in_buffer--;
      if(branching_factor > 0) {
	assert(ocs);
	Event* evt = new Event();
	k = branching_factor > 1 ? rng.equilikely(0, branching_factor-1) : 0;
#ifdef DEBUG_INFO
      printf("%d:%d: %.09lf: queue[%d] sends a job via ocs[%d]\n", 
	     ssf_machine_index(), ssf_processor_index(), now().second(), id, k);
#endif
	ocs[k]->write(evt);
	stats_serviced++;
      }
    }
  }
}

void usage(char* program)
{
  if(!ssf_machine_index()) {
    fprintf(stderr, "Usage: %s t n r b d m s\n", program); 
    fprintf(stderr, " t: simulation time\n");
    fprintf(stderr, " n: total number of nodes\n");
    fprintf(stderr, " r: radius (a node may connect to r nodes before it and r nodes after it)\n");
    fprintf(stderr, " b: branching factor (the number of neighbors of a node within radius)\n");
    fprintf(stderr, " d: channel delay\n");
    fprintf(stderr, " m: mean service time of an exponential distribution\n");
    fprintf(stderr, " j: mean number of init jobs (poisson distributed)\n");
    ssf_print_options(stderr);
  }
  ssf_abort(1);
}

#define CHECKPARAM(x,y) if(!(x)) { if(!id) fprintf(stderr, y "\n"); ssf_abort(1); }

int main(int argc, char* argv[])
{
  ssf_init(argc, argv);

  int p = ssf_num_machines();
  int id = ssf_machine_index();
  int pp = ssf_num_processors();

  if(argc != 8) usage(argv[0]);

  VirtualTime t(argv[1]); 
  CHECKPARAM(t > 0, "runtime t should be positive");

  int n = atoi(argv[2]); 
  CHECKPARAM(n >= p, "number of queues n must be no less than machines");

  int r = atoi(argv[3]); 
  CHECKPARAM(r >= 0, "radius r should be non-negative");
  if(r == 0 || r > n/2) r = n/2;
  int maxb = 2*r; if(maxb > n-1) maxb = n-1;

  int b = atoi(argv[4]); 
  CHECKPARAM(b <= maxb, "invalid radius r or branching factor b");
  if(b <= 2) b = 2; // must have at least two connections

  VirtualTime d(argv[5]);
  CHECKPARAM(d > 0, "channel delay d must be positive");

  VirtualTime m(argv[6]);
  CHECKPARAM(m > 0, "mean service time m should be positive");

  int s = atoi(argv[7]);
  CHECKPARAM(s >= 0, "mean number of init jobs s should be non-negative");

  if(!id) {
    printf("*************************************************************************\n");
    printf("t=%.09lf: simulation time\n", t.second());
    printf("n=%d: total number of nodes\n", n);
    printf("r=%d: radius (a node may connect to r nodes before it and r nodes after it)\n", r);
    printf("b=%d: branching factor (the number of neighbors of a node within radius)\n", b);
    printf("d=%.09lf: channel delay\n", d.second());
    printf("m=%.09lf: mean service time of an exponential distribution\n", m.second());
    printf("j=%d: mean number of init jobs (poisson distributed)\n", s);
    printf("*************************************************************************\n");
  }

#ifdef DEBUG_STATS
  struct timeval t1, t2, t3;
  if(!id) gettimeofday(&t1, 0);
#endif

  //LehmerRandom rng(54321);
  std::vector<Queue*> qset;
  int* nb = new int[b]; assert(nb);
  Queue** alignments = new Queue*[pp]; assert(alignments);
  memset(alignments, 0, pp*sizeof(Queue*));
  int i, j;
  int nq = BLOCK_SIZE(id,p,n);
  for(int qid=BLOCK_LOW(id,p,n); qid<=BLOCK_HIGH(id,p,n); qid++) {
    Queue* q = new Queue(qid, b, m, s); assert(q);
    qset.push_back(q);

    int idx = BLOCK_OWNER(qid-BLOCK_LOW(id,p,n),pp,nq);
    if(alignments[idx]) q->alignto(alignments[idx]);
    else alignments[idx] = q;

    for(i=0; i<b; i++) {
      int x;
      if(i == 0) x = r-1; else if(i == 1) x = r; // connect to adjacent queues
      else x = q->rng.equilikely(0, maxb-1-i);
      for(j=0; j<i; j++) {
	if(x >= nb[j]) x++; 
	else {
	  for(int k=i-1; k>=j; k--) nb[k+1] = nb[k];
	  nb[j] = x;
	  break;
	}
      }
      if(j == i) nb[i] = x;
    }
    for(i=0; i<b; i++) {
      int x = qid-r+nb[i]; if(nb[i] >= r) x++;
      if(x < 0) x += n;
      if(x >= n) x -= n;
      char icname[10]; sprintf(icname, "%d", x);
      q->ocs[i]->mapto(icname, d);
#ifdef DEBUG_INFO_STATIC
      printf("%d: queue[%d].ocs[%d] mapped to queue[%d]\n", id, qid, i, x);
#endif
    }
  }
  delete[] nb;
  delete[] alignments;

#ifdef DEBUG_STATS
  if(!id) gettimeofday(&t2, 0);
#endif

  ssf_start(t);

#ifdef DEBUG_STATS
  if(!id) gettimeofday(&t3, 0);
#endif

#ifdef DEBUG_STATS
  long sum = 0;
  for(std::vector<Queue*>::iterator iter = qset.begin();
      iter != qset.end(); iter++) 
    sum += (*iter)->stats_serviced;
#ifdef HAVE_MPI_H
  if(p > 1) {
    long allsum;
    MPI_Reduce(&sum, &allsum, 1, MPI_LONG, MPI_SUM, 0, ssf_machine_communicator());
    sum = allsum;
  }
#endif
  if(!id) {
    double setup_time = ((t2.tv_sec*1e6+t2.tv_usec)-(t1.tv_sec*1e6+t1.tv_usec))/1e6;
    printf("setup time: %g seconds (wall-clock time)\n", setup_time);
    double exec_time  =((t3.tv_sec*1e6+t3.tv_usec)-(t2.tv_sec*1e6+t2.tv_usec))/1e6;
    printf("execution time: %g seconds (wall-clock time)\n", exec_time);
    printf("total #jobs serviced: %ld\n", sum);
    printf("event density (#jobs serviced per simulated second): %g\n", sum/t.second());
    printf("processing rate (#jobs serviced per wall-clock second): %g\n", sum/exec_time);
  }
#endif

  ssf_finalize();
  return 0;
}
