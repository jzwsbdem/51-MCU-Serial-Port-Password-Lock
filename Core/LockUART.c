#include "LockUART.h"

#include <REGX52.h>
#include "Tools/UART.h"
#include "LockSystem.h"
#include "LockInput.h"
#include "LockConfig.h"
#include "LockFeedback.h"
#include "LockSaver.h"

#define RX_BUF_SIZE 64
#define REPLY_RESERVE 48
#define CMD_BUF_SIZE 16

static volatile unsigned char idata rxBuf[RX_BUF_SIZE / 2];
static volatile unsigned char xdata rxExtra[RX_BUF_SIZE / 2];
static volatile unsigned char rxHead = 0;
static volatile unsigned char rxTail = 0;
static volatile bit rxOverflow = 0;

static char xdata cmdBuf[CMD_BUF_SIZE];
static unsigned char cmdLength = 0;
static bit badLine = 0;
static char * xdata pendingReply;
static bit pendingConfirm, waitForSave;
static LockState xdata replyState;
static unsigned char xdata replyCount, replyWrongCount;
static unsigned int xdata replyLockSeconds, replyUnlockSeconds;

void LockUARTInit()
{
    rxHead = 0;
    rxTail = 0;
    rxOverflow = 0;
    cmdLength = 0;
    badLine = 0;
    pendingReply = 0;
    pendingConfirm = waitForSave = 0;
}


void LockUART_ReceiveISR(unsigned char ch)
{
    static unsigned char nextHead;

    if (rxOverflow)
        return;

    nextHead = (rxHead + 1) % RX_BUF_SIZE;
    if (nextHead == rxTail)
    {
        rxOverflow = 1;
        return;
    }

    if (rxHead < RX_BUF_SIZE / 2) rxBuf[rxHead] = ch;
    else rxExtra[rxHead - RX_BUF_SIZE / 2] = ch;
    rxHead = nextHead;
}

bit LockUARTWaiting()
{
    return pendingConfirm;
}

static void SendNum(unsigned int num)
{
    static char xdata digits[5];
    static unsigned char count;

    count = 0;
    do
    {
        digits[count++] = '0' + num % 10;
        num /= 10;
    } while (num != 0);

    while (count > 0)
        UARTByteSender(digits[--count]);
}


static void SendSnapshot(char *result)
{
    FeedbackRequest(result[0] == 'E' ? FEEDBACK_ERROR : FEEDBACK_NORMAL);

    UARTStringSender(result);
    UARTByteSender(' ');
    switch (replyState)
    {
        case STATE_LOCKED: UARTStringSender("LOCKED"); break;
        case STATE_UNLOCKED: UARTStringSender("UNLOCKED"); break;
        case STATE_NEW1: UARTStringSender("NEW1"); break;
        case STATE_NEW2: UARTStringSender("NEW2"); break;
        case STATE_WAIT: UARTStringSender("WAIT"); break;
        case STATE_PROTECT: UARTStringSender("PROTECT"); break;
        default: UARTStringSender("UNKNOWN"); break;
    }
    UARTByteSender(' ');
    SendNum(replyCount);
    UARTByteSender(' ');
    SendNum(replyWrongCount);
    UARTByteSender(' ');
    SendNum(replyLockSeconds);
    UARTByteSender(' ');
    SendNum(replyUnlockSeconds);
    UARTStringSender("\r\n");
}

static void SendReply(char *result)
{
    replyState = globalLockState;
    replyCount = inputIndex;
    replyWrongCount = globalWrongCount;
    replyLockSeconds = GetLockTimeRemain();
    replyUnlockSeconds = GetUnlockTimeRemain();
    if (waitForSave && LockSaverBusy()) pendingReply = result;
    else SendSnapshot(result);
}

static void ExecuteDigit()
{
    static unsigned char result;

    if (globalLockState != STATE_LOCKED && globalLockState != STATE_NEW1 && globalLockState != STATE_NEW2)
        result = 1;
    else if (inputIndex >= PASSWORD_LENGTH)
        result = 2;
    else
    {
        InputDigit(cmdBuf[6] - '0');
        result = 0;
    }

    if (result == 1)
        SendReply("ERR DENIED");
    else if (result == 2)
        SendReply("ERR FULL");
    else
    {
        SendReply("OK");
    }
}

static void ExecuteDelete()
{
    if (globalLockState != STATE_LOCKED && globalLockState != STATE_NEW1 && globalLockState != STATE_NEW2)
    {
        SendReply("ERR DENIED");
        return;
    }

    Delete();
    SendReply("OK");
}

static void ExecuteClear()
{
    if (globalLockState != STATE_LOCKED && globalLockState != STATE_NEW1 && globalLockState != STATE_NEW2)
    {
        SendReply("ERR DENIED");
        return;
    }

    Clear();
    SendReply("OK");
}

