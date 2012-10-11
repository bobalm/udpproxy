/*************************************************
 *      File name:    tcpproxy.cpp           *
 *************************************************/
#include "tcpproxy.h"

::ofstream gOsLog;
bool background = true;
map<int,ProxySock*> gProxySockMap;
list<int> gSockFdList;
list<int> rmFdList;
//pthread_mutex_t sockMapMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sockListMutex = PTHREAD_MUTEX_INITIALIZER;

//线程函数
void setup_signal_handlers(void);
void signal_handler(int signum);

void listenSigThread(void);
void proxySigThread(void);
void socketProcessThread(void);
int addSigSock(int newFd, struct sockaddr_in* clientAddr, bool isProxySock);
int removeSigSock(int fd);
  
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
    sprintf(buff,"sysTime is :%04d/%02d/%02d %02d:%02d:%02dt",
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
    sprintf(buff,"sysTime is :%04d/%02d/%02d %02d:%02d:%02d\n",
            tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,
            tm->tm_hour,tm->tm_min,tm->tm_sec);

    return string(buff);
}

string getLogFileName()
{
    char buff[128];
    long ti;
    struct tm *tm;
    string logFileName("ttt_");

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

int addSigSock(int newFd, struct sockaddr_in* clientAddr, bool isProxySock)
{
  if (newFd <1)
  {
    return -1;
  }

  fcntl(newFd, F_SETFL, O_NONBLOCK);
  ProxySock* pProxySock = new ProxySock(newFd);
  pProxySock->setIsProxySock(isProxySock);
  string name;
  if (clientAddr)
  {
    if (isProxySock)
    {
      name="Proxy|";
      name = name+inet_ntoa(clientAddr->sin_addr)+":"+int2String(ntohs(clientAddr->sin_port));
      pProxySock->setName(name);
    }
    else
    {
      name="Listen|";
      name = name+inet_ntoa(clientAddr->sin_addr)+":"+int2String(ntohs(clientAddr->sin_port));
      pProxySock->setName(name);
    }
  }

  if (pthread_mutex_lock(&sockListMutex))
  {
    close(newFd);
    return -1;
  }
  gSockFdList.push_back(newFd);
  gProxySockMap.insert(map<int, ProxySock*>::value_type(newFd, pProxySock));
  pthread_mutex_unlock(&sockListMutex);
  printSysTime();
  gOsLog<<"add sockfd="<<newFd<<",name="<<name<<endl; 
  return 0;
}

int removeSigSock(int newFd)
{
  
  if (newFd <1)
  {
    return -1;
  }

  ProxySock* pProxySock = 0;
  if (pthread_mutex_lock(&sockListMutex))
  {
    close(newFd);
    return -1;
  }

  map<int,ProxySock*>::iterator mapitr = gProxySockMap.find(newFd);
  if (gProxySockMap.end()!= mapitr)
  {
    pProxySock = mapitr->second;
    gProxySockMap.erase(mapitr);
  }

  list<int>::iterator itr = gSockFdList.begin();
  for (; itr!=gSockFdList.end(); itr++)
  {
    if (newFd == *itr)
    {
      gSockFdList.erase(itr);
      break;  
    }
  }

  pthread_mutex_unlock(&sockListMutex);

  if (pProxySock)
  {
    ProxySock* pPairSock = pProxySock->getPairSock();
    if (pPairSock)
    {
      pPairSock->setPairSock(0);
      delete pProxySock;
    }
    pProxySock = 0;
  }

  if (close(newFd))
  {
    gOsLog<<"error remove sockfd="<<newFd<<"error:"<<strerror(errno)<<endl; 
  }
  else
  {
    printSysTime();
    gOsLog<<"remove sockfd="<<newFd<<endl;
  }

  return 0;
}

int removeSigSockList()
{

  list<int>::iterator itr = rmFdList.begin();
  for (; itr!=rmFdList.end(); itr++)
  {
    if ( *itr)
    {
      removeSigSock(*itr);
    }
  }
  
  return 0;
}

ProxySock* findProxySockByUid(string & uid)
{
  ProxySock* pProxySock = 0;
  if (pthread_mutex_lock(&sockListMutex))
  {
    return 0;
  }

  map<int,ProxySock*>::iterator itr = gProxySockMap.begin();;
  while (itr != gProxySockMap.end())
  {
    pProxySock = itr->second;
    if (pProxySock && uid == pProxySock->getUid())
    {
      pthread_mutex_unlock(&sockListMutex);
      return pProxySock;
    }
    itr++;          
  }       
  pthread_mutex_unlock(&sockListMutex);
  return 0;
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
  pthread_t tidProxy = 0;
  pthread_t tidProcess = 0;
  if (pthread_create(&tidListen, 0, (void *(*)(void*))&listenSigThread, 0))
  {
    gOsLog<<"create listenSigThread thread fail!"<<endl;
    exit(0);
  }

  if (pthread_create(&tidListen, 0, (void *(*)(void*))&proxySigThread, 0))
  {
    gOsLog<<"create proxySigThread thread fail!"<<endl;
    exit(0);
  }

  if (pthread_create(&tidListen, 0, (void *(*)(void*))&socketProcessThread, 0))
  {
    gOsLog<<"create socketProcessThread thread fail!"<<endl;
    exit(0);
  }

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
  pthread_join(tidProxy, NULL); 
  pthread_join(tidProcess, NULL); 

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

void listenSigThread(void)
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
  myAddr.sin_port = htons(LISTEN_SIG_TCP_PORT);
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
    gOsLog<<"bind() listen on port"<<LISTEN_SIG_TCP_PORT<<endl;
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
      addSigSock(newFd,&clientAddr,false);
    }
  }
  close(listenSockt);
  return;
}

