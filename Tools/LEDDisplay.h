#ifndef LEDDISPLAY_H
#define LEDDISPLAY_H

extern unsigned char isRegisterLEDDisplay;
extern volatile unsigned char buf[9];

void ShowLoop();
void LEDDisplay(unsigned char pos, unsigned char num);
void SetBuf(unsigned char location, unsigned char number);

#endif
