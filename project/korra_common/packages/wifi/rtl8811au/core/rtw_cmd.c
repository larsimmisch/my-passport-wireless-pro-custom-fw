/******************************************************************************
 *
 * Copyright(c) 2007 - 2012 Realtek Corporation. All rights reserved.
 *                                        
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#define _RTW_CMD_C_

#include <drv_types.h>
/*
Caller and the rtw_cmd_thread can protect cmd_q by spin_lock.
No irqsave is necessary.
*/

sint	_rtw_init_cmd_priv (struct	cmd_priv *pcmdpriv)
{
	sint res=_SUCCESS;
	
_func_enter_;	

	_rtw_init_sema(&(pcmdpriv->cmd_queue_sema), 0);
	//_rtw_init_sema(&(pcmdpriv->cmd_done_sema), 0);
	_rtw_init_sema(&(pcmdpriv->terminate_cmdthread_sema), 0);
	
	
	_rtw_init_queue(&(pcmdpriv->cmd_queue));
	
	//allocate DMA-able/Non-Page memory for cmd_buf and rsp_buf
	
	pcmdpriv->cmd_seq = 1;
	
	pcmdpriv->cmd_allocated_buf = rtw_zmalloc(MAX_CMDSZ + CMDBUFF_ALIGN_SZ);
	
	if (pcmdpriv->cmd_allocated_buf == NULL){
		res= _FAIL;
		goto exit;
	}
	
	pcmdpriv->cmd_buf = pcmdpriv->cmd_allocated_buf  +  CMDBUFF_ALIGN_SZ - ( (SIZE_PTR)(pcmdpriv->cmd_allocated_buf) & (CMDBUFF_ALIGN_SZ-1));
		
	pcmdpriv->rsp_allocated_buf = rtw_zmalloc(MAX_RSPSZ + 4);
	
	if (pcmdpriv->rsp_allocated_buf == NULL){
		res= _FAIL;
		goto exit;
	}
	
	pcmdpriv->rsp_buf = pcmdpriv->rsp_allocated_buf  +  4 - ( (SIZE_PTR)(pcmdpriv->rsp_allocated_buf) & 3);

	pcmdpriv->cmd_issued_cnt = pcmdpriv->cmd_done_cnt = pcmdpriv->rsp_cnt = 0;

	_rtw_mutex_init(&pcmdpriv->sctx_mutex);
exit:
	
_func_exit_;	  

	return res;
	
}	

#ifdef CONFIG_C2H_WK
static void c2h_wk_callback(_workitem *work);
#endif
sint _rtw_init_evt_priv(struct evt_priv *pevtpriv)
{
	sint res=_SUCCESS;

_func_enter_;	

#ifdef CONFIG_H2CLBK
	_rtw_init_sema(&(pevtpriv->lbkevt_done), 0);
	pevtpriv->lbkevt_limit = 0;
	pevtpriv->lbkevt_num = 0;
	pevtpriv->cmdevt_parm = NULL;		
#endif		
	
	//allocate DMA-able/Non-Page memory for cmd_buf and rsp_buf
	ATOMIC_SET(&pevtpriv->event_seq, 0);
	pevtpriv->evt_done_cnt = 0;

#ifdef CONFIG_EVENT_THREAD_MODE

	_rtw_init_sema(&(pevtpriv->evt_notify), 0);
	_rtw_init_sema(&(pevtpriv->terminate_evtthread_sema), 0);

	pevtpriv->evt_allocated_buf = rtw_zmalloc(MAX_EVTSZ + 4);	
	if (pevtpriv->evt_allocated_buf == NULL){
		res= _FAIL;
		goto exit;
		}
	pevtpriv->evt_buf = pevtpriv->evt_allocated_buf  +  4 - ((unsigned int)(pevtpriv->evt_allocated_buf) & 3);
	
		
#if defined(CONFIG_SDIO_HCI) || defined(CONFIG_GSPI_HCI)
	pevtpriv->allocated_c2h_mem = rtw_zmalloc(C2H_MEM_SZ +4); 
	
	if (pevtpriv->allocated_c2h_mem == NULL){
		res= _FAIL;
		goto exit;
	}

	pevtpriv->c2h_mem = pevtpriv->allocated_c2h_mem +  4\
	- ( (u32)(pevtpriv->allocated_c2h_mem) & 3);
#ifdef PLATFORM_OS_XP
	pevtpriv->pc2h_mdl= IoAllocateMdl((u8 *)pevtpriv->c2h_mem, C2H_MEM_SZ , FALSE, FALSE, NULL);
	
	if(pevtpriv->pc2h_mdl == NULL){
		res= _FAIL;
		goto exit;
	}
	MmBuildMdlForNonPagedPool(pevtpriv->pc2h_mdl);
#endif
#endif //end of CONFIG_SDIO_HCI

	_rtw_init_queue(&(pevtpriv->evt_queue));

exit:	

#endif //end of CONFIG_EVENT_THREAD_MODE

#ifdef CONFIG_C2H_WK
	_init_workitem(&pevtpriv->c2h_wk, c2h_wk_callback, NULL);
	pevtpriv->c2h_wk_alive = _FALSE;
	pevtpriv->c2h_queue = rtw_cbuf_alloc(C2H_QUEUE_MAX_LEN+1);
#endif

_func_exit_;		 

	return res;
}

void _rtw_free_evt_priv (struct	evt_priv *pevtpriv)
{
_func_enter_;

	RT_TRACE(_module_rtl871x_cmd_c_,_drv_info_,("+_rtw_free_evt_priv \n"));

#ifdef CONFIG_EVENT_THREAD_MODE
	_rtw_free_sema(&(pevtpriv->evt_notify));
	_rtw_free_sema(&(pevtpriv->terminate_evtthread_sema));


	if (pevtpriv->evt_allocated_buf)
		rtw_mfree(pevtpriv->evt_allocated_buf, MAX_EVTSZ + 4);
#endif

#ifdef CONFIG_C2H_WK
	_cancel_workitem_sync(&pevtpriv->c2h_wk);
	while(pevtpriv->c2h_wk_alive)
		rtw_msleep_os(10);

	while (!rtw_cbuf_empty(pevtpriv->c2h_queue)) {
		void *c2h;
		if ((c2h = rtw_cbuf_pop(pevtpriv->c2h_queue)) != NULL
			&& c2h != (void *)pevtpriv) {
			rtw_mfree(c2h, 16);
		}
	}
	rtw_cbuf_free(pevtpriv->c2h_queue);
#endif

	RT_TRACE(_module_rtl871x_cmd_c_,_drv_info_,("-_rtw_free_evt_priv \n"));

_func_exit_;	  	

}

void _rtw_free_cmd_priv (struct	cmd_priv *pcmdpriv)
{
_func_enter_;

	if(pcmdpriv){
		_rtw_spinlock_free(&(pcmdpriv->cmd_queue.lock));
		_rtw_free_sema(&(pcmdpriv->cmd_queue_sema));
		//_rtw_free_sema(&(pcmdpriv->cmd_done_sema));
		_rtw_free_sema(&(pcmdpriv->terminate_cmdthread_sema));

		if (pcmdpriv->cmd_allocated_buf)
			rtw_mfree(pcmdpriv->cmd_allocated_buf, MAX_CMDSZ + CMDBUFF_ALIGN_SZ);
		
		if (pcmdpriv->rsp_allocated_buf)
			rtw_mfree(pcmdpriv->rsp_allocated_buf, MAX_RSPSZ + 4);

		_rtw_mutex_free(&pcmdpriv->sctx_mutex);
	}
_func_exit_;		
}

/*
Calling Context:

rtw_enqueue_cmd can only be called between kernel thread, 
since only spin_lock is used.

ISR/Call-Back functions can't call this sub-function.

*/
#ifdef DBG_CMD_QUEUE
extern u8 dump_cmd_id;
#endif

sint	_rtw_enqueue_cmd(_queue *queue, struct cmd_obj *obj)
{
	_irqL irqL;

_func_enter_;

	if (obj == NULL)
		goto exit;

	if(obj->cmdsz > MAX_CMDSZ ){
		DBG_871X("%s failed due to obj->cmdsz(%d) > MAX_CMDSZ(%d) \n",__FUNCTION__, obj->cmdsz,MAX_CMDSZ);
		goto exit;
	}
	//_enter_critical_bh(&queue->lock, &irqL);
	_enter_critical(&queue->lock, &irqL);	

	rtw_list_insert_tail(&obj->list, &queue->queue);

	#ifdef DBG_CMD_QUEUE
	if(dump_cmd_id){
		printk("%s===> cmdcode:0x%02x\n",__FUNCTION__,obj->cmdcode);
		if(obj->cmdcode == GEN_CMD_CODE(_Set_MLME_EVT)){
			if(obj->parmbuf){
				struct C2HEvent_Header *pc2h_evt_hdr = (struct C2HEvent_Header *)(obj->parmbuf);
				printk("pc2h_evt_hdr->ID:0x%02x(%d)\n",pc2h_evt_hdr->ID,pc2h_evt_hdr->ID);
			}
		}
		if(obj->cmdcode == GEN_CMD_CODE(_Set_Drv_Extra)){
			if(obj->parmbuf){
				struct drvextra_cmd_parm *pdrvextra_cmd_parm =(struct drvextra_cmd_parm*)(obj->parmbuf);
				printk("pdrvextra_cmd_parm->ec_id:0x%02x\n",pdrvextra_cmd_parm->ec_id);
			}
		}
	}	
	
	if (queue->queue.prev->next != &queue->queue)
	{
		DBG_871X("[%d] head %p, tail %p, tail->prev->next %p[tail], tail->next %p[head]\n", __LINE__,
            &queue->queue, queue->queue.prev, queue->queue.prev->prev->next, queue->queue.prev->next);
		
		DBG_871X("==========%s============\n",__FUNCTION__);
		DBG_871X("head:%p,obj_addr:%p\n",&queue->queue,obj);
		DBG_871X("padapter: %p\n",obj->padapter);
		DBG_871X("cmdcode: 0x%02x\n",obj->cmdcode);
		DBG_871X("res: %d\n",obj->res);
		DBG_871X("parmbuf: %p\n",obj->parmbuf);
		DBG_871X("cmdsz: %d\n",obj->cmdsz);
		DBG_871X("rsp: %p\n",obj->rsp);
		DBG_871X("rspsz: %d\n",obj->rspsz);
		DBG_871X("sctx: %p\n",obj->sctx);
		DBG_871X("list->next: %p\n",obj->list.next);
		DBG_871X("list->prev: %p\n",obj->list.prev);
	}
	#endif //DBG_CMD_QUEUE
	
	//_exit_critical_bh(&queue->lock, &irqL);	
	_exit_critical(&queue->lock, &irqL);

exit:	

_func_exit_;

	return _SUCCESS;
}

struct	cmd_obj	*_rtw_dequeue_cmd(_queue *queue)
{
	_irqL irqL;
	struct cmd_obj *obj;

_func_enter_;

	//_enter_critical_bh(&(queue->lock), &irqL);
	_enter_critical(&queue->lock, &irqL);
	
	#ifdef DBG_CMD_QUEUE
	if (queue->queue.prev->next != &queue->queue)
	{
   		 DBG_871X("[%d] head %p, tail %p, tail->prev->next %p[tail], tail->next %p[head]\n", __LINE__,
            &queue->queue, queue->queue.prev, queue->queue.prev->prev->next, queue->queue.prev->next);
	}
	#endif //DBG_CMD_QUEUE


	if (rtw_is_list_empty(&(queue->queue))){
		obj = NULL;
	}
	else
	{
		obj = LIST_CONTAINOR(get_next(&(queue->queue)), struct cmd_obj, list);

		#ifdef DBG_CMD_QUEUE
		if (queue->queue.prev->next != &queue->queue){
				DBG_871X("==========%s============\n",__FUNCTION__);
                          DBG_871X("head:%p,obj_addr:%p\n",&queue->queue,obj);
				DBG_871X("padapter: %p\n",obj->padapter);
				DBG_871X("cmdcode: 0x%02x\n",obj->cmdcode);
				DBG_871X("res: %d\n",obj->res);
				DBG_871X("parmbuf: %p\n",obj->parmbuf);
				DBG_871X("cmdsz: %d\n",obj->cmdsz);
				DBG_871X("rsp: %p\n",obj->rsp);
				DBG_871X("rspsz: %d\n",obj->rspsz);
				DBG_871X("sctx: %p\n",obj->sctx);                        	
				DBG_871X("list->next: %p\n",obj->list.next);
				DBG_871X("list->prev: %p\n",obj->list.prev);
		}
		
		if(dump_cmd_id){
			DBG_871X("%s===> cmdcode:0x%02x\n",__FUNCTION__,obj->cmdcode);
		 	if(obj->cmdcode == GEN_CMD_CODE(_Set_Drv_Extra)){
				if(obj->parmbuf){
                                struct drvextra_cmd_parm *pdrvextra_cmd_parm =(struct drvextra_cmd_parm*)(obj->parmbuf);
                                printk("pdrvextra_cmd_parm->ec_id:0x%02x\n",pdrvextra_cmd_parm->ec_id);
                        }
                	}

		}	
		#endif //DBG_CMD_QUEUE
		
		rtw_list_delete(&obj->list);
	}

	//_exit_critical_bh(&(queue->lock), &irqL);
	_exit_critical(&queue->lock, &irqL);

_func_exit_;	

	return obj;
}

u32	rtw_init_cmd_priv(struct cmd_priv *pcmdpriv)
{
	u32	res;
_func_enter_;	
	res = _rtw_init_cmd_priv (pcmdpriv);
_func_exit_;	
	return res;	
}

u32	rtw_init_evt_priv (struct	evt_priv *pevtpriv)
{
	int	res;
_func_enter_;		
	res = _rtw_init_evt_priv(pevtpriv);
_func_exit_;		
	return res;
}

void rtw_free_evt_priv (struct	evt_priv *pevtpriv)
{
_func_enter_;
	RT_TRACE(_module_rtl871x_cmd_c_,_drv_info_,("rtw_free_evt_priv\n"));
	_rtw_free_evt_priv(pevtpriv);
_func_exit_;		
}	

void rtw_free_cmd_priv (struct	cmd_priv *pcmdpriv)
{
_func_enter_;
	RT_TRACE(_module_rtl871x_cmd_c_,_drv_info_,("rtw_free_cmd_priv\n"));
	_rtw_free_cmd_priv(pcmdpriv);
_func_exit_;	
}	

int rtw_cmd_filter(struct cmd_priv *pcmdpriv, struct cmd_obj *cmd_obj);
int rtw_cmd_filter(struct cmd_priv *pcmdpriv, struct cmd_obj *cmd_obj)
{
	u8 bAllow = _FALSE; //set to _TRUE to allow enqueuing cmd when hw_init_completed is _FALSE
	
	#ifdef SUPPORT_HW_RFOFF_DETECTED
	//To decide allow or not
	if( (adapter_to_pwrctl(pcmdpriv->padapter)->bHWPwrPindetect)
		&&(!pcmdpriv->padapter->registrypriv.usbss_enable)
	)		
	{
		if(cmd_obj->cmdcode == GEN_CMD_CODE(_Set_Drv_Extra) ) 
		{
			struct drvextra_cmd_parm	*pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)cmd_obj->parmbuf; 
			if(pdrvextra_cmd_parm->ec_id == POWER_SAVING_CTRL_WK_CID)
			{	
				//DBG_871X("==>enqueue POWER_SAVING_CTRL_WK_CID\n");
				bAllow = _TRUE; 
			}
		}
	}
	#endif

#ifndef CONFIG_C2H_PACKET_EN
	/* C2H should be always allowed */
	if(cmd_obj->cmdcode == GEN_CMD_CODE(_Set_Drv_Extra)) {
		struct drvextra_cmd_parm *pdrvextra_cmd_parm = (struct drvextra_cmd_parm *)cmd_obj->parmbuf;
		if(pdrvextra_cmd_parm->ec_id == C2H_WK_CID) {
			bAllow = _TRUE;
		}
	}
#endif

	if(cmd_obj->cmdcode == GEN_CMD_CODE(_SetChannelPlan))
		bAllow = _TRUE;

	if( (pcmdpriv->padapter->hw_init_completed ==_FALSE && bAllow == _FALSE)
		|| ATOMIC_READ(&(pcmdpriv->cmdthd_running)) == _FALSE	//com_thread not running
	)
	{
		//DBG_871X("%s:%s: drop cmdcode:%u, hw_init_completed:%u, cmdthd_running:%u\n", caller_func, __FUNCTION__,
		//	cmd_obj->cmdcode,
		//	pcmdpriv->padapter->hw_init_completed,
		//	pcmdpriv->cmdthd_running
		//);

		return _FAIL;
	}	
	return _SUCCESS;
}



u32 rtw_enqueue_cmd(struct cmd_priv *pcmdpriv, struct cmd_obj *cmd_obj)
{
	int res = _FAIL;
	PADAPTER padapter = pcmdpriv->padapter;
	
_func_enter_;	
	
	if (cmd_obj == NULL) {
		goto exit;
	}

	cmd_obj->padapter = padapter;

#ifdef CONFIG_CONCURRENT_MODE
	//change pcmdpriv to primary's pcmdpriv
	if (padapter->adapter_type != PRIMARY_ADAPTER && padapter->pbuddy_adapter)
		pcmdpriv = &(padapter->pbuddy_adapter->cmdpriv);
#endif	

	if( _FAIL == (res=rtw_cmd_filter(pcmdpriv, cmd_obj)) ) {
		rtw_free_cmd_obj(cmd_obj);
		goto exit;
	}

	res = _rtw_enqueue_cmd(&pcmdpriv->cmd_queue, cmd_obj);

	if(res == _SUCCESS)
		_rtw_up_sema(&pcmdpriv->cmd_queue_sema);
	
exit:	
	
_func_exit_;

	return res;
}

struct	cmd_obj	*rtw_dequeue_cmd(struct cmd_priv *pcmdpriv)
{
	struct cmd_obj *cmd_obj;
	
_func_enter_;		

	cmd_obj = _rtw_dequeue_cmd(&pcmdpriv->cmd_queue);
		
_func_exit_;			
	return cmd_obj;
}

void rtw_cmd_clr_isr(struct	cmd_priv *pcmdpriv)
{
_func_enter_;
	pcmdpriv->cmd_done_cnt++;
	//_rtw_up_sema(&(pcmdpriv->cmd_done_sema));
_func_exit_;		
}

