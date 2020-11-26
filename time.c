#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{ 
 // int cpid=0;
  if(argc >2)
  {
    printf(1,"invalid args\n");
    exit();
  }
  else if(argc==1)
  {
    
    int f=fork();
    if(f==0){
      for(int i=0;i<10;i++)
        sleep(5);
    }else{
      int pid, wtime, rtime;
      pid=waitx( &wtime, &rtime);
      printf(1, "pid %d \t Total Time=%d \t Run Time=%d \t Wait Time=%d \n", pid, wtime+rtime, rtime, wtime);
    }
  }
  else
  {
    int f=fork();
    if(f==0){
        exec(argv[1], argv+1);
    }else{
      int pid, wtime, rtime;
      pid=waitx( &wtime, &rtime);
      printf(1, "pid %d \t Total Time=%d \t Run Time=%d \t Wait Time=%d \n", pid, wtime+rtime, rtime, wtime);
    }
  }
  exit();
}