#include <REGX52.h>

sbit I2C_SCL = P2^1;
sbit I2C_SDA = P2^0;

void I2CStart()
{
    I2C_SDA = 1;
    I2C_SCL = 1;
    I2C_SDA = 0; //SCL 高时 SDA 下降 = Start
    I2C_SCL = 0; //钳住总线
}

void I2CStop()
{
    I2C_SCL = 0;
    I2C_SDA = 0;
    I2C_SCL = 1;
    I2C_SDA = 1;
}

void I2CByteSender(unsigned char byte)
{
    unsigned char i;

    for (i = 0; i < 8; i++)
    {
        I2C_SDA = (byte & (0x80 >> i)) ? 1 : 0;
        I2C_SCL = 1;
        I2C_SCL = 0;
    }
}

unsigned char I2CByteReceiver()
{
    unsigned char i;
    unsigned char byte = 0x00;

    I2C_SDA = 1; //释放SDA

    for (i = 0; i < 8; i++)
    {
        I2C_SCL = 1;
        if (I2C_SDA == 1)
            byte |= (0x80 >> i);  //右移i位

        I2C_SCL = 0;
    }

    return byte;
}

void I2CSendACK(unsigned char ACKBit) //0应答 1非应答
{
    I2C_SDA = ACKBit;
    I2C_SCL =1;
    I2C_SCL = 0;
}

unsigned char I2CReceiveACK() //0应答 1非应答
{
    unsigned char ACKBit = 0;

    I2C_SDA = 1; //释放SDA
    I2C_SCL = 1;
    ACKBit = I2C_SDA;
    I2C_SCL = 0;    

    return ACKBit;
}