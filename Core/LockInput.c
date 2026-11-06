#include "LockInput.h"

#include "Tools/MatrixKey.h"
#include "Tools/UART.h"
#include "Tools/Timer.h"

#include "LockSystem.h"
#include "LockConfig.h"
#include "LockSaver.h"
#include "LockFeedback.h"
#include "LockUART.h"

unsigned char xdata inputPassword[PASSWORD_LENGTH];
unsigned char inputIndex = 0;
static ConfirmResult saveResult = CONFIRM_OK;

void LockInputInit()
{
    saveResult = CONFIRM_OK;
    ClearInput();
}

void InputMatrixKey()
{
    static unsigned char key;
    static unsigned char inputNum;

    ConfirmSaveProcess();
    if (globalLockState == STATE_UPGRADE || LockUARTWaiting() || saveResult == CONFIRM_PENDING)
        return;

    inputNum = 0xFF;

    MatrixKeyRegister();
    
    key = MatrixKeyGet();

    if (key == 16)
        return;

    CheckUnlockTimeout();

    switch (key)
    {
        default: break;
        case 0: inputNum = 1; break; // S1
        case 1: inputNum = 2; break; // S2
        case 2: inputNum = 3; break; // S3
        case 4: inputNum = 4; break; // S5
        case 5: inputNum = 5; break; // S6
        case 6: inputNum = 6; break; // S7
        case 8: inputNum = 7; break; // S9
        case 9: inputNum = 8; break; // S10
        case 10: inputNum = 9; break; // S11
        case 13: inputNum = 0; break; // S14
    }

    if (inputNum <= 9)
        InputDigit(inputNum);

    switch (key)
    {
        default: break;
        case 3:
            inputNum = (unsigned char)Confirm();
            if (inputNum == CONFIRM_PENDING) return;
            if (inputNum != CONFIRM_OK) FeedbackRequest(FEEDBACK_ERROR);
            break;
        case 7: Delete(); break;
        case 12: Clear(); break;
        case 11: Change(); break;
        case 14: Cancel(); break;
        case 15: Lock(); break;
    }
    FeedbackExecute();
}

void InputDigit(unsigned char number)
{
    if (number > 9 || inputIndex >= PASSWORD_LENGTH)
    {
        FeedbackRequest(FEEDBACK_ERROR);
        return;
    }
    if (globalLockState != STATE_LOCKED && globalLockState != STATE_NEW1 && globalLockState != STATE_NEW2)
    {
        FeedbackRequest(FEEDBACK_ERROR);
        return;
    }

    inputPassword[inputIndex++] = number;
    RefreshUnlockTime();
}

ConfirmResult Confirm()
{
    static unsigned char i;
    static bit isMatch;

    if (saveResult == CONFIRM_PENDING) return CONFIRM_PENDING;

    if (globalLockState != STATE_LOCKED && globalLockState != STATE_NEW1 && globalLockState != STATE_NEW2)
        return CONFIRM_DENIED;

    if (inputIndex != PASSWORD_LENGTH)
        return CONFIRM_INCOMPLETE;

    if (LockSaverReadFailed()) return CONFIRM_STORAGE;

    if (globalLockState == STATE_LOCKED)
    {
        if (PasswordCheck(inputPassword) == 1)
        {
            OnPasswordRight();
            ClearInput();
            return CONFIRM_OK;
        }

        OnPasswordWrong();
        ClearInput();
        return CONFIRM_WRONG;
    }

    if (globalLockState == STATE_NEW1)
    {
        for (i = 0; i < PASSWORD_LENGTH; i++)
            tempPassword[i] = inputPassword[i];

        globalLockState = STATE_NEW2;
        ClearInput();
        RefreshUnlockTime();
        return CONFIRM_OK;
    }

    isMatch = 1;
    for (i = 0; i < PASSWORD_LENGTH; i++)
    {
        if (tempPassword[i] != inputPassword[i])
        {
            isMatch = 0;
            break;
        }
    }

    if (isMatch)
    {
        TriggerSavePassword();
        RefreshUnlockTime();
        saveResult = CONFIRM_PENDING;
        return CONFIRM_PENDING;
    }

    for (i = 0; i < PASSWORD_LENGTH; i++)
        tempPassword[i] = 0;
    ClearInput();
    EnterUnlocked();
    return isMatch ? CONFIRM_OK : CONFIRM_MISMATCH;
}

void ConfirmSaveProcess()
{
    unsigned char i;

    if (saveResult != CONFIRM_PENDING || LockSaverBusy()) return;
    saveResult = LockSaverFailed() ? CONFIRM_STORAGE : CONFIRM_OK;
    for (i = 0; i < PASSWORD_LENGTH; i++) tempPassword[i] = 0;
    ClearInput();
    EnterUnlocked();
    FeedbackRequest(saveResult == CONFIRM_OK ? FEEDBACK_SUCCESS : FEEDBACK_ERROR);
}

ConfirmResult ConfirmSaveResult()
{
    return saveResult;
}

void Delete()
{
    if (globalLockState != STATE_LOCKED && globalLockState != STATE_NEW1 && globalLockState != STATE_NEW2)
    {
        FeedbackRequest(FEEDBACK_ERROR);
        return;
    }

    if (inputIndex > 0)
    {
        inputIndex--;
        inputPassword[inputIndex] = 0;
        RefreshUnlockTime();
    }
}

void ClearInput()
{
    unsigned char i;

    for (i = 0; i < PASSWORD_LENGTH; i++)
        inputPassword[i] = 0;
    inputIndex = 0;
}

void Clear()
{
    if (globalLockState != STATE_LOCKED && globalLockState != STATE_NEW1 && globalLockState != STATE_NEW2)
    {
        FeedbackRequest(FEEDBACK_ERROR);
        return;
    }

    if (inputIndex > 0)
    {
        ClearInput();
        RefreshUnlockTime();
    }
}

void Lock()
{
    if (globalLockState == STATE_LOCKED)
    {
        ClearInput();
        return;
    }

    if (globalLockState == STATE_UNLOCKED || globalLockState == STATE_NEW1 || globalLockState == STATE_NEW2)
    {
        EnterLocked();
    }
}

void Change()
{
    static unsigned char i;

    if (globalLockState == STATE_UNLOCKED)
    {
        for (i = 0; i < PASSWORD_LENGTH; i++)
            tempPassword[i] = 0;
        ClearInput();

        globalLockState = STATE_NEW1;
        RefreshUnlockTime();
        TriggerSaveState(); 
    }
    else FeedbackRequest(FEEDBACK_ERROR);
}

void Cancel()
{
    static unsigned char i;

    if (globalLockState == STATE_NEW1 || globalLockState == STATE_NEW2)
    {
        for (i = 0; i < PASSWORD_LENGTH; i++)
            tempPassword[i] = 0;
        ClearInput();
        EnterUnlocked();
    }
    else if (globalLockState != STATE_LOCKED && globalLockState != STATE_UNLOCKED)
        FeedbackRequest(FEEDBACK_ERROR);
}
