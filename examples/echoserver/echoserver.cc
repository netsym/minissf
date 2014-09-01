#include <deque>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "ssf.h"

using namespace std;
using namespace minissf;

#define CHANNEL_DELAY VirtualTime(1, VirtualTime::MILLISECOND)
#define SERVICE_TIME VirtualTime(0, VirtualTime::MICROSECOND)
#define RESPONSIVENESS VirtualTime(100, VirtualTime::MICROSECOND)

class Message : public Event {
public:
  Message() {}
  Message(const Message& msg) : Event(msg) {}
  virtual Event* clone() { return new Message(*this); }
  virtual bool isEmulated() { return false; }
  virtual int pack(char* buf, int bufsiz) { return 0; }
  static Event* create_message(char* buf, int bufsiz) { return new Message(); }
  SSF_DECLARE_EVENT(Message);
};
SSF_REGISTER_EVENT(Message, Message::create_message);

class EmulatedMessage : public Message {
public:
  int newsockfd; // socket for the tcp connection
  string msgstr; // message content
  VirtualTime rt_inject; // the real time when 'iothread' injects the event
  VirtualTime rt_emulate; // the real time when 'emulate' is called
  VirtualTime vt_emulate; // the virtual time when 'emulate' is called
  EmulatedMessage(int sock, string buf) : newsockfd(sock), msgstr(buf) {}
  EmulatedMessage(const EmulatedMessage& msg) : 
    Message(msg), newsockfd(msg.newsockfd), msgstr(msg.msgstr),
    rt_inject(msg.rt_inject), rt_emulate(msg.rt_emulate),
    vt_emulate(msg.vt_emulate) {}
  virtual Event* clone() { return new EmulatedMessage(*this); }
  virtual bool isEmulated() { return true; }
  virtual int pack(char* buf, int bufsiz) {
    int pos = 0;
    CompactDataType::serialize(rt_inject.get_ticks(), buf, bufsiz, &pos);
    CompactDataType::serialize(rt_emulate.get_ticks(), buf, bufsiz, &pos);
    CompactDataType::serialize(vt_emulate.get_ticks(), buf, bufsiz, &pos);
    CompactDataType::serialize(newsockfd, buf, bufsiz, &pos);
    CompactDataType::serialize(msgstr.c_str(), buf, bufsiz, &pos);
    /*
    printf("serialize: %lld %lld %lld %d \"%s\"\n", 
	   rt_inject.get_ticks(), rt_emulate.get_ticks(), 
	   vt_emulate.get_ticks(), newsockfd, msgstr.c_str());
    */
    return pos;
  }
  static Event* create_message(char* buf, int bufsiz) {
    int pos = 0;
    int64 rt_inject;
    int64 rt_emulate;
    int64 vt_emulate;
    int mysock;
    char mybuf[256]; //should be big enough
    CompactDataType::deserialize(rt_inject, buf, bufsiz, &pos);
    CompactDataType::deserialize(rt_emulate, buf, bufsiz, &pos);
    CompactDataType::deserialize(vt_emulate, buf, bufsiz, &pos);
    CompactDataType::deserialize(mysock, buf, bufsiz, &pos);
    CompactDataType::deserialize(mybuf, 256, buf, bufsiz, &pos);
    EmulatedMessage* msg = new EmulatedMessage(mysock, mybuf);
    msg->rt_inject = VirtualTime(rt_inject);
    msg->rt_emulate = VirtualTime(rt_emulate);
    msg->vt_emulate = VirtualTime(vt_emulate);
    /*
    printf("deserialize: %lld %lld %lld %d \"%s\"\n", 
	   msg->rt_inject.get_ticks(), msg->rt_emulate.get_ticks(), 
	   msg->vt_emulate.get_ticks(), msg->newsockfd, msg->msgstr.c_str());
    */
    return msg;
  }
  SSF_DECLARE_EVENT(EmulatedMessage);
};
SSF_REGISTER_EVENT(EmulatedMessage, EmulatedMessage::create_message);

class ArriveProcess;
class ServeProcess;

class Node : public Entity {
public:
  int id; // a unique identifier
  Semaphore sem; // for synchronizing arrival and service processes
  int qlen; // number of jobs enqueued
  deque<Message*> buffer; // holding the jobs in queue
  inChannel* ic; // the input port for message arrival
  outChannel* oc; // the output port for message departure
  
  Node(int id, int nnodes); // constructor

  void arrival(Process* p) {
    p->waitOn(); // wait on the default input channel
    Message* msg = (Message*)ic->activeEvent(); // retrieve event from input port
    if(msg->isEmulated()) {
      // it's an emulated message visiting the node
      EmulatedMessage* emsg = (EmulatedMessage*)msg;
      if(id > 0) { // for an ordinary node (other than the first node)
	//printf("entity %d received message: real=%lld, now=%lld\n", 
	//       id, realNow().get_ticks(), now().get_ticks());
	buffer.push_back(emsg);
	if(!qlen++) sem.signal();
      } else { // for the first node, message has wrapped around; send it back
	VirtualTime dt = now()-emsg->vt_emulate;
	// rt_inject, rt_emulate-rt_inject, realnow-rt_inject-(now-vt_emulate), now-vt_emulate
	printf("%lld %lld %lld %lld\n", emsg->rt_inject.get_ticks(),
	       (emsg->rt_emulate-emsg->rt_inject).get_ticks(),
	       (realNow()-emsg->rt_inject-dt).get_ticks(), dt.get_ticks());
	emsg->msgstr.append("\n");
	if(send(emsg->newsockfd, emsg->msgstr.c_str(), emsg->msgstr.length()+1, 0) < 0) {
	  close(emsg->newsockfd); 
	}
	delete emsg;
      }
    } else {
      // it's a simulated message
      buffer.push_back(msg);
      if(!qlen++) sem.signal();
    }
  }

