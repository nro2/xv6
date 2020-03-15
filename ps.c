#include "types.h"
#include "uproc.h"
#include "user.h"

#ifdef CS333_P4

int
main(int argc, char *argv[])
{
  int max = 72;
  struct uproc* tab = malloc(max * sizeof(struct uproc));
  int tabSize = getprocs(max, tab);
 
  if(tabSize == -1){
    printf(1, "Table size error, exiting!\n");
    exit();
  }
  
  int elap1;
  int elap2;
  int elap3;
  int cpu1;
  int cpu2;
  int cpu3;
  printf(1, "PID\tName\tUID\tGID\tPPID\tPRIO\tElapsed\tCPU\tState\tSize\n");
  
  for(int i = 0; i < tabSize; i++){
    elap1 = ((tab[i].elapsed_ticks)/100)%10;
    elap2 = ((tab[i].elapsed_ticks)/10)%10;
    elap3 = (tab[i].elapsed_ticks)%10;
    cpu1  = ((tab[i].CPU_total_ticks)/100)%10;
    cpu2  = ((tab[i].CPU_total_ticks)/10)%10;
    cpu3  = (tab[i].CPU_total_ticks)%10;
    printf(1, "%d\t%s\t%d\t%d\t%d\t%d\t%d.%d%d%d\t%d.%d%d%d\t%s\t%d\n", tab[i].pid, tab[i].name, tab[i].uid, tab[i].gid, tab[i].ppid, tab[i].priority, (tab[i].elapsed_ticks)/1000, elap1, elap2, elap3, (tab[i].CPU_total_ticks)/1000, cpu1, cpu2, cpu3,tab[i].state, tab[i].size);
  }

  free(tab);
  exit();

}

#elif defined(CS333_P2)

int
main(int argc, char *argv[])
{
  int max = 72;

  struct uproc* tab = malloc(max * sizeof(struct uproc));
  int tabSize = getprocs(max, tab);
 
  if(tabSize == -1){
    printf(1, "Table size error, exiting!\n");
    exit();
  }
  
  int elap1;
  int elap2;
  int elap3;
  int cpu1;
  int cpu2;
  int cpu3;
  printf(1, "PID\tName\tUID\tGID\tPPID\tElapsed\tCPU\tState\tSize\n");
  
  for(int i = 0; i < tabSize; i++){
    elap1 = ((tab[i].elapsed_ticks)/100)%10;
    elap2 = ((tab[i].elapsed_ticks)/10)%10;
    elap3 = (tab[i].elapsed_ticks)%10;
    cpu1  = ((tab[i].CPU_total_ticks)/100)%10;
    cpu2  = ((tab[i].CPU_total_ticks)/10)%10;
    cpu3  = (tab[i].CPU_total_ticks)%10;
    printf(1, "%d\t%s\t%d\t%d\t%d\t%d.%d%d%d\t%d.%d%d%d\t%s\t%d\n", tab[i].pid, tab[i].name, tab[i].uid, tab[i].gid, tab[i].ppid, (tab[i].elapsed_ticks)/1000, elap1, elap2, elap3, (tab[i].CPU_total_ticks)/1000, cpu1, cpu2, cpu3,tab[i].state, tab[i].size);
  }

  free(tab);
  exit();

}
#endif
