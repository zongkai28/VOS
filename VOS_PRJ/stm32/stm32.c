/********************************************************************************************************
* 版    权: Copyright (c) 2020, VOS Open source. All rights reserved.
* 文    件: stm32.c
* 作    者: 156439848@qq.com; vincent_cws2008@gmail.com
* 版    本: VOS V1.0
* 历    史：
* --20200801：创建文件
* --20200828：添加注释
*********************************************************************************************************/

#include "misc.h"
#include "vtype.h"
#include "vos.h"

void SystemInit(void);
void HAL_IncTick(void);
void VOSExceptHandler(u32 *sp, s32 is_psp);



/********************************************************************************************************
* 函数：void misc_init();
* 描述:  综合初始化
* 参数:
* 返回：无
* 注意：无
*********************************************************************************************************/
void misc_init()
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	uart_init(115200);
 	//TIM3_Int_Init(5000-1,8400-1);
}

/********************************************************************************************************
* 函数：void VOSSysTickSet();
* 描述:  设置tick间隔，当前1ms间隔
* 参数:
* 返回：无
* 注意：无
*********************************************************************************************************/
void VOSSysTickSet()
{
	SystemCoreClockUpdate();
	SysTick_Config(168000);
}

/********************************************************************************************************
* 函数：void SysTick_Handler();
* 描述:  SysTick中断处理例程
* 参数:
* 返回：无
* 注意：无
*********************************************************************************************************/
void __attribute__ ((section(".after_vectors")))
SysTick_Handler()
{
	VOSIntEnter();
	VOSSysTick();
	VOSIntExit ();
}

/********************************************************************************************************
* 函数：void SVC_Handler_C(u32 *svc_args, s32 is_psp);
* 描述:  SVC中断处理例程
* 参数:
* [1] svc_args: 任务切换中断前的任务栈地址
* [2] is_psp: 指示psp栈还是msp, 因为汇编中断函数lr推入到栈，这里需要做处理
* 返回：无
* 注意：无
*********************************************************************************************************/
void SVC_Handler_C(u32 *svc_args, s32 is_psp)
{

	VOSIntEnter();
	StVosSysCallParam *psa;
	u8 svc_number;
	u32 irq_save;
	u32 offset = 0;
	if (!is_psp) {
		offset = 1;//+1是汇编里把lr也push一个到msp，所以这里要加1
	}
	irq_save = __local_irq_save();
	svc_number = ((char *)svc_args[6+offset])[-2]; //+1是汇编里把lr也push一个，所以这里要加1
	switch(svc_number) {
	case VOS_SVC_NUM_SYSCALL://系统调用
//		psa = (StVosSysCallParam *)svc_args[0+offset];
//		VOSSysCall(psa);
		break;

	case VOS_SVC_PRIVILEGED_MODE: //svc 6 切换到特权并关中断
		svc_args[0+offset] = __switch_privileged_mode();//返回切换前的control[0]状态
		break;

	default:
		kprintf("ERROR: SVC_Handler_C!\r\n");
		while (1) ;
		break;
	}

	__local_irq_restore(irq_save);
	VOSIntExit ();
}

/********************************************************************************************************
* 函数：void Reset_Handler ();
* 描述:  复位后执行的第一行代码
* 参数:  无
* 返回：无
* 注意：无
*********************************************************************************************************/
void __attribute__ ((section(".after_vectors"),noreturn))
Reset_Handler ()
{
	vos_start ();
}

/********************************************************************************************************
* 函数：void HardFault_Handler ();
* 描述:  硬件错误异常复位，多次异常也会上升到硬件异常复位
* 参数:  无
* 返回：无
* 注意：无
*********************************************************************************************************/
void __attribute__ ((section(".after_vectors"),weak))
HardFault_Handler ()
{
	asm volatile(
		  " push 	{lr}    \n" //保存lr到msp栈
		  " tst 	lr, #4  \n" //判断进入异常前是哪个栈
		  " ite 	eq      \n"
		  " mrseq 	r0, msp \n" //r0参数是异常进入前使用的栈
		  " mrsne 	r0, psp \n" //r0参数是异常进入前使用的栈
		  " mrs 	r1, msp \n"
		  " sub 	r1, r0  \n" //r1参数存储的是哪个栈，1：psp, 0: msp
		  " bl 		VOSExceptHandler		\n"
		  " b		. 		\n" //暂停在这里
		  " pop		{pc}	\n" //异常返回
	);
}

