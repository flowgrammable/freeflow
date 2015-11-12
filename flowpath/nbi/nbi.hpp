#ifndef NBI_HPP
#define NBI_HPP

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>

#include <string>

struct Context
{
  Context()
    : current_byte(0)
  { }

  int headers[5];
  int fields[5];
  int current_byte;
  std::string name;
};


extern "C" void fpbind(Context*);

extern "C" int fpload(Context*);

extern "C" void fpgoto_table(Context*);

void app_start();
void app_stop();


#endif