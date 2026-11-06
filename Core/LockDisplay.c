#include "LockDisplay.h"

#include "LockInput.h"
#include "LockSystem.h"
#include "LockConfig.h"
#include "Tools/LEDDisplay.h"

static void FillTime(unsigned int time, unsigned char padWithZero)
{
    unsigned char i;
    for (i = 8; i >= 3; i--) 
    {
        if (time > 0)
        {
            SetBuf(i, time % 10);
            time /= 10;
        }
        else
        {
            if (padWithZero)
                SetBuf(i, 0);
            else
                SetBuf(i, 10);
        }
    }
}

void TaskDisplay()
{
    unsigned char i;

    SetBuf(1, (unsigned char)globalLockState);
    SetBuf(2, globalWrongCount);

    switch (globalLockState)
    {
        case STATE_LOCKED:
            for (i = 0; i < PASSWORD_LENGTH; i++)
            {
                if (i < inputIndex)
                    SetBuf(3 + i, inputPassword[i]);
                else
                    SetBuf(3 + i, 10);
            }
            break;
            
        case STATE_UNLOCKED:
            FillTime(GetUnlockTimeRemain(), 0);
            break;
            
        case STATE_NEW1:
        case STATE_NEW2:
            for (i = 0; i < PASSWORD_LENGTH; i++)
            {
                if (i < inputIndex)
                    SetBuf(3 + i, inputPassword[i]);
                else
                    SetBuf(3 + i, 10);
            }
            break;

        case STATE_WAIT:
        case STATE_PROTECT:
            FillTime(globalWrongTimeRemain, 1);
            break;

        default:
            for (i = 3; i <= 8; i++)
                SetBuf(i, 10);
            break;
    }
}