/********************************************************************************************************
* 函数：void MemManage_Handler ();
* 描述:  内存异常处理
* 参数:  无
* 返回：无
* 注意：无
*********************************************************************************************************/
void __attribute__ ((section(".after_vectors"),weak))
MemManage_Handler ()
{
	asm volatile(
		  " push 	{lr}    \n" //保存lr到msp栈
		  " tst 	lr, #4  \n" //判断进入异常前是哪个栈
		  " ite 	eq      \n"
		  " mrseq 	r0, msp \n" //r0参数是异常进入前使用的栈
		  " mrsne 	r0, psp \n" //r0参数是异常进入前使用的栈
		  " mrs 	r1, msp \n"
		  " sub 	r1, r0  \n" //r1参数存储的是哪个栈，1：psp, 0: msp
		  " bl 		VOSExceptHandler		\n"
		  " b		. 		\n" //暂停在这里
		  " pop		{pc}	\n" //异常返回
	);
}

/********************************************************************************************************
* 函数：void BusFault_Handler ();
* 描述:  总线异常异常处理
* 参数:  无
* 返回：无
* 注意：无
*********************************************************************************************************/
void __attribute__ ((section(".after_vectors"),weak,naked))
BusFault_Handler ()
{
	asm volatile(
		  " push 	{lr}    \n" //保存lr到msp栈
		  " tst 	lr, #4  \n" //判断进入异常前是哪个栈
		  " ite 	eq      \n"
		  " mrseq 	r0, msp \n" //r0参数是异常进入前使用的栈
		  " mrsne 	r0, psp \n" //r0参数是异常进入前使用的栈
		  " mrs 	r1, msp \n"
		  " sub 	r1, r0  \n" //r1参数存储的是哪个栈，1：psp, 0: msp
		  " bl 		VOSExceptHandler		\n"
		  " b		. 		\n" //暂停在这里
		  " pop		{pc}	\n" //异常返回
	);
}
/********************************************************************************************************
* 函数：void UsageFault_Handler ();
* 描述:  用法异常异常处理
* 参数:  无
* 返回：无
* 注意：无
*********************************************************************************************************/
void __attribute__ ((section(".after_vectors"),weak,naked))
UsageFault_Handler ()
{
	asm volatile(
		  " push 	{lr}    \n" //保存lr到msp栈
		  " tst 	lr, #4  \n" //判断进入异常前是哪个栈
		  " ite 	eq      \n"
		  " mrseq 	r0, msp \n" //r0参数是异常进入前使用的栈
		  " mrsne 	r0, psp \n" //r0参数是异常进入前使用的栈
		  " mrs 	r1, msp \n"
		  " sub 	r1, r0  \n" //r1参数存储的是哪个栈，1：psp, 0: msp
		  " bl 		VOSExceptHandler		\n"
		  " b		. 		\n" //暂停在这里
		  " pop		{pc}	\n" //异常返回
	);
}

/********************************************************************************************************
* 函数：void DebugMon_Handler ();
* 描述:  调试监控异常异常处理
* 参数:  无
* 返回：无
* 注意：无
*********************************************************************************************************/
void __attribute__ ((section(".after_vectors"),weak))
DebugMon_Handler ()
{
	asm volatile(
		  " push 	{lr}    \n" //保存lr到msp栈
		  " tst 	lr, #4  \n" //判断进入异常前是哪个栈
		  " ite 	eq      \n"
		  " mrseq 	r0, msp \n" //r0参数是异常进入前使用的栈
		  " mrsne 	r0, psp \n" //r0参数是异常进入前使用的栈
		  " mrs 	r1, msp \n"
		  " sub 	r1, r0  \n" //r1参数存储的是哪个栈，1：psp, 0: msp
		  " bl 		VOSExceptHandler		\n"
		  " b		. 		\n" //暂停在这里
		  " pop		{pc}	\n" //异常返回
	);
}

/********************************************************************************************************
* 函数：void NMI_Handler ();
* 描述:  不可屏蔽中断处理
* 参数:  无
* 返回：无
* 注意：无
*********************************************************************************************************/
void __attribute__ ((section(".after_vectors"),weak))
NMI_Handler ()
{
	asm volatile(
		  " push 	{lr}    \n" //保存lr到msp栈
		  " tst 	lr, #4  \n" //判断进入异常前是哪个栈
		  " ite 	eq      \n"
		  " mrseq 	r0, msp \n" //r0参数是异常进入前使用的栈
		  " mrsne 	r0, psp \n" //r0参数是异常进入前使用的栈
		  " mrs 	r1, msp \n"
		  " sub 	r1, r0  \n" //r1参数存储的是哪个栈，1：psp, 0: msp
		  " bl 		VOSExceptHandler		\n"
		  " b		. 		\n" //暂停在这里
		  " pop		{pc}	\n" //异常返回
	);
}






