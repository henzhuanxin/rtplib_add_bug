#include "StdAfx.h"
#include "Test.h"
#define FEC_FORMAT 122

static int tdav_session_video_rtp_cb(const void* callback_data, const trtp_rtp_packet_t* packet)
{
	//packet = tsk_object_ref(packet);
	Test* pTest = (Test*)callback_data;
	//pTest->jb = (tdav_video_jb_s *)tsk_object_ref(pTest->jb);
	trtp_rtp_header_t *phead = packet->header;
	if(pTest==NULL || pTest->m_isStop == 1)
		goto exitcb;	
	//if(phead->payload_type == 8)
	//{
	/*printf("recv:%d,datasize:%d,ssrc:%d,time:%d, type:%d, marker:%d\n", phead->seq_num, 
													packet->payload.size, 
													phead->ssrc,
													phead->timestamp,
													phead->payload_type,
												phead->marker);*/
	if(pTest->m_exit)
	{
		//return 1;
		goto exitcb;
	}

	else if((phead->payload_type == FEC_FORMAT))
	{
		if(pTest->m_rtpFlag != 1)
			goto exitcb;
		//printf("get fec\n");
		unsigned char tmp[4096]={0};
		int tmplen = 0;
		unsigned char *ptmp = tmp;	
		unsigned char *pbuf = (unsigned char*)packet->payload.data;
		unsigned short sn_base = pbuf[2]<<8|pbuf[3];
		unsigned short mask = pbuf[12]<<8|pbuf[13];		
		int i,j;
		unsigned short len_recover = pbuf[8]<<8|pbuf[9];
		unsigned short len_protect = pbuf[10]<<8|pbuf[11];
		unsigned short seq_num;
		unsigned int ssrc;

		tdav_video_jb_lock(pTest->jb);

		//int find_num = 0;
		 trtp_rtp_packet_t* pkts[16];
		int pkts_num = 0;	
		for(i = 0; i < 15; ++i)	{
			if(!((mask<<i)&0X8000))
				break;
			const trtp_rtp_packet_t* pkt=tdav_video_jb_pkt_find_by_seq(pTest->jb, sn_base+i);
			if (!pkt)	{				
				//printf("not find\n");
				seq_num = sn_base + i;	
			}
			else{				
				//printf("find\n");
				pkts[pkts_num] = const_cast<trtp_rtp_packet_t*>(pkt);
				pkts_num++; 
			}
		}
		
		if(pkts_num == i){		
            //printf("Get FEC pk, but no need to recovery it :(\n");
			tdav_video_jb_unlock(pTest->jb);
			//return 1;
			goto exitcb;
		}
		else if(pkts_num + 1 == i){		
			//printf("create pkts_num=%d\n",pkts_num);
			if(pkts_num==0)
			{
				tdav_video_jb_unlock(pTest->jb);
			//return 1;
				goto exitcb;
			}
			memcpy(&ptmp[0], &pbuf[0], 8);
			memcpy(&ptmp[12], &pbuf[14], packet->payload.size-14);
			int k;
			 const trtp_rtp_packet_t* ppkt;
			for(k = 0; k < pkts_num; ++k) {
				ppkt = pkts[k];
				len_recover ^= ppkt->payload.size;
				printf("len_recover=%d\n",len_recover);
				ptmp[0] ^= (((uint8_t)ppkt->header->version<<6)|
					((uint8_t)ppkt->header->padding<<5)|
					((uint8_t)ppkt->header->extension<<4)|
					((uint8_t)ppkt->header->csrc_count))&0XFF;
				ptmp[1] ^= (((uint8_t)ppkt->header->marker<<7)|
					((uint8_t)ppkt->header->payload_type));
				ptmp[4] ^= ppkt->header->timestamp >> 24;
				ptmp[5] ^= (ppkt->header->timestamp >> 16) & 0xFF;
				ptmp[6] ^= (ppkt->header->timestamp >> 8) & 0xFF;
				ptmp[7] ^= ppkt->header->timestamp & 0xFF;
				ssrc = ppkt->header->ssrc;			
				for(i = 0; i < ppkt->payload.size; ++i) {			
					ptmp[i + 12] ^= *((uint8_t*)ppkt->payload.data + i); 
				}	
			}
		}
		else {			
           // printf("Get FEC pk, but can't recovery it :(\n");
			tdav_video_jb_unlock(pTest->jb);
			goto exitcb;
		}
		ptmp[0] = (ptmp[0]&0X3F)|0X80; 
		ptmp[2] = seq_num >> 8;
		ptmp[3] = seq_num & 0xFF;
		ptmp[8] = ssrc >> 24;
		ptmp[9] = (ssrc >> 16) & 0xFF;
		ptmp[10] = (ssrc >> 8) & 0xFF;
		ptmp[11] = ssrc & 0xFF;	
		printf("len_recover+12=%d\n",len_recover+12);
		trtp_rtp_packet_t* packet_rtp = trtp_rtp_packet_deserialize(ptmp, len_recover+12);
		if (!packet_rtp)
			{TSK_DEBUG_ERROR("deserialize error");goto exitcb;}

		pTest->m_fec_create_num++;
		printf("%u,create num:%u, time:%u,len=%d\n",pTest->m_fec_create_num, packet_rtp->header->seq_num, packet_rtp->header->timestamp,packet_rtp->payload.size);
		if(pTest->jb&&pTest->m_exit==0)
			tdav_video_jb_put(pTest->jb, (trtp_rtp_packet_t*)packet_rtp);

			tdav_video_jb_unlock(pTest->jb);
		TSK_OBJECT_SAFE_FREE(packet_rtp);	
		//printf("exit fec\n");
	}
	else
	{
		
			if(pTest->jb)
			{		
				if(pTest->m_exit==0)
				{
				tdav_video_jb_put(pTest->jb, (trtp_rtp_packet_t*)packet);	
				}
			}
			else 
			{
				//if(pTest->m_exit==0)
				//{
				pTest->pRTPS->RecvRTPCB(packet);
				//}
			}
			
		
	}
	exitcb:
	//tsk_object_ref(pTest->jb);
	//if(pTest->jb!=NULL)
	//	tsk_object_unref(pTest->jb);
	//printf("exit rtp cb\n");
	return 1;
}


