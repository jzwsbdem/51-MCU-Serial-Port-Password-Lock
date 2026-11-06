#include <REGX52.h>
#include "Tools/Timer.h"
#include "Tools/UART.h"
#include "Tools/MatrixKey.h"
#include "Core/LockSystem.h"
#include "Core/LockInput.h"
#include "Core/LockDisplay.h"
#include "Core/LockFeedback.h"
#include "Core/LockSaver.h"
#include "Core/LockUART.h"

unsigned char globalStart = 0;

void main()
{
    EA = 0;
    FeedbackInit();
    Timer0Init();
    LockUARTInit();
    UARTInit();
    LockSaverInit();
    TaskAdd(1, TaskSavePending);
    TaskAdd(25, TaskDisplay);
    MatrixKeyRegister();
    LockInit();
    TaskDisplay();
    globalStart = 1;
    TF2 = 0;
    EA = 1;

    while (1)
    {
        P2_7 = 0;
        LockTaskEnsure();
        TaskProcess();
        if (globalStart == 0)
            continue;
        InputMatrixKey();
        LockUARTProcess();
    }
}