void rtw_free_cmd_obj(struct cmd_obj *pcmd)
{
	struct drvextra_cmd_parm *extra_parm = NULL;
_func_enter_;

	if(pcmd->parmbuf != NULL){
		if((pcmd->cmdcode!=_JoinBss_CMD_) &&(pcmd->cmdcode!= _CreateBss_CMD_))
		{
			//free parmbuf in cmd_obj
			rtw_mfree((unsigned char*)pcmd->parmbuf, pcmd->cmdsz);
		}	
	}
	if(pcmd->rsp!=NULL)
	{
		if(pcmd->rspsz!= 0)
		{
			//free rsp in cmd_obj
			rtw_mfree((unsigned char*)pcmd->rsp, pcmd->rspsz);
		}	
	}	

	//free cmd_obj
	rtw_mfree((unsigned char*)pcmd, sizeof(struct cmd_obj));
	
_func_exit_;		
}


void rtw_stop_cmd_thread(_adapter *adapter)
{
	if(adapter->cmdThread &&
		ATOMIC_READ(&(adapter->cmdpriv.cmdthd_running)) == _TRUE &&
		adapter->cmdpriv.stop_req == 0)
	{
		adapter->cmdpriv.stop_req = 1;
		_rtw_up_sema(&adapter->cmdpriv.cmd_queue_sema);
		_rtw_down_sema(&adapter->cmdpriv.terminate_cmdthread_sema);
	}
}

thread_return rtw_cmd_thread(thread_context context)
{
	u8 ret;
	struct cmd_obj *pcmd;
	u8 *pcmdbuf, *prspbuf;
	u32 cmd_start_time;
	u32 cmd_process_time;
	u8 (*cmd_hdl)(_adapter *padapter, u8* pbuf);
	void (*pcmd_callback)(_adapter *dev, struct cmd_obj *pcmd);
	PADAPTER padapter = (PADAPTER)context;
	struct cmd_priv *pcmdpriv = &(padapter->cmdpriv);
	struct drvextra_cmd_parm *extra_parm = NULL;
	_irqL irqL;
_func_enter_;

	thread_enter("RTW_CMD_THREAD");

	pcmdbuf = pcmdpriv->cmd_buf;
	prspbuf = pcmdpriv->rsp_buf;

	pcmdpriv->stop_req = 0;
	ATOMIC_SET(&(pcmdpriv->cmdthd_running), _TRUE);
	_rtw_up_sema(&pcmdpriv->terminate_cmdthread_sema);

	RT_TRACE(_module_rtl871x_cmd_c_,_drv_info_,("start r871x rtw_cmd_thread !!!!\n"));

	while(1)
	{
		if (_rtw_down_sema(&pcmdpriv->cmd_queue_sema) == _FAIL) {
			DBG_871X_LEVEL(_drv_always_, FUNC_ADPT_FMT" _rtw_down_sema(&pcmdpriv->cmd_queue_sema) return _FAIL, break\n", FUNC_ADPT_ARG(padapter));
			break;
		}

		if ((padapter->bDriverStopped == _TRUE)||(padapter->bSurpriseRemoved == _TRUE))
		{
			DBG_871X_LEVEL(_drv_always_, "%s: DriverStopped(%d) SurpriseRemoved(%d) break at line %d\n",
				__FUNCTION__, padapter->bDriverStopped, padapter->bSurpriseRemoved, __LINE__);
			break;
		}

		if (pcmdpriv->stop_req) {
			DBG_871X_LEVEL(_drv_always_, FUNC_ADPT_FMT" stop_req:%u, break\n", FUNC_ADPT_ARG(padapter), pcmdpriv->stop_req);
			break;
		}
		
		_enter_critical(&pcmdpriv->cmd_queue.lock, &irqL);
		if(rtw_is_list_empty(&(pcmdpriv->cmd_queue.queue)))
		{
			//DBG_871X("%s: cmd queue is empty!\n", __func__);
			_exit_critical(&pcmdpriv->cmd_queue.lock, &irqL);
			continue;
		}
		_exit_critical(&pcmdpriv->cmd_queue.lock, &irqL);

#ifdef CONFIG_LPS_LCLK
		if (rtw_register_cmd_alive(padapter) != _SUCCESS)
		{
			RT_TRACE(_module_hal_xmit_c_, _drv_notice_,
					 ("%s: wait to leave LPS_LCLK\n", __FUNCTION__));
			continue;
		}
#endif

_next:
		if ((padapter->bDriverStopped == _TRUE)||(padapter->bSurpriseRemoved== _TRUE))
		{
			DBG_871X_LEVEL(_drv_always_, "%s: DriverStopped(%d) SurpriseRemoved(%d) break at line %d\n",
				__FUNCTION__, padapter->bDriverStopped, padapter->bSurpriseRemoved, __LINE__);
			break;
		}

		if(!(pcmd = rtw_dequeue_cmd(pcmdpriv))) {
#ifdef CONFIG_LPS_LCLK
			rtw_unregister_cmd_alive(padapter);
#endif
			continue;
		}

		cmd_start_time = rtw_get_current_time();

		if( _FAIL == rtw_cmd_filter(pcmdpriv, pcmd) )
		{
			pcmd->res = H2C_DROPPED;
			if (pcmd->cmdcode == GEN_CMD_CODE(_Set_Drv_Extra)) {
				extra_parm = (struct drvextra_cmd_parm *)pcmd->parmbuf;
				if (extra_parm && extra_parm->pbuf && extra_parm->size > 0)
					rtw_mfree(extra_parm->pbuf, extra_parm->size);
			}
			goto post_process;
		}

		pcmdpriv->cmd_issued_cnt++;

		pcmd->cmdsz = _RND4((pcmd->cmdsz));//_RND4

		if(pcmd->cmdsz > MAX_CMDSZ ){
			DBG_871X("%s cmdsz:%d > MAX_CMDSZ:%d\n",__FUNCTION__,pcmd->cmdsz,MAX_CMDSZ);
		}

		_rtw_memcpy(pcmdbuf, pcmd->parmbuf, pcmd->cmdsz);

		if(pcmd->cmdcode < (sizeof(wlancmds) /sizeof(struct cmd_hdl)))
		{
			cmd_hdl = wlancmds[pcmd->cmdcode].h2cfuns;

			if (cmd_hdl)
			{
				ret = cmd_hdl(pcmd->padapter, pcmdbuf);
				pcmd->res = ret;
			}

			pcmdpriv->cmd_seq++;
		}
		else
		{
			pcmd->res = H2C_PARAMETERS_ERROR;
		}

		cmd_hdl = NULL;

post_process:

		_enter_critical_mutex(&(pcmd->padapter->cmdpriv.sctx_mutex), NULL);
		if (pcmd->sctx) {
			if (0)
				DBG_871X_LEVEL(_drv_always_, FUNC_ADPT_FMT" pcmd->sctx\n",
					FUNC_ADPT_ARG(pcmd->padapter));
			if (pcmd->res == H2C_SUCCESS)
				rtw_sctx_done(&pcmd->sctx);
			else
				rtw_sctx_done_err(&pcmd->sctx, RTW_SCTX_DONE_CMD_ERROR);
		}
		_exit_critical_mutex(&(pcmd->padapter->cmdpriv.sctx_mutex), NULL);


		if((cmd_process_time = rtw_get_passing_time_ms(cmd_start_time)) > 1000)
		{
			if (pcmd->cmdcode == GEN_CMD_CODE(_Set_Drv_Extra)) {
				struct drvextra_cmd_parm *drvextra_parm = (struct drvextra_cmd_parm *)pcmdbuf;
				DBG_871X(ADPT_FMT" cmd=%d,%d,%d process_time=%d > 1 sec\n",
					ADPT_ARG(pcmd->padapter), pcmd->cmdcode, drvextra_parm->ec_id, drvextra_parm->type, cmd_process_time);
				//rtw_warn_on(1);
			} else if(pcmd->cmdcode == GEN_CMD_CODE(_Set_MLME_EVT)){
				struct C2HEvent_Header *pc2h_evt_hdr = (struct C2HEvent_Header *)pcmdbuf;
				DBG_871X(ADPT_FMT" cmd=%d,%d, process_time=%d > 1 sec\n",
					ADPT_ARG(pcmd->padapter), pcmd->cmdcode, pc2h_evt_hdr->ID, cmd_process_time);
				//rtw_warn_on(1);
			} else {
				DBG_871X(ADPT_FMT" cmd=%d, process_time=%d > 1 sec\n",
					ADPT_ARG(pcmd->padapter), pcmd->cmdcode, cmd_process_time);
				//rtw_warn_on(1);
			}
		}

		//call callback function for post-processed
		if(pcmd->cmdcode < (sizeof(rtw_cmd_callback) /sizeof(struct _cmd_callback)))
		{
			pcmd_callback = rtw_cmd_callback[pcmd->cmdcode].callback;
			if(pcmd_callback == NULL)
			{
				RT_TRACE(_module_rtl871x_cmd_c_,_drv_info_,("mlme_cmd_hdl(): pcmd_callback=0x%p, cmdcode=0x%x\n", pcmd_callback, pcmd->cmdcode));
				rtw_free_cmd_obj(pcmd);
			}
			else
			{
				//todo: !!! fill rsp_buf to pcmd->rsp if (pcmd->rsp!=NULL)
				pcmd_callback(pcmd->padapter, pcmd);//need conider that free cmd_obj in rtw_cmd_callback
			}
		}
		else
		{
			RT_TRACE(_module_rtl871x_cmd_c_,_drv_err_,("%s: cmdcode=0x%x callback not defined!\n", __FUNCTION__, pcmd->cmdcode));
			rtw_free_cmd_obj(pcmd);
		}

		flush_signals_thread();

		goto _next;

	}

	// free all cmd_obj resources
	do{
		pcmd = rtw_dequeue_cmd(pcmdpriv);
		if(pcmd==NULL){
#ifdef CONFIG_LPS_LCLK
			rtw_unregister_cmd_alive(padapter);
#endif
			break;
		}
		//DBG_871X("%s: leaving... drop cmdcode:%u size:%d\n", __FUNCTION__, pcmd->cmdcode, pcmd->cmdsz);

		if (pcmd->cmdcode == GEN_CMD_CODE(_Set_Drv_Extra)) {
			extra_parm = (struct drvextra_cmd_parm *)pcmd->parmbuf;
			if(extra_parm->pbuf && extra_parm->size > 0) {
				rtw_mfree(extra_parm->pbuf, extra_parm->size);
			}
		}

		rtw_free_cmd_obj(pcmd);	
	}while(1);

	_rtw_up_sema(&pcmdpriv->terminate_cmdthread_sema);
	ATOMIC_SET(&(pcmdpriv->cmdthd_running), _FALSE);

_func_exit_;

	thread_exit();

}


#ifdef CONFIG_EVENT_THREAD_MODE
u32 rtw_enqueue_evt(struct evt_priv *pevtpriv, struct evt_obj *obj)
{
	_irqL irqL;
	int	res;
	_queue *queue = &pevtpriv->evt_queue;
	
_func_enter_;	

	res = _SUCCESS; 		

	if (obj == NULL) {
		res = _FAIL;
		goto exit;
	}	

	_enter_critical_bh(&queue->lock, &irqL);

	rtw_list_insert_tail(&obj->list, &queue->queue);
	
	_exit_critical_bh(&queue->lock, &irqL);

	//rtw_evt_notify_isr(pevtpriv);

exit:
	
_func_exit_;		

	return res;	
}

struct evt_obj *rtw_dequeue_evt(_queue *queue)
{
	_irqL irqL;
	struct	evt_obj	*pevtobj;
	
_func_enter_;		

	_enter_critical_bh(&queue->lock, &irqL);

	if (rtw_is_list_empty(&(queue->queue)))
		pevtobj = NULL;
	else
	{
		pevtobj = LIST_CONTAINOR(get_next(&(queue->queue)), struct evt_obj, list);
		rtw_list_delete(&pevtobj->list);
	}

	_exit_critical_bh(&queue->lock, &irqL);
	
_func_exit_;			

	return pevtobj;	
}

void rtw_free_evt_obj(struct evt_obj *pevtobj)
{
_func_enter_;

	if(pevtobj->parmbuf)
		rtw_mfree((unsigned char*)pevtobj->parmbuf, pevtobj->evtsz);
	
	rtw_mfree((unsigned char*)pevtobj, sizeof(struct evt_obj));
	
_func_exit_;		
}

void rtw_evt_notify_isr(struct evt_priv *pevtpriv)
{
_func_enter_;
	pevtpriv->evt_done_cnt++;
	_rtw_up_sema(&(pevtpriv->evt_notify));
_func_exit_;	
}
#endif


/*
u8 rtw_setstandby_cmd(unsigned char  *adapter) 
*/
u8 rtw_setstandby_cmd(_adapter *padapter, uint action)
{
	struct cmd_obj*			ph2c;
	struct usb_suspend_parm*	psetusbsuspend;
	struct cmd_priv 			*pcmdpriv=&padapter->cmdpriv;

	u8 ret = _SUCCESS;
	
_func_enter_;	

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if (ph2c == NULL) {
		ret = _FAIL;
		goto exit;
	}
	
	psetusbsuspend = (struct usb_suspend_parm*)rtw_zmalloc(sizeof(struct usb_suspend_parm)); 
	if (psetusbsuspend == NULL) {
		rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
		ret = _FAIL;
		goto exit;
	}

	psetusbsuspend->action = action;

	init_h2fwcmd_w_parm_no_rsp(ph2c, psetusbsuspend, GEN_CMD_CODE(_SetUsbSuspend));

	ret = rtw_enqueue_cmd(pcmdpriv, ph2c);	
	
exit:	
	
_func_exit_;		

	return ret;
}

/*
rtw_sitesurvey_cmd(~)
	### NOTE:#### (!!!!)
	MUST TAKE CARE THAT BEFORE CALLING THIS FUNC, YOU SHOULD HAVE LOCKED pmlmepriv->lock
*/
u8 rtw_sitesurvey_cmd(_adapter  *padapter, NDIS_802_11_SSID *ssid, int ssid_num,
	struct rtw_ieee80211_channel *ch, int ch_num)
{
	u8 res = _FAIL;
	struct cmd_obj		*ph2c;
	struct sitesurvey_parm	*psurveyPara;
	struct cmd_priv 	*pcmdpriv = &padapter->cmdpriv;
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;
#ifdef CONFIG_P2P
	struct wifidirect_info *pwdinfo= &(padapter->wdinfo);
#endif //CONFIG_P2P

_func_enter_;

#ifdef CONFIG_LPS
	if(check_fwstate(pmlmepriv, _FW_LINKED) == _TRUE){
		rtw_lps_ctrl_wk_cmd(padapter, LPS_CTRL_SCAN, 1);
	}
#endif

#ifdef CONFIG_P2P_PS
	if (check_fwstate(pmlmepriv, _FW_LINKED) == _TRUE) {
		p2p_ps_wk_cmd(padapter, P2P_PS_SCAN, 1);
	}
#endif //CONFIG_P2P_PS

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if (ph2c == NULL)
		return _FAIL;

	psurveyPara = (struct sitesurvey_parm*)rtw_zmalloc(sizeof(struct sitesurvey_parm)); 
	if (psurveyPara == NULL) {
		rtw_mfree((unsigned char*) ph2c, sizeof(struct cmd_obj));
		return _FAIL;
	}

	rtw_free_network_queue(padapter, _FALSE);

	RT_TRACE(_module_rtl871x_cmd_c_, _drv_info_, ("%s: flush network queue\n", __FUNCTION__));

	init_h2fwcmd_w_parm_no_rsp(ph2c, psurveyPara, GEN_CMD_CODE(_SiteSurvey));

	/* psurveyPara->bsslimit = 48; */
	psurveyPara->scan_mode = pmlmepriv->scan_mode;

	/* prepare ssid list */
	if (ssid) {
		int i;
		for (i=0; i<ssid_num && i< RTW_SSID_SCAN_AMOUNT; i++) {
			if (ssid[i].SsidLength) {
				_rtw_memcpy(&psurveyPara->ssid[i], &ssid[i], sizeof(NDIS_802_11_SSID));
				psurveyPara->ssid_num++;
				if (0)
					DBG_871X(FUNC_ADPT_FMT" ssid:(%s, %d)\n", FUNC_ADPT_ARG(padapter),
						psurveyPara->ssid[i].Ssid, psurveyPara->ssid[i].SsidLength);
			}
		}
	}

	/* prepare channel list */
	if (ch) {
		int i;
		for (i=0; i<ch_num && i< RTW_CHANNEL_SCAN_AMOUNT; i++) {
			if (ch[i].hw_value && !(ch[i].flags & RTW_IEEE80211_CHAN_DISABLED)) {
				_rtw_memcpy(&psurveyPara->ch[i], &ch[i], sizeof(struct rtw_ieee80211_channel));
				psurveyPara->ch_num++;
				if (0)
					DBG_871X(FUNC_ADPT_FMT" ch:%u\n", FUNC_ADPT_ARG(padapter),
						psurveyPara->ch[i].hw_value);
			}
		}
	}

	set_fwstate(pmlmepriv, _FW_UNDER_SURVEY);

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

	if(res == _SUCCESS) {

		pmlmepriv->scan_start_time = rtw_get_current_time();

#ifdef CONFIG_STA_MODE_SCAN_UNDER_AP_MODE
		if((padapter->pbuddy_adapter->mlmeextpriv.mlmext_info.state&0x03) == WIFI_FW_AP_STATE)
		{
			#if 0
			if(IsSupported5G(padapter->registrypriv.wireless_mode) 
				&& IsSupported24G(padapter->registrypriv.wireless_mode)) //dual band
				mlme_set_scan_to_timer(pmlmepriv, CONC_SCANNING_TIMEOUT_DUAL_BAND);
			else //single band
				mlme_set_scan_to_timer(pmlmepriv, CONC_SCANNING_TIMEOUT_SINGLE_BAND);
			#endif
			u16 scan_timeout;
			scan_timeout = SURVEY_TO*RTW_CHANNEL_SCAN_AMOUNT+(RTW_CHANNEL_SCAN_AMOUNT/RTW_SCAN_NUM_OF_CH)*SURVEY_TO*RTW_STAY_AP_CH_MILLISECOND+3000;
			mlme_set_scan_to_timer(pmlmepriv, scan_timeout);
		}		
		else
#endif //CONFIG_STA_MODE_SCAN_UNDER_AP_MODE
			mlme_set_scan_to_timer(pmlmepriv, SCANNING_TIMEOUT);

		rtw_led_control(padapter, LED_CTL_SITE_SURVEY);
	} else {
		_clr_fwstate_(pmlmepriv, _FW_UNDER_SURVEY);
	}

_func_exit_;		

	return res;
}

