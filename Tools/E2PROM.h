#ifndef E2PROM_H
#define E2PROM_H

#include <REGX52.h>

bit E2PROMIsReady();
bit E2PROMIsBusy();
bit E2PROMReadOK();
bit E2PROMAsyncByteWriter(unsigned char address, unsigned char _data);
unsigned char E2PROMByteReader(unsigned char address);
void E2PROMWriteTask();
void ClearE2PROM();

#endif
