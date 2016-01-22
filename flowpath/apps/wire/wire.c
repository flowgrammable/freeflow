// Simple wire application.
struct Context;
struct Port;

extern int puts(const char*);
extern void fp_output_port(struct Context*, struct Port*);
extern struct Port* fp_get_port(char const*);


void pipeline(struct Context* cxt)
{
  puts("[wire] called pipeline\n");
  fp_output_port(cxt, fp_get_port("p1"));
}

void config(void)
{
  puts("[wire] called config\n");
}

void ports(void* ret)
{
  *((int*)ret) = 2;
  puts("[wire] called ports\n");
}