u8 rtw_setdatarate_cmd(_adapter *padapter, u8 *rateset)
{
	struct cmd_obj*			ph2c;
	struct setdatarate_parm*	pbsetdataratepara;
	struct cmd_priv*		pcmdpriv = &padapter->cmdpriv;
	u8	res = _SUCCESS;

_func_enter_;	

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if (ph2c == NULL) {
		res = _FAIL;
		goto exit;
	}

	pbsetdataratepara = (struct setdatarate_parm*)rtw_zmalloc(sizeof(struct setdatarate_parm)); 
	if (pbsetdataratepara == NULL) {
		rtw_mfree((u8 *) ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	init_h2fwcmd_w_parm_no_rsp(ph2c, pbsetdataratepara, GEN_CMD_CODE(_SetDataRate));
#ifdef MP_FIRMWARE_OFFLOAD
	pbsetdataratepara->curr_rateidx = *(u32*)rateset;
//	_rtw_memcpy(pbsetdataratepara, rateset, sizeof(u32));
#else
	pbsetdataratepara->mac_id = 5;
	_rtw_memcpy(pbsetdataratepara->datarates, rateset, NumRates);
#endif
	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
exit:

_func_exit_;

	return res;
}

u8 rtw_setbasicrate_cmd(_adapter *padapter, u8 *rateset)
{
	struct cmd_obj*			ph2c;
	struct setbasicrate_parm*	pssetbasicratepara;
	struct cmd_priv*		pcmdpriv=&padapter->cmdpriv;
	u8	res = _SUCCESS;

_func_enter_;

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if (ph2c == NULL) {
		res= _FAIL;
		goto exit;
	}
	pssetbasicratepara = (struct setbasicrate_parm*)rtw_zmalloc(sizeof(struct setbasicrate_parm)); 

	if (pssetbasicratepara == NULL) {
		rtw_mfree((u8*) ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	init_h2fwcmd_w_parm_no_rsp(ph2c, pssetbasicratepara, _SetBasicRate_CMD_);

	_rtw_memcpy(pssetbasicratepara->basicrates, rateset, NumRates);	   

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);	
exit:	

_func_exit_;		

	return res;
}


/*
unsigned char rtw_setphy_cmd(unsigned char  *adapter) 

1.  be called only after rtw_update_registrypriv_dev_network( ~) or mp testing program
2.  for AdHoc/Ap mode or mp mode?

*/
u8 rtw_setphy_cmd(_adapter *padapter, u8 modem, u8 ch)
{
	struct cmd_obj*			ph2c;
	struct setphy_parm*		psetphypara;
	struct cmd_priv 			*pcmdpriv=&padapter->cmdpriv;
//	struct mlme_priv			*pmlmepriv = &padapter->mlmepriv;
//	struct registry_priv*		pregistry_priv = &padapter->registrypriv;
	u8	res=_SUCCESS;

_func_enter_;	

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if(ph2c==NULL){
		res= _FAIL;
		goto exit;
		}
	psetphypara = (struct setphy_parm*)rtw_zmalloc(sizeof(struct setphy_parm)); 

	if(psetphypara==NULL){
		rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
		res= _FAIL;
		goto exit;
	}

	init_h2fwcmd_w_parm_no_rsp(ph2c, psetphypara, _SetPhy_CMD_);

	RT_TRACE(_module_rtl871x_cmd_c_,_drv_info_,("CH=%d, modem=%d", ch, modem));

	psetphypara->modem = modem;
	psetphypara->rfchannel = ch;

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);	
exit:	
_func_exit_;		
	return res;
}

u8 rtw_setbbreg_cmd(_adapter*padapter, u8 offset, u8 val)
{	
	struct cmd_obj*			ph2c;
	struct writeBB_parm*		pwritebbparm;
	struct cmd_priv 			*pcmdpriv=&padapter->cmdpriv;	
	u8	res=_SUCCESS;
_func_enter_;
	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if(ph2c==NULL){
		res= _FAIL;
		goto exit;
		}
	pwritebbparm = (struct writeBB_parm*)rtw_zmalloc(sizeof(struct writeBB_parm)); 

	if(pwritebbparm==NULL){
		rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
		res= _FAIL;
		goto exit;
	}

	init_h2fwcmd_w_parm_no_rsp(ph2c, pwritebbparm, GEN_CMD_CODE(_SetBBReg));	

	pwritebbparm->offset = offset;
	pwritebbparm->value = val;

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);	
exit:	
_func_exit_;	
	return res;
}

u8 rtw_getbbreg_cmd(_adapter  *padapter, u8 offset, u8 *pval)
{	
	struct cmd_obj*			ph2c;
	struct readBB_parm*		prdbbparm;
	struct cmd_priv 			*pcmdpriv=&padapter->cmdpriv;
	u8	res=_SUCCESS;
	
_func_enter_;
	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if(ph2c==NULL){
		res=_FAIL;
		goto exit;
		}
	prdbbparm = (struct readBB_parm*)rtw_zmalloc(sizeof(struct readBB_parm)); 

	if(prdbbparm ==NULL){
		rtw_mfree((unsigned char *) ph2c, sizeof(struct	cmd_obj));
		return _FAIL;
	}

	_rtw_init_listhead(&ph2c->list);
	ph2c->cmdcode =GEN_CMD_CODE(_GetBBReg);
	ph2c->parmbuf = (unsigned char *)prdbbparm;
	ph2c->cmdsz =  sizeof(struct readBB_parm);
	ph2c->rsp = pval;
	ph2c->rspsz = sizeof(struct readBB_rsp);
	
	prdbbparm ->offset = offset;
	
	res = rtw_enqueue_cmd(pcmdpriv, ph2c);	
exit:
_func_exit_;	
	return res;
}

u8 rtw_setrfreg_cmd(_adapter  *padapter, u8 offset, u32 val)
{	
	struct cmd_obj*			ph2c;
	struct writeRF_parm*		pwriterfparm;
	struct cmd_priv 			*pcmdpriv=&padapter->cmdpriv;	
	u8	res=_SUCCESS;
_func_enter_;
	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if(ph2c==NULL){
		res= _FAIL;	
		goto exit;
	}
	pwriterfparm = (struct writeRF_parm*)rtw_zmalloc(sizeof(struct writeRF_parm)); 

	if(pwriterfparm==NULL){
		rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
		res= _FAIL;
		goto exit;
	}

	init_h2fwcmd_w_parm_no_rsp(ph2c, pwriterfparm, GEN_CMD_CODE(_SetRFReg));	

	pwriterfparm->offset = offset;
	pwriterfparm->value = val;

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);	
exit:
_func_exit_;	
	return res;
}

u8 rtw_getrfreg_cmd(_adapter  *padapter, u8 offset, u8 *pval)
{	
	struct cmd_obj*			ph2c;
	struct readRF_parm*		prdrfparm;
	struct cmd_priv 			*pcmdpriv=&padapter->cmdpriv;	
	u8	res=_SUCCESS;

_func_enter_;

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if(ph2c==NULL){
		res= _FAIL;
		goto exit;
	}

	prdrfparm = (struct readRF_parm*)rtw_zmalloc(sizeof(struct readRF_parm)); 
	if(prdrfparm ==NULL){
		rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
		res= _FAIL;
		goto exit;
	}

	_rtw_init_listhead(&ph2c->list);
	ph2c->cmdcode =GEN_CMD_CODE(_GetRFReg);
	ph2c->parmbuf = (unsigned char *)prdrfparm;
	ph2c->cmdsz =  sizeof(struct readRF_parm);
	ph2c->rsp = pval;
	ph2c->rspsz = sizeof(struct readRF_rsp);
	
	prdrfparm ->offset = offset;
	
	res = rtw_enqueue_cmd(pcmdpriv, ph2c);	

exit:

_func_exit_;	

	return res;
}

void rtw_getbbrfreg_cmdrsp_callback(_adapter*	padapter,  struct cmd_obj *pcmd)
{       
 _func_enter_;  
		
	//rtw_free_cmd_obj(pcmd);
	rtw_mfree((unsigned char*) pcmd->parmbuf, pcmd->cmdsz);
	rtw_mfree((unsigned char*) pcmd, sizeof(struct cmd_obj));
	
#ifdef CONFIG_MP_INCLUDED
	if (padapter->registrypriv.mp_mode == 1)
		padapter->mppriv.workparam.bcompleted= _TRUE;
#endif	
_func_exit_;		
}

void rtw_readtssi_cmdrsp_callback(_adapter*	padapter,  struct cmd_obj *pcmd)
{
 _func_enter_;  

	rtw_mfree((unsigned char*) pcmd->parmbuf, pcmd->cmdsz);
	rtw_mfree((unsigned char*) pcmd, sizeof(struct cmd_obj));
	
#ifdef CONFIG_MP_INCLUDED
	if (padapter->registrypriv.mp_mode == 1)
		padapter->mppriv.workparam.bcompleted= _TRUE;
#endif

_func_exit_;
}

u8 rtw_createbss_cmd(_adapter  *padapter)
{
	struct cmd_obj*			pcmd;
	struct cmd_priv 			*pcmdpriv=&padapter->cmdpriv;
	struct mlme_priv			*pmlmepriv = &padapter->mlmepriv;
	WLAN_BSSID_EX		*pdev_network = &padapter->registrypriv.dev_network;
	u8	res=_SUCCESS;

_func_enter_;

	rtw_led_control(padapter, LED_CTL_START_TO_LINK);

	if (pmlmepriv->assoc_ssid.SsidLength == 0){
		RT_TRACE(_module_rtl871x_cmd_c_,_drv_info_,(" createbss for Any SSid:%s\n",pmlmepriv->assoc_ssid.Ssid));		
	} else {
		RT_TRACE(_module_rtl871x_cmd_c_,_drv_info_,(" createbss for SSid:%s\n", pmlmepriv->assoc_ssid.Ssid));
	}
		
	pcmd = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if(pcmd==NULL){
		res= _FAIL;
		goto exit;
	}

	_rtw_init_listhead(&pcmd->list);
	pcmd->cmdcode = _CreateBss_CMD_;
	pcmd->parmbuf = (unsigned char *)pdev_network;
	pcmd->cmdsz = get_WLAN_BSSID_EX_sz((WLAN_BSSID_EX*)pdev_network);
	pcmd->rsp = NULL;
	pcmd->rspsz = 0;	
	
	pdev_network->Length = pcmd->cmdsz;	

#ifdef CONFIG_RTL8712
	//notes: translate IELength & Length after assign the Length to cmdsz;
	pdev_network->Length = cpu_to_le32(pcmd->cmdsz);
	pdev_network->IELength = cpu_to_le32(pdev_network->IELength);
	pdev_network->Ssid.SsidLength = cpu_to_le32(pdev_network->Ssid.SsidLength);
#endif

	res = rtw_enqueue_cmd(pcmdpriv, pcmd);	

exit:

_func_exit_;	

	return res;
}

u8 rtw_createbss_cmd_ex(_adapter  *padapter, unsigned char *pbss, unsigned int sz)
{
	struct cmd_obj*	pcmd;
	struct cmd_priv 	*pcmdpriv=&padapter->cmdpriv;
	u8	res=_SUCCESS;
	
_func_enter_;
			
	pcmd = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if(pcmd==NULL){
		res= _FAIL;
		goto exit;
	}

	_rtw_init_listhead(&pcmd->list);
	pcmd->cmdcode = GEN_CMD_CODE(_CreateBss);
	pcmd->parmbuf = pbss;
	pcmd->cmdsz =  sz;
	pcmd->rsp = NULL;
	pcmd->rspsz = 0;

	res = rtw_enqueue_cmd(pcmdpriv, pcmd);

exit:
	
_func_exit_;	

	return res;	
}

u8 rtw_startbss_cmd(_adapter  *padapter, int flags)
{
	struct cmd_obj* pcmd;
	struct cmd_priv  *pcmdpriv=&padapter->cmdpriv;
	struct submit_ctx sctx;
	u8 res=_SUCCESS;

_func_enter_;

	if (flags & RTW_CMDF_DIRECTLY) {
		/* no need to enqueue, do the cmd hdl directly and free cmd parameter */
		start_bss_network(padapter, (u8*)&(padapter->mlmepriv.cur_network.network));
	} else {
		/* need enqueue, prepare cmd_obj and enqueue */
		pcmd = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
		if (pcmd == NULL) {
			res = _FAIL;
			goto exit;
		}

		_rtw_init_listhead(&pcmd->list);
		pcmd->cmdcode = GEN_CMD_CODE(_CreateBss);
		pcmd->parmbuf = NULL;
		pcmd->cmdsz =  0;
		pcmd->rsp = NULL;
		pcmd->rspsz = 0;

		if (flags & RTW_CMDF_WAIT_ACK) {
			pcmd->sctx = &sctx;
			rtw_sctx_init(&sctx, 2000);
		}

		res = rtw_enqueue_cmd(pcmdpriv, pcmd);

		if (res == _SUCCESS && (flags & RTW_CMDF_WAIT_ACK)) {
			rtw_sctx_wait(&sctx, __func__);
			_enter_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
			if (sctx.status == RTW_SCTX_SUBMITTED)
				pcmd->sctx = NULL;
			_exit_critical_mutex(&pcmdpriv->sctx_mutex, NULL);
		}
	}

exit:
	
_func_exit_;

	return res;
}

u8 rtw_joinbss_cmd(_adapter  *padapter, struct wlan_network* pnetwork)
{
	u8	*auth, res = _SUCCESS;
	uint	t_len = 0;
	WLAN_BSSID_EX		*psecnetwork;
	struct cmd_obj		*pcmd;
	struct cmd_priv		*pcmdpriv=&padapter->cmdpriv;
	struct mlme_priv		*pmlmepriv = &padapter->mlmepriv;
	struct qos_priv		*pqospriv= &pmlmepriv->qospriv;
	struct security_priv	*psecuritypriv=&padapter->securitypriv;
	struct registry_priv	*pregistrypriv = &padapter->registrypriv;
#ifdef CONFIG_80211N_HT
	struct ht_priv			*phtpriv = &pmlmepriv->htpriv;
#endif //CONFIG_80211N_HT
#ifdef CONFIG_80211AC_VHT
	struct vht_priv		*pvhtpriv = &pmlmepriv->vhtpriv;
#endif //CONFIG_80211AC_VHT
	NDIS_802_11_NETWORK_INFRASTRUCTURE ndis_network_mode = pnetwork->network.InfrastructureMode;
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	u32 tmp_len;
	u8 *ptmp=NULL;
_func_enter_;

	rtw_led_control(padapter, LED_CTL_START_TO_LINK);

	if (pmlmepriv->assoc_ssid.SsidLength == 0){
		RT_TRACE(_module_rtl871x_cmd_c_, _drv_info_, ("+Join cmd: Any SSid\n"));
	} else {
		RT_TRACE(_module_rtl871x_cmd_c_, _drv_notice_, ("+Join cmd: SSid=[%s]\n", pmlmepriv->assoc_ssid.Ssid));
	}

	pcmd = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if(pcmd==NULL){
		res=_FAIL;
		RT_TRACE(_module_rtl871x_cmd_c_, _drv_err_, ("rtw_joinbss_cmd: memory allocate for cmd_obj fail!!!\n"));
		goto exit;
	}
	/* // for IEs is pointer 
	t_len = sizeof (ULONG) + sizeof (NDIS_802_11_MAC_ADDRESS) + 2 + 
			sizeof (NDIS_802_11_SSID) + sizeof (ULONG) + 
			sizeof (NDIS_802_11_RSSI) + sizeof (NDIS_802_11_NETWORK_TYPE) + 
			sizeof (NDIS_802_11_CONFIGURATION) +	
			sizeof (NDIS_802_11_NETWORK_INFRASTRUCTURE) +   
			sizeof (NDIS_802_11_RATES_EX)+ sizeof(WLAN_PHY_INFO)+ sizeof (ULONG) + MAX_IE_SZ;
	*/
	//for IEs is fix buf size
	t_len = sizeof(WLAN_BSSID_EX);


	//for hidden ap to set fw_state here
	if (check_fwstate(pmlmepriv, WIFI_STATION_STATE|WIFI_ADHOC_STATE) != _TRUE)
	{
		switch(ndis_network_mode)
		{
			case Ndis802_11IBSS:
				set_fwstate(pmlmepriv, WIFI_ADHOC_STATE);
				break;

			case Ndis802_11Infrastructure:
				set_fwstate(pmlmepriv, WIFI_STATION_STATE);
				break;

			case Ndis802_11APMode:
			case Ndis802_11AutoUnknown:
			case Ndis802_11InfrastructureMax:
			case Ndis802_11Monitor:
				break;

		}
	}

	pmlmeinfo->assoc_AP_vendor = check_assoc_AP(pnetwork->network.IEs, pnetwork->network.IELength);

	psecnetwork=(WLAN_BSSID_EX *)&psecuritypriv->sec_bss;
	if(psecnetwork==NULL)
	{
		if(pcmd !=NULL)
			rtw_mfree((unsigned char *)pcmd, sizeof(struct	cmd_obj));
		
		res=_FAIL;
		
		RT_TRACE(_module_rtl871x_cmd_c_, _drv_err_, ("rtw_joinbss_cmd :psecnetwork==NULL!!!\n"));
		
		goto exit;
	}

	_rtw_memset(psecnetwork, 0, t_len);

	_rtw_memcpy(psecnetwork, &pnetwork->network, get_WLAN_BSSID_EX_sz(&pnetwork->network));
	
	auth=&psecuritypriv->authenticator_ie[0];
	psecuritypriv->authenticator_ie[0]=(unsigned char)psecnetwork->IELength;

	if((psecnetwork->IELength-12) < (256-1)) {
		_rtw_memcpy(&psecuritypriv->authenticator_ie[1], &psecnetwork->IEs[12], psecnetwork->IELength-12);
	} else {
		_rtw_memcpy(&psecuritypriv->authenticator_ie[1], &psecnetwork->IEs[12], (256-1));
	}
	  
	psecnetwork->IELength = 0;
	// Added by Albert 2009/02/18
	// If the the driver wants to use the bssid to create the connection.
	// If not,  we have to copy the connecting AP's MAC address to it so that
	// the driver just has the bssid information for PMKIDList searching.
        
	if ( pmlmepriv->assoc_by_bssid == _FALSE )
	{
		_rtw_memcpy( &pmlmepriv->assoc_bssid[ 0 ], &pnetwork->network.MacAddress[ 0 ], ETH_ALEN );
	}

	psecnetwork->IELength = rtw_restruct_sec_ie(padapter, &pnetwork->network.IEs[0], &psecnetwork->IEs[0], pnetwork->network.IELength);


	pqospriv->qos_option = 0;
	
	if(pregistrypriv->wmm_enable)	
	{
		tmp_len = rtw_restruct_wmm_ie(padapter, &pnetwork->network.IEs[0], &psecnetwork->IEs[0], pnetwork->network.IELength, psecnetwork->IELength);	

		if (psecnetwork->IELength != tmp_len)		
		{
			psecnetwork->IELength = tmp_len;
			pqospriv->qos_option = 1; //There is WMM IE in this corresp. beacon
		}
		else 
		{
			pqospriv->qos_option = 0;//There is no WMM IE in this corresp. beacon
		}		
	}	

#ifdef CONFIG_80211N_HT
	phtpriv->ht_option = _FALSE;
	ptmp = rtw_get_ie(&pnetwork->network.IEs[12], _HT_CAPABILITY_IE_, &tmp_len, pnetwork->network.IELength-12);
	if(pregistrypriv->ht_enable && ptmp && tmp_len>0)
	{
		//	Added by Albert 2010/06/23
		//	For the WEP mode, we will use the bg mode to do the connection to avoid some IOT issue.
		//	Especially for Realtek 8192u SoftAP.
		if (	( padapter->securitypriv.dot11PrivacyAlgrthm != _WEP40_ ) &&
			( padapter->securitypriv.dot11PrivacyAlgrthm != _WEP104_ ) &&
			( padapter->securitypriv.dot11PrivacyAlgrthm != _TKIP_ ))
		{
			rtw_ht_use_default_setting(padapter);

			rtw_build_wmm_ie_ht(padapter, &psecnetwork->IEs[0], &psecnetwork->IELength);

			//rtw_restructure_ht_ie
			rtw_restructure_ht_ie(padapter, &pnetwork->network.IEs[12], &psecnetwork->IEs[0], 
									pnetwork->network.IELength-12, &psecnetwork->IELength,
									pnetwork->network.Configuration.DSConfig);
		}
	}

#ifdef CONFIG_80211AC_VHT
	pvhtpriv->vht_option = _FALSE;
	if (phtpriv->ht_option && pregistrypriv->vht_enable) {
		rtw_restructure_vht_ie(padapter, &pnetwork->network.IEs[0], &psecnetwork->IEs[0], 
								pnetwork->network.IELength, &psecnetwork->IELength);
	}
#endif

	rtw_append_exented_cap(padapter, &psecnetwork->IEs[0], &psecnetwork->IELength);

#endif //CONFIG_80211N_HT

	#if 0
	psecuritypriv->supplicant_ie[0]=(u8)psecnetwork->IELength;

	if(psecnetwork->IELength < (256-1))
	{
		_rtw_memcpy(&psecuritypriv->supplicant_ie[1], &psecnetwork->IEs[0], psecnetwork->IELength);
	}
	else
	{
		_rtw_memcpy(&psecuritypriv->supplicant_ie[1], &psecnetwork->IEs[0], (256-1));
	}
	#endif
	
	pcmd->cmdsz = get_WLAN_BSSID_EX_sz(psecnetwork);//get cmdsz before endian conversion

#ifdef CONFIG_RTL8712
	//wlan_network endian conversion	
	psecnetwork->Length = cpu_to_le32(psecnetwork->Length);
	psecnetwork->Ssid.SsidLength= cpu_to_le32(psecnetwork->Ssid.SsidLength);
	psecnetwork->Privacy = cpu_to_le32(psecnetwork->Privacy);
	psecnetwork->Rssi = cpu_to_le32(psecnetwork->Rssi);
	psecnetwork->NetworkTypeInUse = cpu_to_le32(psecnetwork->NetworkTypeInUse);
	psecnetwork->Configuration.ATIMWindow = cpu_to_le32(psecnetwork->Configuration.ATIMWindow);
	psecnetwork->Configuration.BeaconPeriod = cpu_to_le32(psecnetwork->Configuration.BeaconPeriod);
	psecnetwork->Configuration.DSConfig = cpu_to_le32(psecnetwork->Configuration.DSConfig);
	psecnetwork->Configuration.FHConfig.DwellTime=cpu_to_le32(psecnetwork->Configuration.FHConfig.DwellTime);
	psecnetwork->Configuration.FHConfig.HopPattern=cpu_to_le32(psecnetwork->Configuration.FHConfig.HopPattern);
	psecnetwork->Configuration.FHConfig.HopSet=cpu_to_le32(psecnetwork->Configuration.FHConfig.HopSet);
	psecnetwork->Configuration.FHConfig.Length=cpu_to_le32(psecnetwork->Configuration.FHConfig.Length);	
	psecnetwork->Configuration.Length = cpu_to_le32(psecnetwork->Configuration.Length);
	psecnetwork->InfrastructureMode = cpu_to_le32(psecnetwork->InfrastructureMode);
	psecnetwork->IELength = cpu_to_le32(psecnetwork->IELength);      
#endif

	_rtw_init_listhead(&pcmd->list);
	pcmd->cmdcode = _JoinBss_CMD_;//GEN_CMD_CODE(_JoinBss)
	pcmd->parmbuf = (unsigned char *)psecnetwork;
	pcmd->rsp = NULL;
	pcmd->rspsz = 0;

	res = rtw_enqueue_cmd(pcmdpriv, pcmd);

exit:
	
_func_exit_;

	return res;
}

