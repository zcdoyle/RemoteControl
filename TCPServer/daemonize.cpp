/*************************************************
Copyright: SmartLight
Author: albert
Date: 2015-12-28
Description：创建守护程序，脱离终端
**************************************************/


#include "daemonize.h"

#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>


/*************************************************
Description:    创建守护程序，脱离终端
Calls:          main
Input:          无
Output:         无
Return:         无
*************************************************/
void daemonize()
{
    pid_t pid;
    struct sigaction sa;

    //become a session leader to lose controlling tty
    if((pid = fork()) < 0)
    {
        printf( "Can't fork");
        exit(-1);
    }
    else if(pid != 0)
    {
        //parent
        exit(0);
    }
    setsid();

    //ensure future opens won't allocate controlling ttys
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGHUP, &sa, NULL) < 0)
    {
        printf("Can't ignore SIGHUP");
        exit(-1);
    }
    if((pid = fork()) < 0)
    {
        printf("Can;t fork");
        exit(-1);
    }
    else if(pid != 0)
    {
        exit(0);
    }
}