static int _tdav_session_video_jb_cb(const tdav_video_jb_cb_data_xt * data)
{
	if (data&&data->usr_data&&data->type == tdav_video_jb_cb_data_type_rtp)
	{
		//printf("%d\n", data->rtp.pkt->header->seq_num);
		if(((Test*)data->usr_data)->m_exit == 0)
		{
			RtpSession* ps = ((Test*)data->usr_data)->pRTPS;
			if(ps)
			{
				if(((Test*)data->usr_data)->m_isStop != 1)
					ps->RecvRTPCB(data->rtp.pkt);
				else
					printf("LLLLLLLLLLLLLLLLL\n");
			}
		}
		
	}
	return 1;
}
Test::Test(void)
{
	m_isStop = 0;
	m_exit = 0;
	Init();
pRTPS = NULL;
}
Test::Test(RtpSession * ps)
{
	m_isStop =0;
	m_exit = 0;
	pRTPS = ps;
	Init();
}
int Test::GetManagerSocket() {
if (manager) {
	return manager->transport->master->fd;
}
return 0;
}
int Test::CreateRTPManager()
{
	if(!(manager = trtp_manager_create(tsk_false, NULL, tsk_false, tmedia_srtp_type_none,tmedia_srtp_mode_none))){
		printf("***********create error\n");
		return -1;
	}
	manager->use_rtcp = 0;
	return 0;
}
int Test::CreateVideoJB()
{
	jb = tdav_video_jb_create();
	if(jb == NULL)
		return -1;
	//printf("not thread jb=%02X\n", (unsigned long)jb);
	//printf("create jb min=%d\n", get_jb_min(jb));
	tdav_video_jb_set_callback(jb, _tdav_session_video_jb_cb, this);
	tdav_video_jb_start(jb);
	return 0;
}
int Test::CreateFec()
{
	fec = tsk_fec_create();
	if(fec==NULL)
		
	{printf("create FEC ERROR\n");return -1;}
	return 0;
}
int Test::InitManagerParam(uint16_t port, char* remoteIP, uint16_t remotePORT, int rtpType, int frameRate)
{
	if(manager == NULL)
	{
		printf("**************manager is NONE\n");
		return -1;
	}
	if(frameRate == 0)
		frameRate = 15;
	this->frameRate = frameRate;
	if(frameRate != 0)
		m_duration = 90000/frameRate;

	trtp_manager_set_rtp_callback(manager, tdav_session_video_rtp_cb, this);

	//trtp_manager_set_port_range(manager, port, port + 100);
	trtp_manager_set_payload_type(manager, rtpType);
	trtp_manager_set_port_range(manager, 1024, 65535);
	int tryi=0;
	for(; tryi < 5; ++tryi) {
		if(trtp_manager_prepare(manager)){
		
			printf("###########################, try=%d\n", tryi);
			//return -1;
		}
		else
			break;
	}
	if(tryi == 5) {
		printf("###########################,try error\n");
		return -1;
	}
	if((remoteIP!=NULL)&&remotePORT!=0)
	{
	if(trtp_manager_set_rtp_remote(manager, remoteIP, remotePORT)){
		printf("**************set remote error\n");
		return -1;		
	}
	}
	
	//fec_time = m_duration;

	return 0;

}
int Test::StartManager()
{
	if(manager_started==0)
	{
		m_isStop = 0;
		if(trtp_manager_start(manager)){
			printf("**************start error\n");
			return -1;	
		}
		manager_started = 1;
		return 0;
	}
	return 0;
}
int Test::StopManager()
{
	int ret = 0;
	if(manager_started)
		ret = trtp_manager_stop(manager);
	manager_started = 0;
	return ret;
}
int Test::SetRemoteAddr(char* ip, uint16_t port)
{
	if(manager == NULL || ip == NULL)
		return -1;
	if(trtp_manager_set_rtp_remote(manager, ip, port)){
		printf("**************set remote error\n");
		return -1;		
	}
	return 0;
}

