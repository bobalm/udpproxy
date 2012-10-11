/*************************************************
 *      File name:    proxy.cpp           *
 *************************************************/
#include "proxy.h"

static PortRange RTPPortRange;
::ofstream gOsLog;

bool background = true;

//线程函数
void listenThread(void);
void proxySocketThread(int argument);
void setup_signal_handlers(void);
void signal_handler(int signum);
int checkSingletonHws();

//连接一个socket,
//返回值即为建立的连接,失败时为0;
int UDPListen(int portNumber)
{

	int sockfd;
	struct sockaddr_in  serv_addr;
   
	// create a UDP socket (an Internet datagram socket)
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		gOsLog<<"server: can't create datagram socket"<<endl;
		return sockfd;
    	}

    int yes = 1;
    if( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        gOsLog<<"UDP setsockopt fail"<<endl;
    }

    // bind local address so that the client can send to us.

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port        = htons(portNumber);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        gOsLog<<"server: can't bind udp port="<<portNumber<<" error:"<<strerror(errno)<<endl;
        close (sockfd);
        return 0;
    }
    
    return sockfd;
}

/*
static int SetNonBlocking(int fd)
{
	if (fd < 0)
	{
		return -1;
	}
	fcntl(sockfd,F_SETFL,O_NONBLOCK);
	return fd;

	// Set non-blocking so we can use select calls to break I/O block on close
	int cmd = 1;
	if (::ioctl(fd, FIONBIO, &cmd) == 0 && ::fcntl(fd, F_SETFD, 1) == 0)
	{
		return fd;
	}
	close(fd);
	return -1;

}

*/
	
string& trimString(string& src)
{
    string::size_type posStartChar, posEndChar;
    posStartChar = src.find_first_not_of(' ');
    if (string::npos == posStartChar)
    {
        src = "";
        return src;   
    }
    else
    {
        posEndChar = src.find_last_not_of(' ');
        src = src.substr(posStartChar, posEndChar-posStartChar+1);
        return src;
    }
}

string getTagContent (string src, string startTag, string endTag)
{
    string::size_type posStartTag, posEndTag;
    string strContent;

    posStartTag = src.find(startTag, 0);
    posEndTag = src.find(endTag, 0);
    
    if( posStartTag == string::npos || posEndTag == string::npos )
    {
        return "";
    }
    strContent = src.substr(posStartTag+startTag.length(), posEndTag-posStartTag-startTag.length());
    return trimString(strContent);
}

void printSysTime ()
{
    char buff[128];
    long ti;
    struct tm *tm;

    time(&ti);
    tm=localtime(&ti);
    sprintf(buff,"sysTime is :%04d/%02d/%02d %02d:%02d:%02d\n",
            tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,
            tm->tm_hour,tm->tm_min,tm->tm_sec);
    gOsLog<<buff<<endl;
    return ;
}

string getSysTime ()
{
    char buff[128];
    long ti;
    struct tm *tm;

    time(&ti);
    tm=localtime(&ti);
    sprintf(buff,"sysTime is :%04d/%02d/%02d %02d:%02d:%02d",
            tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,
            tm->tm_hour,tm->tm_min,tm->tm_sec);

    return string(buff);
}

string getLogFileName()
{
    char buff[128];
    long ti;
    struct tm *tm;
    string logFileName("proxy_");

    time(&ti);
    tm=localtime(&ti);
    sprintf(buff,"%04d_%02d%02d_%02d%02d",
            tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,
            tm->tm_hour,tm->tm_min);

    return logFileName+string(buff);
}

