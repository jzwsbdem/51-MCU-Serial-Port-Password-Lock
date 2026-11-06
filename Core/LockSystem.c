#include "LockSystem.h"
#include "LockConfig.h"
#include "LockSaver.h"
#include "LockInput.h"
#include "LockFeedback.h"

#include <REGX52.h> 

#include "Tools/Timer.h"
#include "Tools/E2PROM.h"
#include "Tools/UART.h"
#include <intrins.h>

#ifndef __VSCODE_C51__
sfr ISP_CONTR = 0xE7;
#else
extern unsigned char ISP_CONTR;
#endif

#define ISP_WAIT_TX 1
#define ISP_WAIT_DELAY 2
#define ISP_TRIGGERED 3

static unsigned char ispPhase = 0;
static unsigned long xdata ispWaitStart;

unsigned char xdata globalPassword[PASSWORD_LENGTH] = PASSWORD_DEFAULT;
unsigned char xdata tempPassword[PASSWORD_LENGTH];

LockState globalLockState = STATE_LOCKED;
unsigned char globalWrongCount = 0;
unsigned int globalWrongTimeRemain = 0;
unsigned int globalUnlockTimeRemain = 0;

void LockInit()
{
    LockInputInit();
    globalWrongTimeRemain = globalUnlockTimeRemain = 0;
    if (!LockSaverLoad())
    {
        globalLockState = STATE_LOCKED;
        return;
    }
    if (globalLockState == STATE_WAIT || globalLockState == STATE_PROTECT)
    {
        globalWrongTimeRemain = globalLockState == STATE_WAIT ? WAIT_TIME : PROTECT_TIME;
        TaskAdd(1000, TaskLockTimeRemain);
    }
    else
        globalLockState = STATE_LOCKED;
}

void EnterLocked()
{
    unsigned char i;

    TaskDelete(TaskUnlockTimeRemain);
    TaskDelete(TaskLockTimeRemain);

    if (globalLockState == STATE_PROTECT)
    {
        globalWrongCount = 0;
        TriggerSaveWrongCount();
    }

    globalLockState = STATE_LOCKED;
    globalWrongTimeRemain = 0;
    globalUnlockTimeRemain = 0;
    ClearInput();
    for (i = 0; i < PASSWORD_LENGTH; i++)
        tempPassword[i] = 0;
    TriggerSaveState();
}

void RefreshUnlockTime()
{
    if (globalLockState != STATE_UNLOCKED && globalLockState != STATE_NEW1 && globalLockState != STATE_NEW2)
        return;

    globalUnlockTimeRemain = UNLOCK_TIME;
    if (TaskExist(TaskUnlockTimeRemain))
        TaskSetPeriod(1000, TaskUnlockTimeRemain);
    else
        TaskAdd(1000, TaskUnlockTimeRemain);
}

static unsigned long TimeLeftMs(TaskFunction task, unsigned int seconds)
{
    unsigned long deadline, now;

    if (seconds == 0 || !TaskExist(task)) return seconds * 1000UL;
    deadline = TaskGetNextTick(task) + (seconds - 1UL) * 1000UL;
    now = TaskGetTick();
    if ((long)(deadline - now) <= 0) return 0;
    return deadline - now;
}

unsigned int GetUnlockTimeRemain()
{
    if (globalLockState != STATE_UNLOCKED && globalLockState != STATE_NEW1 && globalLockState != STATE_NEW2)
        return 0;
    return (unsigned int)((TimeLeftMs(TaskUnlockTimeRemain, globalUnlockTimeRemain) + 999UL) / 1000UL);
}

unsigned int GetLockTimeRemain()
{
    if (globalLockState != STATE_WAIT && globalLockState != STATE_PROTECT) return 0;
    return (unsigned int)((TimeLeftMs(TaskLockTimeRemain, globalWrongTimeRemain) + 999UL) / 1000UL);
}

void CheckUnlockTimeout()
{
    if ((globalLockState == STATE_UNLOCKED || globalLockState == STATE_NEW1 || globalLockState == STATE_NEW2) &&
        GetUnlockTimeRemain() == 0)
        EnterLocked();
    else if ((globalLockState == STATE_WAIT || globalLockState == STATE_PROTECT) && GetLockTimeRemain() == 0)
        EnterLocked();
}

void EnterUnlocked()
{
    TaskDelete(TaskLockTimeRemain);
    globalLockState = STATE_UNLOCKED;
    TriggerSaveState();
    RefreshUnlockTime();
}

