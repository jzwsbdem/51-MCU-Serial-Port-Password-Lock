#include "E2PROM.h"
#include "I2C.h"

#define SLAVE_ADDRESS 0xA0 // 设备地址 + 写位

static unsigned char clearAddress;
static bit readOK;
static bit clearRequested = 0;
static volatile unsigned char state = 0; // 0=空闲 1=等待写周期结束

bit E2PROMIsReady()
{
    bit ack;
    I2CStart();
    I2CByteSender(SLAVE_ADDRESS);
    ack = I2CReceiveACK(); //0=就绪
    I2CStop();
    return (ack == 0) ? 1 : 0;
}

bit E2PROMIsBusy()
{
    return (state != 0 || clearRequested) ? 1 : 0;
}

static bit E2PROMByteWriter(unsigned char address, unsigned char _data)
{
    I2CStart();
    I2CByteSender(SLAVE_ADDRESS);
    if (I2CReceiveACK()) { I2CStop(); return 0; }

    I2CByteSender(address);
    if (I2CReceiveACK()) { I2CStop(); return 0; }

    I2CByteSender(_data);
    if (I2CReceiveACK()) { I2CStop(); return 0; }

    I2CStop();
    return 1;
}

bit E2PROMAsyncByteWriter(unsigned char address, unsigned char _data)
{
    if (state != 0 || clearRequested) return 0; // 上一次未完成
    if (!E2PROMByteWriter(address, _data)) return 0;
    state = 1;
    return 1;
}

unsigned char E2PROMByteReader(unsigned char address)
{
    unsigned char d;

    readOK = 0;

    I2CStart();
    I2CByteSender(SLAVE_ADDRESS);
    if (I2CReceiveACK()) { I2CStop(); return 0xFF; }

    I2CByteSender(address);
    if (I2CReceiveACK()) { I2CStop(); return 0xFF; }

    I2CStart();
    I2CByteSender(SLAVE_ADDRESS | 0x01);
    if (I2CReceiveACK()) { I2CStop(); return 0xFF; }

    d = I2CByteReceiver();
    I2CSendACK(1);
    I2CStop();

    readOK = 1;
    return d;
}

bit E2PROMReadOK()
{
    return readOK;
}

void E2PROMWriteTask()
{
    if (state != 0)
    {
        if (!E2PROMIsReady())
            return;
        state = 0;
    }
    if (clearRequested && E2PROMByteWriter(clearAddress, 0xFF))
    {
        state = 1;
        if (clearAddress == 255)
            clearRequested = 0;
        else
            clearAddress++;
    }
}

void ClearE2PROM()
{
    if (clearRequested)
        return;
    clearAddress = 0;
    clearRequested = 1;
}