void checkLogFile()
{
    static long prevLogTime = 0;
    long nowTime = 0;
    struct tm *tm;

    time(&nowTime);
    if (0 < prevLogTime)
    {
        if ((nowTime-prevLogTime) < changeLogDiffTime)
        {
            //时间未到
            return;
        }

        tm = localtime(&nowTime);
        if (tm->tm_hour == changeLogTime)//一天一个日志
        {
            string newLogFileName = getLogFileName();
            gOsLog<<"before close!"<<endl;
            gOsLog<<"new log file is "<<newLogFileName<<endl;

            prevLogTime = nowTime;
            gOsLog.close();
            gOsLog<<"after close!"<<endl;
            gOsLog.open(newLogFileName.c_str());
            gOsLog<<"%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%after open!%%%%%%%%%%%%%%%%%%%%%%%"<<endl;
            if (false == gOsLog.is_open()) 
            {
                cout<<"Unable to open "<<newLogFileName<<"for log!"<<endl;
            }
        }
    }//end of if ()
    else
    {
        string newLogFileName = getLogFileName();
        prevLogTime = nowTime;
        gOsLog.open(newLogFileName.c_str());
        if (false == gOsLog.is_open()) 
        {
            gOsLog<<"Unable to open "<<newLogFileName<<"for log!"<<endl;
        }
    }
    
    return;
}

string int2String(int value)
{
    ostringstream os;
    os<<value;
    return os.str();
}

int main(int argc, char *argv[])
{
	if (1 >= argc)
	{
		background = true;
	}
	else
	{
		background = false;
	}
	
	if ( background )
	{
		pid_t pid = fork();
		if (pid < 0)
		{
			cerr << "fork: unable to fork" << endl;
			return -1;
		}
		if (pid ) //parent
		{
			exit(0);
		}
	}

	checkLogFile();
	
	setup_signal_handlers();
	pid_t pidt = getpid();
	gOsLog<<"main thread pid="<<(int)pidt<<endl;

	printSysTime ();
//
	bSysStop = false;
	pthread_t tidListen = 0;
	if (pthread_create(&tidListen, 0, (void *(*)(void*))&listenThread, 0))
	{
		gOsLog<<"create gk thread fail!"<<endl;
		exit(0);
	}

//
	//signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	// main loop for dialogic SR event
	while(!bSysStop)
	{
		sleep(10);

		if (1 >= argc)
		{
		    checkLogFile();
		}
	}

	pthread_join(tidListen, NULL); 
	//closeSys();
	printSysTime ();
	gOsLog<<"exit from the main thread!"<<endl;
	return 0;
}

void setup_signal_handlers(void) 
{
    struct sigaction act;
    
    act.sa_handler = signal_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGPIPE, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
}

void signal_handler(int signum) 
{
    gOsLog<<"signum="<<signum<<endl;
    switch (signum) 
    {
        case SIGINT:
        	gOsLog<<"signum=SIGINT"<<endl;
        	//bSysStop = true;
            break;
    
        case SIGHUP:
        	gOsLog<<"signum=SIGHUP"<<endl;
            break;
        case SIGQUIT:
        	gOsLog<<"signum=SIGQUIT"<<endl;
            break;
        case SIGPIPE:
        	gOsLog<<"signum=SIGPIPE"<<endl;
        	//bSysStop = true;
            break;
        case SIGTERM:
        	gOsLog<<"signum=SIGTERM"<<endl;
        	bSysStop = true;
            break;
        }
}

