#ifndef UART_H
#define UART_H

#include "Timer.h"

#define UART_TX_SIZE 64

void UARTInit();
void UARTRoutine();
unsigned char UARTTxFree();
unsigned char UARTTransmitIdle();
unsigned char UARTByteSender(unsigned char byte);
unsigned char UARTStringSender(char *string);

#endif