void EnterWait()
{
    TaskDelete(TaskUnlockTimeRemain);
    TaskDelete(TaskLockTimeRemain);
    globalLockState = STATE_WAIT;
    globalWrongTimeRemain = WAIT_TIME;
    FeedbackRequest(FEEDBACK_LIMIT);
    TaskAdd(1000, TaskLockTimeRemain);
    TriggerSaveWrongCount(); 
    TriggerSaveState();
}

void EnterProtect()
{
    TaskDelete(TaskUnlockTimeRemain);
    TaskDelete(TaskLockTimeRemain);
    globalLockState = STATE_PROTECT;
    globalWrongTimeRemain = PROTECT_TIME;
    FeedbackRequest(FEEDBACK_LIMIT);
    TaskAdd(1000, TaskLockTimeRemain);
    TriggerSaveWrongCount(); 
    TriggerSaveState();
}

void EnterUpgrade()
{
    if (globalLockState != STATE_UNLOCKED)
        return;

    if (!UARTStringSender("OK ISP\r\n"))
        return;

    EnterLocked();
    Clear();
    globalUnlockTimeRemain = 0;
    globalLockState = STATE_UPGRADE;
    ispPhase = ISP_WAIT_TX;
    if (!TaskExist(TaskISP)) TaskAdd(1, TaskISP);
}

bit PasswordCheck(const unsigned char *input)
{
    static unsigned char i;
    static const unsigned char * xdata cursor;

    cursor = input;
    for (i = 0; i < PASSWORD_LENGTH; i++)
    {
        if (*cursor != globalPassword[i])
            return 0;
        cursor++;
    }

    return 1;
}

void OnPasswordRight()
{
    if (globalLockState != STATE_LOCKED)
        return;

    globalWrongCount = 0;
    TriggerSaveWrongCount();
    EnterUnlocked();
    FeedbackRequest(FEEDBACK_SUCCESS);
}

void OnPasswordWrong()
{
    if (globalLockState != STATE_LOCKED)
        return;

    globalWrongCount++; 

    if (globalWrongCount >= PROTECT_COUNT_MAX) 
    {
        EnterProtect();
    }
    else if (globalWrongCount == WAIT_COUNT_MAX)
    {
        EnterWait();
    }
    else 
    {
        TriggerSaveWrongCount(); 
    }
}

void TaskLockTimeRemain()
{
    if (globalLockState != STATE_WAIT && globalLockState != STATE_PROTECT)
    {
        TaskDelete(TaskLockTimeRemain);
        return;
    }

    if (globalWrongTimeRemain > 0)
    {
        globalWrongTimeRemain--;
    }

    if (globalWrongTimeRemain == 0)
    {
        TaskDelete(TaskLockTimeRemain);
        EnterLocked();
    }
}

void TaskUnlockTimeRemain()
{
    if (globalLockState != STATE_UNLOCKED && globalLockState != STATE_NEW1 && globalLockState != STATE_NEW2)
    {
        TaskDelete(TaskUnlockTimeRemain);
        return;
    }

    if (globalUnlockTimeRemain > 0)
        globalUnlockTimeRemain--;
    
    if (globalUnlockTimeRemain == 0)
    {
        TaskDelete(TaskUnlockTimeRemain);
        EnterLocked();
    }
}

void TaskISP()
{
    if (globalLockState != STATE_UPGRADE)
    {
        ispPhase = 0;
        TaskDelete(TaskISP);
        return;
    }

    if (!UARTTransmitIdle())
    {
        ispPhase = ISP_WAIT_TX;
        return;
    }

    if (ispPhase == ISP_WAIT_TX)
    {
        ispWaitStart = TaskGetTick();
        ispPhase = ISP_WAIT_DELAY;
        return;
    }

    if (ispPhase != ISP_WAIT_DELAY || TaskGetTick() - ispWaitStart < ISP_DELAY_MS)
        return;

    TaskDelete(TaskISP);
    ispPhase = ISP_TRIGGERED;
    EA = 0;
    ISP_CONTR = 0x60;
    _nop_();
    _nop_();
}

void LockTaskEnsure()
{
    ConfirmSaveProcess();
    CheckUnlockTimeout();

    if (globalLockState == STATE_WAIT || globalLockState == STATE_PROTECT)
    {
        if (!TaskExist(TaskLockTimeRemain)) TaskAdd(1000, TaskLockTimeRemain);
    }
    else if (globalLockState == STATE_UNLOCKED || globalLockState == STATE_NEW1 || globalLockState == STATE_NEW2)
    {
        if (!TaskExist(TaskUnlockTimeRemain)) TaskAdd(1000, TaskUnlockTimeRemain);
    }
    else if (globalLockState == STATE_UPGRADE && ispPhase != ISP_TRIGGERED)
    {
        if (!TaskExist(TaskISP) && !TaskAdd(1, TaskISP)) TaskISP();
    }
}
