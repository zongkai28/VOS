//----------------------------------------------------
// Copyright (c) 2020, VOS Open source. All rights reserved.
// Author: 156439848@qq.com; vincent_cws2008@gmail.com
// History:
//	     2020-08-01: initial by vincent.
//------------------------------------------------------

#include "vconf.h"
#include "vos.h"
#include "vlist.h"

#ifndef MAX_VOS_SEMAPHONRE_NUM
#define MAX_VOS_SEMAPHONRE_NUM  2
#endif

#ifndef MAX_VOS_MUTEX_NUM
#define MAX_VOS_MUTEX_NUM   2
#endif

#ifndef MAX_VOS_MSG_QUE_NUM
#define MAX_VOS_MSG_QUE_NUM   2
#endif

extern struct StVosTask *pRunningTask;
extern volatile u32  gVOSTicks;
extern volatile u32 gMarkTicksNearest;

static struct list_head gListSemaphore;//空闲信号量链表

static struct list_head gListMutex;//空闲互斥锁链表

static struct list_head gListMsgQue;//空闲消息队列链表

StVOSSemaphore gVOSSemaphore[MAX_VOS_SEMAPHONRE_NUM];

StVOSMutex gVOSMutex[MAX_VOS_MUTEX_NUM];

StVOSMsgQueue gVOSMsgQue[MAX_VOS_MSG_QUE_NUM];

extern volatile u32 VOSIntNesting;

void VOSSemInit()
{
	s32 i = 0;
	u32 irq_save = 0;
	irq_save = __vos_irq_save();
	//初始化信号量队列
	INIT_LIST_HEAD(&gListSemaphore);
	//把所有任务链接到空闲信号量队列中
	for (i=0; i<MAX_VOS_SEMAPHONRE_NUM; i++) {
		list_add_tail(&gVOSSemaphore[i].list, &gListSemaphore);
	}
	__vos_irq_restore(irq_save);
}

StVOSSemaphore *VOSSemCreate(s32 max_sems, s32 init_sems, s8 *name)
{
	StVOSSemaphore *pSemaphore = 0;
	u32 irq_save = 0;
	irq_save = __vos_irq_save();
	if (!list_empty(&gListSemaphore)) {
		pSemaphore = list_entry(gListSemaphore.next, StVOSSemaphore, list); //获取第一个空闲信号量
		list_del(gListSemaphore.next);
		pSemaphore->name = name;
		pSemaphore->max = max_sems;
		if (init_sems > max_sems) {
			init_sems = max_sems;
		}
		pSemaphore->left = init_sems; //初始化信号量为满
		INIT_LIST_HEAD(&pSemaphore->list_task);
		memset(pSemaphore->bitmap, 0, sizeof(pSemaphore->bitmap)); //清空位图，位图指向占用该信号量的任务id.
	}
	__vos_irq_restore(irq_save);
	return  pSemaphore;
}


