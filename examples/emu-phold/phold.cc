#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <algorithm>
#include <vector>

#define EMU 1
#define TPC 10 // timelines per core

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

  Queue(int id, int branching_factor, VirtualTime mean_service_time, int init_jobs);
  Queue(int id, int branching_factor, VirtualTime mean_service_time, int init_jobs, VirtualTime resp);
  void common_constructor();
  ~Queue(); // destructor

  void arrival(Process* p); //! SSF PROCEDURE
  void service(Process* p); //! SSF PROCEDURE

  void calc_timeliness();
  virtual void emulate(Event* evt);
};

#define MAXVECSIZ 400000
std::vector<int64> sample_timeliness;
std::vector<int64> sample_responsiveness;
int emuidx = 0;
int respidx = 0;
Queue* emu_queue = 0;

class MyEmuEvent : public Event {
public:
  int64 gentime;
  MyEmuEvent(int64 u) : gentime(u) {}
  MyEmuEvent(const MyEmuEvent& e) : Event(e), gentime(e.gentime) {}
  virtual ~MyEmuEvent() {}
  virtual Event* clone() { return new MyEmuEvent(*this); }
  virtual int pack(char* buf, int bufsiz) { int pos = 0; CompactDataType::serialize(gentime, buf, bufsiz, &pos); return pos; }
  static Event* create_emu_event(char* buf, int bufsiz) { int pos = 0; int64 u; CompactDataType::deserialize(u, buf, bufsiz, &pos); return new MyEmuEvent(u); }
  SSF_DECLARE_EVENT(MyEmuEvent);
};
SSF_REGISTER_EVENT(MyEmuEvent, MyEmuEvent::create_emu_event);

void* genevt(void* args)
{
  //usleep(1000000);
  for(;;) {
    usleep(1000); // 1 ms?
    MyEmuEvent* e = new MyEmuEvent(ssf_wallclock_in_nanoseconds());
    emu_queue->insertEmulatedEvent((Event*&)e);
  }
  return 0;
}

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
  common_constructor();
}

Queue::Queue(int i, int b, VirtualTime mst, int j, VirtualTime resp) :
  Entity(true, resp), id(i), // identifier
  branching_factor(b), // branching factor
  rng(12345+i), // init with random seed
  sem(this, j>0?1:0), // depends on whether we start with jobs in queue
  //num_in_buffer(j), // number of jobs at the start
  mean_service_time(mst), // mean service time
  stats_serviced(0) // number of job serviced
{
  num_in_buffer = (int)rng.poisson((double)j);
  common_constructor();
}

