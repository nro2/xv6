#include "types.h"
#include "user.h"


int
main(int argc, char *argv[])
{
  //int beg = uptime();
  int pid; 
  argv++;
 
  pid = fork();
  int beg = uptime();
  if(pid < 0){
    printf(1, "Fork error");
    exit();
  }
     
  if(pid){
    pid = wait();
  }
  
  else{ 
     sleep(3);
    if(argv[0] != NULL){
      int ret = exec(argv[0], argv);
      if(ret < 0)
        printf(1, "Invalid argument!\n");     	
    }
    exit();
  }
  int end = uptime();
  
  int d1 = ((end - beg)/100)%10;
  int d2 = ((end - beg)/10)%10;
  int d3 = (end - beg) % 10;


  printf(1 , "%s ran in %d.%d%d%d seconds.\n", argv[0], (end - beg)/1000, d1, d2, d3);
  exit();
} 
  