u8 rtw_disassoc_cmd(_adapter*padapter, u32 deauth_timeout_ms, bool enqueue) /* for sta_mode */
{
	struct cmd_obj *cmdobj = NULL;
	struct disconnect_parm *param = NULL;
	struct cmd_priv *cmdpriv = &padapter->cmdpriv;
	u8 res = _SUCCESS;

_func_enter_;

	RT_TRACE(_module_rtl871x_cmd_c_, _drv_notice_, ("+rtw_disassoc_cmd\n"));

	/* prepare cmd parameter */
	param = (struct disconnect_parm *)rtw_zmalloc(sizeof(*param));
	if (param == NULL) {
		res = _FAIL;
		goto exit;
	}
	param->deauth_timeout_ms = deauth_timeout_ms;

	if (enqueue) {
		/* need enqueue, prepare cmd_obj and enqueue */
		cmdobj = (struct cmd_obj *)rtw_zmalloc(sizeof(*cmdobj));
		if (cmdobj == NULL) {
			res = _FAIL;
			rtw_mfree((u8 *)param, sizeof(*param));
			goto exit;
		}
		init_h2fwcmd_w_parm_no_rsp(cmdobj, param, _DisConnect_CMD_);
		res = rtw_enqueue_cmd(cmdpriv, cmdobj);
	} else {
		/* no need to enqueue, do the cmd hdl directly and free cmd parameter */
		if (H2C_SUCCESS != disconnect_hdl(padapter, (u8 *)param))
			res = _FAIL;
		rtw_mfree((u8 *)param, sizeof(*param));
	}

exit:

_func_exit_;	

	return res;
}

u8 rtw_setopmode_cmd(_adapter  *padapter, NDIS_802_11_NETWORK_INFRASTRUCTURE networktype, bool enqueue)
{
	struct	cmd_obj*	ph2c;
	struct	setopmode_parm* psetop;

	struct	cmd_priv   *pcmdpriv= &padapter->cmdpriv;
	u8	res=_SUCCESS;

_func_enter_;
	psetop = (struct setopmode_parm*)rtw_zmalloc(sizeof(struct setopmode_parm)); 

	if(psetop==NULL){		
		res=_FAIL;
		goto exit;
	}
	psetop->mode = (u8)networktype;
	
	if(enqueue){
		ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));			
		if(ph2c==NULL){		
			rtw_mfree((u8 *)psetop, sizeof(*psetop));
			res= _FAIL;
			goto exit;
		}	

		init_h2fwcmd_w_parm_no_rsp(ph2c, psetop, _SetOpMode_CMD_);
		res = rtw_enqueue_cmd(pcmdpriv, ph2c);
	}
	else{
		setopmode_hdl(padapter, (u8 *)psetop);
		rtw_mfree((u8 *)psetop, sizeof(*psetop));
	}
exit:

_func_exit_;	

	return res;
}

u8 rtw_setstakey_cmd(_adapter *padapter, struct sta_info *sta, u8 key_type, bool enqueue)
{
	struct cmd_obj*			ph2c;
	struct set_stakey_parm	*psetstakey_para;
	struct cmd_priv 			*pcmdpriv=&padapter->cmdpriv;
	struct set_stakey_rsp		*psetstakey_rsp = NULL;
	
	struct mlme_priv			*pmlmepriv = &padapter->mlmepriv;
	struct security_priv 		*psecuritypriv = &padapter->securitypriv;
	u8	res=_SUCCESS;

_func_enter_;

	psetstakey_para = (struct set_stakey_parm*)rtw_zmalloc(sizeof(struct set_stakey_parm));
	if(psetstakey_para==NULL){	
		res=_FAIL;
		goto exit;
	}
		
	_rtw_memcpy(psetstakey_para->addr, sta->hwaddr,ETH_ALEN);
		
	if(check_fwstate(pmlmepriv, WIFI_STATION_STATE)){
			psetstakey_para->algorithm =(unsigned char) psecuritypriv->dot11PrivacyAlgrthm;
	}else{
		GET_ENCRY_ALGO(psecuritypriv, sta, psetstakey_para->algorithm, _FALSE);
	}

	if (key_type == GROUP_KEY) {
		_rtw_memcpy(&psetstakey_para->key, &psecuritypriv->dot118021XGrpKey[psecuritypriv->dot118021XGrpKeyid].skey, 16);
	}
	else if (key_type == UNICAST_KEY) {
		_rtw_memcpy(&psetstakey_para->key, &sta->dot118021x_UncstKey, 16);
	}
#ifdef CONFIG_TDLS
	else if(key_type == TDLS_KEY){
			_rtw_memcpy(&psetstakey_para->key, sta->tpk.tk, 16);
		psetstakey_para->algorithm=(u8)sta->dot118021XPrivacy;
       }
#endif /* CONFIG_TDLS */

	//jeff: set this becasue at least sw key is ready
	padapter->securitypriv.busetkipkey=_TRUE;

	if(enqueue)
	{
		ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
		if ( ph2c == NULL){
			rtw_mfree((u8 *) psetstakey_para, sizeof(struct set_stakey_parm));
			res= _FAIL;
			goto exit;
		}	

		psetstakey_rsp = (struct set_stakey_rsp*)rtw_zmalloc(sizeof(struct set_stakey_rsp)); 
		if(psetstakey_rsp == NULL){
			rtw_mfree((u8 *) ph2c, sizeof(struct cmd_obj));
			rtw_mfree((u8 *) psetstakey_para, sizeof(struct set_stakey_parm));
			res=_FAIL;
			goto exit;
		}

		init_h2fwcmd_w_parm_no_rsp(ph2c, psetstakey_para, _SetStaKey_CMD_);
		ph2c->rsp = (u8 *) psetstakey_rsp;
		ph2c->rspsz = sizeof(struct set_stakey_rsp);
		res = rtw_enqueue_cmd(pcmdpriv, ph2c);	
	}
	else{
		set_stakey_hdl(padapter, (u8 *)psetstakey_para);
		rtw_mfree((u8 *) psetstakey_para, sizeof(struct set_stakey_parm));
	}
exit:

_func_exit_;	

	return res;
}

u8 rtw_clearstakey_cmd(_adapter *padapter, struct sta_info *sta, u8 enqueue)
{
	struct cmd_obj*			ph2c;
	struct set_stakey_parm	*psetstakey_para;
	struct cmd_priv 			*pcmdpriv=&padapter->cmdpriv;
	struct set_stakey_rsp		*psetstakey_rsp = NULL;	
	struct mlme_priv			*pmlmepriv = &padapter->mlmepriv;
	struct security_priv 		*psecuritypriv = &padapter->securitypriv;
	s16 cam_id = 0;
	u8	res=_SUCCESS;

_func_enter_;

	if(!enqueue)
	{
		while((cam_id = rtw_camid_search(padapter, sta->hwaddr, -1)) >= 0) {
			DBG_871X_LEVEL(_drv_always_, "clear key for addr:"MAC_FMT", camid:%d\n", MAC_ARG(sta->hwaddr), cam_id);
			clear_cam_entry(padapter, cam_id);
			rtw_camid_free(padapter, cam_id);
		}
	}
	else
	{
		ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
		if ( ph2c == NULL){
			res= _FAIL;
			goto exit;
		}

		psetstakey_para = (struct set_stakey_parm*)rtw_zmalloc(sizeof(struct set_stakey_parm));
		if(psetstakey_para==NULL){
			rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
			res=_FAIL;
			goto exit;
		}

		psetstakey_rsp = (struct set_stakey_rsp*)rtw_zmalloc(sizeof(struct set_stakey_rsp)); 
		if(psetstakey_rsp == NULL){
			rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
			rtw_mfree((u8 *) psetstakey_para, sizeof(struct set_stakey_parm));
			res=_FAIL;
			goto exit;
		}

		init_h2fwcmd_w_parm_no_rsp(ph2c, psetstakey_para, _SetStaKey_CMD_);
		ph2c->rsp = (u8 *) psetstakey_rsp;
		ph2c->rspsz = sizeof(struct set_stakey_rsp);

		_rtw_memcpy(psetstakey_para->addr, sta->hwaddr, ETH_ALEN);

		psetstakey_para->algorithm = _NO_PRIVACY_;
	
		res = rtw_enqueue_cmd(pcmdpriv, ph2c);	
		
	}
	
exit:

_func_exit_;	

	return res;
}

u8 rtw_setrttbl_cmd(_adapter  *padapter, struct setratable_parm *prate_table)
{
	struct cmd_obj*			ph2c;
	struct setratable_parm *	psetrttblparm;	
	struct cmd_priv 			*pcmdpriv=&padapter->cmdpriv;
	u8	res=_SUCCESS;
_func_enter_;	

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if(ph2c==NULL){
		res= _FAIL;
		goto exit;
		}
	psetrttblparm = (struct setratable_parm*)rtw_zmalloc(sizeof(struct setratable_parm)); 

	if(psetrttblparm==NULL){
		rtw_mfree((unsigned char *) ph2c, sizeof(struct	cmd_obj));
		res= _FAIL;
		goto exit;
	}

	init_h2fwcmd_w_parm_no_rsp(ph2c, psetrttblparm, GEN_CMD_CODE(_SetRaTable));

	_rtw_memcpy(psetrttblparm,prate_table,sizeof(struct setratable_parm));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);	
exit:
_func_exit_;	
	return res;

}

u8 rtw_getrttbl_cmd(_adapter  *padapter, struct getratable_rsp *pval)
{
	struct cmd_obj*			ph2c;
	struct getratable_parm *	pgetrttblparm;	
	struct cmd_priv 			*pcmdpriv=&padapter->cmdpriv;
	u8	res=_SUCCESS;
_func_enter_;	

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if(ph2c==NULL){
		res= _FAIL;
		goto exit;
	}
	pgetrttblparm = (struct getratable_parm*)rtw_zmalloc(sizeof(struct getratable_parm)); 

	if(pgetrttblparm==NULL){
		rtw_mfree((unsigned char *) ph2c, sizeof(struct	cmd_obj));
		res= _FAIL;
		goto exit;
	}

//	init_h2fwcmd_w_parm_no_rsp(ph2c, psetrttblparm, GEN_CMD_CODE(_SetRaTable));

	_rtw_init_listhead(&ph2c->list);
	ph2c->cmdcode =GEN_CMD_CODE(_GetRaTable);
	ph2c->parmbuf = (unsigned char *)pgetrttblparm;
	ph2c->cmdsz =  sizeof(struct getratable_parm);
	ph2c->rsp = (u8*)pval;
	ph2c->rspsz = sizeof(struct getratable_rsp);
	
	pgetrttblparm ->rsvd = 0x0;
	
	res = rtw_enqueue_cmd(pcmdpriv, ph2c);	
exit:
_func_exit_;	
	return res;

}

u8 rtw_setassocsta_cmd(_adapter  *padapter, u8 *mac_addr)
{
	struct cmd_priv 		*pcmdpriv = &padapter->cmdpriv;
	struct cmd_obj*			ph2c;
	struct set_assocsta_parm	*psetassocsta_para;	
	struct set_stakey_rsp		*psetassocsta_rsp = NULL;

	u8	res=_SUCCESS;

_func_enter_;	

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if(ph2c==NULL){
		res= _FAIL;
		goto exit;
	}

	psetassocsta_para = (struct set_assocsta_parm*)rtw_zmalloc(sizeof(struct set_assocsta_parm));
	if(psetassocsta_para==NULL){
		rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
		res=_FAIL;
		goto exit;
	}

	psetassocsta_rsp = (struct set_stakey_rsp*)rtw_zmalloc(sizeof(struct set_assocsta_rsp)); 
	if(psetassocsta_rsp==NULL){
		rtw_mfree((u8 *) ph2c, sizeof(struct	cmd_obj));
		rtw_mfree((u8 *) psetassocsta_para, sizeof(struct set_assocsta_parm));
		return _FAIL;
	}

	init_h2fwcmd_w_parm_no_rsp(ph2c, psetassocsta_para, _SetAssocSta_CMD_);
	ph2c->rsp = (u8 *) psetassocsta_rsp;
	ph2c->rspsz = sizeof(struct set_assocsta_rsp);

	_rtw_memcpy(psetassocsta_para->addr, mac_addr,ETH_ALEN);
	
	res = rtw_enqueue_cmd(pcmdpriv, ph2c);	

exit:

_func_exit_;	

	return res;
 }

u8 rtw_addbareq_cmd(_adapter*padapter, u8 tid, u8 *addr)
{
	struct cmd_priv		*pcmdpriv = &padapter->cmdpriv;
	struct cmd_obj*		ph2c;
	struct addBaReq_parm	*paddbareq_parm;

	u8	res=_SUCCESS;
	
_func_enter_;	

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));	
	if(ph2c==NULL){
		res= _FAIL;
		goto exit;
	}
	
	paddbareq_parm = (struct addBaReq_parm*)rtw_zmalloc(sizeof(struct addBaReq_parm)); 
	if(paddbareq_parm==NULL){
		rtw_mfree((unsigned char *)ph2c, sizeof(struct	cmd_obj));
		res= _FAIL;
		goto exit;
	}

	paddbareq_parm->tid = tid;
	_rtw_memcpy(paddbareq_parm->addr, addr, ETH_ALEN);

	init_h2fwcmd_w_parm_no_rsp(ph2c, paddbareq_parm, GEN_CMD_CODE(_AddBAReq));

	//DBG_871X("rtw_addbareq_cmd, tid=%d\n", tid);

	//rtw_enqueue_cmd(pcmdpriv, ph2c);	
	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
	
exit:
	
