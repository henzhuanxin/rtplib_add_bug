#pragma once
struct trtp_rtp_packet_s;
class RtpSession
{

public:
	 RtpSession(void) ;
	virtual ~RtpSession(void);
public:
	
	int SendRawPacket(char* data, int len,unsigned int duration, int marker);


	int Init(unsigned short int port, char* remoteIP, unsigned short int remotePORT, int rtpType, int frameRate);
	unsigned short int  GetLocalPort();
	int SetRtpFlag(int flag);//// 0 video , 1 video fec, 2 audio, 3 audio fec 	
	int SetRemoteAddr(char* ip, short int port);	
	int Start();
	int Stop();
	int GetSocketFd();
	virtual void RecvRTPCB(const struct trtp_rtp_packet_s * const packet){};
	static void SetUp();

private:
	unsigned long RtpSession_n_;

};
