// Simple wire application.

struct Context;

extern int puts(const char*);

void pipeline(struct Context* cxt)
{
  puts("[wire] called pipeline\n");
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