_func_exit_;

	return res;
}
//add for CONFIG_IEEE80211W, none 11w can use it
u8 rtw_reset_securitypriv_cmd(_adapter*padapter)
{
	struct cmd_obj*		ph2c;
	struct drvextra_cmd_parm  *pdrvextra_cmd_parm;	
	struct cmd_priv	*pcmdpriv=&padapter->cmdpriv;
	u8	res=_SUCCESS;
	
_func_enter_;	

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));	
	if(ph2c==NULL){
		res= _FAIL;
		goto exit;
	}
	
	pdrvextra_cmd_parm = (struct drvextra_cmd_parm*)rtw_zmalloc(sizeof(struct drvextra_cmd_parm)); 
	if(pdrvextra_cmd_parm==NULL){
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res= _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = RESET_SECURITYPRIV;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	
	//rtw_enqueue_cmd(pcmdpriv, ph2c);	
	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
	
exit:
	
_func_exit_;

	return res;

}

u8 rtw_free_assoc_resources_cmd(_adapter*padapter)
{
	struct cmd_obj*		ph2c;
	struct drvextra_cmd_parm  *pdrvextra_cmd_parm;	
	struct cmd_priv	*pcmdpriv=&padapter->cmdpriv;
	u8	res=_SUCCESS;
	
_func_enter_;	

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));	
	if(ph2c==NULL){
		res= _FAIL;
		goto exit;
	}
	
	pdrvextra_cmd_parm = (struct drvextra_cmd_parm*)rtw_zmalloc(sizeof(struct drvextra_cmd_parm)); 
	if(pdrvextra_cmd_parm==NULL){
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res= _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = FREE_ASSOC_RESOURCES;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	
	//rtw_enqueue_cmd(pcmdpriv, ph2c);	
	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
	
exit:
	
_func_exit_;

	return res;

}

u8 rtw_dynamic_chk_wk_cmd(_adapter*padapter)
{
	struct cmd_obj*		ph2c;
	struct drvextra_cmd_parm  *pdrvextra_cmd_parm;	
	struct cmd_priv	*pcmdpriv=&padapter->cmdpriv;
	u8	res=_SUCCESS;
	
_func_enter_;	

	//only  primary padapter does this cmd
/*
#ifdef CONFIG_CONCURRENT_MODE
	if (padapter->adapter_type != PRIMARY_ADAPTER && padapter->pbuddy_adapter)
		pcmdpriv = &(padapter->pbuddy_adapter->cmdpriv);
#endif
*/

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));	
	if(ph2c==NULL){
		res= _FAIL;
		goto exit;
	}
	
	pdrvextra_cmd_parm = (struct drvextra_cmd_parm*)rtw_zmalloc(sizeof(struct drvextra_cmd_parm)); 
	if(pdrvextra_cmd_parm==NULL){
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res= _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = DYNAMIC_CHK_WK_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;
	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	
	//rtw_enqueue_cmd(pcmdpriv, ph2c);	
	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
	
exit:
	
_func_exit_;

	return res;

}

u8 rtw_set_ch_cmd(_adapter*padapter, u8 ch, u8 bw, u8 ch_offset, u8 enqueue)
{
	struct cmd_obj *pcmdobj;
	struct set_ch_parm *set_ch_parm;
	struct cmd_priv *pcmdpriv = &padapter->cmdpriv;

	u8 res=_SUCCESS;

_func_enter_;

	DBG_871X(FUNC_NDEV_FMT" ch:%u, bw:%u, ch_offset:%u\n",
		FUNC_NDEV_ARG(padapter->pnetdev), ch, bw, ch_offset);

	/* check input parameter */

	/* prepare cmd parameter */
	set_ch_parm = (struct set_ch_parm *)rtw_zmalloc(sizeof(*set_ch_parm));
	if (set_ch_parm == NULL) {
		res= _FAIL;
		goto exit;
	}
	set_ch_parm->ch = ch;
	set_ch_parm->bw = bw;
	set_ch_parm->ch_offset = ch_offset;

	if (enqueue) {
		/* need enqueue, prepare cmd_obj and enqueue */
		pcmdobj = (struct cmd_obj*)rtw_zmalloc(sizeof(struct	cmd_obj));
		if(pcmdobj == NULL){
			rtw_mfree((u8 *)set_ch_parm, sizeof(*set_ch_parm));
			res=_FAIL;
			goto exit;
		}

		init_h2fwcmd_w_parm_no_rsp(pcmdobj, set_ch_parm, GEN_CMD_CODE(_SetChannel));
		res = rtw_enqueue_cmd(pcmdpriv, pcmdobj);
	} else {
		/* no need to enqueue, do the cmd hdl directly and free cmd parameter */
		if( H2C_SUCCESS !=set_ch_hdl(padapter, (u8 *)set_ch_parm) )
			res = _FAIL;
		
		rtw_mfree((u8 *)set_ch_parm, sizeof(*set_ch_parm));
	}

	/* do something based on res... */

exit:

	DBG_871X(FUNC_NDEV_FMT" res:%u\n", FUNC_NDEV_ARG(padapter->pnetdev), res);

_func_exit_;	

	return res;
}

u8 rtw_set_chplan_cmd(_adapter*padapter, u8 chplan, u8 enqueue, u8 swconfig)
{
	struct	cmd_obj*	pcmdobj;
	struct	SetChannelPlan_param *setChannelPlan_param;
	struct	cmd_priv   *pcmdpriv = &padapter->cmdpriv;

	u8	res=_SUCCESS;

_func_enter_;

	RT_TRACE(_module_rtl871x_cmd_c_, _drv_notice_, ("+rtw_set_chplan_cmd\n"));

	// check if allow software config
	if (swconfig && rtw_hal_is_disable_sw_channel_plan(padapter) == _TRUE)
	{
		res = _FAIL;
		goto exit;
	}

	//check input parameter
	if(!rtw_is_channel_plan_valid(chplan)) {
		res = _FAIL;
		goto exit;
	}

	//prepare cmd parameter
	setChannelPlan_param = (struct	SetChannelPlan_param *)rtw_zmalloc(sizeof(struct SetChannelPlan_param));
	if(setChannelPlan_param == NULL) {
		res= _FAIL;
		goto exit;
	}
	setChannelPlan_param->channel_plan=chplan;

	if(enqueue)
	{
		//need enqueue, prepare cmd_obj and enqueue
		pcmdobj = (struct	cmd_obj*)rtw_zmalloc(sizeof(struct	cmd_obj));
		if(pcmdobj == NULL){
			rtw_mfree((u8 *)setChannelPlan_param, sizeof(struct SetChannelPlan_param));
			res=_FAIL;
			goto exit;
		}

		init_h2fwcmd_w_parm_no_rsp(pcmdobj, setChannelPlan_param, GEN_CMD_CODE(_SetChannelPlan));
		res = rtw_enqueue_cmd(pcmdpriv, pcmdobj);
	}
	else
	{
		//no need to enqueue, do the cmd hdl directly and free cmd parameter
		if( H2C_SUCCESS != set_chplan_hdl(padapter, (unsigned char *)setChannelPlan_param) )
			res = _FAIL;
		
		rtw_mfree((u8 *)setChannelPlan_param, sizeof(struct SetChannelPlan_param));
	}
	
exit:

_func_exit_;	

	return res;
}

u8 rtw_led_blink_cmd(_adapter*padapter, PVOID pLed)
{
	struct	cmd_obj*	pcmdobj;
	struct	LedBlink_param *ledBlink_param;
	struct	cmd_priv   *pcmdpriv = &padapter->cmdpriv;

	u8	res=_SUCCESS;

_func_enter_;

	RT_TRACE(_module_rtl871x_cmd_c_, _drv_notice_, ("+rtw_led_blink_cmd\n"));
	
	pcmdobj = (struct	cmd_obj*)rtw_zmalloc(sizeof(struct	cmd_obj));
	if(pcmdobj == NULL){
		res=_FAIL;
		goto exit;
	}

	ledBlink_param = (struct	LedBlink_param *)rtw_zmalloc(sizeof(struct	LedBlink_param));
	if(ledBlink_param == NULL) {
		rtw_mfree((u8 *)pcmdobj, sizeof(struct cmd_obj));
		res= _FAIL;
		goto exit;
	}

	ledBlink_param->pLed=pLed;
	
	init_h2fwcmd_w_parm_no_rsp(pcmdobj, ledBlink_param, GEN_CMD_CODE(_LedBlink));
	res = rtw_enqueue_cmd(pcmdpriv, pcmdobj);
	
exit:

_func_exit_;	

	return res;
}

u8 rtw_set_csa_cmd(_adapter*padapter, u8 new_ch_no)
{
	struct	cmd_obj*	pcmdobj;
	struct	SetChannelSwitch_param*setChannelSwitch_param;
	struct 	mlme_priv *pmlmepriv = &padapter->mlmepriv;
	struct	cmd_priv   *pcmdpriv = &padapter->cmdpriv;

	u8	res=_SUCCESS;

_func_enter_;

	RT_TRACE(_module_rtl871x_cmd_c_, _drv_notice_, ("+rtw_set_csa_cmd\n"));
	
	pcmdobj = (struct	cmd_obj*)rtw_zmalloc(sizeof(struct	cmd_obj));
	if(pcmdobj == NULL){
		res=_FAIL;
		goto exit;
	}

	setChannelSwitch_param = (struct SetChannelSwitch_param *)rtw_zmalloc(sizeof(struct	SetChannelSwitch_param));
	if(setChannelSwitch_param == NULL) {
		rtw_mfree((u8 *)pcmdobj, sizeof(struct cmd_obj));
		res= _FAIL;
		goto exit;
	}

	setChannelSwitch_param->new_ch_no=new_ch_no;
	
	init_h2fwcmd_w_parm_no_rsp(pcmdobj, setChannelSwitch_param, GEN_CMD_CODE(_SetChannelSwitch));
	res = rtw_enqueue_cmd(pcmdpriv, pcmdobj);
	
exit:

_func_exit_;	

	return res;
}

u8 rtw_tdls_cmd(_adapter *padapter, u8 *addr, u8 option)
{
	struct	cmd_obj*	pcmdobj;
	struct	TDLSoption_param	*TDLSoption;
	struct 	mlme_priv *pmlmepriv = &padapter->mlmepriv;
	struct	cmd_priv   *pcmdpriv = &padapter->cmdpriv;

	u8	res=_SUCCESS;

_func_enter_;

#ifdef CONFIG_TDLS

	RT_TRACE(_module_rtl871x_cmd_c_, _drv_notice_, ("+rtw_set_tdls_cmd\n"));

	pcmdobj = (struct	cmd_obj*)rtw_zmalloc(sizeof(struct	cmd_obj));
	if(pcmdobj == NULL){
		res=_FAIL;
		goto exit;
	}

	TDLSoption= (struct TDLSoption_param *)rtw_zmalloc(sizeof(struct TDLSoption_param));
	if(TDLSoption == NULL) {
		rtw_mfree((u8 *)pcmdobj, sizeof(struct cmd_obj));
		res= _FAIL;
		goto exit;
	}

	_rtw_spinlock(&(padapter->tdlsinfo.cmd_lock));
	if (addr != NULL)
		_rtw_memcpy(TDLSoption->addr, addr, 6);
	TDLSoption->option = option;
	_rtw_spinunlock(&(padapter->tdlsinfo.cmd_lock));
	init_h2fwcmd_w_parm_no_rsp(pcmdobj, TDLSoption, GEN_CMD_CODE(_TDLS));
	res = rtw_enqueue_cmd(pcmdpriv, pcmdobj);

#endif	//CONFIG_TDLS
	
exit:


_func_exit_;	

	return res;
}

u8 rtw_enable_hw_update_tsf_cmd(_adapter *padapter)
{
	struct cmd_obj *ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;
	struct cmd_priv *pcmdpriv = &padapter->cmdpriv;
	u8	res = _SUCCESS;
	

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));	
	if(ph2c==NULL){
		res= _FAIL;
		goto exit;
	}
		
	pdrvextra_cmd_parm = (struct drvextra_cmd_parm*)rtw_zmalloc(sizeof(struct drvextra_cmd_parm)); 
	if(pdrvextra_cmd_parm==NULL){
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res= _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = EN_HW_UPDATE_TSF_WK_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:
	return res;
}

static void collect_traffic_statistics(_adapter *padapter)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);

#ifdef CONFIG_CONCURRENT_MODE
	if (padapter->adapter_type != PRIMARY_ADAPTER)
		return;
#endif

	// Tx
	pdvobjpriv->traffic_stat.tx_bytes = padapter->xmitpriv.tx_bytes;
	pdvobjpriv->traffic_stat.tx_pkts = padapter->xmitpriv.tx_pkts;
	pdvobjpriv->traffic_stat.tx_drop = padapter->xmitpriv.tx_drop;

	// Rx
	pdvobjpriv->traffic_stat.rx_bytes = padapter->recvpriv.rx_bytes;
	pdvobjpriv->traffic_stat.rx_pkts = padapter->recvpriv.rx_pkts;
	pdvobjpriv->traffic_stat.rx_drop = padapter->recvpriv.rx_drop;

#ifdef CONFIG_CONCURRENT_MODE
	// Add secondary adapter statistics
	if(rtw_buddy_adapter_up(padapter))
	{
		// Tx
		pdvobjpriv->traffic_stat.tx_bytes += padapter->pbuddy_adapter->xmitpriv.tx_bytes;
		pdvobjpriv->traffic_stat.tx_pkts += padapter->pbuddy_adapter->xmitpriv.tx_pkts;
		pdvobjpriv->traffic_stat.tx_drop += padapter->pbuddy_adapter->xmitpriv.tx_drop;

		// Rx
		pdvobjpriv->traffic_stat.rx_bytes += padapter->pbuddy_adapter->recvpriv.rx_bytes;
		pdvobjpriv->traffic_stat.rx_pkts += padapter->pbuddy_adapter->recvpriv.rx_pkts;
		pdvobjpriv->traffic_stat.rx_drop += padapter->pbuddy_adapter->recvpriv.rx_drop;
	}
#endif

	// Calculate throughput in last interval
	pdvobjpriv->traffic_stat.cur_tx_bytes = pdvobjpriv->traffic_stat.tx_bytes - pdvobjpriv->traffic_stat.last_tx_bytes;
	pdvobjpriv->traffic_stat.cur_rx_bytes = pdvobjpriv->traffic_stat.rx_bytes - pdvobjpriv->traffic_stat.last_rx_bytes;
	pdvobjpriv->traffic_stat.last_tx_bytes = pdvobjpriv->traffic_stat.tx_bytes;
	pdvobjpriv->traffic_stat.last_rx_bytes = pdvobjpriv->traffic_stat.rx_bytes;

	pdvobjpriv->traffic_stat.cur_tx_tp = (u32)(pdvobjpriv->traffic_stat.cur_tx_bytes *8/2/1024/1024);
	pdvobjpriv->traffic_stat.cur_rx_tp = (u32)(pdvobjpriv->traffic_stat.cur_rx_bytes *8/2/1024/1024);
}

