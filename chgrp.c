#include "types.h"
#include "user.h"
#include "stat.h"

int
main(int argc, char *argv[])
{
  if(argc < 3) {
    printf(2, "Usage: chgrp GID TARGET\n");
    exit();
  }

  char *gid = argv[1];
  char *pathname = argv[2];

  int gidInt = atoi(gid);
  if(gidInt < 0 || gidInt > 32767) {
    printf(2, "Error: invalid gID\n");
    exit();
  }

  int rc = chgrp(pathname, gidInt);
  if(rc == -1) {
    printf(2, "chgrp failed\n");
 
  }
  exit();
}
