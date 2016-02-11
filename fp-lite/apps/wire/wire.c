
// A simple, full-duplex wire. Frames from one endpoint are
// delivered to the other endpoint.

struct Context;
struct Port;

extern int puts(const char*);

extern struct Port* fp_get_input_port(struct Context*);
extern struct Port* fp_set_output_port(struct Context*, struct Port*);

struct Port* p1;
struct Port* p2;
struct Port* drop;

void
pipeline(struct Context* cxt)
{
  // The forwarding table.
  if (fp_get_input_port(cxt) == p1)
    fp_set_output_port(cxt, p2);
  else if (fp_get_input_port(cxt) == p2)
    fp_set_output_port(cxt, p1);
  else
    fp_set_output_port(cxt, drop);
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