void Queue::common_constructor()
{
#ifdef DEBUG_INFO_STATIC
  printf("%d: queue[%d] created with b=%d, mst=%g, j=%d\n", 
	 ssf_machine_index(), id, branching_factor, mean_service_time.second(), num_in_buffer);
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

void Queue::calc_timeliness()
{
  // IMPORTANT: assuming there's only one emulated timeline on each machine!!!
  if(isEmulated()) {
    int64 x = (realNow()-now()).get_ticks();
    if(emuidx >= MAXVECSIZ) sample_timeliness[emuidx%MAXVECSIZ] = x;
    else sample_timeliness.push_back(x);
    emuidx++;
  }
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
    calc_timeliness();
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
    calc_timeliness();
    while(num_in_buffer > 0) {
      t = rng.exponential(1.0/mean_service_time);
#ifdef DEBUG_INFO
      printf("%d:%d: %.09lf: queue[%d] services a job (%d in system) for %.09lf (until %.09lf)\n", 
	     ssf_machine_index(), ssf_processor_index(), now().second(), 
	     id, num_in_buffer, t.second(), (now()+t).second());
#endif
      p->waitFor(t);
      calc_timeliness();

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

void Queue::emulate(Event* e)
{
  assert(e);
  int64 u = ssf_wallclock_in_nanoseconds();
  u -= ((MyEmuEvent*)e)->gentime;
  delete e;
  
  if(respidx >= MAXVECSIZ) sample_responsiveness[respidx%MAXVECSIZ] = u;
  else sample_responsiveness.push_back(u);
  respidx++;
}

void usage(char* program)
{
  if(!ssf_machine_index()) {
    fprintf(stderr, "Usage: %s t n r b d m s [resp]\n", program); 
    fprintf(stderr, " t: simulation time\n");
    fprintf(stderr, " n: total number of nodes\n");
    fprintf(stderr, " r: radius (a node may connect to r nodes before it and r nodes after it)\n");
    fprintf(stderr, " b: branching factor (the number of neighbors of a node within radius)\n");
    fprintf(stderr, " d: channel delay\n");
    fprintf(stderr, " m: mean service time of an exponential distribution\n");
    fprintf(stderr, " j: mean number of init jobs (poisson distributed)\n");
    fprintf(stderr, " resp: time interval for demand sample_responsiveness (optional)\n");
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
  int pp = ssf_num_processors()*TPC;

  if(argc != 8 && argc != 9) usage(argv[0]);

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

  VirtualTime resp = VirtualTime::INFINITY;
  if(argc == 9) resp = VirtualTime(argv[8]);

  if(!id) {
    //printf("#*************************************************************************\n");
    printf("#t=%.09lf: simulation time\n", t.second());
    printf("#n=%d: total number of nodes\n", n);
    printf("#r=%d: radius (a node may connect to r nodes before it and r nodes after it)\n", r);
    printf("#b=%d: branching factor (the number of neighbors of a node within radius)\n", b);
    printf("#d=%.09lf: channel delay\n", d.second());
    printf("#m=%.09lf: mean service time of an exponential distribution\n", m.second());
    printf("#j=%d: mean number of init jobs (poisson distributed)\n", s);
    printf("#resp=%.09lf: demand sample_responsiveness\n", resp.second());
    //printf("#*************************************************************************\n");
  }

  sample_timeliness.reserve(360000);
  sample_responsiveness.reserve(100000);

#ifdef DEBUG_STATS
  int64 t1, t2, t3;
  if(!id) t1 = ssf_wallclock_in_nanoseconds();
#endif

  //LehmerRandom rng(54321);
  std::vector<Queue*> qset;
  int* nb = new int[b]; assert(nb);
  Queue** alignments = new Queue*[pp]; assert(alignments);
  memset(alignments, 0, pp*sizeof(Queue*));
  int i, j;
  int nq = BLOCK_SIZE(id,p,n);
  for(int qid=BLOCK_LOW(id,p,n); qid<=BLOCK_HIGH(id,p,n); qid++) {
    Queue* q;
#if EMU
    if(!qid) emu_queue = q = new Queue(qid, b, m, s, resp); // only one emulated
    else q = new Queue(qid, b, m, s); 
#else
    q = new Queue(qid, b, m, s);
#endif
    assert(q);
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
  if(!id) t2 = ssf_wallclock_in_nanoseconds();
#endif

  pthread_t tid;
  if(emu_queue) pthread_create(&tid, NULL, &genevt, NULL);
#if EMU
  ssf_start(t, 1.0);
#else
  ssf_start(t);
#endif
  if(emu_queue) pthread_cancel(tid);

#ifdef DEBUG_STATS
  if(!id) t3 = ssf_wallclock_in_nanoseconds();
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
    double setup_time = (t2-t1)*1e-9;
    printf("#setup time: %g seconds (wall-clock time)\n", setup_time);
    double exec_time  = (t3-t2)*1e-9;
    printf("#execution time: %g seconds (wall-clock time)\n", exec_time);
    printf("#total num of jobs serviced: %ld\n", sum);
    printf("#event density (num of jobs serviced per simulated second): %g\n", sum/t.second());
    printf("#processing rate (num of jobs serviced per wall-clock second): %g\n", sum/exec_time);

    int sz = sample_timeliness.size();
    if(sz > 0) {
      char fname[100];
      sprintf(fname, "timeliness-mst%g-rsp%g.out", m.second(), resp.second());
      FILE* fp = fopen(fname, "w");
      int s = sz/2;
      int t = s+10000; if(t > sz) t = sz;
      for(int i=s; i<t; i++) fprintf(fp, "%d %lld\n", i, sample_timeliness[i]);
      fclose(fp);

      std::sort(sample_timeliness.begin(), sample_timeliness.end());
      printf("MST=%g E=%d %lld %lld %lld", m.second(), emuidx,
	     sample_timeliness[sz*.25], sample_timeliness[sz*.50], sample_timeliness[sz*.75]);
    } else printf("MST=%g E=0 0 0 0", m.second());

    sz = sample_responsiveness.size();
    if(sz > 0) {
      char fname[100];
      sprintf(fname, "responsiveness-mst%g-rsp%g.out", m.second(), resp.second());
      FILE* fp = fopen(fname, "w");
      int s = sz/2;
      int t = s+10000; if(t > sz) t = sz;
      for(int i=s; i<t; i++) fprintf(fp, "%d %lld\n", i, sample_responsiveness[i]);
      fclose(fp);

      std::sort(sample_responsiveness.begin(), sample_responsiveness.end());
      printf(" E=%d %lld %lld %lld\n", respidx, sample_responsiveness[sz*.25], 
	     sample_responsiveness[sz*.50], sample_responsiveness[sz*.75]);
    } else printf(" E=0 0 0 0\n");
  }
#endif

  ssf_finalize();
  return 0;
}
