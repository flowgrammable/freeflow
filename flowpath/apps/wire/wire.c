#include <stdint.h>
#include <stdio.h>

// Simple wire application.
struct Context;
struct Port;

extern int puts(const char*);
extern void fp_output_port(struct Context*, struct Port*);
extern void fp_drop(struct Context*);
extern struct Port* fp_get_port(char const*);
extern unsigned int fp_get_portId(struct Port*);
extern unsigned int fp_get_inputPortId(struct Context*);
extern char const* fp_get_out_port(struct Context*);

typedef uint32_t port_id_t; // port id (this should be defined in types)

struct Port* p1;
struct Port* p2;
port_id_t p1_id, p2_id; // port_id_t should also match fp::Port::Id

void
pipeline(struct Context* cxt)
{
  port_id_t in = fp_get_inputPortId(cxt);
  //printf("inputPortId = %d\n", in);

  if (in == p1_id) {
    fp_output_port(cxt, p2);
  }
  else if (in == p2_id) {
    fp_output_port(cxt, p1);
  }
  else {
    puts("[wire] dropped pkt, invalid inputPortId");
    fp_drop(cxt);
  }
}

void
config(void)
{
  puts("[wire] called config\n");
  p1 = fp_get_port("p1");
  p1_id = fp_get_portId(p1);
  p2 = fp_get_port("p2");
  p2_id = fp_get_portId(p2);
}

void
ports(void* ret)
{
  *((int*)ret) = 2;
  puts("[wire] called ports\n");
}
