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

	for(int i=0;i<10;i++)
	{
		proxyRtpPort = RTPPortRange.getRTPPort();
		listenRtpPort = RTPPortRange.getRTPPort();

		proxyRtpFd = UDPListen(proxyRtpPort);
		if (1 > proxyRtpFd)
		{
			proxyRtpFd = 0;
			continue;
		}
		proxyRtcpFd = UDPListen(proxyRtpPort+1);

		if (1 > proxyRtcpFd)
		{
			close(proxyRtpFd);
			proxyRtcpFd = 0;
			proxyRtpFd = 0;
			continue;
		}

		listenRtpFd = UDPListen(listenRtpPort);
		if (1 > listenRtpFd)
		{
			close(proxyRtpFd);
			close(proxyRtcpFd);
			proxyRtcpFd = 0;
			proxyRtpFd = 0;
			listenRtpFd = 0;
			continue;
		}

		listenRtcpFd = UDPListen(listenRtpPort+1);
		if (1 > listenRtcpFd)
		{
			close(proxyRtpFd);
			close(proxyRtcpFd);
			close(listenRtpFd);
			proxyRtcpFd = 0;
			proxyRtpFd = 0;
			listenRtpFd = 0;
			listenRtcpFd = 0;
			continue;
		}
		break;
	}

	if (proxyFd < 1 || proxyRtpFd < 1 || proxyRtcpFd < 1
		|| listenRtpFd < 1|| listenRtcpFd < 1 )
	{
		if (proxyRtpFd > 0)
		{
			close(proxyRtpFd);
		}
		if (proxyRtcpFd > 0)
		{
			close(proxyRtcpFd);
		}
		if (listenRtpFd > 0)
		{
			close(listenRtpFd);
		}
		if (listenRtcpFd > 0)
		{
			close(listenRtcpFd);
		}
		close(proxyFd);
		gOsLog<<"can't get 4 udp port.exit thread."<<endl;
		pthread_exit(0); // Notice the cast
	}

	maxfd = listenRtcpFd;
	if (listenRtpFd > maxfd)
	{
		maxfd = listenRtpFd;
	}
	if (proxyRtcpFd > maxfd)
	{
		maxfd = proxyRtcpFd;
	}
	if (proxyRtpFd > maxfd)
	{
		maxfd = proxyRtpFd;
	}
	if (proxyFd > maxfd)
	{
		maxfd = proxyFd;
	}
	
	gOsLog<<"listen UDP on proxyRtp="<<proxyRtpPort<<endl;

	recvBuff = new unsigned char [MAXLINE];
	maxfd ++;
	while (!bSysStop)
	{
		// Set up polling.
		FD_ZERO(&fds);
		FD_SET(proxyFd,&fds);
		FD_SET(proxyRtpFd,&fds);
		FD_SET(proxyRtcpFd,&fds);
		FD_SET(listenRtpFd,&fds);
		FD_SET(listenRtcpFd,&fds);

		struct timeval tv;
		/* Wait up to 30*60 seconds. */
		tv.tv_sec = 1800;
		tv.tv_usec = 0;

		//* Wait for some input.
		nready = select(maxfd, &fds, (fd_set *) 0, (fd_set *) 0, &tv);
		//nready = select(maxfd, &fds, (fd_set *) 0, (fd_set *) 0,NULL);
		switch (nready) 
		{  
		case -1:  
			//error
			gOsLog<<"select  error:"<<strerror(errno)<<endl;
		        break;
		case 0:  
			//timeout 
			gOsLog<<"select timeout, error:"<<strerror(errno)<<endl;
		        break;
		default:  
			break;
		}  
		
		if( FD_ISSET(proxyFd, &fds))
		{
			
			char tcpBuff;
			nread = recv(proxyFd, &tcpBuff, 1, 0);
			// If error or eof, terminate. 
			if(nread < 1)
			{
				gOsLog<<"recv proxyFd error:"<<strerror(errno)<<endl;
				break;
			}

			if (readHead)
			{
				if (0 == readStep && 3 == tcpBuff)
				{
					readStep ++;
				}
				else if (1 == readStep && 0 == tcpBuff)
				{
					readStep++;
				}
				else if (2 == readStep )
				{
					readStep ++;
					readMsgLen = tcpBuff<<8;
					//gOsLog<<"readStep=2,tcpBuff="<<tcpBuff<<",readMsgLen="<<readMsgLen<<endl;
				}
				else if (3 == readStep )
				{
					readStep =0;
					readMsgLen += tcpBuff;
					readMsgLen -=4;//len include 03,00,LH,LL
					readHead = false;
					sMessage = "";
					//gOsLog<<"readStep=3,tcpBuff="<<tcpBuff<<",readMsgLen="<<readMsgLen<<endl;
				}
				else
				{
					readStep = 0;
					readMsgLen = 0;
					readHead = true;
					//no use data read
				}
			}
			else if (readMsgLen > 0)
			{
				sMessage += tcpBuff;
				readMsgLen --;
				if (0 == readMsgLen)
				{
					readMsgLen = 0;
					readHead = true;
					//string sMessage = string((const char*)recvBuff);
					printSysTime();
					gOsLog<<"proxyFd="<<proxyFd<<",read message: "<<sMessage<<endl;
					string sCommand = getTagContent(sMessage,"<Command>","</Command>");
					if ("Close" == sCommand)
					{
						break;
					}
					else if ("Start" == sCommand)
					{
						int len = 0;
						//<Message><ProxyRTP>RTPPort</ProxyRTP><ListenRTP>RTPPort</ListenRTP></Message>
						sMessage = "<Message><ProxyRTP>";
						sMessage += int2String(proxyRtpPort);
						sMessage +="</ProxyRTP><ListenRTP>";
						sMessage += int2String(listenRtpPort);
						sMessage +="</ListenRTP></Message>";
						//3,0,length(H,L),Message
						len = sMessage.length()+4;
	
						//string msg;
						unsigned char sendbuf[1024];
						sendbuf[0]=3;
						sendbuf[1]=0;
						sendbuf[2]=((len >> 8) & 0x0ff);
						sendbuf[3]=(len & 0x0ff);
						memcpy(sendbuf+4, sMessage.data(),len-4);
						sendbuf[len] ='\0';
						len = send(proxyFd, sendbuf, len+1, 0);
						if (0 > len)
						{
						    bSysStop = true;
						    gOsLog<<"error send msg,error:"<<strerror(errno)<<endl;    
						    break;
						}
					}//end of start
					else if (!listenRtpClientInited && "SetClientAddr" == sCommand)
					{
						string clientAddr = getTagContent(sMessage,"<Addr>","</Addr>");
						int port = atoi(getTagContent(sMessage,"<RTPPort>","</RTPPort>").c_str());
						
						gOsLog<<"listenRtpClientInited."<<endl;
						listenRtpClientInited = 1;
						listenRtpCliAddr.sin_family = AF_INET;     // host byte order
						listenRtpCliAddr.sin_port = htons(port);
						listenRtpCliAddr.sin_addr.s_addr = inet_addr(clientAddr.c_str()); // short, network byte order
						memset(&(listenRtpCliAddr.sin_zero), '\0', 8); // zero the rest of the struct

						listenRtcpClientInited = 1;
						listenRtcpCliAddr.sin_family = AF_INET;     // host byte order
						listenRtcpCliAddr.sin_port =  htons(port+1);
						listenRtcpCliAddr.sin_addr.s_addr = inet_addr(clientAddr.c_str()); // short, network byte order
						memset(&(listenRtcpCliAddr.sin_zero), '\0', 8); // zero the rest of the struct
					}
					else
					{
						gOsLog<<"unknow command"<<endl;    					
					}
				}//end of process msg
			}
			else
			{
				readStep = 0;
				readMsgLen = 0;
				readHead = true;
				sMessage = "";
			}
		}//end of  if( FD_ISSET(proxyFd, &fds))

		if( FD_ISSET(proxyRtpFd, &fds))
		{
			socklen_t fromLen = sizeof(clientAddr);
			nread = recvfrom(proxyRtpFd, recvBuff, MAXLINE, 0, (struct sockaddr *)&clientAddr, &fromLen);
			if (nread < 0)
			{
				gOsLog<<"recvfrom proxyRtpFd error,nread="<<nread<<" error:"<<strerror(errno)<<endl;
			}
			else
			{
				if (!proxyRtpClientInited)
				{
					gOsLog<<"proxyRtpClientInited,IP="<<inet_ntoa(clientAddr.sin_addr);
					gOsLog<<",Port="<<ntohs(clientAddr.sin_port)<<endl;
					proxyRtpClientInited = 1;
					proxyRtpCliAddr.sin_family = AF_INET;     // host byte order
					proxyRtpCliAddr.sin_port = clientAddr.sin_port; // short, network byte order
					proxyRtpCliAddr.sin_addr = clientAddr.sin_addr;
					memset(&(proxyRtpCliAddr.sin_zero), '\0', 8); // zero the rest of the struct
				}

				if (listenRtpClientInited)
				{
					result = sendto(listenRtpFd,recvBuff,nread,0,(struct sockaddr *)&listenRtpCliAddr,sock_len);
					if (result < 0)
					{
						gOsLog<<"listenRtpFd send to  error"<<endl;
						break;
					}
				}
				else
				{
					gOsLog<<"listenRtpClientInited is false"<<endl;
				}
			}
		}

		if( FD_ISSET(proxyRtcpFd, &fds))
		{
			socklen_t fromLen = sizeof(clientAddr);
			nread = recvfrom(proxyRtcpFd, recvBuff, MAXLINE, 0, (struct sockaddr *)&clientAddr, &fromLen);
			if (nread < 0)
			{
				gOsLog<<"recvfrom proxyRtcpFd error:"<<strerror(errno)<<endl;
			}
			else
			{
				if (!proxyRtcpClientInited)
				{
					gOsLog<<"proxyRtcpClientInited,IP="<<inet_ntoa(clientAddr.sin_addr);
					gOsLog<<",Port="<<ntohs(clientAddr.sin_port)<<endl;

					proxyRtcpClientInited = 1;
					proxyRtcpCliAddr.sin_family = AF_INET;     // host byte order
					proxyRtcpCliAddr.sin_port = clientAddr.sin_port; // short, network byte order
					proxyRtcpCliAddr.sin_addr = clientAddr.sin_addr;
					memset(&(proxyRtcpCliAddr.sin_zero), '\0', 8); // zero the rest of the struct
				}

				if (listenRtcpClientInited)
				{
					result = sendto(listenRtcpFd,recvBuff,nread,0,(struct sockaddr *)&listenRtcpCliAddr,sock_len);
					if (result < 0)
					{
						gOsLog<<"listenRtcpFd send to  error"<<endl;
						break;
					}
				}

			}
		}

		if( FD_ISSET(listenRtpFd, &fds))
		{
			socklen_t fromLen = sizeof(clientAddr);
			nread = recvfrom(listenRtpFd, recvBuff, MAXLINE, 0, (struct sockaddr *)&clientAddr, &fromLen);
			if (nread < 0)
			{
				gOsLog<<"recvfrom listenRtpFd error:"<<strerror(errno)<<endl;
			}
			else
			{
				/*
				if (!listenRtpClientInited)
				{
					gOsLog<<"listenRtpClientInited."<<endl;
					listenRtpClientInited = 1;
					listenRtpCliAddr.sin_family = AF_INET;     // host byte order
					listenRtpCliAddr.sin_port = clientAddr.sin_port; // short, network byte order
					listenRtpCliAddr.sin_addr = clientAddr.sin_addr;
					memset(&(listenRtpCliAddr.sin_zero), '\0', 8); // zero the rest of the struct
				}
				*/
				if (proxyRtpClientInited)
				{
					result = sendto(proxyRtpFd,recvBuff,nread,0,(struct sockaddr *)&proxyRtpCliAddr,sock_len);
					if (result < 0)
					{
						gOsLog<<"proxyRtpFd send to  error"<<endl;
						break;
					}
				}
				else
				{
					gOsLog<<"proxyRtpClientInited is false."<<endl;
				}

			}
		}

		if( FD_ISSET(listenRtcpFd, &fds))
		{
			socklen_t fromLen = sizeof(clientAddr);
			nread = recvfrom(listenRtcpFd, recvBuff, MAXLINE, 0, (struct sockaddr *)&clientAddr, &fromLen);
			if (nread < 0)
			{
				gOsLog<<"recvfrom listenRtcpFd error:"<<strerror(errno)<<endl;
			}
			else
			{
				if (proxyRtcpClientInited)
				{
					result = sendto(proxyRtcpFd,recvBuff,nread,0,(struct sockaddr *)&proxyRtcpCliAddr,sock_len);
					if (result < 0)
					{
						gOsLog<<"proxyRtcpFd send to  error"<<endl;
						break;
					}
				}

			}
		}
	}

	close(proxyFd);
	close(proxyRtcpFd);
	close(proxyRtpFd);
	close(listenRtcpFd);
	close(listenRtpFd);
	delete[] recvBuff;
	printSysTime();
	gOsLog<<"end proxy one Ep"<<endl;
	pthread_exit(0); // Notice the cast
}

PortRange::PortRange()
{
	minport = 3000;
	maxport = 9000;
	port = minport;
	pthread_mutex_init(&mutex,0);
}

PortRange::~PortRange()
{
	pthread_mutex_destroy(&mutex);
}

int PortRange::getPort()
{
       int result = port;
       if (port > 0) 
       {
		if (pthread_mutex_lock(&mutex))
		{
		    gOsLog<<"lock port mutex fail."<<endl;
		    return -1;
		}
               ++port;
               if (port > maxport)
               {
                       port = minport;
               }
		pthread_mutex_unlock( &mutex );               
       }
       return result;
}

int PortRange::getRTPPort()
{
 	int port = getPort();
	if (port & 1) // make sure it is even
	{
		port = getPort();
	}
	getPort(); // skip one
	return port;
}

//end of proxy.cpp


