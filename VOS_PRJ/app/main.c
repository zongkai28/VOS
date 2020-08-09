#include "cmsis/stm32f407xx.h"
#include "../vos/vtype.h"
#include "../vos/vos.h"

int kprintf(char* format, ...);
void sem_test();
void event_test();
void mq_test();
void mutex_test();
void delay_test();
void schedule_test();
void uart_test();
int aaa = 0;
void main(void *param)
{
	kprintf("main function!\r\n");
	//event_test();
	//sem_test();
	//mq_test();
	//mutex_test();
	//delay_test();
	//schedule_test();
	uart_test();
	VOSTaskPrtList(VOS_LIST_READY);
	VOSTaskPrtList(VOS_LIST_BLOCK);
	while (1) {
		if (aaa) {
			VOSTaskPrtList(VOS_LIST_READY);
			VOSTaskPrtList(VOS_LIST_BLOCK);
		}
		VOSTaskDelay(1*1000);
	}
}