s32 VOSSemTryWait(StVOSSemaphore *pSem)
{
	s32 ret = -1;
	u32 irq_save = 0;
	if (pSem==0) return -1; //信号量可能不存在或者被释放

	irq_save = __vos_irq_save();

	if (pSem->left > 0) {
		pSem->left--;
		ret = 1;
	}
	__vos_irq_restore(irq_save);
	return ret;
}
s32 VOSSemWait(StVOSSemaphore *pSem, u32 timeout_ms)
{
	s32 ret = 0;
	u32 irq_save = 0;
	if (pSem==0) return -1; //信号量可能不存在或者被释放

	irq_save = __vos_irq_save();

	if (pSem->left > 0) {
		pSem->left--;
		ret = 1;	}
	else if (VOSIntNesting==0) {
		//把当前任务切换到阻塞队列
		pRunningTask->status = VOS_STA_BLOCK; //添加到阻塞队列

		//信号量阻塞类型
		pRunningTask->block_type |= VOS_BLOCK_SEMP;//信号量类型
		pRunningTask->psyn = pSem;
		//同时是超时时间类型
		pRunningTask->ticks_start = gVOSTicks;
		pRunningTask->ticks_alert = gVOSTicks + MAKE_TICKS(timeout_ms);
		//gMarkTicksNearest > pRunningTask->ticks_alert 就需要更新时间
		if (TICK_CMP(gMarkTicksNearest,pRunningTask->ticks_alert,pRunningTask->ticks_start)>0) {
			gMarkTicksNearest = pRunningTask->ticks_alert;//更新为最近的闹钟
		}
		pRunningTask->block_type |= VOS_BLOCK_DELAY;//指明阻塞类型为自延时

		//插入到延时队列，也同时插入到阻塞队列
		VOSTaskDelayListInsert(pRunningTask);
		VOSTaskBlockListInsert(pRunningTask, &pSem->list_task);

	}
	if (VOSIntNesting) {//在中断上下文里,直接返回-1
		ret = -1;
	}
	__vos_irq_restore(irq_save);

	if (ret==0) { //没信号量，进入阻塞队列

		VOSTaskSchedule(); //任务调度并进入阻塞队列
		switch(pRunningTask->wakeup_from) { //阻塞后是被定时器唤醒或者信号量唤醒
		case VOS_WAKEUP_FROM_DELAY:
			ret = 0;
			break;
		case VOS_WAKEUP_FROM_SEM:
			ret = 1;
			break;
		case VOS_WAKEUP_FROM_SEM_DEL:
			ret = -1;
			break;
		default:
			ret = 0;
			//printf info here
			break;
		}
	}
	return ret;
}

s32 VOSSemRelease(StVOSSemaphore *pSem)
{
	s32 ret = 0;
	struct StVosTask *pBlockTask = 0;
	u32 irq_save = 0;
	if (pSem == 0) return -1; //信号量可能不存在或者被释放

	irq_save = __vos_irq_save();

	if (pSem->left < pSem->max) {
		if (!list_empty(&pSem->list_task)) {
			pBlockTask = list_entry(pSem->list_task.next, struct StVosTask, list);
			VOSTaskBlockListRelease(pBlockTask);//唤醒在阻塞队列里阻塞的等待该信号量的任务,每次都唤醒优先级最高的那个
		}
		else {//没任务阻塞，就直接加加信号量
			pSem->left++; //释放信号量
		}
		//修改状态
		pBlockTask->status = VOS_STA_READY;
		pBlockTask->block_type = 0;
		pBlockTask->ticks_start = 0;
		pBlockTask->ticks_alert = 0;
		pBlockTask->psyn = 0;
		if (pSem->distory == 0) {
			pBlockTask->wakeup_from = VOS_WAKEUP_FROM_SEM;
		}
		else {
			pBlockTask->wakeup_from = VOS_WAKEUP_FROM_SEM_DEL;
		}
		pRunningTask->psyn = 0; //清除指向资源的指针。
		ret = 1;
	}
	__vos_irq_restore(irq_save);
	if (VOSIntNesting == 0 && ret == 1) { //如果在中断上下文调用(VOSIntNesting!=0),不用主动调度
		//唤醒后，立即调用任务调度，万一唤醒的任务优先级高于当前任务，则切换,
		//但不能用VOSTaskSwitch(TASK_SWITCH_USER);这是必须在特权模式下使用。
		VOSTaskSchedule();
	}
	return ret;
}



s32 VOSSemDelete(StVOSSemaphore *pSem)
{
	s32 ret = -1;
	u32 irq_save = 0;

	if (pSem==0) return -1; //信号量可能不存在或者被释放

	irq_save = __vos_irq_save();
	if (!list_empty(&gListSemaphore)) {

		if (pSem->max == pSem->left) {//所有任务都归还信号量，这时可以释放。
			list_del(pSem);
			list_add_tail(&pSem->list, &gListSemaphore);
			pSem->max = 0;
			pSem->name = 0;
			pSem->left = 0;
			//pSem->distory = 0;
			ret = 0;
		}
	}
	__vos_irq_restore(irq_save);
	return ret;
}