void listenThread(void)
{
	int listenSockt = 0;
	int yes = 1;

	// init socket to receive message from AppServer
	if( (listenSockt = socket(AF_INET, SOCK_STREAM, 0) ) == -1 )
	{
		gOsLog<<"receive socket() error:"<<strerror(errno)<<endl;
		return;
	}
	
	if( setsockopt(listenSockt, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 )
	{
	 	gOsLog<<"setsockopt error:"<<strerror(errno)<<endl;
	 	close(listenSockt);
	 	return;
	}

	//  int appserversock_fd;
	sockaddr_in myAddr,clientAddr;
	// fill in connection parameters
	myAddr.sin_family = AF_INET;
	myAddr.sin_port = htons(PROXY_TCP_PORT);
	myAddr.sin_addr.s_addr = INADDR_ANY;
	bzero( &(myAddr.sin_zero), 8);

	if( bind(listenSockt, (sockaddr*)&myAddr, sizeof(sockaddr)) == -1 )
	{
		gOsLog<<"bind() error:"<<strerror(errno)<<endl;
		close(listenSockt);
		return;
	}
	else
	{
		gOsLog<<"bind() on port"<<PROXY_TCP_PORT<<endl;
	}
	
	if( listen(listenSockt, MAXCONNECTION) == -1 )
	{
		gOsLog<<"listen()  error:"<<strerror(errno)<<endl;
		close(listenSockt);
		return;
	}

	gOsLog<<"waiting for connection request from client...\n"<<endl;
	
	int sin_size = sizeof(sockaddr_in);
	int newFd = 0;
	
	while(!bSysStop)
	{
		
		newFd = accept(listenSockt, (sockaddr*)&clientAddr, (socklen_t *)&sin_size);
		if( -1 == newFd)
		{
		    gOsLog<<"accept()  error:"<<strerror(errno)<<endl;
		    continue; // try to get in another connection request from ep
		}
		else
		{
			gOsLog<<"got socket fd="<<newFd<<" connection from client:"<<inet_ntoa(clientAddr.sin_addr)<<":"<<int2String(ntohs(clientAddr.sin_port));
			int status = 0;
			do
			{
				pthread_t tidProxy = 0;
				pthread_attr_t attr;
				pthread_attr_init(&attr);
				pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
				status = pthread_create(&tidProxy, &attr, (void *(*)(void*))proxySocketThread, (void*)newFd);
				if (status)
				{
					gOsLog<<"create proxySocketThread thread fail! status="<<status<<",error:"<<strerror(errno)<<endl;
				}
				pthread_attr_destroy(&attr);
			}while(0 != status && errno == EINTR);
		}
	}
	
	close(listenSockt);
	return;
}

void proxySocketThread(int argument)
{

	string sMessage;
	unsigned char* recvBuff = 0;
	int proxyFd = 0;
	int proxyRtpFd = 0;
	int proxyRtcpFd = 0;
	int proxyRtpPort = 0;
	int listenRtpFd = 0;
	int listenRtcpFd = 0;
	int listenRtpPort = 0;
	fd_set fds;	// set of file descriptors to check.
	int nread = 0;	// return from read() 
	int nready = 0;	// # fd's ready. 
	int maxfd = 0;	// fd's 0 to maxfd-1 checked.
	int proxyRtpClientInited = 0;
	int listenRtpClientInited = 0;
	int proxyRtcpClientInited = 0;
	int listenRtcpClientInited = 0;
	int result = 0;
	int sock_len = sizeof(struct sockaddr);
	int readMsgLen = 0;
	int readStep = 0;
	bool readHead = true;
	
	struct sockaddr_in clientAddr;
	struct sockaddr_in proxyRtpCliAddr;
	struct sockaddr_in listenRtpCliAddr;
	struct sockaddr_in proxyRtcpCliAddr;
	struct sockaddr_in listenRtcpCliAddr;

	proxyFd = argument;
	if (0 == proxyFd)
	{
		gOsLog<<"argument is null ,exit from the thread!"<<endl;
		pthread_exit(0); // Notice the cast
	}


	sendBuff = new unsigned char [MAXLINE];
	maxfd ++;
	while (!bSysStop)
	{
			result = sendto(proxyFd,sendBuff,nread,0,(struct sockaddr *)&listenRtpCliAddr,sock_len);
			if (result < 0)
			{
				gOsLog<<"listenRtpFd send to  error"<<endl;
				break;
			}
	}

	close(proxyFd);
	delete[] recvBuff;
	printSysTime();
	gOsLog<<"end proxy one Ep"<<endl;
	pthread_exit(0); // Notice the cast
}
//end of proxy.cpp


