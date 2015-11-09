#include "nbi.hpp"

extern "C" void fpbind(Context* c)
{
  c->name += "hello bind\n";
}


extern "C" int fpload(Context* c)
{
  c->name += "hello load\n";
  return 0;
}


extern "C" void fpgoto_table(Context* c)
{
  c->name += "hello table\n";
}


void app_start()
{
  Context* c = new Context();

  void* handle;
  void (*process)(Context*);
  char *error;

  handle = dlopen ("../output.so", RTLD_LAZY);
  if (!handle) {
      fputs (dlerror(), stderr);
      exit(1);
  }

  // typecast
  process = (void (*)(Context*)) dlsym(handle, "process");
  if ((error = dlerror()) != NULL)  {
      fputs(error, stderr);
      exit(1);
  }

  (*process)(c);

  dlclose(handle);

  puts(c->name.c_str());
}


void app_stop()
{

}