void VOSMutexInit()
{
	s32 i = 0;
	u32 irq_save = 0;
	irq_save = __vos_irq_save();
	//初始化信号量队列
	INIT_LIST_HEAD(&gListMutex);
	//把所有任务链接到空闲信号量队列中
	for (i=0; i<MAX_VOS_MUTEX_NUM; i++) {
		list_add_tail(&gVOSMutex[i].list, &gListMutex);
		gVOSMutex[i].counter = -1; //初始化为-1，表示无效
		gVOSMutex[i].ptask_owner = 0;
	}
	__vos_irq_restore(irq_save);
}
//创建是可以在任务之外创建
StVOSMutex *VOSMutexCreate(s8 *name)
{
	StVOSMutex *pMutex = 0;
	u32 irq_save = 0;

	irq_save = __vos_irq_save();
	if (!list_empty(&gListMutex)) {
		pMutex = list_entry(gListMutex.next, StVOSMutex, list); //获取第一个空闲互斥锁
		list_del(gListMutex.next);
		pMutex->name = name;
		pMutex->counter = 1;
		pMutex->ptask_owner =  0;
		INIT_LIST_HEAD(&pMutex->list_task);
	}
	__vos_irq_restore(irq_save);
	return  pMutex;
}


s32 VOSMutexWait(StVOSMutex *pMutex, u32 timeout_ms)
{
	s32 ret = 0;
	u32 irq_save = 0;
	if (pRunningTask == 0) return -1; //需要在用户任务里执行
	if (pMutex->counter == -1) return -1; //信号量可能不存在或者被释放

	if (VOSIntNesting != 0) return -1;

	if (pMutex->counter > 1) pMutex->counter = 1;

	irq_save = __vos_irq_save();

	if (pMutex->counter > 0) {
		pMutex->counter--;
		pMutex->ptask_owner = pRunningTask;
		ret = 1;
	}
	else {//把当前任务切换到阻塞队列
		pRunningTask->status = VOS_STA_BLOCK; //添加到阻塞队列

		//信号量阻塞类型
		pRunningTask->block_type |= VOS_BLOCK_MUTEX;//互斥锁类型
		pRunningTask->psyn = pMutex;
		//同时是超时时间类型
		pRunningTask->ticks_start = gVOSTicks;
		pRunningTask->ticks_alert = gVOSTicks + MAKE_TICKS(timeout_ms);

		//gMarkTicksNearest > pRunningTask->ticks_alert 就需要更新时间
		if (TICK_CMP(gMarkTicksNearest,pRunningTask->ticks_alert,pRunningTask->ticks_start) > 0) {
			gMarkTicksNearest = pRunningTask->ticks_alert;//更新为最近的闹钟
		}
		pRunningTask->block_type |= VOS_BLOCK_DELAY;//指明阻塞类型为自延时

		VOSTaskRaisePrioBeforeBlock(pMutex); //阻塞前处理优先级反转问题，提升就绪队列里的任务优先级
		//插入到延时队列，也同时插入到阻塞队列
		VOSTaskDelayListInsert(pRunningTask);
		VOSTaskBlockListInsert(pRunningTask, &pMutex->list_task);
	}

	__vos_irq_restore(irq_save);

	if (ret==0) { //没获取互斥锁，进入阻塞队列

		VOSTaskSchedule(); //任务调度并进入阻塞队列
		switch(pRunningTask->wakeup_from) { //阻塞后是被定时器唤醒或者互斥锁唤醒
		case VOS_WAKEUP_FROM_DELAY:
			ret = 0;
			break;
		case VOS_WAKEUP_FROM_MUTEX:
			ret = 1;
			break;
		case VOS_WAKEUP_FROM_MUTEX_DEL:
			ret = -1;
			break;
		default:
			ret = 0;
			break;
		}
	}
	return ret;
}