void proxySigThread(void)
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
  myAddr.sin_port = htons(PROXY_SIG_TCP_PORT);
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
    gOsLog<<"bind() proxy on port"<<PROXY_SIG_TCP_PORT<<endl;
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
      addSigSock(newFd,&clientAddr,true);
    }
  }
  close(listenSockt);
  return;
}

void socketProcessThread()
{
  fd_set fds; // set of file descriptors to check.
  int nready = 0; // # fd's ready. 
  int fd = 0;
  int maxfd = 0;  // fd's 0 to maxfd-1 checked.
  list<int>::iterator itrList;
  map<int,ProxySock*>::iterator itrMap;
  ProxySock* pProxySock = 0;
  int result = 0;
  struct timeval tv;

  rmFdList.clear();

  while (!bSysStop)
  {
    // Set up polling.
    FD_ZERO(&fds);
    maxfd = 0;
    itrList = gSockFdList.begin();
    for (; itrList!=gSockFdList.end(); itrList++)
    {
      fd = *itrList;
      FD_SET(fd,&fds);
      if (maxfd < fd)
      {
        maxfd = fd;
      }
    }

    if (0 ==  maxfd)
    {
      //there is no socket connected 
      sleep(1);
      continue;
    }
    
    tv.tv_sec = 0; 
    tv.tv_usec = 500;
    maxfd ++;
    nready = select(maxfd, &fds, (fd_set *) 0, (fd_set *) 0,&tv);
    switch (nready) 
    {  
    case -1:  
      //error
      gOsLog<<"select  error:"<<strerror(errno)<<endl;
            break;
    case 0:  
      //timeout 
      continue;
            break;
    default:  
      break;
    }  
    
    if (nready > 0)
    {
      itrList = gSockFdList.begin();
      for (; itrList!=gSockFdList.end(); itrList++)
      {
        fd = *itrList;
        if (FD_ISSET(fd, &fds))
        {
          //fd have something to read
          itrMap = gProxySockMap.find(fd);
              if (gProxySockMap.end()== itrMap)
              {
                rmFdList.push_back(fd);
                //removeSigSock(fd);
                continue;
              }
              pProxySock = itrMap->second;
              if (0 == pProxySock)
              {
                rmFdList.push_back(fd);
                //removeSigSock(fd);
                continue;
              }
              result = pProxySock->receiveData();
              if (RC_CLOSE_SOCKET == result )
              {
            gOsLog<<"fd="<<fd<<",m_fd="<<pProxySock->getFd()<<endl;             
                rmFdList.push_back(fd);
            //removeSigSock(fd);
              }
        }//end of if (FD_ISSET(fd, &fds))
      }//end of for

      if (!rmFdList.empty())
      {
        removeSigSockList();
        rmFdList.clear();
      }
    }//end of if (nready > 1)
  }//end of while
  gOsLog<<"end proxy thread."<<endl;
  return;
}//end of 

ProxySock::ProxySock(int fd)
{
  m_fd = fd;
  m_pairSock = 0;
  m_readStep = 0;
  m_readMsgLen = 0;
  m_readBuffLen = 0;
  m_isReadHead = true;
  m_isProxySock = false;
  m_readBuffLen = 0;
  m_pReadBuffTmp = 0;
  m_isDeleteAble = false;
  m_readBuff = 0;
  m_recvedMsgLen = 0;
  pthread_mutex_init(&m_mutex,0);
}

ProxySock::~ProxySock()
{

  if (m_readBuff)
  {
    delete[] m_readBuff;
    m_readBuff = 0; 
  }
  pthread_mutex_destroy(&m_mutex);
}

