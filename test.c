
#include "types.h"
#include "user.h"

int number_of_processes = 10;

int main(int argc, char *argv[])
{
  
  int j;
  for (j = 0; j < number_of_processes; j++)
  {
    int pid = fork();
    if (pid < 0)
    {
      printf(1, "Fork failed\n");
      continue;
    }
    if (pid == 0)
    {
      volatile int i;
      for (volatile int k = 0; k < number_of_processes; k++)
      {
        if (k <= j)
        {
          sleep(200); //io time
        }
        else
        {
          for (i = 0; i < 100000000; i++)
          {
            ; //cpu time
          }
        }
      }
       printf(1, "Process: %d Finished\n", j);
      exit();
    }
    else{
        
       set_priority(100-(20+j),pid); // will only matter for PBS, comment it out if not implemented yet (better priorty for more IO intensive jobs)
    }
  }
  for (j = 0; j < number_of_processes+5; j++)
  {
   // int status, wtime, rtime;
    //status=waitx( &wtime, &rtime);
    //printf(1, "processid %d \t Total Time=%d \t Run Time=%d\tWait Time=%d   \n",status, wtime+rtime, rtime, wtime);
    wait();
  }
  exit();
}
/*
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[]) {
  int pid;
  int k, n;
  int x, z;

  if(argc < 2)
	n = 1; //Default
  else
	n = atoi(argv[1]);
  if (n < 0 ||n > 20)
	n = 2;
  x = 0;
  pid = 0;

  for ( k = 0; k < n; k++ ) {
    pid = fork ();
    if ( pid < 0 ) {
      printf(1, "%d failed in fork!\n", getpid());
    } else if (pid > 0) {
      // parent
      printf(1, "Parent %d creating child %d\n",getpid(), pid);
      wait();
      }
      else{
	printf(1,"Child %d created\n",getpid());
	for(z = 0; z < 40000; z+=1)
	    x = x + 3.14*89.64; //Useless calculation to consume CPU Time
	break;
      }
  }
  exit();
}*/
  