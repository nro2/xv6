// Below is a series of tests that exercise
// the functionality of the MLFQ added in
// CS333_P4. It shows that the scheduler
// works correctly, and that promotion and
// demotion occur as expected.

#ifdef CS333_P4
#include "types.h"
#include "user.h"

int
makeproc()
{
  //int start_ticks = uptime();
  int pid = fork();

  if(pid == 0)
  {
     while(1);
  }
  else
    printf(1, "Process %d created!\n", pid);

  return pid;
}


void
getsetprio_test()
{
  //int start_ticks = uptime();
  int pid = fork();

  if(pid == 0)
  {
     while(1);
     //exit();
  }
  else
  {
    printf(1, "\nChild process %d created.", pid);
    printf(1, "\nParent process PID is %d.", getpid());

    printf(1, "\nParent process priority is %d.", getpriority(getpid()));
    printf(1, "\nChild process initial priority is %d.", getpriority(pid));
    printf(1, "\nPlease verify with ctrl-p/ctrl-r: \n");

    sleep(7000);

    printf(1, "\nNow, changing child process to lowest priority (0).");

    if(setpriority(pid, 0) < 0)
      printf(1, "\nsetpriority() failed.\n");
    else
      printf(1, "\nsetpriority() returned success.\nPlease verify with ctrl-p/ctrl-r:\n");

    sleep(7000);

    printf(1, "\nNow, attempting to set priority of PID: %d with invalid priority (MAXPRIO + 1)", pid);

    if(setpriority(pid, MAXPRIO + 1) < 0)
      printf(1, "\nsetpriority() failed, as expected. Verify with ctrl-p/ctrl-r:\n");
    else
      printf(1, "\nsetpriority() returned success. Test failed.\n");

    sleep(7000);

    printf(1, "\nNow, attempting to set parent priority (PID: %d) to its current priority.", getpid());
    printf(1, "\nThe current priority is %d.", getpriority(getpid()));
    printf(1, "\nBudget and position should not change.");

    if(setpriority(getpid(), getpriority(getpid())) < 0)
      printf(1, "\nsetpriority() failed.\n");
    else
      printf(1, "\nsetpriority() returned success.\nPlease verify with ctrl-p/ctrl-r:\n");

    sleep(7000);

    printf(1, "\nFinally, attempting to set an older child (%d) to it's current priority (%d).\n", pid-1, getpriority(pid-1));
    printf(1, "\nBudget and position should not change.");

    if(setpriority(pid-1, getpriority(pid-1)) < 0)
      printf(1, "\nsetpriority() failed.\n");
    else
      printf(1, "\nsetpriority() returned success.\nPlease verify with ctrl-p/ctrl-r:\n");

    sleep(7000);

    printf(1, "\nAttempting to call getpriority() on the non current process, PID 1\n");
    
    int rc = getpriority(1);
    if(rc == MAXPRIO){
      printf(1, "\ngetpriority() returned a priority of %d, this test passed\n", rc);
    }
    else{
      printf(1, "\ngetpriority() returned the wrong priority, test failed\n");
    }
    
    printf(1, "\nAttempting to call getpriority() on the current process\n");
    int rc2 = getpriority(getpid());
    printf(1, "\nPriority of current process is %d, use ctrl - p to confirm.\n", rc2);
    sleep(3000);
    
    printf(1, "\nAttempting to call getpriority() on a non-existent PID, (PID 100).\n");

    if(getpriority(100) < 0)
      printf(1, "\ngetpriority() failed, as expected.\n");
    else
      printf(1, "\ngetpriority() did not return failure, test NOT passed.\n");
  }

}






int
main()
{
  int num_procs = 6, high_pid;

  printf(1, "\nYou have %d queues set up. MAXPRIO is %d.\n", MAXPRIO+1, MAXPRIO);
  printf(1, "The DEFAULT_BUDGET is set to %d, and the TICKS_TO_PROMOTE is %d.\n", DEFAULT_BUDGET, TICKS_TO_PROMOTE);

  for(int i = 0; i < num_procs; i++)
  {
    high_pid = makeproc();
  }

  // ********************************************************************//
  // UNCOMMENT TO TEST getpriority() AND setpriority():
  //
  printf(1, "\n\nTEST OF setpriority() and getpriority(): ");
  getsetprio_test();


  // Now, we have a number of processes.
  printf(1, "\nSCHEDULER SELECTION, PROMOTION, DEMOTION: \n");
  printf(1, "Press control-r to confirm scheduler behavior...\n\n");
  //sleep(7000);

  // *********************************************************************//
  // UNCOMMENT TO TEST PROMOTION:
  // (set budget HIGH and promotion time LOW)
  //
  // Tests promotion by setting all PID's to lowest priority
  // and watching them move up. Sleeping processes should
  // also be promoted.
  /*
  for(int i = 0; i <= high_pid; i++)
  {
    // Wow, putting current process at lowest prio
    // really screws up this test, skip setting
    // this one to 0.
    if(getpid() != i)
    {
      if(setpriority(i, 0) == 0)
        printf(1, "\nPID %d set to lowest priority!", i);
    }
  }
  */





  // *********************************************************************//
  // TO TEST DEMOTION:
  // (set budget LOW and promotion time HIGH)

  while(wait() != -1);

  // To get rid of warning when testing demotion, this will never execute:
  printf(1, "%d", high_pid);
  exit();
}
#endif // CS333_P$
