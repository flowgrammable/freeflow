
// A simple, full-duplex wire. Frames from one endpoint are
// delivered to the other endpoint.

struct Dataplane;
struct Context;
struct Port;

extern int puts(char const*);
extern int printf(char const*, ...);


extern int fp_port_get_id(struct Port*);

extern struct Port* fp_context_get_input_port(struct Context*);
extern void         fp_context_set_output_port(struct Context*, struct Port*);

extern struct Port* fp_get_drop_port(struct Dataplane*);

struct Port* port1;
struct Port* port2;
struct Port* drop;


// When an application is loaded, it must perform an initial
// discovery of its environment. Minimally, we should establish
// bindings for reserved ports.
int
load(struct Dataplane* dp)
{
  puts("[wire] load");
  port1 = 0;
  port2 = 0;
  drop = fp_get_drop_port(dp);
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
process(struct Context* cxt)
{
  puts("[wire] processing context");

  struct Port* in = fp_context_get_input_port(cxt);
  printf("[wire] input on %d\n", fp_port_get_id(in));

  // Learn input ports as we see them.
  if (!port1)
    port1 = in;
  if (!port2)
    port2 = in;

  // Forward on port to another.
  struct Port* out;
  if (in == port1 && port2)
    out = port2;
  else if (in == port2 && port1)
    out = port1;
  else
    out = drop;
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