int Test::SendRawPacket(char* data, int len,  unsigned int set_duration,int marker)
{
	if(manager == NULL)
	{
		printf("**************manager is null\n");
		return -1;
	}
	tsk_size_t s=0;
	int result = 0;
	if(data == NULL)
	{
		printf("**************data is NULL\n");
		return 0;
	}
	if (m_rtpFlag==0 || m_rtpFlag==2|| m_rtpFlag==3)
	{
		return trtp_manager_send_rtp(manager, data, len, set_duration, marker,marker?tsk_true:tsk_false);
	}
	//int ret=trtp_manager_send_rtp(manager, data, len, duration, marker,tsk_false);
	trtp_rtp_packet_t* packet =  trtp_rtp_packet_create(manager->rtp.ssrc.local, manager->rtp.seq_num, manager->rtp.timestamp, manager->rtp.payload_type, marker);
	if(packet == NULL)
	{
		printf("create packet error\n");
		return -1;
	}
	if (packet)
	{
		if(marker)
		{
			if(!last_frame_time)
			{					
				last_frame_time = tsk_time_now();
				manager->rtp.timestamp += m_duration;
			}
			else{
				uint64_t now = tsk_time_now();
				uint32_t duration = (uint32_t)(now - last_frame_time);
				manager->rtp.timestamp += (duration * 90/* 90KHz */);
				last_frame_time = now;
			}
		}// end marker

		packet->payload.data_const = data;
		packet->payload.size = len;
		
		result = s = trtp_manager_send_rtp_packet(manager, packet, tsk_false); // encrypt and send data
	
		++manager->rtp.seq_num; // seq_num must be incremented here (before the bail) because already used by SRTP context
		
	}	
	//return len;
	if((s > TRTP_RTP_HEADER_MIN_SIZE) && fec){
		int rtp_hdr_size = TRTP_RTP_HEADER_MIN_SIZE + (packet->header->csrc_count << 2);
		packet->payload.data_const = (((const uint8_t*)manager->rtp.serial_buffer.ptr) + rtp_hdr_size);
		packet->payload.size = (s - rtp_hdr_size);

		int ret = tdav_codec_ulpfec_enc_protect(fec, packet);
	
		
		if ((manager->rtp.seq_num)%2 == 0)
		{
			trtp_rtp_packet_t* packet_fec;
			if((packet_fec = trtp_rtp_packet_create(manager->rtp.ssrc.local, fec_seq_num++, fec_time, FEC_FORMAT, tsk_true)))
			{

				s = tdav_codec_ulpfec_enc_serialize((const struct tdav_codec_ulpfec_s*)fec, ((void**)&fec_video_buf), &fec_video_buf_out_size);
						
				if(s > 0)
				{
					packet_fec->payload.data_const = fec_video_buf;
					packet_fec->payload.size = s;
					s = trtp_manager_send_rtp_packet(manager, packet_fec, tsk_true/* already encrypted */);
					//printf("send fec ok\n");	
				}
				TSK_OBJECT_SAFE_FREE(packet_fec);

			}
			fec_time += m_duration;
			tdav_codec_ulpfec_enc_reset((struct tdav_codec_ulpfec_s*)fec);
		}//%2
	
	}
	TSK_OBJECT_SAFE_FREE(packet);

		
	return result;
}
int Test::SetRtpFlag(int value)
{
	m_rtpFlag = value;
	if(value == 1 /*|| value ==3*/)
	{
		if(CreateVideoJB() != 0)
		{
			printf("create JB error\n");
			return -1;
		}
		if(CreateFec() != 0)
		{
			printf("create FEC error\n");
			return -1;
		}
	}
	return 0;
}
int Test::GetRtpFlag()
{
	return m_rtpFlag;
	
}
void Test::Init()
{
	manager_started = 0;
	 manager = NULL;
	jb=NULL;
	fec=NULL; 
	m_rtpFlag = 0;
	m_fec_create_num = 0;
	last_frame_time = 0;
	fec_time = 0;
	fec_seq_num = 0;
	fec_video_buf = (char*)tsk_malloc(VIDEO_BUF_SIZE);
	tnet_startup();
	
	return;
	if(!(manager = trtp_manager_create(tsk_true, NULL, tsk_false, tmedia_srtp_type_none,tmedia_srtp_mode_none))){
		printf("create error\n");
	}
	manager->use_rtcp = 0;
	//trtp_manager_set_port_range(manager, 8888, 9000);
	trtp_manager_set_payload_type(manager, 8);

	trtp_manager_set_rtp_callback(manager, tdav_session_video_rtp_cb, this);

	if(trtp_manager_prepare(manager)){
		printf("trtp_manager_prepare error\n");
		goto bail;
	}
	printf("local port: %d\n",manager->rtp.public_port);
	if(trtp_manager_set_rtp_remote(manager, "192.168.1.139", manager->rtp.public_port)){
		printf("set remote error\n");
		goto bail;
	}
	//////////////////////////////////////

	jb = tdav_video_jb_create();
	printf("not thread jb=%02X\n", (unsigned long)jb);
	//printf("create jb min=%d\n", get_jb_min(jb));
	tdav_video_jb_set_callback(jb, _tdav_session_video_jb_cb, pRTPS);
	tdav_video_jb_start(jb);
	//printf("start jb min=%d\n", get_jb_min(jb));
	////////////////////////////////////
	fec = tsk_fec_create();
	//////////////////////////////////
	if(trtp_manager_start(manager)){
		printf("start error\n");
		goto bail;
		//return;
	}
	int ret ;
	int seq_num = 0;
	int time = 0;
	int isLastFrame =0;
	uint64_t last_frame_time = 0;
	tsk_size_t m_duration = 90000/15;
	for(i=0;i<1000; i++){
		//trtp_manager_send_rtp(trtp_manager_t* self, const void* data, tsk_size_t size, uint32_t duration, tsk_bool_t marker, tsk_bool_t last_packet)
		if ((i+1)%4 == 0)
		{
			isLastFrame = 1;
			ret=trtp_manager_send_rtp(manager, "test", tsk_strlen("test"), 160, tsk_true,tsk_true);
		}
		else
		{
			isLastFrame = 0;
			ret=trtp_manager_send_rtp(manager, "test", tsk_strlen("test"), 160, tsk_false,tsk_false);

		}
		if(ret){


			if(isLastFrame)
			{
				if(!last_frame_time)
				{
						// For the first frame it's not possible to compute the duration as there is no previous one.
						// In this case, we trust the duration from the result (computed based on the codec fps and rate).
						last_frame_time = tsk_time_now();
						manager->rtp.timestamp += m_duration;
					}
					else{
						uint64_t now = tsk_time_now();
						uint32_t duration = (uint32_t)(now - last_frame_time);
						manager->rtp.timestamp += (duration * 90/* 90KHz */);
						last_frame_time = now;
					}
			}


			//printf("send error\n");
			//goto bail;
			Sleep(20);


			trtp_rtp_packet_t* packet_fec;

			if((packet_fec = trtp_rtp_packet_create(manager->rtp.ssrc.local, seq_num++, time, FEC_FORMAT, tsk_true))){
						// serialize the FEC payload packet packet						
						time += 160;

						packet_fec->payload.data_const = "111111";
					
							packet_fec->payload.size = strlen("111111");

						trtp_manager_send_rtp_packet(manager, packet_fec, tsk_true/* already encrypted */);
						TSK_OBJECT_SAFE_FREE(packet_fec);
				}
		}
		//printf("%d,%d\n", i,ret);
	}
bail:
		getchar();
}
int Test::Stop()
{
	m_isStop = 1;
	StopManager();
	return 0;
}
Test::~Test(void)
{
	m_exit = 1;
	printf("delet Test\n");
	
	
	if(jb)
	{
		tdav_video_jb_stop(jb);
		TSK_OBJECT_SAFE_FREE(jb);
		printf("del jb\n");
	}
	if(manager)
	{
		StopManager();
		/////////////////////////////
		TSK_OBJECT_SAFE_FREE(manager);
		printf("del manager\n");
	}
	if(fec)
	{
		//////////////////////////
		TSK_OBJECT_SAFE_FREE(fec);
		printf("del fec\n");
	}
	tsk_free((void**)&fec_video_buf);

		tnet_cleanup();
		printf("test del ok\n");
}
