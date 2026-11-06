#ifndef LOCKUART_H
#define LOCKUART_H

#include <REGX52.h>
bit LockUARTWaiting();

void LockUARTInit(); //开中断前初始化
void LockUARTProcess(); //主循环处理完整命令
void LockUART_ReceiveISR(unsigned char ch); //串口中断保存接收字节

#endif
