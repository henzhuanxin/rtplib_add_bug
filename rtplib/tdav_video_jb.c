/*
* Copyright (C) 2011 Doubango Telecom <http://www.doubango.org>
*
* Contact: Mamadou Diop <diopmamadou(at)doubango(DOT)org>
*	
* This file is part of Open Source Doubango Framework.
*
* DOUBANGO is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*	
* DOUBANGO is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*	
* You should have received a copy of the GNU General Public License
* along with DOUBANGO.
*
*/

/**@file tdav_video_jb.c
 * @brief Video Jitter Buffer
 *
 * @author Mamadou Diop <diopmamadou(at)doubango(DOT)org>
 */
#include "jb/tdav_video_jb.h"
#include "jb/tdav_video_frame.h"

#include "tinyrtp/rtp/trtp_rtp_packet.h"

#include "tsk_time.h"
#include "tsk_memory.h"
#include "tsk_thread.h"
#include "tsk_condwait.h"
#include "tsk_debug.h"

#if TSK_UNDER_WINDOWS
#	include <windows.h>
#endif

#define TDAV_VIDEO_JB_DISABLE           0


#define TDAV_VIDEO_JB_TAIL_MAX	6
static const tdav_video_frame_t* _tdav_video_jb_get_frame(struct tdav_video_jb_s* self, uint32_t timestamp, uint8_t pt, tsk_bool_t *pt_matched);
static void* TSK_STDCALL _tdav_video_jb_decode_thread_func(void *arg);

typedef struct tdav_video_jb_s
{
	TSK_DECLARE_OBJECT;

	tsk_bool_t started;
	
	tdav_video_frames_L_t *frames;
	int64_t frames_count;
	tsk_size_t tail_max;

	tsk_size_t latency_min;
	tsk_size_t latency_max;
	
	tdav_video_jb_cb_f callback;
	const void* callback_data;

	// to avoid locking use different cb_data
	tdav_video_jb_cb_data_xt cb_data_rtp;
	tdav_video_jb_cb_data_xt cb_data_fdd;
	tdav_video_jb_cb_data_xt cb_data_any;
	

	TSK_DECLARE_SAFEOBJ;
}
tdav_video_jb_t;


static tsk_object_t* tdav_video_jb_ctor(tsk_object_t * self, va_list * app)
{
	tdav_video_jb_t *jb = self;
	if(jb){
		if(!(jb->frames = tsk_list_create())){
			TSK_DEBUG_ERROR("Failed to create list");
			return tsk_null;
		}
	
		jb->cb_data_fdd.type = tdav_video_jb_cb_data_type_fdd;
		jb->cb_data_rtp.type = tdav_video_jb_cb_data_type_rtp;
	
	

		tsk_safeobj_init(jb);
	}
	return self;
}
static tsk_object_t* tdav_video_jb_dtor(tsk_object_t * self)
{ 
	tdav_video_jb_t *jb = self;
	if(jb){
		if(jb->started){
			tdav_video_jb_stop(jb);
		}
		TSK_OBJECT_SAFE_FREE(jb->frames);
	
		tsk_safeobj_deinit(jb);
	}

	return self;
}
static const tsk_object_def_t tdav_video_jb_def_s = 
{
	sizeof(tdav_video_jb_t),
	tdav_video_jb_ctor, 
	tdav_video_jb_dtor,
	tsk_null, 
};

tdav_video_jb_t* tdav_video_jb_create()
{
	tdav_video_jb_t* jb;

	if((jb = tsk_object_new(&tdav_video_jb_def_s))){
	
		jb->tail_max = TDAV_VIDEO_JB_TAIL_MAX;
	}
	return jb;
}


int tdav_video_jb_set_callback(tdav_video_jb_t* self, tdav_video_jb_cb_f callback, const void* usr_data)
{
	if(!self){
		TSK_DEBUG_ERROR("Invalid parameter");
		return -1;
	}
	self->callback = callback;
	self->cb_data_any.usr_data = usr_data;
	self->cb_data_fdd.usr_data = usr_data;
	self->cb_data_rtp.usr_data = usr_data;
	return 0;
}