s32 VOSMutexRelease(StVOSMutex *pMutex)
{
	s32 ret = 0;
	struct StVosTask *pBlockTask = 0;
	u32 irq_save = 0;

	if (pMutex->counter == -1) return -1; //互斥锁可能不存在或者被释放
	if (pMutex->counter > 1) pMutex->counter = 1;

	if (VOSIntNesting != 0) return -1;

	irq_save = __vos_irq_save();

	if (pMutex->counter < 1 && pMutex->ptask_owner == pRunningTask) {//互斥量必须要由拥有者才能释放

		if (!list_empty(&pMutex->list_task)) {
			pBlockTask = list_entry(pMutex->list_task.next, struct StVosTask, list);
			pMutex->ptask_owner = pBlockTask;
			VOSTaskBlockListRelease(pBlockTask);//唤醒在阻塞队列里阻塞的等待该信号量的任务,每次都唤醒优先级最高的那个
		}
		else {//释放互斥锁, 没等待该锁任务，直接释放
			pMutex->counter++;
		}
		VOSTaskRestorePrioBeforeRelease();//恢复自身的优先级
		pRunningTask->psyn = 0; //清除指向资源的指针。
		pMutex->ptask_owner = 0; //清除拥有者

		//修改状态
		pBlockTask->status = VOS_STA_READY;
		pBlockTask->block_type = 0;
		pBlockTask->ticks_start = 0;
		pBlockTask->ticks_alert = 0;
		//ptask_block->psyn = 0;
		if (pMutex->distory == 0) {
			pBlockTask->wakeup_from = VOS_WAKEUP_FROM_MUTEX;
		}
		else {
			pBlockTask->wakeup_from = VOS_WAKEUP_FROM_MUTEX_DEL;
		}

		ret = 1;
	}
	__vos_irq_restore(irq_save);
	if (ret == 1) {
		//唤醒后，立即调用任务调度，万一唤醒的任务优先级高于当前任务，则切换,
		//但不能用VOSTaskSwitch(TASK_SWITCH_USER);这是必须在特权模式下使用。
		VOSTaskSchedule();
	}
	return ret;
}

s32 VOSMutexDelete(StVOSMutex *pMutex)
{
	s32 ret = -1;
	u32 irq_save = 0;

	if (VOSIntNesting != 0) return -1;

	irq_save = __vos_irq_save();
	if (!list_empty(&gListSemaphore)) {

		if (pMutex->ptask_owner == 0 && pMutex->counter == 1) {//互斥锁没被引用，直接释放
			//删除自己
			list_del(pMutex);
			list_add_tail(&pMutex->list, &gListMutex);
			pMutex->name = 0;
			pMutex->counter = -1;
			//pMutex->distory = 0;
			ret = 0;
		}
	}
	__vos_irq_restore(irq_save);
	return ret;
}

s32 VOSEventWait(u32 event, u32 timeout_ms)
{
	s32 ret = 0;
	u32 irq_save = 0;

	if (VOSIntNesting != 0) return -1; //中断上下文，直接返回-1

	irq_save = __vos_irq_save();

	if (event & pRunningTask->event_mask) {
		pRunningTask->event_mask &= (~event); //事件match，就直接退出
	}
	else {
		//把当前任务切换到阻塞队列
		pRunningTask->status = VOS_STA_BLOCK; //添加到阻塞队列

		//信号量阻塞类型
		pRunningTask->block_type |= VOS_BLOCK_EVENT;//事件类型
		pRunningTask->psyn = 0;
		//pRunningTask->event_req = event; //记录请求的事件
		pRunningTask->event_mask |= event;
		//同时是超时时间类型
		pRunningTask->ticks_start = gVOSTicks;
		pRunningTask->ticks_alert = gVOSTicks + MAKE_TICKS(timeout_ms);
		//gMarkTicksNearest > pRunningTask->ticks_alert 就需要更新时间
		if (TICK_CMP(gMarkTicksNearest,pRunningTask->ticks_alert,pRunningTask->ticks_start) > 0) {
			gMarkTicksNearest = pRunningTask->ticks_alert;//更新为最近的闹钟
		}
		pRunningTask->block_type |= VOS_BLOCK_DELAY;//指明阻塞类型为自延时

		//直接挂在延时全局链表，唤醒就直接在全局延时里查询
		VOSTaskDelayListInsert(pRunningTask);
	}

	__vos_irq_restore(irq_save);

	if (ret==0) { //没获取互斥锁，进入阻塞队列

		VOSTaskSchedule(); //任务调度并进入阻塞队列
		switch(pRunningTask->wakeup_from) { //阻塞后是被定时器唤醒或者互斥锁唤醒
		case VOS_WAKEUP_FROM_DELAY:
			ret = 0;
			break;
		case VOS_WAKEUP_FROM_EVENT:
			ret = 1;
			break;
		default:
			ret = -1;
			break;
		}
	}
	return ret;
}

