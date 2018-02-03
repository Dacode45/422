#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"


// how many child processes to create
#define N 5

//project 1 test - create some processes and run runnable
void
runnabletest(void)
{
  printf(1,"testing runnable: test1\n");
  int pids[N];
  int i;
  for(i = 0; i < N; i++){
     pids[i] = fork();   //fork a new process
     if(pids[i] == 0){  // if you're the child, loop forever
        for(;;)
          ;
     }
  }
  // at this point, there should be N new processes. The init process should
  // be in a sleep state after boot, the shell process (sh) should be in a
  // sleep state after running a process.  So, N + 3 total processes, 2 have
  // state = SLEEP, 2 have state = RUNNING.  So N - 1 runnable proccesses,
  // assuming we haven't created any background proccesses uutside of this
  // program
  int runnables = runnable();
  printf(1,"runnable processes: %d\n",runnables);
  if(runnables == N - 1) {
     printf(1,"runnable test1: passed\n");
  }
  else {
     printf(1,"runnable test1: failed\n");
  }
  // kill all of our running processes
  for(i = 0; i < N; i++){
     kill(pids[i]);
  }

  // wait for all children to finish being killed
  for(i = 0; i < N; i++){
     wait();
  }
}

int
main(int argc, char *argv[])
{
  printf(1, "project1tests starting*****\n");

  runnabletest();

  printf(1,"project1tests finished*****\n");
  exit();
}
