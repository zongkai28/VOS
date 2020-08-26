//----------------------------------------------------
// Copyright (c) 2020, VOS Open source. All rights reserved.
// Author: 156439848@qq.com; vincent_cws2008@gmail.com
// History:
//	     2020-08-20: initial by vincent.
//------------------------------------------------------

#include "vtype.h"
#include "vbitmap.h"



const u8 TabTaskPrio[] =
{
	0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
};
s32 TaskHighestPrioGet(u32 *bitmap, s32 num)
{
	s32 i = 0;
	u32 tmp = 0;
	s32 ret = -1;
	for (i = 0; i < num; i++) {
		if (bitmap[i] != 0) {
			tmp = bitmap[i];
			if (tmp & 0x000000FF) {
				ret = i * 32 + TabTaskPrio[(u8)tmp];
			}
			else if (tmp & 0x0000FF00) {
				ret = i * 32 + 8 + TabTaskPrio[(u8)(tmp >> 8)];
			}
			else if (tmp & 0x00FF0000) {
				ret = i * 32 + 16 + TabTaskPrio[(u8)(tmp >> 16)];
			}
			else {
				ret = i * 32 + 24 + TabTaskPrio[(u8)(tmp >> 24)];
			}
			break;
		}
	}
	return ret;
}


//位图测试

u32 bitmap[10];//4*10 bits
int bitmap_prinf(u8 *bitmap, s32 num)
{
	s32 line_num = 0;
	u32 n = 0;
	bitmap_for_each(n, num)
	{
		//printf("%d ", bitmap_get(n, bitmap) ? 1 : 0);
		kprintf("%d ", bitmap_get(n, bitmap));
		line_num++;
		if (line_num % 32 == 0) kprintf("\r\n");
	}
	kprintf("\r\n");
}

//遍历位图
s32 bitmap_iterate(void **iter)
{
	u32 pos = (u32)*iter;
	u8 *p8 = (u8*)bitmap;
	while (pos < sizeof(bitmap) * 8) {
		if ((pos & 0x7) == 0 && p8[pos>>3] == 0) {//处理一个字节
			pos += 8;
		}
		else {//处理8bit
			do {
				if (p8[pos>>3] & 1 << (pos & 0x7)) {
					*iter = (void*)(pos + 1);
					goto END_ITERATE;
				}
				pos++;
			} while (pos & 0x7);
		}
	}
	return -1;

END_ITERATE:
	return pos;
}

int bitmap_test() {
	u32 n = 0;
	memset(bitmap, 0, sizeof(bitmap));
	kprintf("TaskHighestPrioGet=%d\r\n", TaskHighestPrioGet(bitmap, MAX_COUNTS(bitmap)));
	bitmap_set(32*10-1, bitmap);
	bitmap_prinf(bitmap, sizeof(bitmap));
	kprintf("TaskHighestPrioGet=%d\r\n", TaskHighestPrioGet(bitmap, MAX_COUNTS(bitmap)));
	bitmap_clr(2, bitmap);
	bitmap_prinf(bitmap, sizeof(bitmap));
	kprintf("TaskHighestPrioGet=%d\r\n", TaskHighestPrioGet(bitmap, MAX_COUNTS(bitmap)));
	bitmap_set(7, bitmap);
	bitmap_prinf(bitmap, sizeof(bitmap));
	kprintf("TaskHighestPrioGet=%d\r\n", TaskHighestPrioGet(bitmap, MAX_COUNTS(bitmap)));
	bitmap_set(8, bitmap);
	bitmap_prinf(bitmap, sizeof(bitmap));
	kprintf("TaskHighestPrioGet=%d\r\n", TaskHighestPrioGet(bitmap, MAX_COUNTS(bitmap)));
	bitmap_set(1, bitmap);
	bitmap_prinf(bitmap, sizeof(bitmap));
	kprintf("TaskHighestPrioGet=%d\r\n", TaskHighestPrioGet(bitmap, MAX_COUNTS(bitmap)));
	bitmap_set(0, bitmap);
	bitmap_prinf(bitmap, sizeof(bitmap));
	kprintf("TaskHighestPrioGet=%d\r\n", TaskHighestPrioGet(bitmap, MAX_COUNTS(bitmap)));
	bitmap_clr(0, bitmap);
	bitmap_prinf(bitmap, sizeof(bitmap));
	kprintf("TaskHighestPrioGet=%d\r\n", TaskHighestPrioGet(bitmap, MAX_COUNTS(bitmap)));
	bitmap_set(16, bitmap);
	bitmap_prinf(bitmap, sizeof(bitmap));
	kprintf("TaskHighestPrioGet=%d\r\n", TaskHighestPrioGet(bitmap, MAX_COUNTS(bitmap)));
	void *iter = 0; //从头遍历
	s32 prio;
	while (-1 != (prio = bitmap_iterate(&iter))) {
		kprintf("======>prio=%d<======\r\n", prio);
	}
#if 0
	bitmap_for_each(n, bitmap) {
		if (bitmap_get(n, bitmap)) {

		}
	}
#endif
}