s32 VOSEventSet(s32 task_id, u32 event)
{
	s32 ret = -1;
	u32 irq_save = 0;

	irq_save = __vos_irq_save();
	StVosTask *pTask = VOSGetTaskFromId(task_id);
	if (pTask && pTask->block_type == (VOS_BLOCK_DELAY|VOS_BLOCK_EVENT)) {

		if ((pTask->event_stop & event) == 0 && //屏蔽接收指定信号
				pTask->event_mask & event) { //或者设置的事件被mask到就触发操作

			VOSTaskBlockListRelease(pTask);//唤醒在阻塞队列里阻塞的等待该信号量的任务,每次都唤醒优先级最高的那个
			//修改状态
			pTask->status = VOS_STA_READY;
			pTask->block_type = 0;
			pTask->ticks_start = 0;
			pTask->ticks_alert = 0;
			//pTask->event_req = 0;
			pTask->event_mask &= (~event);//清除事件位，该阻塞任务已经获取到事件通知
			pTask->wakeup_from = VOS_WAKEUP_FROM_EVENT;
			ret = 1;
		}
		else {
			pTask->event_mask |= event;
		}
	}
	else {//设置该事件位有效
		pTask->event_mask |= event;
	}
	__vos_irq_restore(irq_save);

	if (VOSIntNesting == 0 && ret == 1) {
		//唤醒后，立即调用任务调度，万一唤醒的任务优先级高于当前任务，则切换,
		//但不能用VOSTaskSwitch(TASK_SWITCH_USER);这是必须在特权模式下使用。
		VOSTaskSchedule();
	}
	return ret;
}

u32 VOSEventGet(s32 task_id)
{
	u32 event_mask = 0;
	u32 irq_save = 0;

	if (VOSIntNesting != 0) return -1;

	irq_save = __vos_irq_save();
	StVosTask *pTask = VOSGetTaskFromId(task_id);
	if (pTask) {
		event_mask = pTask->event_mask;
	}
	__vos_irq_restore(irq_save);

	return event_mask;
}

s32 VOSEventDisable(s32 task_id, u32 event)
{
	u32 mask = 0;
	u32 irq_save = 0;

	if (VOSIntNesting != 0) return -1;

	irq_save = __vos_irq_save();
	StVosTask *pTask = VOSGetTaskFromId(task_id);
	if (pTask) {
		pTask->event_stop |= event;
		mask = pTask->event_stop;
	}
	__vos_irq_restore(irq_save);

	return mask;
}
s32 VOSEventEnable(s32 task_id, u32 event)
{
	u32 mask = 0;
	u32 irq_save = 0;

	if (VOSIntNesting != 0) return -1;

	irq_save = __vos_irq_save();
	StVosTask *pTask = VOSGetTaskFromId(task_id);
	if (pTask) {
		pTask->event_stop &= (~event);
		mask = pTask->event_stop;
	}
	__vos_irq_restore(irq_save);

	return mask;
}
/**************************************
 * 消息队列和邮箱直接使用信号量实现
 **************************************/