//from_timer == 1 means driver is in LPS
u8 traffic_status_watchdog(_adapter *padapter, u8 from_timer)
{
	u8	bEnterPS = _FALSE;
#ifdef CONFIG_BT_COEXIST
	u16	BusyThresholdHigh = 25;
	u16	BusyThresholdLow = 10;
#else
	u16	BusyThresholdHigh = 100;
	u16	BusyThresholdLow = 75;
#endif
	u16	BusyThreshold = BusyThresholdHigh;
	u8	bBusyTraffic = _FALSE, bTxBusyTraffic = _FALSE, bRxBusyTraffic = _FALSE;
	u8	bHigherBusyTraffic = _FALSE, bHigherBusyRxTraffic = _FALSE, bHigherBusyTxTraffic = _FALSE;

	struct mlme_priv		*pmlmepriv = &(padapter->mlmepriv);
#ifdef CONFIG_TDLS
	struct tdls_info *ptdlsinfo = &(padapter->tdlsinfo);
	struct tdls_txmgmt txmgmt;
	u8 baddr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
#endif //CONFIG_TDLS

	RT_LINK_DETECT_T * link_detect = &pmlmepriv->LinkDetectInfo;

	collect_traffic_statistics(padapter);

	//
	// Determine if our traffic is busy now
	//
	if((check_fwstate(pmlmepriv, _FW_LINKED)== _TRUE) 
		/*&& !MgntInitAdapterInProgress(pMgntInfo)*/)
	{
		// if we raise bBusyTraffic in last watchdog, using lower threshold.
		if (pmlmepriv->LinkDetectInfo.bBusyTraffic)
				BusyThreshold = BusyThresholdLow;

		if( pmlmepriv->LinkDetectInfo.NumRxOkInPeriod > BusyThreshold ||
			pmlmepriv->LinkDetectInfo.NumTxOkInPeriod > BusyThreshold )
		{
			bBusyTraffic = _TRUE;

			if (pmlmepriv->LinkDetectInfo.NumRxOkInPeriod > pmlmepriv->LinkDetectInfo.NumTxOkInPeriod)
				bRxBusyTraffic = _TRUE;
			else
				bTxBusyTraffic = _TRUE;
		}

		// Higher Tx/Rx data.
		if( pmlmepriv->LinkDetectInfo.NumRxOkInPeriod > 4000 ||
			pmlmepriv->LinkDetectInfo.NumTxOkInPeriod > 4000 )
		{
			bHigherBusyTraffic = _TRUE;

			if (pmlmepriv->LinkDetectInfo.NumRxOkInPeriod > pmlmepriv->LinkDetectInfo.NumTxOkInPeriod)
				bHigherBusyRxTraffic = _TRUE;
			else
				bHigherBusyTxTraffic = _TRUE;
		}

#ifdef CONFIG_TRAFFIC_PROTECT
#define TX_ACTIVE_TH 10
#define RX_ACTIVE_TH 20
#define TRAFFIC_PROTECT_PERIOD_MS 4500

	if (link_detect->NumTxOkInPeriod > TX_ACTIVE_TH
		|| link_detect->NumRxUnicastOkInPeriod > RX_ACTIVE_TH) {
		
		DBG_871X_LEVEL(_drv_info_, FUNC_ADPT_FMT" acqiure wake_lock for %u ms(tx:%d,rx_unicast:%d)\n",
			FUNC_ADPT_ARG(padapter),
			TRAFFIC_PROTECT_PERIOD_MS,
			link_detect->NumTxOkInPeriod,
			link_detect->NumRxUnicastOkInPeriod);

		rtw_lock_traffic_suspend_timeout(TRAFFIC_PROTECT_PERIOD_MS);
	}
#endif
		
#ifdef CONFIG_TDLS
#ifdef CONFIG_TDLS_AUTOSETUP
		/* TDLS_WATCHDOG_PERIOD * 2sec, periodically send */
		if ((ptdlsinfo->watchdog_count % TDLS_WATCHDOG_PERIOD ) == 0) {
			_rtw_memcpy(txmgmt.peer, baddr, ETH_ALEN);
			issue_tdls_dis_req( padapter, &txmgmt );
		}
		ptdlsinfo->watchdog_count++;
#endif //CONFIG_TDLS_AUTOSETUP
#endif //CONFIG_TDLS

#ifdef CONFIG_LPS
		// check traffic for  powersaving.
		if( ((pmlmepriv->LinkDetectInfo.NumRxUnicastOkInPeriod + pmlmepriv->LinkDetectInfo.NumTxOkInPeriod) > 8 ) ||
#ifdef CONFIG_LPS_SLOW_TRANSITION			
			(pmlmepriv->LinkDetectInfo.NumRxUnicastOkInPeriod > 2) 
#else //CONFIG_LPS_SLOW_TRANSITION
			(pmlmepriv->LinkDetectInfo.NumRxUnicastOkInPeriod > 4) 
#endif //CONFIG_LPS_SLOW_TRANSITION
			)
		{
#ifdef DBG_RX_COUNTER_DUMP
			if( padapter->dump_rx_cnt_mode & DUMP_DRV_TRX_COUNTER_DATA)
				DBG_871X("(-)Tx = %d, Rx = %d \n",pmlmepriv->LinkDetectInfo.NumTxOkInPeriod,pmlmepriv->LinkDetectInfo.NumRxUnicastOkInPeriod);
#endif	
			bEnterPS= _FALSE;
#ifdef CONFIG_LPS_SLOW_TRANSITION
			if(bBusyTraffic == _TRUE)
			{
				if(pmlmepriv->LinkDetectInfo.TrafficTransitionCount <= 4)
					pmlmepriv->LinkDetectInfo.TrafficTransitionCount = 4;

				pmlmepriv->LinkDetectInfo.TrafficTransitionCount++;

				//DBG_871X("Set TrafficTransitionCount to %d\n", pmlmepriv->LinkDetectInfo.TrafficTransitionCount);
			
				if(pmlmepriv->LinkDetectInfo.TrafficTransitionCount > 30/*TrafficTransitionLevel*/)
				{
					pmlmepriv->LinkDetectInfo.TrafficTransitionCount = 30;
				}	
			}
#endif //CONFIG_LPS_SLOW_TRANSITION
	
		}
		else
		{
#ifdef DBG_RX_COUNTER_DUMP		
			if( padapter->dump_rx_cnt_mode & DUMP_DRV_TRX_COUNTER_DATA)
				DBG_871X("(+)Tx = %d, Rx = %d \n",pmlmepriv->LinkDetectInfo.NumTxOkInPeriod,pmlmepriv->LinkDetectInfo.NumRxUnicastOkInPeriod);
#endif			
#ifdef CONFIG_LPS_SLOW_TRANSITION
			if(pmlmepriv->LinkDetectInfo.TrafficTransitionCount>=2)
				pmlmepriv->LinkDetectInfo.TrafficTransitionCount -=2;
			else
				pmlmepriv->LinkDetectInfo.TrafficTransitionCount = 0;

			if(pmlmepriv->LinkDetectInfo.TrafficTransitionCount == 0)
				bEnterPS= _TRUE;
#else //CONFIG_LPS_SLOW_TRANSITION
				bEnterPS= _TRUE;
#endif //CONFIG_LPS_SLOW_TRANSITION
		}

#ifdef CONFIG_DYNAMIC_DTIM
		if(pmlmepriv->LinkDetectInfo.LowPowerTransitionCount == 8)
			bEnterPS= _FALSE;

		DBG_871X("LowPowerTransitionCount=%d\n", pmlmepriv->LinkDetectInfo.LowPowerTransitionCount);
#endif //CONFIG_DYNAMIC_DTIM

		// LeisurePS only work in infra mode.
		if(bEnterPS)
		{
			if(!from_timer)
			{
#ifdef CONFIG_DYNAMIC_DTIM
				if(pmlmepriv->LinkDetectInfo.LowPowerTransitionCount < 8)
				{					
					adapter_to_pwrctl(padapter)->dtim = 1;
				}	
				else
				{					
					adapter_to_pwrctl(padapter)->dtim = 3;
				}
#endif //CONFIG_DYNAMIC_DTIM
				LPS_Enter(padapter, "TRAFFIC_IDLE");
			}	
			else
			{
				//do this at caller
				//rtw_lps_ctrl_wk_cmd(adapter, LPS_CTRL_ENTER, 1);
				//rtw_hal_dm_watchdog_in_lps(padapter);
			}				
#ifdef CONFIG_DYNAMIC_DTIM
			if (adapter_to_pwrctl(padapter)->bFwCurrentInPSMode ==_TRUE )
				pmlmepriv->LinkDetectInfo.LowPowerTransitionCount++;
#endif //CONFIG_DYNAMIC_DTIM
		}
		else
		{
#ifdef CONFIG_DYNAMIC_DTIM
			if(pmlmepriv->LinkDetectInfo.LowPowerTransitionCount != 8)
				pmlmepriv->LinkDetectInfo.LowPowerTransitionCount = 0;
			else
				pmlmepriv->LinkDetectInfo.LowPowerTransitionCount++;
#endif //CONFIG_DYNAMIC_DTIM			
			if(!from_timer)
			{
				LPS_Leave(padapter, "TRAFFIC_BUSY");
			}
			else
			{
#ifdef CONFIG_CONCURRENT_MODE
			 	if(padapter->iface_type == IFACE_PORT0) 
#endif
					rtw_lps_ctrl_wk_cmd(padapter, LPS_CTRL_TRAFFIC_BUSY, 1);
			}
		}
	
#endif // CONFIG_LPS
	}
	else
	{
#ifdef CONFIG_LPS
		struct dvobj_priv *dvobj = adapter_to_dvobj(padapter);
		int n_assoc_iface = 0;
		int i;

		for (i = 0; i < dvobj->iface_nums; i++) {
			if (check_fwstate(&(dvobj->padapters[i]->mlmepriv), WIFI_ASOC_STATE))
				n_assoc_iface++;
		}

		if(!from_timer && n_assoc_iface == 0)
			LPS_Leave(padapter, "NON_LINKED");
#endif
	}

	pmlmepriv->LinkDetectInfo.NumRxOkInPeriod = 0;
	pmlmepriv->LinkDetectInfo.NumTxOkInPeriod = 0;
	pmlmepriv->LinkDetectInfo.NumRxUnicastOkInPeriod = 0;
	pmlmepriv->LinkDetectInfo.bBusyTraffic = bBusyTraffic;
	pmlmepriv->LinkDetectInfo.bTxBusyTraffic = bTxBusyTraffic;
	pmlmepriv->LinkDetectInfo.bRxBusyTraffic = bRxBusyTraffic;
	pmlmepriv->LinkDetectInfo.bHigherBusyTraffic = bHigherBusyTraffic;
	pmlmepriv->LinkDetectInfo.bHigherBusyRxTraffic = bHigherBusyRxTraffic;
	pmlmepriv->LinkDetectInfo.bHigherBusyTxTraffic = bHigherBusyTxTraffic;

	return bEnterPS;
	
}

void edca_parameter_check(_adapter *padapter)
{
	#include <hal_data.h>
	struct mlme_priv *pmlmepriv, *puddy_mlmepriv;
	PADAPTER pbuddy_adapter = padapter->pbuddy_adapter;
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(padapter);
	PDM_ODM_T		pDM_Odm = &(pHalData->odmpriv);
	u16	BusyThresholdHigh = 100;
	u16	BusyThresholdLow = 75;

	if (padapter->registrypriv.wifi_spec == 1)
		return;

	pmlmepriv = &(padapter->mlmepriv);
	puddy_mlmepriv = &(padapter->pbuddy_adapter->mlmepriv);

	/* secondary adapter first enter */
	if (!is_primary_adapter(padapter)) {
		if (pmlmepriv->LinkDetectInfo.NumTxOkInPeriod > BusyThresholdLow
			&& puddy_mlmepriv->LinkDetectInfo.NumTxOkInPeriod > BusyThresholdLow) {

			/* two interface are busytraffic, disable trubo edca */
			if (pDM_Odm->SupportAbility & ODM_MAC_EDCA_TURBO)
				pDM_Odm->SupportAbility &= (~ODM_MAC_EDCA_TURBO);

			/* use station mode edca parameter */
			if (is_client_associated_to_ap(padapter)) {
				if (rtw_read32(padapter, REG_EDCA_BE_PARAM) != padapter->backup_acParam_BE) {
					rtw_write32(padapter, REG_EDCA_BE_PARAM, padapter->backup_acParam_BE);
					rtw_write32(padapter, REG_EDCA_BK_PARAM, padapter->backup_acParam_BK);
					rtw_write32(padapter, REG_EDCA_VI_PARAM, padapter->backup_acParam_VI);
					rtw_write32(padapter, REG_EDCA_VO_PARAM, padapter->backup_acParam_VO);
				}
				
			} else if (is_client_associated_to_ap(pbuddy_adapter)) {
				if (rtw_read32(pbuddy_adapter, REG_EDCA_BE_PARAM) != pbuddy_adapter->backup_acParam_BE) {
					rtw_write32(pbuddy_adapter, REG_EDCA_BE_PARAM, pbuddy_adapter->backup_acParam_BE);
					rtw_write32(pbuddy_adapter, REG_EDCA_BK_PARAM, pbuddy_adapter->backup_acParam_BK);
					rtw_write32(pbuddy_adapter, REG_EDCA_VI_PARAM, pbuddy_adapter->backup_acParam_VI);
					rtw_write32(pbuddy_adapter, REG_EDCA_VO_PARAM, pbuddy_adapter->backup_acParam_VO);
				}
			}
			
		} else {
		/* only one interface is busytraffic or both is idle, enable trubo edca */
			if (!(pDM_Odm->SupportAbility & ODM_MAC_EDCA_TURBO))
				pDM_Odm->SupportAbility |= ODM_MAC_EDCA_TURBO;
		}
	}
}

void dynamic_chk_wk_hdl(_adapter *padapter)
{
	struct mlme_priv *pmlmepriv;
	
	pmlmepriv = &(padapter->mlmepriv);

#ifdef CONFIG_ACTIVE_KEEP_ALIVE_CHECK
#ifdef CONFIG_AP_MODE
	if(check_fwstate(pmlmepriv, WIFI_AP_STATE) == _TRUE)
	{			
		expire_timeout_chk(padapter);
	}
#endif
#endif //CONFIG_ACTIVE_KEEP_ALIVE_CHECK

	if (padapter->registrypriv.wifi_spec) {
		if (check_fwstate(pmlmepriv, WIFI_AP_STATE) == _TRUE) {
			struct mlme_ext_priv *pmlmeext = &padapter->mlmeextpriv;

			if (pmlmeext->bstart_bss) {
				static u8 count = 1;

				if (count % 10 == 0) {
					count = 1;
					if (_FALSE == ATOMIC_READ(&pmlmepriv->olbc)
						&& _FALSE == ATOMIC_READ(&pmlmepriv->olbc_ht))

					if (rtw_ht_operation_update(padapter) > 0) {
						update_beacon(padapter, _HT_CAPABILITY_IE_, NULL, _FALSE);
						update_beacon(padapter, _HT_ADD_INFO_IE_, NULL, _TRUE);
					}
				}

				if (count == 1) {
					ATOMIC_SET(&pmlmepriv->olbc, _FALSE);
					ATOMIC_SET(&pmlmepriv->olbc_ht, _FALSE);
				}

				if (_FALSE != ATOMIC_READ(&pmlmepriv->olbc)
					&& _FALSE != ATOMIC_READ(&pmlmepriv->olbc_ht)) {
					
					if (rtw_ht_operation_update(padapter) > 0) {
						update_beacon(padapter, _HT_CAPABILITY_IE_, NULL, _FALSE);
						update_beacon(padapter, _HT_ADD_INFO_IE_, NULL, _TRUE);

					}

					ATOMIC_SET(&pmlmepriv->olbc, _FALSE);
					ATOMIC_SET(&pmlmepriv->olbc_ht, FALSE);
					count = 1;
				}
				count ++;
			}
		}
	}

#ifdef DBG_CONFIG_ERROR_DETECT	
	rtw_hal_sreset_xmit_status_check(padapter);		
	rtw_hal_sreset_linked_status_check(padapter);
#endif	

	//for debug purpose
	_linked_info_dump(padapter);


	//if(check_fwstate(pmlmepriv, _FW_UNDER_LINKING|_FW_UNDER_SURVEY)==_FALSE)
	{
		linked_status_chk(padapter, 0);
		edca_parameter_check(padapter);
		traffic_status_watchdog(padapter, 0);
		#ifdef DBG_RX_COUNTER_DUMP
		rtw_dump_rx_counters(padapter);
		#endif
		dm_DynamicUsbTxAgg(padapter, 0);
	}

#ifdef CONFIG_BEAMFORMING
	beamforming_watchdog(padapter);
#endif

	rtw_hal_dm_watchdog(padapter);

	//check_hw_pbc(padapter, pdrvextra_cmd->pbuf, pdrvextra_cmd->type);

#ifdef CONFIG_BT_COEXIST
	//
	// BT-Coexist
	//
	rtw_btcoex_Handler(padapter);
#endif

	
#ifdef CONFIG_IPS_CHECK_IN_WD
	//always call rtw_ps_processor() at last one.
	if(is_primary_adapter(padapter))
		rtw_ps_processor(padapter);
#endif

#ifdef CONFIG_MCC_MODE
	if(is_primary_adapter(padapter) && _TRUE == padapter->registrypriv.en_mcc) {
		rtw_hal_mcc_status_check(padapter);
	}
#endif /* CONFIG_MCC_MODE */
}

#ifdef CONFIG_LPS

void lps_ctrl_wk_hdl(_adapter *padapter, u8 lps_ctrl_type);
void lps_ctrl_wk_hdl(_adapter *padapter, u8 lps_ctrl_type)
{
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(padapter);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	u8	mstatus;
	
_func_enter_;

	if((check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE) == _TRUE)
		|| (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE) == _TRUE))
	{
		return;
	}

	switch(lps_ctrl_type)
	{
		case LPS_CTRL_SCAN:
			//DBG_871X("LPS_CTRL_SCAN \n");
#ifdef CONFIG_BT_COEXIST
			rtw_btcoex_ScanNotify(padapter, _TRUE);
#endif // CONFIG_BT_COEXIST
			if (check_fwstate(pmlmepriv, _FW_LINKED) == _TRUE)
			{
				// connect
				LPS_Leave(padapter, "LPS_CTRL_SCAN");
			}
			break;
		case LPS_CTRL_JOINBSS:
			//DBG_871X("LPS_CTRL_JOINBSS \n");
			LPS_Leave(padapter, "LPS_CTRL_JOINBSS");
			break;
		case LPS_CTRL_CONNECT:
			//DBG_871X("LPS_CTRL_CONNECT \n");
			mstatus = 1;//connect
			// Reset LPS Setting
			pwrpriv->LpsIdleCount = 0;
			rtw_hal_set_hwreg(padapter, HW_VAR_H2C_FW_JOINBSSRPT, (u8 *)(&mstatus));
#ifdef CONFIG_BT_COEXIST
			rtw_btcoex_MediaStatusNotify(padapter, mstatus);
#endif // CONFIG_BT_COEXIST
			break;
		case LPS_CTRL_DISCONNECT:
			//DBG_871X("LPS_CTRL_DISCONNECT \n");
			mstatus = 0;//disconnect
#ifdef CONFIG_BT_COEXIST
			rtw_btcoex_MediaStatusNotify(padapter, mstatus);
#endif // CONFIG_BT_COEXIST
			LPS_Leave(padapter, "LPS_CTRL_DISCONNECT");
			rtw_hal_set_hwreg(padapter, HW_VAR_H2C_FW_JOINBSSRPT, (u8 *)(&mstatus));
			break;
		case LPS_CTRL_SPECIAL_PACKET:
			//DBG_871X("LPS_CTRL_SPECIAL_PACKET \n");
			pwrpriv->DelayLPSLastTimeStamp = rtw_get_current_time();
#ifdef CONFIG_BT_COEXIST
			rtw_btcoex_SpecialPacketNotify(padapter, PACKET_DHCP);
#endif // CONFIG_BT_COEXIST
			LPS_Leave(padapter, "LPS_CTRL_SPECIAL_PACKET");
			break;
		case LPS_CTRL_LEAVE:
			//DBG_871X("LPS_CTRL_LEAVE \n");
			LPS_Leave(padapter, "LPS_CTRL_LEAVE");
			break;
		case LPS_CTRL_TRAFFIC_BUSY:
			LPS_Leave(padapter, "LPS_CTRL_TRAFFIC_BUSY");
			break;
		case LPS_CTRL_TX_TRAFFIC_LEAVE:
			LPS_Leave(padapter, "LPS_CTRL_TX_TRAFFIC_LEAVE");
			break;
		case LPS_CTRL_RX_TRAFFIC_LEAVE:
			LPS_Leave(padapter, "LPS_CTRL_RX_TRAFFIC_LEAVE");
			break;
		case LPS_CTRL_ENTER:
			LPS_Enter(padapter, "TRAFFIC_IDLE_1");
			break;
		default:
			break;
	}

_func_exit_;
}

u8 rtw_lps_ctrl_wk_cmd(_adapter*padapter, u8 lps_ctrl_type, u8 enqueue)
{
	struct cmd_obj	*ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;
	struct cmd_priv	*pcmdpriv = &padapter->cmdpriv;
	//struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(padapter);
	u8	res = _SUCCESS;
	
_func_enter_;

	//if(!pwrctrlpriv->bLeisurePs)
	//	return res;

	if(enqueue)
	{
		ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));	
		if(ph2c==NULL){
			res= _FAIL;
			goto exit;
		}
		
		pdrvextra_cmd_parm = (struct drvextra_cmd_parm*)rtw_zmalloc(sizeof(struct drvextra_cmd_parm)); 
		if(pdrvextra_cmd_parm==NULL){
			rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
			res= _FAIL;
			goto exit;
		}

		pdrvextra_cmd_parm->ec_id = LPS_CTRL_WK_CID;
		pdrvextra_cmd_parm->type = lps_ctrl_type;
		pdrvextra_cmd_parm->size = 0;
		pdrvextra_cmd_parm->pbuf = NULL;

		init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

		res = rtw_enqueue_cmd(pcmdpriv, ph2c);
	}
	else
	{
		lps_ctrl_wk_hdl(padapter, lps_ctrl_type);
	}
	
exit:
	
_func_exit_;

	return res;

}

void rtw_dm_in_lps_hdl(_adapter*padapter)
{
	rtw_hal_set_hwreg(padapter, HW_VAR_DM_IN_LPS, NULL);
}

