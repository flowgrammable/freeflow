
// A simple, full-duplex wire. Frames from one endpoint are
// delivered to the other endpoint.

struct Dataplane;
struct Context;
struct Port;

extern int puts(char const*);
extern int printf(char const*, ...);

extern int           fp_num_system_ports(struct Dataplane*);
extern struct Port** fp_get_system_ports(struct Dataplane*);
extern struct Port*  fp_get_drop_port(struct Dataplane*);

extern struct Port* fp_get_input_port(struct Context*);
extern struct Port* fp_set_output_port(struct Context*, struct Port*);

struct Port* p1;
struct Port* p2;
struct Port* drop;


int
load(struct Dataplane* dp)
{
  puts("[wire] load");
  return 0;
}


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

  // Make sure we have the right number of ports.
  int nports = fp_num_system_ports(dp);
  if (nports < 2) {
    puts("[wire] error: a wire needs two endpoints");
    return 1;
  }
  if (nports > 2) {
    puts("[wire] error: a wire cannot have more than two endpoints");
    return 1;
  }

  struct Port** ports = fp_get_system_ports(dp);
  p1 = ports[0];
  p2 = ports[1];
  drop = fp_get_drop_port(dp);
  return 0;
}


// Forget ports on shutdown.
int
stop(struct Dataplane* dp)
{
  puts("[wire] stop");
  p1 = 0;
  p2 = 0;
  return 0;
}


int
process(struct Context* cxt)
{
  // The forwarding table.
  if (fp_get_input_port(cxt) == p1)
    fp_set_output_port(cxt, p2);
  else if (fp_get_input_port(cxt) == p2)
    fp_set_output_port(cxt, p1);
  else
    fp_set_output_port(cxt, drop);
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