int tdav_video_jb_start(tdav_video_jb_t* self)
{
	int ret = 0;
	if(!self){
		TSK_DEBUG_ERROR("Invalid parameter");
		return -1;
	}
	if(self->started){
		return 0;
	}

	self->started = tsk_true;
	
	return ret;
}

int tdav_video_jb_put(tdav_video_jb_t* self, trtp_rtp_packet_t* rtp_pkt)
{

	const trtp_rtp_packet_t* old_frame;
	tsk_bool_t pt_matched = tsk_false, is_frame_late_or_dup = tsk_false, is_restarted = tsk_false;
	

	if(!self || !rtp_pkt || !rtp_pkt->header){
		TSK_DEBUG_ERROR("Invalid parameter");
		return -1;
	}

	if(!self->started){
		TSK_DEBUG_INFO("Video jitter buffer not started");
		return 0;
	}
	//tsk_safeobj_lock(self);

	old_frame = tdav_video_jb_pkt_find_by_seq(self, rtp_pkt->header->seq_num);	
	if(old_frame)
	{
		//tsk_safeobj_unlock(self);
		return 0;
	}

	
	if(!old_frame){
			//tsk_list_lock(self->frames);
			rtp_pkt = tsk_object_ref(rtp_pkt);
			tsk_list_push_ascending_data(self->frames, (void**)&rtp_pkt);
			//tsk_list_unlock(self->frames);
			self->frames_count++;
	
	}
	//tsk_safeobj_unlock(self);	
	
	_tdav_video_jb_decode_thread_func(self);

	return 0;
}

int tdav_video_jb_stop(tdav_video_jb_t* self)
{
	int ret;
	if(!self){
		TSK_DEBUG_ERROR("Invalid parameter");
		return -1;
	}
	if(!self->started){
		return 0;
	}

	TSK_DEBUG_INFO("tdav_video_jb_stop()");

	self->started = tsk_false;

	/*ret = tsk_condwait_broadcast(self->decode_thread_cond);
	
	if(self->decode_thread[0]){
		ret = tsk_thread_join(&self->decode_thread[0]);
	}
	*/
	return 0;
}

static const tdav_video_frame_t* _tdav_video_jb_get_frame(tdav_video_jb_t* self, uint32_t timestamp, uint8_t pt, tsk_bool_t *pt_matched)
{
	

	return NULL;
}
void tdav_video_jb_lock(tdav_video_jb_t *self)
{	
     tsk_safeobj_lock(self); 	
}
void tdav_video_jb_unlock(tdav_video_jb_t *self)
{    
			tsk_safeobj_unlock(self);
}

const trtp_rtp_packet_t* tdav_video_jb_pkt_find_by_seq(tdav_video_jb_t* self, unsigned  short seq)
{
	const trtp_rtp_packet_t* ret = tsk_null;
	const tsk_list_item_t *item;
	
	//tsk_list_lock(self->frames);
	tsk_list_foreach(item, self->frames){
		if(((trtp_rtp_packet_t*)(item->data))->header->seq_num == seq){
			ret = item->data;
			break;
		}
		
	}
	//tsk_list_unlock(self->frames);
	return ret;
}

static void* TSK_STDCALL _tdav_video_jb_decode_thread_func(void *arg)
{
	tdav_video_jb_t* jb = (tdav_video_jb_t*)arg;
	
	const tdav_video_frame_t* frame;
	tsk_list_item_t* item;	
	tsk_bool_t postpone;
	tsk_size_t missing_seq_num_start;
	tsk_size_t missing_seq_num_count;
	if(!jb->started)
	{
		return NULL;
	}

		

		if(jb->frames_count > jb->tail_max)
		{
			item = tsk_null;
			postpone = tsk_false;
            
			//tsk_safeobj_lock(jb); // against get_frame()
			tsk_list_lock(jb->frames); // against put()			
			
			item = tsk_list_pop_first_item(jb->frames);
			--jb->frames_count;
		
			tsk_list_unlock(jb->frames);
			//tsk_safeobj_unlock(jb);
            
			if(item)
			{
				
				if(jb->callback)
				{
					trtp_rtp_packet_t* pkt = item->data; 								
					jb->cb_data_rtp.rtp.pkt = pkt;
					jb->callback(&jb->cb_data_rtp);				
				}
	            
				TSK_OBJECT_SAFE_FREE(item);
			}
		}

	return tsk_null;
}