u8 rtw_dm_in_lps_wk_cmd(_adapter*padapter)
{
	struct cmd_obj	*ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;
	struct cmd_priv	*pcmdpriv = &padapter->cmdpriv;
	u8	res = _SUCCESS;
	

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));	
	if(ph2c==NULL){
		res= _FAIL;
		goto exit;
	}
		
	pdrvextra_cmd_parm = (struct drvextra_cmd_parm*)rtw_zmalloc(sizeof(struct drvextra_cmd_parm)); 
	if(pdrvextra_cmd_parm==NULL){
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res= _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = DM_IN_LPS_WK_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
	
exit:
	
	return res;

}

void rtw_lps_change_dtim_hdl(_adapter *padapter, u8 dtim)
{
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(padapter);

	if(dtim <=0 || dtim > 16)
		return;

#ifdef CONFIG_BT_COEXIST
	if (rtw_btcoex_IsBtControlLps(padapter) == _TRUE)
		return;
#endif

#ifdef CONFIG_LPS_LCLK
	_enter_pwrlock(&pwrpriv->lock);
#endif

	if(pwrpriv->dtim!=dtim)
	{
		DBG_871X("change DTIM from %d to %d, bFwCurrentInPSMode=%d, ps_mode=%d\n", pwrpriv->dtim, dtim, 
			pwrpriv->bFwCurrentInPSMode, pwrpriv->pwr_mode);
		
		pwrpriv->dtim = dtim;
	}	

	if((pwrpriv->bFwCurrentInPSMode ==_TRUE) && (pwrpriv->pwr_mode > PS_MODE_ACTIVE))		 
	{
		u8 ps_mode = pwrpriv->pwr_mode;

		//DBG_871X("change DTIM from %d to %d, ps_mode=%d\n", pwrpriv->dtim, dtim, ps_mode);
	
		rtw_hal_set_hwreg(padapter, HW_VAR_H2C_FW_PWRMODE, (u8 *)(&ps_mode));
	}
	
#ifdef CONFIG_LPS_LCLK
	_exit_pwrlock(&pwrpriv->lock);
#endif

}

#endif

u8 rtw_lps_change_dtim_cmd(_adapter*padapter, u8 dtim)
{
	struct cmd_obj	*ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;
	struct cmd_priv	*pcmdpriv = &padapter->cmdpriv;
	u8	res = _SUCCESS;
/*
#ifdef CONFIG_CONCURRENT_MODE
	if (padapter->iface_type != IFACE_PORT0)
		return res;
#endif
*/
	{
		ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));	
		if(ph2c==NULL){
			res= _FAIL;
			goto exit;
		}
		
		pdrvextra_cmd_parm = (struct drvextra_cmd_parm*)rtw_zmalloc(sizeof(struct drvextra_cmd_parm)); 
		if(pdrvextra_cmd_parm==NULL){
			rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
			res= _FAIL;
			goto exit;
		}

		pdrvextra_cmd_parm->ec_id = LPS_CHANGE_DTIM_CID;
		pdrvextra_cmd_parm->type = dtim;
		pdrvextra_cmd_parm->size = 0;
		pdrvextra_cmd_parm->pbuf = NULL;

		init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

		res = rtw_enqueue_cmd(pcmdpriv, ph2c);
	}
	
exit:
	
	return res;

}

#if (RATE_ADAPTIVE_SUPPORT==1)
void rpt_timer_setting_wk_hdl(_adapter *padapter, u16 minRptTime)
{
	rtw_hal_set_hwreg(padapter, HW_VAR_RPT_TIMER_SETTING, (u8 *)(&minRptTime));
}

u8 rtw_rpt_timer_cfg_cmd(_adapter*padapter, u16 minRptTime)
{
	struct cmd_obj		*ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;	
	struct cmd_priv	*pcmdpriv = &padapter->cmdpriv;

	u8	res = _SUCCESS;

_func_enter_;
	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));	
	if(ph2c==NULL){
		res= _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm*)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if(pdrvextra_cmd_parm==NULL){
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res= _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = RTP_TIMER_CFG_WK_CID;
	pdrvextra_cmd_parm->type = minRptTime;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;
	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));
	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
exit:

_func_exit_;

	return res;

}

#endif

#ifdef CONFIG_ANTENNA_DIVERSITY
void antenna_select_wk_hdl(_adapter *padapter, u8 antenna)
{
	rtw_hal_set_hwreg(padapter, HW_VAR_ANTENNA_DIVERSITY_SELECT, (u8 *)(&antenna));
}

u8 rtw_antenna_select_cmd(_adapter*padapter, u8 antenna,u8 enqueue)
{
	struct cmd_obj		*ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;	
	struct cmd_priv	*pcmdpriv = &padapter->cmdpriv;
	u8 	bSupportAntDiv = _FALSE;
	u8	res = _SUCCESS;

_func_enter_;
	rtw_hal_get_def_var(padapter, HAL_DEF_IS_SUPPORT_ANT_DIV, &(bSupportAntDiv));
	if(_FALSE == bSupportAntDiv )	return res;

	if(_TRUE == enqueue)
	{
		ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));	
		if(ph2c==NULL){
			res= _FAIL;
			goto exit;
		}

		pdrvextra_cmd_parm = (struct drvextra_cmd_parm*)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
		if(pdrvextra_cmd_parm==NULL){
			rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
			res= _FAIL;
			goto exit;
		}

		pdrvextra_cmd_parm->ec_id = ANT_SELECT_WK_CID;
		pdrvextra_cmd_parm->type = antenna;
		pdrvextra_cmd_parm->size = 0;
		pdrvextra_cmd_parm->pbuf = NULL;
		init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

		res = rtw_enqueue_cmd(pcmdpriv, ph2c);
	}
	else{
		antenna_select_wk_hdl(padapter,antenna );
	}
exit:

_func_exit_;

	return res;

}
#endif

void rtw_dm_ra_mask_hdl(_adapter *padapter, struct sta_info *psta)
{
	if (psta) {
		set_sta_rate(padapter, psta);
	}
}

u8 rtw_dm_ra_mask_wk_cmd(_adapter*padapter, u8 *psta)
{
	struct cmd_obj	*ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;
	struct cmd_priv	*pcmdpriv = &padapter->cmdpriv;
	u8	res = _SUCCESS;
	

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));	
	if(ph2c==NULL){
		res= _FAIL;
		goto exit;
	}
		
	pdrvextra_cmd_parm = (struct drvextra_cmd_parm*)rtw_zmalloc(sizeof(struct drvextra_cmd_parm)); 
	if(pdrvextra_cmd_parm==NULL){
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res= _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = DM_RA_MSK_WK_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = psta;

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
	
exit:
	
	return res;

}

void power_saving_wk_hdl(_adapter *padapter)
{
	 rtw_ps_processor(padapter);
}

//add for CONFIG_IEEE80211W, none 11w can use it
void reset_securitypriv_hdl(_adapter *padapter)
{
	 rtw_reset_securitypriv(padapter);
}

void free_assoc_resources_hdl(_adapter *padapter)
{
	 rtw_free_assoc_resources(padapter, 1);
}

#ifdef CONFIG_P2P
u8 p2p_protocol_wk_cmd(_adapter*padapter, int intCmdType )
{
	struct cmd_obj	*ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;
	struct wifidirect_info	*pwdinfo= &(padapter->wdinfo);
	struct cmd_priv	*pcmdpriv = &padapter->cmdpriv;
	u8	res = _SUCCESS;
	
_func_enter_;

	if(rtw_p2p_chk_state(pwdinfo, P2P_STATE_NONE))
	{
		return res;
	}

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));	
	if(ph2c==NULL){
		res= _FAIL;
		goto exit;
	}
			
	pdrvextra_cmd_parm = (struct drvextra_cmd_parm*)rtw_zmalloc(sizeof(struct drvextra_cmd_parm)); 
	if(pdrvextra_cmd_parm==NULL){
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res= _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = P2P_PROTO_WK_CID;
	pdrvextra_cmd_parm->type = intCmdType;	//	As the command tppe.
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;		//	Must be NULL here

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
	
exit:
	
_func_exit_;

	return res;

}
#endif //CONFIG_P2P

u8 rtw_ps_cmd(_adapter*padapter)
{
	struct cmd_obj		*ppscmd;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;	
	struct cmd_priv	*pcmdpriv = &padapter->cmdpriv;
	
	u8	res = _SUCCESS;
_func_enter_;

#ifdef CONFIG_CONCURRENT_MODE
	if (padapter->adapter_type != PRIMARY_ADAPTER)
		goto exit;
#endif
	
	ppscmd = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));	
	if(ppscmd==NULL){
		res= _FAIL;
		goto exit;
	}
		
	pdrvextra_cmd_parm = (struct drvextra_cmd_parm*)rtw_zmalloc(sizeof(struct drvextra_cmd_parm)); 
	if(pdrvextra_cmd_parm==NULL){
		rtw_mfree((unsigned char *)ppscmd, sizeof(struct cmd_obj));
		res= _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = POWER_SAVING_CTRL_WK_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;
	init_h2fwcmd_w_parm_no_rsp(ppscmd, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ppscmd);
	
exit:
	
_func_exit_;

	return res;

}

#ifdef CONFIG_AP_MODE

static void rtw_chk_hi_queue_hdl(_adapter *padapter)
{
	struct sta_info *psta_bmc;
	struct sta_priv *pstapriv = &padapter->stapriv;
	u32 start = rtw_get_current_time();
	u8 empty = _FALSE;

	psta_bmc = rtw_get_bcmc_stainfo(padapter);
	if(!psta_bmc)
		return;

	rtw_hal_get_hwreg(padapter, HW_VAR_CHK_HI_QUEUE_EMPTY, &empty);

	while(_FALSE == empty && rtw_get_passing_time_ms(start) < rtw_get_wait_hiq_empty_ms())
	{
		rtw_msleep_os(100);
		rtw_hal_get_hwreg(padapter, HW_VAR_CHK_HI_QUEUE_EMPTY, &empty);
	}

	if(psta_bmc->sleepq_len==0)
	{
		if(empty == _SUCCESS)
		{
			bool update_tim = _FALSE;

			if (pstapriv->tim_bitmap & BIT(0))
				update_tim = _TRUE;

			pstapriv->tim_bitmap &= ~BIT(0);
			pstapriv->sta_dz_bitmap &= ~BIT(0);

			if (update_tim == _TRUE)
				_update_beacon(padapter, _TIM_IE_, NULL, _TRUE, "bmc sleepq and HIQ empty");
		}
		else //re check again
		{
			rtw_chk_hi_queue_cmd(padapter);
		}
		
	}	
	
}

u8 rtw_chk_hi_queue_cmd(_adapter*padapter)
{
	struct cmd_obj	*ph2c;
	struct drvextra_cmd_parm	*pdrvextra_cmd_parm;	
	struct cmd_priv	*pcmdpriv = &padapter->cmdpriv;
	u8	res = _SUCCESS;
	
	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));	
	if(ph2c==NULL){
		res= _FAIL;
		goto exit;
	}
			
	pdrvextra_cmd_parm = (struct drvextra_cmd_parm*)rtw_zmalloc(sizeof(struct drvextra_cmd_parm)); 
	if(pdrvextra_cmd_parm==NULL){
		rtw_mfree((unsigned char *)ph2c, sizeof(struct cmd_obj));
		res= _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = CHECK_HIQ_WK_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = 0;
	pdrvextra_cmd_parm->pbuf = NULL;

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
	
exit:
	
	return res;

}
#endif

#ifdef CONFIG_BT_COEXIST
struct btinfo {
	u8 cid;
	u8 len;

	u8 bConnection:1;
	u8 bSCOeSCO:1;
	u8 bInQPage:1;
	u8 bACLBusy:1;
	u8 bSCOBusy:1;
	u8 bHID:1;
	u8 bA2DP:1;
	u8 bFTP:1;

	u8 retry_cnt:4;
	u8 rsvd_34:1;
	u8 rsvd_35:1;
	u8 rsvd_36:1;
	u8 rsvd_37:1;

	u8 rssi;

	u8 rsvd_50:1;
	u8 rsvd_51:1;
	u8 rsvd_52:1;
	u8 rsvd_53:1;
	u8 rsvd_54:1;
	u8 rsvd_55:1;
	u8 eSCO_SCO:1;
	u8 Master_Slave:1;

	u8 rsvd_6;
	u8 rsvd_7;
};

void btinfo_evt_dump(void *sel, void *buf)
{
	struct btinfo *info = (struct btinfo *)buf;
	
	DBG_871X_SEL_NL(sel, "cid:0x%02x, len:%u\n", info->cid, info->len);

	if (info->len > 2)
		DBG_871X_SEL_NL(sel, "byte2:%s%s%s%s%s%s%s%s\n"
			, info->bConnection?"bConnection ":""
			, info->bSCOeSCO?"bSCOeSCO ":""
			, info->bInQPage?"bInQPage ":""
			, info->bACLBusy?"bACLBusy ":""
			, info->bSCOBusy?"bSCOBusy ":""
			, info->bHID?"bHID ":""
			, info->bA2DP?"bA2DP ":""
			, info->bFTP?"bFTP":""
		);

	if (info->len > 3)
		DBG_871X_SEL_NL(sel, "retry_cnt:%u\n", info->retry_cnt);

	if (info->len > 4)
		DBG_871X_SEL_NL(sel, "rssi:%u\n", info->rssi);

	if (info->len > 5)
		DBG_871X_SEL_NL(sel, "byte5:%s%s\n"
			, info->eSCO_SCO?"eSCO_SCO ":""
			, info->Master_Slave?"Master_Slave ":""
		);
}

static void rtw_btinfo_hdl(_adapter *adapter, u8 *buf, u16 buf_len)
{
	#define BTINFO_WIFI_FETCH 0x23
	#define BTINFO_BT_AUTO_RPT 0x27
#ifdef CONFIG_BT_COEXIST_SOCKET_TRX
	struct btinfo_8761ATV *info = (struct btinfo_8761ATV *)buf;
#else //!CONFIG_BT_COEXIST_SOCKET_TRX
	struct btinfo *info = (struct btinfo *)buf;
#endif //CONFIG_BT_COEXIST_SOCKET_TRX
	u8 cmd_idx;
	u8 len;

	cmd_idx = info->cid;

	if (info->len > buf_len-2) {
		rtw_warn_on(1);
		len = buf_len-2;
	} else {
		len = info->len;
	}

//#define DBG_PROC_SET_BTINFO_EVT
#ifdef DBG_PROC_SET_BTINFO_EVT
#ifdef CONFIG_BT_COEXIST_SOCKET_TRX
	//DBG_871X("%s: btinfo[0]=%x,btinfo[1]=%x,btinfo[2]=%x,btinfo[3]=%x
	//			btinfo[4]=%x,btinfo[5]=%x,btinfo[6]=%x,btinfo[7]=%x",__func__,buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7]);
#else//!CONFIG_BT_COEXIST_SOCKET_TRX
	btinfo_evt_dump(RTW_DBGDUMP, info);
#endif //CONFIG_BT_COEXIST_SOCKET_TRX
#endif // DBG_PROC_SET_BTINFO_EVT

	/* transform BT-FW btinfo to WiFI-FW C2H format and notify */
	if (cmd_idx == BTINFO_WIFI_FETCH)
		buf[1] = 0;
	else if (cmd_idx == BTINFO_BT_AUTO_RPT)
		buf[1] = 2;
#ifdef CONFIG_BT_COEXIST_SOCKET_TRX
	else if(0x01 == cmd_idx || 0x02 == cmd_idx)
		buf[1] = buf[0];
#endif //CONFIG_BT_COEXIST_SOCKET_TRX
	rtw_btcoex_BtInfoNotify(adapter ,len+1, &buf[1]);
}

