
// A simple, full-duplex wire. Frames from one endpoint are
// delivered to the other endpoint.

typedef unsigned int Port;

struct Dataplane;
struct Context;
struct Port;

extern int puts(char const*);
extern int printf(char const*, ...);

extern int fp_port_id_is_up(struct Dataplane*, Port);
extern int fp_port_id_is_down(struct Dataplane*, Port);

extern Port fp_context_get_input_port(struct Context*);
extern void fp_context_set_output_port(struct Context*, Port);


Port port1;
Port port2;
Port drop;
struct Dataplane* dp_;

// When an application is loaded, it must perform an initial
// discovery of its environment. Minimally, we should establish
// bindings for reserved ports.
int
load(struct Dataplane* dp)
{
  puts("[wire] load");
  port1 = 0;
  port2 = 0;
  drop = 0;
  dp_ = dp;
  return 0;
}


// When an appliation is unlaoded, it must release any resources
// that it has acquired.
int
unload(struct Dataplane* dp)
{
  puts("[wire] unload");
  return 0;
}


// Learn ports on startup.
int
start(struct Dataplane* dp)
{
  puts("[wire] start");
  return 0;
}


// Forget ports on shutdown.
int
stop(struct Dataplane* dp)
{
  puts("[wire] stop");
  return 0;
}


int
port_changed(Port port_id)
{
  if (fp_port_id_is_up(dp_, port_id)) {
    printf("[wire] port up: %d\n", port_id);
    if (port1 == 0)
      port1 = port_id;
    else if (port2 == 0)
      port2 = port_id;
  }
  else if (fp_port_id_is_down(dp_, port_id)) {
    printf("[wire] port down: %d\n", port_id);
    if (port_id == port1)
      port1 = 0;
    else if (port_id == port2)
      port2 = 0;
  }
  return 0;
}


int
process(struct Context* cxt)
{
  Port in = fp_context_get_input_port(cxt);
  //printf("[wire] input from: %d\n", fp_port_get_id(in));
  Port out;
  if (in == port1 && port2)
    out = port2;
  else if (in == port2 && port1)
    out = port1;
  else
    return 0;
  //printf("[wire] output to: %d\n", fp_port_get_id(out));
  fp_context_set_output_port(cxt, out);
  return 0;
}


// FIXME: What does this actually do/?
void
config()
{
  puts("[wire] called config");
  // p1 = fp_get_port("p1");
}


// FIXME: What does this actually do?
void
ports(void* ret)
{
  puts("[wire] called ports");
  // *((int*)ret) = 2;
}
