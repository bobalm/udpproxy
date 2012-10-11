/***************************************************************************
    fileName    : proxy.h  -  Communication class header file
    begin       : 2002-09-17
    copyright   : (C) 2002 by BobHuang
 ***************************************************************************/

#ifndef PROXY_H
#define PROXY_H 1
// system basic includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

// socket and inet includes
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
extern "C"
{
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/socket.h>
#include <linux/tcp.h>
}

// STD library include
#include <iostream>
#include <string>
#include <list>
#include <map>
#include <fstream>
#include <sstream>
#include <vector>
using namespace std;

bool bSysStop = false;
bool sysReset = false;

//日志更换常量
const long oneDaySecs = 86400;//60*60*24
const long changeLogDays = 1;//一天换一个日志
const long changeLogDiffTime = oneDaySecs*changeLogDays;
const int changeLogTime = 4;//哪一刻换日志,04:00换
const int MAXCONNECTION 	= 1024;
const int PROXY_TCP_PORT 	= 1720;
const int MAXLINE		= 5000;
const int RC_SUCCESS		= 0;
const int RC_NORMAL_ERROR	= -1;

class PortRange 
{
public:
	PortRange();
	~PortRange();
       int getPort();
       int getRTPPort();
private:
       int port, minport, maxport;
       pthread_mutex_t mutex;
};


#endif
//end of file proxy.h

