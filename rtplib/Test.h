#pragma once
#include"stdio.h"
#include"RtpSession.h"
#include <stdio.h>
#include <tchar.h>
#include "tinyrtp.h"
#include <string.h>
#include <stdlib.h>

#include "jb/tdav_video_jb.h"
#include "jb/tdav_video_frame.h"
#include "tinydav/codecs/fec/tdav_codec_ulpfec.h"
#define VIDEO_BUF_SIZE 2400
class Test//: public RtpSession
{

public:
	Test(void);
	Test(RtpSession * ps);
	~Test(void);
	void Init();
	int CreateRTPManager();
	int CreateVideoJB();
	int CreateFec();
	int InitManagerParam(uint16_t port, char* remoteIP, uint16_t remotePORT, int rtpType, int frameRate);
	int SetRemoteAddr(char* ip, uint16_t port);
	uint16_t GetLocalPort(){if(manager==NULL) return 0; return manager->rtp.public_port;}
	int SendRawPacket(char* data, int len, unsigned int duration, int marker);
	int StartManager();
	int StopManager();

	int Stop();
	int GetManagerSocket();
	int SetRtpFlag(int value);
	int GetRtpFlag();
public:
	RtpSession * pRTPS;
	tsk_size_t i;
	trtp_manager_t* manager;
	struct tdav_video_jb_s *jb;
	tdav_codec_ulpfec_t* fec; 
	int frameRate;
	int m_use_jb;
	int m_use_fec;
	int m_av_flag;// 0 normal, 1 video, 2 audio
	uint64_t last_frame_time;
	tsk_size_t m_duration;
	uint32_t fec_seq_num ;
	uint32_t fec_time;
	char *fec_video_buf;
	tsk_size_t fec_video_buf_out_size;
	
	uint32_t m_fec_create_num;
	int m_exit;
	int m_rtpFlag;// 0 video , 1 video fec, 2 audio, 3 audio fec 
	int manager_started;

	int m_isStop;
};
/*
int _tmain(int argc, _TCHAR* argv[])
{
	printf("sfsfgs\n");
	return 1;
}*/