static void ExecuteConfirm()
{
    static ConfirmResult result;

    result = Confirm();
    waitForSave = result == CONFIRM_WRONG || (result == CONFIRM_OK && globalLockState == STATE_UNLOCKED);
    switch (result)
    {
        case CONFIRM_OK: SendReply("OK"); break;
        case CONFIRM_DENIED: SendReply("ERR DENIED"); break;
        case CONFIRM_INCOMPLETE: SendReply("ERR INCOMPLETE"); break;
        case CONFIRM_WRONG: SendReply("ERR WRONG"); break;
        case CONFIRM_MISMATCH: SendReply("ERR MISMATCH"); break;
        case CONFIRM_STORAGE: SendReply("ERR STORAGE"); break;
        case CONFIRM_PENDING: pendingConfirm = 1; break;
    }
    waitForSave = 0;
}

static void ExecuteChange()
{
    if (globalLockState != STATE_UNLOCKED)
    {
        SendReply("ERR DENIED");
        return;
    }

    Change();
    SendReply("OK");
}

static void ExecuteCancel()
{
    if (globalLockState != STATE_LOCKED && globalLockState != STATE_UNLOCKED &&
        globalLockState != STATE_NEW1 && globalLockState != STATE_NEW2)
    {
        SendReply("ERR DENIED");
        return;
    }

    Cancel();
    SendReply("OK");
}

static void ExecuteLock()
{
    if (globalLockState != STATE_LOCKED && globalLockState != STATE_UNLOCKED &&
        globalLockState != STATE_NEW1 && globalLockState != STATE_NEW2 &&
        globalLockState != STATE_WAIT && globalLockState != STATE_PROTECT)
    {
        SendReply("ERR DENIED");
        return;
    }

    Lock();
    SendReply("OK");
}


static bit CommandIs(char *name)
{
    static char * xdata cursor;
    static unsigned char index;

    cursor = name;
    index = 0;
    while (*cursor != '\0')
    {
        if (index >= cmdLength || cmdBuf[index] != *cursor)
            return 0;
        index++;
        cursor++;
    }
    return index == cmdLength || cmdBuf[index] == ' ';
}

static void ExecuteCommand()
{
    unsigned char command;

    CheckUnlockTimeout();
    if (badLine)
    {
        SendReply("ERR BAD_CMD");
        return;
    }
    cmdBuf[cmdLength] = '\0';
    if (CommandIs("STATUS") && cmdLength == 6)
    {
        SendReply("OK");
        return;
    }
    if (CommandIs("VERSION") && cmdLength == 7)
    {
        UARTStringSender("OK VERSION " LOCK_VERSION "\r\n");
        return;
    }

    if (CommandIs("DIGIT") && cmdLength == 7 && cmdBuf[5] == ' ' && cmdBuf[6] >= '0' && cmdBuf[6] <= '9')
        command = 1;
    else if (CommandIs("OK") && cmdLength == 2) command = 2;
    else if (CommandIs("DEL") && cmdLength == 3) command = 3;
    else if (CommandIs("CLEAR") && cmdLength == 5) command = 4;
    else if (CommandIs("CHANGE") && cmdLength == 6) command = 5;
    else if (CommandIs("CANCEL") && cmdLength == 6) command = 6;
    else if (CommandIs("LOCK") && cmdLength == 4) command = 7;
    else if (CommandIs("ISP") && cmdLength == 3) command = 8;
    else
    {
        SendReply("ERR BAD_CMD");
        return;
    }

    switch (command)
    {
        case 1: ExecuteDigit(); break;
        case 2: ExecuteConfirm(); break;
        case 3: ExecuteDelete(); break;
        case 4: ExecuteClear(); break;
        case 5: ExecuteChange(); break;
        case 6: ExecuteCancel(); break;
        case 7: ExecuteLock(); break;
        case 8:
            if (globalLockState == STATE_UNLOCKED) EnterUpgrade();
            else SendReply("ERR DENIED");
            break;
    }
}

void LockUARTProcess()
{
    static unsigned char ch;
    static bit serialEnabled;
    unsigned char budget;

    ConfirmSaveProcess();
    if (globalLockState == STATE_UPGRADE || UARTTxFree() < REPLY_RESERVE)
        return;

    if (ConfirmSaveResult() == CONFIRM_PENDING) return;
    if (pendingConfirm)
    {
        pendingConfirm = 0;
        SendReply(ConfirmSaveResult() == CONFIRM_OK ? "OK" : "ERR STORAGE");
        return;
    }
    if (pendingReply != 0)
    {
        if (LockSaverBusy()) return;
        SendSnapshot(LockSaverFailed() ? "ERR STORAGE" : pendingReply);
        pendingReply = 0;
        return;
    }

    budget = RX_BUF_SIZE;
    while (budget--)
    {
        if (rxOverflow)
        {
            serialEnabled = ES;
            ES = 0;
            rxTail = rxHead;
            rxOverflow = 0;
            ES = serialEnabled;
            cmdLength = 0;
            badLine = 1;
        }
        if (rxTail == rxHead)
            return;

        if (rxTail < RX_BUF_SIZE / 2) ch = rxBuf[rxTail];
        else ch = rxExtra[rxTail - RX_BUF_SIZE / 2];
        rxTail = (rxTail + 1) % RX_BUF_SIZE;

        if (ch == '\n')
        {
            ExecuteCommand();
            cmdLength = 0;
            badLine = 0;
            return;
        }
        else if (cmdLength < CMD_BUF_SIZE - 1)
            cmdBuf[cmdLength++] = ch;
        else
            badLine = 1;
    }
}
