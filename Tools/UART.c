#include "UART.h"
#include <REGX52.h>
#include "Core/LockUART.h"

static volatile unsigned char idata txBuf[UART_TX_SIZE];
static volatile unsigned char txHead;
static volatile unsigned char txTail;
static volatile bit txBusy;

void UARTInit()
{
    ES = 0;
    txHead = 0;
    txTail = 0;
    txBusy = 0;
    PCON |= 0x80;
    SCON = 0x50;
    TI = 0;
    RI = 0;
    TMOD = (TMOD & 0x0F) | 0x20;
    TL1 = 0xFA;
    TH1 = 0xFA;
    ET1 = 0;
    TR1 = 1;
    ES = 1;
}

void UARTRoutine() INTERRUPT(4)
{
    if (RI)
    {
        RI = 0;
        LockUART_ReceiveISR(SBUF);
    }
    if (TI)
    {
        TI = 0;
        if (txTail != txHead)
        {
            SBUF = txBuf[txTail];
            txTail = (txTail + 1) & (UART_TX_SIZE - 1);
        }
        else
            txBusy = 0;
    }
}

unsigned char UARTTxFree()
{
    return (txTail - txHead - 1) & (UART_TX_SIZE - 1);
}

unsigned char UARTTransmitIdle()
{
    return !txBusy;
}

static void StartTransmit()
{
    bit enabled;

    enabled = ES;
    ES = 0;
    if (!txBusy && txTail != txHead)
    {
        txBusy = 1;
        SBUF = txBuf[txTail];
        txTail = (txTail + 1) & (UART_TX_SIZE - 1);
    }
    ES = enabled;
}

unsigned char UARTByteSender(unsigned char byte)
{
    if (UARTTxFree() == 0)
        return 0;
    txBuf[txHead] = byte;
    txHead = (txHead + 1) & (UART_TX_SIZE - 1);
    StartTransmit();
    return 1;
}

unsigned char UARTStringSender(char *string)
{
    unsigned char length = 0, head;
    static char * xdata cursor;

    cursor = string;
    while (*cursor)
    {
        if (++length >= UART_TX_SIZE)
            return 0;
        cursor++;
    }
    if (UARTTxFree() < length)
        return 0;
    head = txHead;
    cursor = string;
    while (length--)
    {
        txBuf[head] = *cursor++;
        head = (head + 1) & (UART_TX_SIZE - 1);
    }
    txHead = head;
    StartTransmit();
    return 1;
}
