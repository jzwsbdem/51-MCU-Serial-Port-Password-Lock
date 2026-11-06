#ifndef LOCKINPUT_H
#define LOCKINPUT_H

#include "LockConfig.h"

extern unsigned char xdata inputPassword[PASSWORD_LENGTH];
extern unsigned char inputIndex;

void InputMatrixKey();
void LockInputInit();
void InputDigit(unsigned char number);
void ClearInput();

typedef enum
{
    CONFIRM_OK,
    CONFIRM_DENIED,
    CONFIRM_INCOMPLETE,
    CONFIRM_WRONG,
    CONFIRM_MISMATCH,
    CONFIRM_PENDING,
    CONFIRM_STORAGE
} 
ConfirmResult;

ConfirmResult Confirm();
void ConfirmSaveProcess();
ConfirmResult ConfirmSaveResult();
void Delete();
void Clear();
void Lock();
void Change();
void Cancel();

#endif
