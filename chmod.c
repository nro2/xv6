#include "types.h"
#include "user.h"
#include "stat.h"



int
main(int argc, char *argv[])
{

  if(argc < 3) {
    printf(2, "Usage: chmod MODE TARGET\n");
    exit();
  }
 
  char *mode = argv[1];
  char *pathname = argv[2];
  if(strlen(mode) != 4) {
    printf(2, "Error: invalid mode 1\n");
    exit();
  }

  if(mode[0] < '0' || mode[0] > '1') {
    printf(2, "Error: invalid mode 2\n");
    exit();
  }

  for(int i = 1; i < 4; i++) {
    if(mode[i] < '0' || mode[i] > '7'){
      printf(2, "Error: invalid mode 3\n");
      exit();
    }
  }
  int octMode = atoo(mode);
  
  int rc = chmod(pathname, octMode);
  if(rc < 0) {
    printf(2, "chmod failed\n");
  }
  exit();
}

