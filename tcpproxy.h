/***************************************************************************
    fileName    : tcpproxy.h  -  Communication class header file
    begin       : 2002-09-17
    copyright   : (C) 2002 by BobHuang
 ***************************************************************************/

#ifndef TCPPROXY_H
#define TCPPROXY_H 1
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
const int MAXCONNECTION 		= 1024;
const int PROXY_TCP_PORT 		= 1720;
const int MAXLINE			= 5000;

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

//
typedef unsigned short	   WORD;
typedef unsigned char	   BYTE;	// 1 byte

struct TPKTV3 
{
	TPKTV3() {}
	TPKTV3(WORD);
	BYTE header, padding;
	WORD length;
};

inline TPKTV3::TPKTV3(WORD len)
	: header(3), padding(0)
{
	length = htons(WORD(len + sizeof(TPKTV3)));
}

const int PROXY_SIG_TCP_PORT = 1620;
const int LISTEN_SIG_TCP_PORT = 1621;

const int RC_SUCCESS 		= 0;
const int RC_ERROR 		= -1;
const int RC_CLOSE_SOCKET 	= -2;

class ProxySock 
{
public:
	ProxySock(int fd);
	~ProxySock();
    string& getUid(){return m_uid;}
    void setUid(string& value) {m_uid = value;}
    string& getName(){return m_name;}
    void setName(string& value) {m_name = value;}
    int getFd() {return m_fd;}
    void setFd(int value) {m_fd = value;}
    ProxySock* getPairSock() {return m_pairSock;}
    void setPairSock(ProxySock* value) {m_pairSock = value;}
    bool getIsProxySock() {return m_isProxySock;}
    void setIsProxySock(bool value) {m_isProxySock = value;}
    bool getIsDeleteAble() {return m_isDeleteAble;}
    void setIsDeleteAble(bool value) {m_isDeleteAble = value;}
    int receiveData();
    int sendData(unsigned char* buff, int len);
    int onSetUp();
private:
	unsigned char* m_readBuff;
	unsigned char* m_pReadBuffTmp;
	string m_uid;	
	string m_name;	
    	int m_fd;
    	int m_readStep;
    	int m_readMsgLen;
    	int m_readBuffLen;//include 3,0,lh,ll
    	int m_recvedMsgLen;//include 3,0,lh,ll
    	ProxySock* m_pairSock;
    	pthread_mutex_t m_mutex;
    	unsigned char m_lengHigh;
    	unsigned char m_lengLow;
    	bool m_isProxySock;
    	bool m_isReadHead;
    	bool m_isDeleteAble;
};

enum MsgTypes 
{
	NationalEscapeMsg  = 0x00,
	AlertingMsg        = 0x01,
	CallProceedingMsg  = 0x02,
	ConnectMsg         = 0x07,
	ConnectAckMsg      = 0x0f,
	ProgressMsg        = 0x03,
	SetupMsg           = 0x05,
	SetupAckMsg        = 0x0d,
	ResumeMsg          = 0x26,
	ResumeAckMsg       = 0x2e,
	ResumeRejectMsg    = 0x22,
	SuspendMsg         = 0x25,
	SuspendAckMsg      = 0x2d,
	SuspendRejectMsg   = 0x21,
	UserInformationMsg = 0x20,
	DisconnectMsg      = 0x45,
	ReleaseMsg         = 0x4d,
	ReleaseCompleteMsg = 0x5a,
	RestartMsg         = 0x46,
	RestartAckMsg      = 0x4e,
	SegmentMsg         = 0x60,
	CongestionCtrlMsg  = 0x79,
	InformationMsg     = 0x7b,
	NotifyMsg          = 0x6e,
	StatusMsg          = 0x7d,
	StatusEnquiryMsg   = 0x75,
	FacilityMsg        = 0x62
};

enum InformationElementCodes 
{
	BearerCapabilityIE      = 0x04,
	CauseIE                 = 0x08,
	ChannelIdentificationIE = 0x18,
	FacilityIE              = 0x1c,
	ProgressIndicatorIE     = 0x1e,
	CallStateIE             = 0x14,
	DisplayIE               = 0x28,
	KeypadIE                = 0x2c,
	SignalIE                = 0x34,
	ConnectedNumberIE       = 0x4c,
	CallingPartyNumberIE    = 0x6c,
	CalledPartyNumberIE     = 0x70,
	RedirectingNumberIE     = 0x74,
	UserUserIE              = 0x7e
};

#endif
//end of file proxy.h

