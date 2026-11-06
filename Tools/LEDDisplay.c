#include "LEDDisplay.h"

#include <REGX52.h>
#include "Timer.h"

volatile unsigned char buf[9] = { 0 };
unsigned char code table[11] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F, 0x40}; 
volatile unsigned char isRegisterLEDDisplay = 0;

void LEDDisplay(unsigned char pos, unsigned char num)
{
    P0 = 0x00; 

    switch(pos)
    {
        case 1: P2_4=1; P2_3=1; P2_2=1; break;
        case 2: P2_4=1; P2_3=1; P2_2=0; break;
        case 3: P2_4=1; P2_3=0; P2_2=1; break;
        case 4: P2_4=1; P2_3=0; P2_2=0; break;
        case 5: P2_4=0; P2_3=1; P2_2=1; break;
        case 6: P2_4=0; P2_3=1; P2_2=0; break;
        case 7: P2_4=0; P2_3=0; P2_2=1; break;
        case 8: P2_4=0; P2_3=0; P2_2=0; break;
    }
    P0 = table[num];
}

void ShowLoop()
{
    static unsigned char i = 1;
    LEDDisplay(i, buf[i]);
    i++;
    if (i >= 9) i = 1;
}

void SetBuf(unsigned char location, unsigned char number)
{
    buf[location] = number;
}
