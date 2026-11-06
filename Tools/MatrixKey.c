#include "MatrixKey.h"

#include <REGX52.h>
#include "Timer.h"

static unsigned char lastKey = 16;
unsigned char isMatrixKeyRegistered = 0;
volatile unsigned char globalMatrixKey = 16;

void MatrixKeyRegister()
{
    if (!TaskExist(MatrixKeyScan))
        isMatrixKeyRegistered = TaskAdd(20, MatrixKeyScan);
    else
        isMatrixKeyRegistered = 1;
}

unsigned char MatrixKeyGet()
{
    unsigned char key;

    if (globalMatrixKey == 16)
        return 16;

    key = globalMatrixKey;
    globalMatrixKey = 16;

    return key;
}

void MatrixKeyScan()
{
    unsigned char keyNumber = 16;

    P1 = 0x0F;
    if (P1 == 0x0F)
    {
        lastKey = 16;
        return;
    }


    P1 = 0x0F;

    switch (P1)
    {
        default: lastKey = 16; return;
        case 0x07: keyNumber = 0; break;
        case 0x0B: keyNumber = 1; break;
        case 0x0D: keyNumber = 2; break;
        case 0x0E: keyNumber = 3; break;
    }

    P1 = 0xF0;

    switch (P1)
    {
        default: lastKey = 16; return;
        case 0x70: break;
        case 0xB0: keyNumber += 4; break;
        case 0xD0: keyNumber += 8; break;
        case 0xE0: keyNumber += 12; break;
    }


    P1 = 0x0F;

    if (keyNumber == lastKey)
        return;

    lastKey = keyNumber;
    globalMatrixKey = keyNumber;

    return;
}