int ProxySock::receiveData()
{
  int result = 0;
  if (m_fd < 1)
  {
    gOsLog<<"m_fd="<<m_fd<<endl;
    m_fd = 0;
    return RC_CLOSE_SOCKET; 
  }

  unsigned char tcpBuff;
  int nread = 0;
  nread = recv(m_fd, &tcpBuff, 1, 0);
  // If error or eof, terminate. 
  if(nread < 0)
  {
    gOsLog<<"m_fd="<<m_fd<<",name="<<m_name<<endl;
    gOsLog<<"recv m_fd error:"<<strerror(errno)<<endl;
    m_isDeleteAble = true;
    return RC_CLOSE_SOCKET;
  }
  else if (0 == nread)
  {
    gOsLog<<"nread=0!,m_fd="<<m_fd<<endl;
    m_isDeleteAble = true;
    return RC_CLOSE_SOCKET;
  }
  
  //gOsLog<<"tcpBuff="<<tcpBuff<<endl;
  if (m_isReadHead)
  {
    if (0 == m_readStep && 3 == tcpBuff)
    {
      m_readStep ++;
    }
    else if (1 == m_readStep && 0 == tcpBuff)
    {
      m_readStep++;
    }
    else if (2 == m_readStep )
    {
      m_readStep ++;
      m_lengHigh = tcpBuff;
      //m_readMsgLen = tcpBuff<<8;
      //gOsLog<<"readStep=2,m_lengHigh="<<(int)m_lengHigh<<endl;
    }
    else if (3 == m_readStep )
    {
      m_readStep =0;
      m_lengLow = tcpBuff;
      m_readBuffLen = (m_lengHigh << 8) + m_lengLow ;
      m_recvedMsgLen = m_readBuffLen;
      m_readMsgLen = m_readBuffLen - 4;//len include 03,00,LH,LL
      m_isReadHead = false;
      //gOsLog<<"readStep=3,m_lengLow="<<(int)m_lengLow<<",readMsgLen="<<m_readMsgLen<<endl;
      if (1 > m_readMsgLen || 99999< m_readMsgLen)
      {
        m_readStep = 0;
        m_readMsgLen = 0;
        m_readBuffLen = 0;
        m_recvedMsgLen = 0;
        m_isReadHead = true;
        return result;
      }
      
      if (0 == m_readBuff)
      {
        m_readBuff = new unsigned char[m_readMsgLen+8];
        m_readBuffLen = m_readMsgLen+8;
      }
      else if (m_readBuffLen < (m_readMsgLen+8))
      {
        delete[] m_readBuff;
        m_readBuff = new unsigned char[m_readMsgLen+8];
        m_readBuffLen = m_readMsgLen+8;
        
      }
      m_pReadBuffTmp = m_readBuff;
      *m_pReadBuffTmp++ = 0x03;
      *m_pReadBuffTmp++ = 0x00;
      *m_pReadBuffTmp++ = m_lengHigh;
      *m_pReadBuffTmp++ = m_lengLow;

      //gOsLog<<"readStep=3,m_lengLow="<<m_lengLow<<",readMsgLen="<<m_readMsgLen<<endl;
    }
    else
    {
      m_readStep = 0;
      m_readMsgLen = 0;
      m_readBuffLen = 0;
      m_recvedMsgLen = 0;
      m_isReadHead = true;
      //no use data read
    }
  }
  else if (m_readMsgLen > 0)
  {
    *m_pReadBuffTmp++ = tcpBuff;
    m_readMsgLen --;
    if (0 == m_readMsgLen)
    {
      //read a packge
      m_readStep = 0;
      m_isReadHead = true;
      printSysTime();
      gOsLog<<"read data from: "<<m_name<<endl;     
      if (false == m_isProxySock)
      {
        //this is listened signal socket
        if (0 == m_pairSock)
        {
          onSetUp();
          if (0 == m_pairSock)
          {
            gOsLog<<"m_pairSock is null,close connected socket."<<endl;
            m_isDeleteAble = true;
            return RC_CLOSE_SOCKET;
          }
        }
        if (RC_CLOSE_SOCKET == m_pairSock->sendData(m_readBuff,m_recvedMsgLen))
        {
          result = RC_CLOSE_SOCKET;
        }
        return result;
      }

      //this is proxy socket
      if ('<' == *(m_readBuff+4) && 'M' == *(m_readBuff+5))
      {
        //this is command   
        if ('\0' != *(m_pReadBuffTmp-1))
        {
          *m_pReadBuffTmp = '\0';
        }
        string sMessage = (const char*)(m_readBuff+4);
        gOsLog<<"read message from "<<m_name<<",get="<<sMessage<<endl;
        string sCommand = getTagContent(sMessage,"<Command>","</Command>");
        if ("Close" == sCommand)
        {
          m_isDeleteAble = true;
          return RC_CLOSE_SOCKET;
        }
        else if ("Start" == sCommand)
        {
          string uid =  getTagContent(sMessage,"<UID>","</UID>");
          ProxySock* pProxySock = 0;
          pProxySock = findProxySockByUid(uid);
          if (pProxySock)
          {
            rmFdList.push_back(pProxySock->getFd());
          }
          m_uid = uid;
        }//end of start
        else
        {
          gOsLog<<"unknow command"<<endl;             
        }
      }//end of command 
      else
      {
        //this is signal
        if (0 == m_pairSock)
        {
          gOsLog<<m_name<<" m_pairSock is null,drop the packge."<<endl;
          return result;
        }
        
        if (RC_CLOSE_SOCKET == m_pairSock->sendData(m_readBuff,m_recvedMsgLen))
        {
          result = RC_CLOSE_SOCKET;
        }
        return result;
      }//end of proxy signal
    }//end of process msg
  }
  else
  {
    m_readStep = 0;
    m_readMsgLen = 0;
    m_readBuffLen = 0;
    m_recvedMsgLen = 0;
    m_isReadHead = true;
  }
  return result;
}