u8 rtw_btinfo_cmd(_adapter *adapter, u8 *buf, u16 len)
{
	struct cmd_obj *ph2c;
	struct drvextra_cmd_parm *pdrvextra_cmd_parm;
	u8 *btinfo;
	struct cmd_priv *pcmdpriv = &adapter->cmdpriv;
	u8	res = _SUCCESS;

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if (ph2c == NULL) {
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm*)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (pdrvextra_cmd_parm == NULL) {
		rtw_mfree((u8*)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	btinfo = rtw_zmalloc(len);
	if (btinfo == NULL) {
		rtw_mfree((u8*)ph2c, sizeof(struct cmd_obj));
		rtw_mfree((u8*)pdrvextra_cmd_parm, sizeof(struct drvextra_cmd_parm));
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = BTINFO_WK_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = len;
	pdrvextra_cmd_parm->pbuf = btinfo;

	_rtw_memcpy(btinfo, buf, len);

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:
	return res;
}
#endif //CONFIG_BT_COEXIST

//#ifdef CONFIG_C2H_PACKET_EN
u8 rtw_c2h_packet_wk_cmd(PADAPTER padapter, u8 *pbuf, u16 length)
{
	struct cmd_obj *ph2c;
	struct drvextra_cmd_parm *pdrvextra_cmd_parm;
	struct cmd_priv *pcmdpriv = &padapter->cmdpriv;
	u8	res = _SUCCESS;

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if (ph2c == NULL) {
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm*)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (pdrvextra_cmd_parm == NULL) {
		rtw_mfree((u8*)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = C2H_WK_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size = length;
	pdrvextra_cmd_parm->pbuf = pbuf;

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);

exit:
	return res;
}

//#else //CONFIG_C2H_PACKET_EN
/* dont call R/W in this function, beucase SDIO interrupt have claim host */
/* or deadlock will happen and cause special-systemserver-died in android */

u8 rtw_c2h_wk_cmd(PADAPTER padapter, u8 *c2h_evt)
{
	struct cmd_obj *ph2c;
	struct drvextra_cmd_parm *pdrvextra_cmd_parm;
	struct cmd_priv	*pcmdpriv = &padapter->cmdpriv;
	u8	res = _SUCCESS;

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if (ph2c == NULL) {
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm = (struct drvextra_cmd_parm*)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (pdrvextra_cmd_parm == NULL) {
		rtw_mfree((u8*)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	pdrvextra_cmd_parm->ec_id = C2H_WK_CID;
	pdrvextra_cmd_parm->type = 0;
	pdrvextra_cmd_parm->size =  c2h_evt?16:0;
	pdrvextra_cmd_parm->pbuf = c2h_evt;

	init_h2fwcmd_w_parm_no_rsp(ph2c, pdrvextra_cmd_parm, GEN_CMD_CODE(_Set_Drv_Extra));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
	
exit:
	
	return res;
}
//#endif //CONFIG_C2H_PACKET_EN

u8 rtw_run_in_thread_cmd(PADAPTER padapter, void (*func)(void*), void* context)
{
	struct cmd_priv *pcmdpriv;
	struct cmd_obj *ph2c;
	struct RunInThread_param *parm;
	s32 res = _SUCCESS;

_func_enter_;

	pcmdpriv = &padapter->cmdpriv;

	ph2c = (struct cmd_obj*)rtw_zmalloc(sizeof(struct cmd_obj));
	if (NULL == ph2c) {
		res = _FAIL;
		goto exit;
	}

	parm = (struct RunInThread_param*)rtw_zmalloc(sizeof(struct RunInThread_param));
	if (NULL == parm) {
		rtw_mfree((u8*)ph2c, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	parm->func = func;
	parm->context = context;
	init_h2fwcmd_w_parm_no_rsp(ph2c, parm, GEN_CMD_CODE(_RunInThreadCMD));

	res = rtw_enqueue_cmd(pcmdpriv, ph2c);
exit:

_func_exit_;

	return res;
}

s32 c2h_evt_hdl(_adapter *adapter, u8 *c2h_evt, c2h_id_filter filter)
{
	s32 ret = _FAIL;
	u8 buf[16];

	if (!c2h_evt) {
		/* No c2h event in cmd_obj, read c2h event before handling*/
		if (rtw_hal_c2h_evt_read(adapter, buf) == _SUCCESS) {
			c2h_evt = buf;
			
			if (filter && filter(c2h_evt) == _FALSE)
				goto exit;

			ret = rtw_hal_c2h_handler(adapter, c2h_evt);
		}
	} else {

		if (filter && filter(c2h_evt) == _FALSE)
			goto exit;

		ret = rtw_hal_c2h_handler(adapter, c2h_evt);
	}
exit:
	return ret;
}

#ifdef CONFIG_C2H_WK
static void c2h_wk_callback(_workitem *work)
{
	struct evt_priv *evtpriv = container_of(work, struct evt_priv, c2h_wk);
	_adapter *adapter = container_of(evtpriv, _adapter, evtpriv);
	u8 *c2h_evt;
	c2h_id_filter ccx_id_filter = rtw_hal_c2h_id_filter_ccx(adapter);

	evtpriv->c2h_wk_alive = _TRUE;

	while (!rtw_cbuf_empty(evtpriv->c2h_queue)) {
		if ((c2h_evt = (u8 *)rtw_cbuf_pop(evtpriv->c2h_queue)) != NULL) {
			/* This C2H event is read, clear it */
			c2h_evt_clear(adapter);
		} else if ((c2h_evt = (u8 *)rtw_malloc(16)) != NULL) {
			/* This C2H event is not read, read & clear now */
			if (rtw_hal_c2h_evt_read(adapter, c2h_evt) != _SUCCESS) {
				rtw_mfree(c2h_evt, 16);
				continue;
			}
		}

		/* Special pointer to trigger c2h_evt_clear only */
		if ((void *)c2h_evt == (void *)evtpriv)
			continue;

		if (!rtw_hal_c2h_valid(adapter, c2h_evt)) {
			rtw_mfree(c2h_evt, 16);
			continue;
		}
		
		if (ccx_id_filter(c2h_evt) == _TRUE) {
			/* Handle CCX report here */
			rtw_hal_c2h_handler(adapter, c2h_evt);
			rtw_mfree(c2h_evt, 16);
		} else {
			/* Enqueue into cmd_thread for others */
			rtw_c2h_wk_cmd(adapter, c2h_evt);
		}
	}

	evtpriv->c2h_wk_alive = _FALSE;
}
#endif

u8 rtw_drvextra_cmd_hdl(_adapter *padapter, unsigned char *pbuf)
{
	struct drvextra_cmd_parm *pdrvextra_cmd;

	if(!pbuf)
		return H2C_PARAMETERS_ERROR;

	pdrvextra_cmd = (struct drvextra_cmd_parm*)pbuf;
	
	switch(pdrvextra_cmd->ec_id)
	{
		case DYNAMIC_CHK_WK_CID://only  primary padapter go to this cmd, but execute dynamic_chk_wk_hdl() for two interfaces
#ifdef CONFIG_CONCURRENT_MODE
			if(padapter->pbuddy_adapter)
			{
				dynamic_chk_wk_hdl(padapter->pbuddy_adapter);
			}	
#endif //CONFIG_CONCURRENT_MODE
			dynamic_chk_wk_hdl(padapter);
			break;
		case POWER_SAVING_CTRL_WK_CID:
			power_saving_wk_hdl(padapter);	
			break;
#ifdef CONFIG_LPS
		case LPS_CTRL_WK_CID:
			lps_ctrl_wk_hdl(padapter, (u8)pdrvextra_cmd->type);
			break;
		case DM_IN_LPS_WK_CID:			
			rtw_dm_in_lps_hdl(padapter);	
			break;
		case LPS_CHANGE_DTIM_CID:
			rtw_lps_change_dtim_hdl(padapter, (u8)pdrvextra_cmd->type);
			break;
#endif
#if (RATE_ADAPTIVE_SUPPORT==1)
		case RTP_TIMER_CFG_WK_CID:
			rpt_timer_setting_wk_hdl(padapter, pdrvextra_cmd->type);
			break;
#endif
#ifdef CONFIG_ANTENNA_DIVERSITY
		case ANT_SELECT_WK_CID:
			antenna_select_wk_hdl(padapter, pdrvextra_cmd->type);
			break;
#endif
#ifdef CONFIG_P2P_PS
		case P2P_PS_WK_CID:
			p2p_ps_wk_hdl(padapter, pdrvextra_cmd->type);
			break;
#endif //CONFIG_P2P_PS
#ifdef CONFIG_P2P
		case P2P_PROTO_WK_CID:
			//	Commented by Albert 2011/07/01
			//	I used the type_size as the type command
			p2p_protocol_wk_hdl( padapter, pdrvextra_cmd->type );
			break;
#endif //CONFIG_P2P
#ifdef CONFIG_AP_MODE
		case CHECK_HIQ_WK_CID:
			rtw_chk_hi_queue_hdl(padapter);
			break;
#endif //CONFIG_AP_MODE
#ifdef CONFIG_INTEL_WIDI
		case INTEl_WIDI_WK_CID:
			intel_widi_wk_hdl(padapter, pdrvextra_cmd->type, pdrvextra_cmd->pbuf);
			break;
#endif //CONFIG_INTEL_WIDI
		//add for CONFIG_IEEE80211W, none 11w can use it
		case RESET_SECURITYPRIV:
			reset_securitypriv_hdl(padapter);
			break;
		case FREE_ASSOC_RESOURCES:
			free_assoc_resources_hdl(padapter);
			break;
		case C2H_WK_CID:
#ifdef CONFIG_C2H_PACKET_EN
			rtw_hal_set_hwreg_with_buf(padapter, HW_VAR_C2H_HANDLE, pdrvextra_cmd->pbuf, pdrvextra_cmd->size);
#else		
			c2h_evt_hdl(padapter, pdrvextra_cmd->pbuf, NULL);
#endif
			break;
#ifdef CONFIG_BEAMFORMING
		case BEAMFORMING_WK_CID:
			beamforming_wk_hdl(padapter, pdrvextra_cmd->type, pdrvextra_cmd->pbuf);
			break;
#endif //CONFIG_BEAMFORMING
		case DM_RA_MSK_WK_CID:
			rtw_dm_ra_mask_hdl(padapter, (struct sta_info *)pdrvextra_cmd->pbuf);
			break;
#ifdef CONFIG_BT_COEXIST
		case BTINFO_WK_CID:
			rtw_btinfo_hdl(padapter ,pdrvextra_cmd->pbuf, pdrvextra_cmd->size);
			break;
#endif
		case EN_HW_UPDATE_TSF_WK_CID:
			rtw_hal_set_hwreg(padapter, HW_VAR_EN_HW_UPDATE_TSF, NULL);
			break;
		default:
			break;
	}

	if (pdrvextra_cmd->pbuf && pdrvextra_cmd->size>0)
	{
		rtw_mfree(pdrvextra_cmd->pbuf, pdrvextra_cmd->size);
	}

	return H2C_SUCCESS;
}

void rtw_survey_cmd_callback(_adapter*	padapter ,  struct cmd_obj *pcmd)
{
	struct 	mlme_priv *pmlmepriv = &padapter->mlmepriv;

_func_enter_;

	if(pcmd->res == H2C_DROPPED)
	{
		//TODO: cancel timer and do timeout handler directly...
		//need to make timeout handlerOS independent
		mlme_set_scan_to_timer(pmlmepriv, 1);
	}
	else if (pcmd->res != H2C_SUCCESS) {
		mlme_set_scan_to_timer(pmlmepriv, 1);
		RT_TRACE(_module_rtl871x_cmd_c_,_drv_err_,("\n ********Error: MgntActrtw_set_802_11_bssid_LIST_SCAN Fail ************\n\n."));
	} 

	// free cmd
	rtw_free_cmd_obj(pcmd);

_func_exit_;	
}
void rtw_disassoc_cmd_callback(_adapter*	padapter,  struct cmd_obj *pcmd)
{
	_irqL	irqL;
	struct 	mlme_priv *pmlmepriv = &padapter->mlmepriv;
	
_func_enter_;	

	if (pcmd->res != H2C_SUCCESS)
	{
		_enter_critical_bh(&pmlmepriv->lock, &irqL);
		set_fwstate(pmlmepriv, _FW_LINKED);
		_exit_critical_bh(&pmlmepriv->lock, &irqL);
				
		RT_TRACE(_module_rtl871x_cmd_c_,_drv_err_,("\n ***Error: disconnect_cmd_callback Fail ***\n."));

		goto exit;
	}
#ifdef CONFIG_BR_EXT
	else //clear bridge database
		nat25_db_cleanup(padapter);
#endif //CONFIG_BR_EXT

	// free cmd
	rtw_free_cmd_obj(pcmd);
	
exit:
	
_func_exit_;	
}


void rtw_joinbss_cmd_callback(_adapter*	padapter,  struct cmd_obj *pcmd)
{
	struct 	mlme_priv *pmlmepriv = &padapter->mlmepriv;

_func_enter_;	

	if(pcmd->res == H2C_DROPPED)
	{
		//TODO: cancel timer and do timeout handler directly...
		//need to make timeout handlerOS independent
		_set_timer(&pmlmepriv->assoc_timer, 1);
	}
	else if(pcmd->res != H2C_SUCCESS)
	{
		_set_timer(&pmlmepriv->assoc_timer, 1);
	}

	rtw_free_cmd_obj(pcmd);
	
_func_exit_;	
}

void rtw_createbss_cmd_callback(_adapter *padapter, struct cmd_obj *pcmd)
{	
	_irqL irqL;
	u8 timer_cancelled;
	struct sta_info *psta = NULL;
	struct wlan_network *pwlan = NULL;		
	struct 	mlme_priv *pmlmepriv = &padapter->mlmepriv;	
	WLAN_BSSID_EX *pnetwork = (WLAN_BSSID_EX *)pcmd->parmbuf;
	struct wlan_network *tgt_network = &(pmlmepriv->cur_network);

_func_enter_;	

	if (pcmd->parmbuf == NULL)
		goto exit;

	if((pcmd->res != H2C_SUCCESS))
	{	
		RT_TRACE(_module_rtl871x_cmd_c_,_drv_err_,("\n ********Error: rtw_createbss_cmd_callback  Fail ************\n\n."));
		_set_timer(&pmlmepriv->assoc_timer, 1 );		
	}
	
	_cancel_timer(&pmlmepriv->assoc_timer, &timer_cancelled);

#ifdef CONFIG_FW_MLMLE
       //endian_convert
	pnetwork->Length = le32_to_cpu(pnetwork->Length);
  	pnetwork->Ssid.SsidLength = le32_to_cpu(pnetwork->Ssid.SsidLength);
	pnetwork->Privacy =le32_to_cpu(pnetwork->Privacy);
	pnetwork->Rssi = le32_to_cpu(pnetwork->Rssi);
	pnetwork->NetworkTypeInUse =le32_to_cpu(pnetwork->NetworkTypeInUse);	
	pnetwork->Configuration.ATIMWindow = le32_to_cpu(pnetwork->Configuration.ATIMWindow);
	//pnetwork->Configuration.BeaconPeriod = le32_to_cpu(pnetwork->Configuration.BeaconPeriod);
	pnetwork->Configuration.DSConfig =le32_to_cpu(pnetwork->Configuration.DSConfig);
	pnetwork->Configuration.FHConfig.DwellTime=le32_to_cpu(pnetwork->Configuration.FHConfig.DwellTime);
	pnetwork->Configuration.FHConfig.HopPattern=le32_to_cpu(pnetwork->Configuration.FHConfig.HopPattern);
	pnetwork->Configuration.FHConfig.HopSet=le32_to_cpu(pnetwork->Configuration.FHConfig.HopSet);
	pnetwork->Configuration.FHConfig.Length=le32_to_cpu(pnetwork->Configuration.FHConfig.Length);	
	pnetwork->Configuration.Length = le32_to_cpu(pnetwork->Configuration.Length);
	pnetwork->InfrastructureMode = le32_to_cpu(pnetwork->InfrastructureMode);
	pnetwork->IELength = le32_to_cpu(pnetwork->IELength);
#endif
	
	_enter_critical_bh(&pmlmepriv->lock, &irqL);
	
	
	if(check_fwstate(pmlmepriv, WIFI_AP_STATE) )
	{
		psta = rtw_get_stainfo(&padapter->stapriv, pnetwork->MacAddress);
		if(!psta)
		{
		psta = rtw_alloc_stainfo(&padapter->stapriv, pnetwork->MacAddress);
		if (psta == NULL) 
		{ 
			RT_TRACE(_module_rtl871x_cmd_c_,_drv_err_,("\nCan't alloc sta_info when createbss_cmd_callback\n"));
			goto createbss_cmd_fail ;
		}
		}	
			
		rtw_indicate_connect( padapter);
	}
	else
	{	
		_irqL	irqL;

		pwlan = _rtw_alloc_network(pmlmepriv);
		_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
		if ( pwlan == NULL)
		{
			pwlan = rtw_get_oldest_wlan_network(&pmlmepriv->scanned_queue);
			if( pwlan == NULL)
			{
				RT_TRACE(_module_rtl871x_cmd_c_,_drv_err_,("\n Error:  can't get pwlan in rtw_joinbss_event_callback \n"));
				_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
				goto createbss_cmd_fail;
			}
			pwlan->last_scanned = rtw_get_current_time();			
		}	
		else
		{
			rtw_list_insert_tail(&(pwlan->list), &pmlmepriv->scanned_queue.queue);
		}
				
		pnetwork->Length = get_WLAN_BSSID_EX_sz(pnetwork);
		_rtw_memcpy(&(pwlan->network), pnetwork, pnetwork->Length);
		//pwlan->fixed = _TRUE;

		//rtw_list_insert_tail(&(pwlan->list), &pmlmepriv->scanned_queue.queue);

		// copy pdev_network information to 	pmlmepriv->cur_network
		_rtw_memcpy(&tgt_network->network, pnetwork, (get_WLAN_BSSID_EX_sz(pnetwork)));

		// reset DSConfig
		//tgt_network->network.Configuration.DSConfig = (u32)rtw_ch2freq(pnetwork->Configuration.DSConfig);

		_clr_fwstate_(pmlmepriv, _FW_UNDER_LINKING);

#if 0		
		if((pmlmepriv->fw_state) & WIFI_AP_STATE)
		{
			psta = rtw_alloc_stainfo(&padapter->stapriv, pnetwork->MacAddress);

			if (psta == NULL) { // for AP Mode & Adhoc Master Mode
				RT_TRACE(_module_rtl871x_cmd_c_,_drv_err_,("\nCan't alloc sta_info when createbss_cmd_callback\n"));
				goto createbss_cmd_fail ;
			}
			
			rtw_indicate_connect( padapter);
		}
		else {

			//rtw_indicate_disconnect(dev);
		}		
#endif
		_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
		// we will set _FW_LINKED when there is one more sat to join us (rtw_stassoc_event_callback)
			
	}

createbss_cmd_fail:
	
	_exit_critical_bh(&pmlmepriv->lock, &irqL);
exit:
	rtw_free_cmd_obj(pcmd);
	
_func_exit_;	

}



void rtw_setstaKey_cmdrsp_callback(_adapter*	padapter ,  struct cmd_obj *pcmd)
{
	
	struct sta_priv * pstapriv = &padapter->stapriv;
	struct set_stakey_rsp* psetstakey_rsp = (struct set_stakey_rsp*) (pcmd->rsp);
	struct sta_info*	psta = rtw_get_stainfo(pstapriv, psetstakey_rsp->addr);

_func_enter_;	

	if(psta==NULL)
	{
		RT_TRACE(_module_rtl871x_cmd_c_,_drv_err_,("\nERROR: rtw_setstaKey_cmdrsp_callback => can't get sta_info \n\n"));
		goto exit;
	}
	
	//psta->aid = psta->mac_id = psetstakey_rsp->keyid; //CAM_ID(CAM_ENTRY)
	
exit:	

	rtw_free_cmd_obj(pcmd);
	
_func_exit_;	

}
void rtw_setassocsta_cmdrsp_callback(_adapter*	padapter,  struct cmd_obj *pcmd)
{
	_irqL	irqL;
	struct sta_priv * pstapriv = &padapter->stapriv;
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;	
	struct set_assocsta_parm* passocsta_parm = (struct set_assocsta_parm*)(pcmd->parmbuf);
	struct set_assocsta_rsp* passocsta_rsp = (struct set_assocsta_rsp*) (pcmd->rsp);		
	struct sta_info*	psta = rtw_get_stainfo(pstapriv, passocsta_parm->addr);

_func_enter_;	
	
	if(psta==NULL)
	{
		RT_TRACE(_module_rtl871x_cmd_c_,_drv_err_,("\nERROR: setassocsta_cmdrsp_callbac => can't get sta_info \n\n"));
		goto exit;
	}
	
	psta->aid = psta->mac_id = passocsta_rsp->cam_id;

	_enter_critical_bh(&pmlmepriv->lock, &irqL);

	if ((check_fwstate(pmlmepriv, WIFI_MP_STATE) == _TRUE) && (check_fwstate(pmlmepriv, _FW_UNDER_LINKING) == _TRUE))           	
		_clr_fwstate_(pmlmepriv, _FW_UNDER_LINKING);

	set_fwstate(pmlmepriv, _FW_LINKED);
	_exit_critical_bh(&pmlmepriv->lock, &irqL);

exit:
	rtw_free_cmd_obj(pcmd);

_func_exit_;
}

void rtw_getrttbl_cmd_cmdrsp_callback(_adapter*	padapter,  struct cmd_obj *pcmd);
void rtw_getrttbl_cmd_cmdrsp_callback(_adapter*	padapter,  struct cmd_obj *pcmd)
{
_func_enter_;

	rtw_free_cmd_obj(pcmd);
#ifdef CONFIG_MP_INCLUDED
	if (padapter->registrypriv.mp_mode == 1)
		padapter->mppriv.workparam.bcompleted=_TRUE;
#endif

_func_exit_;

}

