#include "types.h"
#include "user.h"
#include "stat.h"

int
main(int argc, char *argv[])
{
  if(argc < 3) {
    printf(2, "Usage: chown UID TARGET\n");
    exit();
  }

  char *uid = argv[1];
  char *pathname = argv[2];

  int uidInt = atoi(uid);

  if(uidInt < 0 || uidInt > 32767) {
    printf(2, "Error: invalid UID\n");
    exit();
  }

  int rc = chown(pathname, uidInt);
  if(rc == -1) {
    printf(2, "chown failed\n");
 
  }
  exit();
}
