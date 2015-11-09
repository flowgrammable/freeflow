#include <iostream>
#include <unistd.h>

#include "wire.hpp"
#include "thread.hpp"
#include "port_udp.hpp"

// Global factory object for creating/destroying wire objects
static Factory factory_;

// Ports.
static fp::Port* ports[2];

// Thread pool for pipeline work.
static fp::Thread_pool pipeline_pool(3, true, pipeline);

// The wire object.
static Wire* wire;


//
extern "C" void*
factory() 
{ 
  return &factory_; 
}


//
fp::Application* 
Factory::create(fp::Dataplane& dp) 
{ 
  return new Wire(dp); 
}


//
void 
Factory::destroy(fp::Application& a) 
{ 
  delete& a;
}


//
Wire::Wire(fp::Dataplane& dp) 
  : fp::Application(dp, "wire")
{
	// Initialize ports.
	ports[0] = ports[1] = nullptr;
	
	// Set static pointer to self.
	wire = this;
  std::cout << "application 'wire' created\n";
}


//
Wire::~Wire() 
{ 
  std::cout << "application 'wire' destroyed\n";
}


// The main wire pipeline processing loop. This routine will be executed 
// by a separate thread from the application. It receives work from the
// the applications work queue that the port workers populate.
static void*
pipeline(void* args)
{
	int id = *((int*)args);
	fp::Task* tsk = nullptr;
	std::cerr << "[pipeline_thread:" << id << "] starting\n";
	
	while (wire->state != fp::Application::STOPPED) {
		// Get the next packet to be processed in the pipeline, if available.
		if (pipeline_pool.request(*tsk)) {
			tsk->execute();
			delete tsk;
		}
	}

	std::cerr << "[thread:" << id << "] stopped\n";
	return 0;
}


// The port tx/rx loop. Each port runs in its own thread with an id that
// is equal to the local port id.
//
// FIXME: Put this stuff inside the port receive function. You should be able
// to simply call port->recv() and port->send() and have the ports definition
// do all the ugly work.
static void*
port(void* args)
{
	// Figure out who I am.
	int id = *((int*)args);
	fp::Port_udp* self = (fp::Port_udp*)ports[id];
	fp::Context* cxt;

	// Setup (e)polling
	int pfd = epoll_create(1);
	static struct epoll_event handler, *events;
	handler.events = EPOLLIN | EPOLLET;
	handler.data.fd = self->sock_fd_;
	epoll_ctl(pfd, EPOLL_CTL_ADD, self->sock_fd_, &handler);

	// Start rx/tx.
	std::cerr << "[port_thread:" << id << "] starting\n";
	while (wire->state != fp::Application::STOPPED) {
		// Check for rx (poll). 
		int n = epoll_wait(pfd, events, 10, -1);
		if (n < 0)
			throw("epoll_wait error");
		
		// Receive data.
		cxt = self->recv();
		
		// Assign to the pipeline work queue.
		pipeline_pool.assign(fp::Task(process, cxt));

		// Check if there is anything to send.
		if (self->tx_queue_.size())
			self->send();
	}
	
	// Cleanup.
	delete[] events;
	close(pfd);

	std::cerr << "[thread:" << id << "] stopped\n";
	return 0;
}


// The 'main' function of the wire application.
static void*
process(void* arg)
{
  fp::Context* cxt = (fp::Context*)arg;
  wire->ingress(*cxt);
  wire->route(*cxt);
  wire->egress(*cxt);
  return 0;
}


// Starts the wire application.
void
Wire::start()
{
	// Set the running flag.
	state = fp::Application::RUNNING;

	// Change the port configuration to up.
	if (ports[0] && ports[1]) {
		ports[0]->up();
		ports[1]->up();
	}
	else
		throw("Port(s) not allocated.");

	std::cerr << "[wire] started\n";
}


// Stops the wire application.
void
Wire::stop()
{
	// Set the stopped flag, causing threads to stop.
	state = fp::Application::STOPPED;

	// Change the port configuration to down.
	ports[0]->down();
	ports[1]->down();

	std::cerr << "[wire] stopped\n";
}


// Ingress for the wire is undefined since there is no
// packet decode necessary for this application.
void
Wire::ingress(fp::Context& cxt)
{ }


// Egress assigns the context to the out_port for transmission.
void
Wire::egress(fp::Context& cxt)
{
	if (cxt.out_port == ports[0]->id_)
		ports[0]->send(&cxt);
	else if (cxt.out_port == ports[1]->id_)
		ports[1]->send(&cxt);
	else
		throw("invalid out_port id");
}


// Set the output port based on the input port. Since there is no
// meta data provided from ingress, there is no table look up.
void
Wire::route(fp::Context& cxt)
{
	cxt.out_port = ((cxt.in_port ^ ports[0]->id_) ^ ports[1]->id_);
	#if 0
	if (cxt.in_port == ports[0]->id_)
		cxt.out_port = ports[1]->id_;
	else
		cxt.out_port = ports[0]->id_;
	#endif
}


// Adds the port to the application.
void
Wire::add_port(fp::Port* p)
{
	if (!ports[0]) {
		ports[0] = p;
	}
	else if (!ports[1]){
		ports[1] = p;
	}
	else
		throw("All ports allocated");
}


// Removes the port from the application.
void
Wire::remove_port(fp::Port* p)
{
	if (ports[0] == p)
		ports[0] = nullptr;
	else if (ports[1] == p)
		ports[1] = nullptr;
}