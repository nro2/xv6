#ifdef CS333_P4
#include "types.h"
#include "user.h"
#include "param.h"

int main(int argc, char *argv[]){


  int prio = getpriority(atoi(argv[1]));
  int prioFail = getpriority(-1);
  
  if(prio < 0){
    printf(1, "INVALID PID\n");
    exit();
  }
  printf(1, "Process %d priority is %d\n", atoi(argv[1]), prio);
  sleep(1000);
  printf(1, "Priority of the gpt test is %d\n", getpriority(getpid()));
  sleep(1000);
  printf(1, "Process -1 priority is %d\n", prioFail);
  printf(1, "GET PRIO TEST COMPLETE SLEEPING FOR 3 SECONDS\n");
  sleep(3000);
  printf(1, "Setting -1 priority to MAXPRIO\n");
  int setFail = setpriority(-1, MAXPRIO);
  if(setFail < 0){
    printf(1, "PRIORITY SET FAILED...INVALID PID\n");
  }
  sleep(3000);
  printf(1, "Setting %d priority to 0\n", atoi(argv[1]));
  setpriority(atoi(argv[1]), 0);
  printf(1, "Press CTRL-P\n");
  sleep(3000);
  printf(1, "Setting %d priority to MAXPRIO\n", atoi(argv[1]), prio);
  setpriority(atoi(argv[1]), MAXPRIO);
  printf(1, "Press CTRL-P\n");
  sleep(3000);
  printf(1, "Setting %d priority to MAXPRIO + 2...SHOULD FAIL\n", atoi(argv[1]), prio);
  int setPrio = setpriority(atoi(argv[1]), MAXPRIO + 2);
  if(setPrio < 0){
    printf(1, "INVALID PRIORITY\n");
  }
  printf(1, "Press CTRL-P\n");
  sleep(3000);
  printf(1, "Setting %d priority to -2...SHOULD FAIL\n", atoi(argv[1]), prio);
  setPrio = setpriority(atoi(argv[1]), -2);
  if(setPrio < 0){
    printf(1, "INVALID PRIORITY\n");
  }
  printf(1, "Press CTRL-P\n");
  sleep(3000);
  



  

  exit();
}
#endif
