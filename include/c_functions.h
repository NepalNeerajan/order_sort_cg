

#ifndef _ext_c_code_h
#define _ext_c_code_h

extern "C" {
#include "transfer.h"
#include "dataspy.h"

int msgDefineMessage (u_int id, u_int xclass, u_int level, char *source, char *body) {

  //  printf("id=%d, class=%d, level=%d, source=%s: %s\n",id, xclass, level, source, body);
  return 0;
}
  
void signal_block ()  {}

}

#endif