void VOSMsgQueInit()
{
	s32 i = 0;
	u32 irq_save = 0;

	irq_save = __vos_irq_save();
	//初始化信号量队列
	INIT_LIST_HEAD(&gListMsgQue);
	//把所有任务链接到空闲信号量队列中
	for (i=0; i<MAX_VOS_MSG_QUE_NUM; i++) {
		list_add_tail(&gVOSMsgQue[i].list, &gListMsgQue);
	}
	__vos_irq_restore(irq_save);
}

StVOSMsgQueue *VOSMsgQueCreate(s8 *pRingBuf, s32 length, s32 msg_size, s8 *name)
{
	StVOSMsgQueue *pMsgQue = 0;
	u32 irq_save = 0;

	irq_save = __vos_irq_save();

	if (!list_empty(&gListMsgQue)) {
		pMsgQue = list_entry(gListMsgQue.next, StVOSMsgQueue, list);
		list_del(gListMsgQue.next);
		pMsgQue->name = name;
		pMsgQue->pdata = pRingBuf;
		pMsgQue->length = length;
		pMsgQue->pos_head = 0;
		pMsgQue->pos_tail = 0;
		pMsgQue->msg_cnts = 0;
		pMsgQue->msg_maxs = length/msg_size;
		pMsgQue->msg_size = msg_size;
		pMsgQue->psem = VOSSemCreate(pMsgQue->msg_maxs, 0, "mq_vos");
	}
	__vos_irq_restore(irq_save);
	return  pMsgQue;
}
//返回添加的个数，成功返回1，表示添加一个消息成功；如果返回0，表示队列满。
s32 VOSMsgQuePut(StVOSMsgQueue *pMQ, void *pmsg, s32 len)
{
	s32 ret = 0;
	u8 *ptail = 0;

	if (pMQ->psem == 0) return -1;

	if (pMQ->pos_tail != pMQ->pos_head || //头不等于尾，可以添加新消息
		pMQ->msg_cnts == 0) { //队列为空，可以添加新消息
		ptail = pMQ->pdata + pMQ->pos_tail * pMQ->msg_size;
		len = (len <= pMQ->msg_size) ? len : pMQ->msg_size;
		memcpy(ptail, pmsg, len);
		pMQ->pos_tail++;
		pMQ->pos_tail = pMQ->pos_tail % pMQ->msg_maxs;
		pMQ->msg_cnts++;

		VOSSemRelease(pMQ->psem);
	}
	return ret;
}
//返回添加的个数
s32 VOSMsgQueGet(StVOSMsgQueue *pMQ, void *pmsg, s32 len, u32 timeout_ms)
{
	s32 ret = 0;
	u8 *phead = 0;

	if (pMQ->psem == 0) return -1;

	if ((ret = VOSSemWait(pMQ->psem, timeout_ms)) > 0) {
		phead = pMQ->pdata + pMQ->pos_head * pMQ->msg_size;
		len = (len <= pMQ->msg_size) ? len : pMQ->msg_size;
		memcpy(pmsg, phead, len);
		pMQ->pos_head++;
		pMQ->pos_head = pMQ->pos_head % pMQ->msg_maxs;
		pMQ->msg_cnts--;
	}
	return ret;
}

s32 VOSMsgQueFree(StVOSMsgQueue *pMQ)
{
	s32 ret = -1;
	u32 irq_save = 0;

	irq_save = __vos_irq_save();
	if (!list_empty(&gListMsgQue)) {

		if (pMQ->msg_cnts == 0) {//消息为空，释放

			list_del(pMQ);
			pMQ->distory = 0;
			pMQ->name = 0;
			pMQ->pdata = 0;
			pMQ->length = 0;
			pMQ->pos_head = 0;
			pMQ->pos_tail = 0;
			pMQ->msg_cnts = 0;
			pMQ->msg_maxs = 0;
			pMQ->msg_size = 0;
			ret = 0;
		}
	}
	__vos_irq_restore(irq_save);
	return ret;
}

