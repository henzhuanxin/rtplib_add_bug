#include "StdAfx.h"
#include "RtpSession.h"
#include "Test.h"
#define RS(n) (*(Test **)&n)
#include "msjexhnd.h"
void RtpSession::SetUp() {
	printf("new rtpsession===========================\n");
	MSJExceptionHandler::SetLogFileName(_T("c:\\ipcsdk_crash"));
	//static MSJExceptionHandler g_MSJExceptionHandler;
	printf("set log session=========================\n");
}
RtpSession::RtpSession(void)
{
	RtpSession_n_ = 0;
	/*static int setflag= 0;
	printf("new rtpsession===========================\n");
	if(1)
	{
		printf("set log session=========================\n");
		setflag = 1;
	g_MSJExceptionHandler.SetLogFileName(_T("c:\\ipcsdk_crash"));
	}*/

}

RtpSession::~RtpSession(void)
{
	if (RtpSession_n_)
	{
		printf("**************Inner delete \n");
		delete *(Test **)&RtpSession_n_;
		printf("**************Inner delete ok\n");
	}
}
int RtpSession::Init(uint16_t port, char* remoteIP, uint16_t remotePORT, int rtpType, int frameRate)
{
	printf("**************Inner init\n");
	Test* ptest = new Test(this);
	if (ptest == NULL)
	{
		printf("**************create inner test error\n");
		RtpSession_n_ = 0;
		return -1;
	}
	*(Test **)&RtpSession_n_ = ptest;
	if(RS(RtpSession_n_)->CreateRTPManager() != 0)
	{
		printf("**************create manager error\n");
		return -1;
	}
	/*if(RS(RtpSession_n_)->CreateVideoJB() != 0)
	{
		printf("create JB error\n");
		return -1;
	}
	if(RS(RtpSession_n_)->CreateFec() != 0)
	{
		printf("create FEC error\n");
		return -1;
	}*/
	printf("**************Inner init manager\n");
	if(RS(RtpSession_n_)->InitManagerParam(port, remoteIP, remotePORT, rtpType, frameRate)!=0)
	{
		printf("**************Init manager ERROR\n");
		return -1;
	}
	printf("**************Inner init OK\n");
	return 0;


}
int RtpSession::GetSocketFd() {
	return RS(RtpSession_n_)->GetManagerSocket();
}
unsigned short int RtpSession::GetLocalPort()
{
	return RS(RtpSession_n_)->GetLocalPort();
}
int RtpSession::SendRawPacket(char* data, int len, unsigned int duration, int marker)
{
	//int flag = RS(RtpSession_n_)->GetRtpFlag();
	return RS(RtpSession_n_)->SendRawPacket(data, len ,duration, marker);
}
int  RtpSession::SetRemoteAddr(char* ip, short int port)
{
	return RS(RtpSession_n_)->SetRemoteAddr(ip, port);
}
int RtpSession::Start()
{
	return RS(RtpSession_n_)->StartManager();
}
int RtpSession::Stop()
{
	return RS(RtpSession_n_)->Stop();
}
int RtpSession::SetRtpFlag(int value)
{
	return RS(RtpSession_n_)->SetRtpFlag(value);
}
/* RtpSession* RtpSession::CreateInstance()
 {
	return new Test();
 }*/

/*
RtpSession* RtpSession::CreateInstance()
{
}
 RtpSession* CreateInstance()
 {
	return new Test();
 }*/
