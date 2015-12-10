// Simple wire application.
struct Context;

extern int puts(const char*);
extern void fp_output_port(struct Context*, const char*);

void pipeline(struct Context* cxt)
{
  puts("[wire] called pipeline\n");
  fp_output_port(cxt, "");
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
