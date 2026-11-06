#ifndef __I2C_H__
#define __I2C_H__

void I2CStart();
void I2CStop();
void I2CByteSender(unsigned char byte);
unsigned char I2CByteReceiver();
void I2CSendACK(unsigned char ACKBit);
unsigned char I2CReceiveACK();

#endif