  void service(Process* p) {
    sem.wait();
    while(qlen > 0) {
      p->waitFor(SERVICE_TIME);
      Message* msg = buffer.front();
      buffer.pop_front();
      qlen--;

      // append something to the emulated message
      if(msg->isEmulated()) {
	char abuf[16];
	sprintf(abuf, "#%d", id);
	((EmulatedMessage*)msg)->msgstr.append(abuf);
      }
      oc->write((Event*&)msg);
    }
  }

  virtual void emulate(Event* evt) {
    assert(!id);
    EmulatedMessage* msg = (EmulatedMessage*)evt;
    msg->vt_emulate = now();
    msg->rt_emulate = realNow();
    oc->write((Event*&)msg);
  }
};

class ArriveProcess : public Process {
public:
  ArriveProcess(Node* owner) : Process(owner) {}
  virtual void action() { ((Node*)owner())->arrival(this); }
};

class ServeProcess : public Process {
public:
  ServeProcess(Node* owner) : Process(owner) {}
  virtual void action() { ((Node*)owner())->service(this); }
};

Node::Node(int i, int nnodes) : 
  Entity(!i, RESPONSIVENESS), // only the first is emulated
  id(i), sem(this), qlen(0) 
{
  // create and map the channels
  char icname[16];
  sprintf(icname, "IN%d", id);
  ic = new inChannel(this, icname);
  oc = new outChannel(this); 
  sprintf(icname, "IN%d", (id+1)%nnodes);
  oc->mapto(icname, CHANNEL_DELAY);

  // start the processes
  Process* p = new ArriveProcess(this);
  p->waitsOn(ic);
  new ServeProcess(this);

  // entity 0 initiates a simulated message
  /*
  if(!id) {
    Message* msg = new Message;
    oc->write((Event*&)msg);
  }
  */
}

Node* emunode = 0;

void* echoserver(void* data)
{
  int portno = *(int*)data;
  delete (int*)data;

  int sockfd = socket(AF_INET, SOCK_STREAM, 0); 
  if(sockfd < 0) { fprintf(stderr, "ERROR: can't create socket\n"); return 0; }

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  if(bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "ERROR: can't bind to socket on port %d\n", portno);
    return 0;
  }
  printf("# echo server ready on port %d...\n", portno);

  listen(sockfd, 5); 

  struct sockaddr_in clt_addr;
  socklen_t addrlen = sizeof(clt_addr); 
  int newsockfd = accept(sockfd, (struct sockaddr*)&clt_addr, &addrlen);
  if(newsockfd < 0) { fprintf(stderr, "ERROR: can't accept connection\n"); return 0; }
  printf("# new connection from %s:%d\n", inet_ntoa(clt_addr.sin_addr), 
	 ntohs(clt_addr.sin_port));

  for(;;) {
    char buffer[256];
    int n = recv(newsockfd, buffer, 255, 0); 
    if(n < 0) break;
    buffer[n] = '\0';
  
    // strip off the newline characters in the end
    int len = strlen(buffer);
    while(len > 0 && (buffer[len-1] == '\n' || buffer[len-1] == '\r'))
      buffer[--len] = '\0';
    //printf("got message: %s\n", buffer); 
    
    if(!strcmp(buffer, "quit")) {
      send(newsockfd, "bye!\n", 5, 0);
      break;
    }

    EmulatedMessage* msg = new EmulatedMessage(newsockfd, buffer);
    msg->rt_inject = emunode->realNow();
    emunode->insertEmulatedEvent((Event*&)msg);
  }

  close(newsockfd);
  close(sockfd); 
  printf("# echo server terminated...\n");
  return 0;
}

#define CHECKPARAM(x,y) if(!(x)) { if(!machid) fprintf(stderr, y "\n"); ssf_abort(1); }
#define BLOCK_LOW(id,p,n)  ((id)*(n)/(p))
#define BLOCK_HIGH(id,p,n) (BLOCK_LOW((id)+1,p,n)-1)
#define BLOCK_SIZE(id,p,n) (BLOCK_HIGH(id,p,n)-BLOCK_LOW(id,p,n)+1)
#define BLOCK_OWNER(j,p,n) (((p)*((j)+1)-1)/(n))

int main(int argc, char *argv[])
{
  ssf_init(argc, argv);

  int nmachs = ssf_num_machines();
  int machid = ssf_machine_index();
  //int nprocs = ssf_num_processors();

  /* parse command-line arguments */

  if(argc != 4) {
    if(!machid) {
      fprintf(stderr, "Usage: %s end_time num_nodes port_no\n", argv[0]);
      ssf_print_options(stderr);
    }
    ssf_abort(1);
  }

  VirtualTime end_time(argv[1]); 
  CHECKPARAM(end_time > 0, "end_time must be positive");

  int nnodes = atoi(argv[2]); 
  CHECKPARAM(nnodes >= nmachs, "number of queues n must be no less than machines");

  int portno = atoi(argv[3]);
  CHECKPARAM((1024<portno&&portno<65536), "port number out of range");

  /* create the nodes and map them in a ring */

  for(int i=BLOCK_LOW(machid,nmachs,nnodes); 
      i<=BLOCK_HIGH(machid,nmachs,nnodes); i++) {
    Node* node = new Node(i, nnodes); // only the first node is emulated
    if(!i) emunode = node;
  }

  /* create a separate thread to handle network connection */
  if(!machid) {
    pthread_t pid;
    int* ptr = new int; *ptr = portno;
    pthread_create(&pid, NULL, echoserver, ptr);
  }

  ssf_start(end_time, 1.0);
  ssf_finalize();

  //pthread_join(pid, NULL);
  return 0;
}