int ProxySock::sendData(unsigned char* buff, int len)
{
  int sendlen = send(m_fd, buff, len, 0);
  if (0 > sendlen)
  {
    gOsLog<<m_name<<" error send msg,error:"<<strerror(errno)<<endl;    
    m_isDeleteAble = true;
    return RC_CLOSE_SOCKET;
  }

  gOsLog<<" send data to "<<m_name<<endl;    
  return sendlen;
}

int ProxySock::onSetUp()
{
  unsigned int protocolDiscriminator = 0;
  unsigned int callReference = 0;
  bool fromDestination = false;
  MsgTypes messageType;
  unsigned char* data = m_readBuff+4;
  int buffLen = m_readBuffLen -4;
  int result = RC_ERROR;
  
  if (buffLen < 5) // Packet too short
  {
    return result;
  }
  
  protocolDiscriminator = *data;
  
  if ((*(data+1)) != 2) // Call reference must be 2 bytes long
  {
    return result;
  }
  
  callReference = (((*(data+2))&0x7f) << 8) | (*(data+3));
  fromDestination = ((*(data+2))&0x80) != 0;
  
  messageType = (MsgTypes)(*(data+4));
  if (SetupMsg == messageType)
  {
    printSysTime();
    gOsLog<<"This is SetupMsg. "<<endl;   
  }
  else
  {
    return result;
  }
  
  // Have preamble, start getting the informationElements into buffers
  int offset = 5;
  while (offset < buffLen) 
  {
    // Get field discriminator
    int discriminator = *(data+offset);
    offset++;
  
    // For discriminator with high bit set there is no data
    if ((discriminator&0x80) == 0) 
    {
      int len = *(data+offset);
      offset++;
      
      if (discriminator == CalledPartyNumberIE)
      {
        if (m_pairSock)
        {
          gOsLog<<"m_pairSock is not null,name ="<<m_pairSock->getName()<<endl;
          return -1;
        }
        
        string calledUid("057156187444-1234567890");
        gOsLog<<"len="<<len<<endl;
        //calledUid.resize(len+1,'\0');
        memcpy((void*)calledUid.data(), data+offset, len);
        //get called Uid=<81>057156187444^@
        //calledUid.append('\0');
        if (len > 1)
        {
          calledUid = calledUid.substr(1,len-1);
        }
        gOsLog<<"called Uid="<<calledUid<<endl;
        
        m_pairSock = findProxySockByUid(calledUid);
        if (m_pairSock)
        {
          gOsLog<<m_name<<" set m_pairSock="<<m_pairSock->getName()<<endl;
          if (0 == m_pairSock->getPairSock())
          {
            m_pairSock->setPairSock(this);
          }
          else
          {
            gOsLog<<"WARNING:"<<m_pairSock->getName()<<"'s pair is ";
            gOsLog<<m_pairSock->getPairSock()->getName()<<endl;
          }
        }
        else
        {
          gOsLog<<m_name<<" no m_pairSock find!!!"<<endl;
        }
        return RC_SUCCESS;
      }
      if (discriminator == UserUserIE) 
      {
        // Special case of User-user field. See 7.2.2.31/H.225.0v4.
        len <<= 8;
        len |= *(data+offset);
        offset++;
        // we also have a protocol discriminator, which we ignore
        offset++;
        // before decrementing the length, make sure it is not zero
        if (len == 0)
        {
            return result;
          }
    
        // adjust for protocol discriminator
        len--;
      }
      if (offset + len > buffLen)
      {
        return result;
      }
      //memcpy(item->GetPointer(len), (const BYTE *)data+offset, len);
      offset += len;
    }//end of if ((discriminator&0x80) == 0) 
  }//end of while
  return RC_SUCCESS;
}

//end of tcpproxy.cpp


