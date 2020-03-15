#include "types.h"
#include "uproc.h"
#include "user.h"


int
main(int argc, char *argv[])
{
  int procs = 64;

  for(int i = 0; i < procs; i++){
    fork();
  }
  wait();
  exit();
}
