#include "types.h"
#include "user.h"

int main(int argc, char *argv[])
{
    if(argc!=3)
    {    
        printf(1,"invalid arguments\n");
        exit();
    }
    //int ret=0;
    printf(1,"argv1 is %s and argv2 is %s",argv[1],argv[2]);
    set_priority(*argv[1],*argv[2]);
    
    exit();

}