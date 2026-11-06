#ifndef LOCKSYSTEM_H
#define LOCKSYSTEM_H

#include <REGX52.h>
#include "LockConfig.h"

typedef enum 
{
    STATE_LOCKED = 0,
    STATE_UNLOCKED = 1,
    STATE_NEW1 = 2,
    STATE_NEW2 = 3,
    STATE_WAIT = 4,
    STATE_PROTECT = 5,
    STATE_UPGRADE = 6
} 
LockState;

extern unsigned char xdata globalPassword[PASSWORD_LENGTH];
extern unsigned char xdata tempPassword[PASSWORD_LENGTH];

extern LockState globalLockState;
extern unsigned char globalWrongCount;
extern unsigned int globalWrongTimeRemain;
extern unsigned int globalUnlockTimeRemain;

void LockInit();
void LockTaskEnsure();
void RefreshUnlockTime();
unsigned int GetUnlockTimeRemain();
unsigned int GetLockTimeRemain();
void CheckUnlockTimeout();
void EnterLocked();
void EnterUnlocked();
void EnterWait();
void EnterProtect();
void EnterUpgrade();
bit PasswordCheck(const unsigned char *input);
void OnPasswordRight();
void OnPasswordWrong();
void TaskLockTimeRemain();
void TaskUnlockTimeRemain();
void TaskISP();

#endif
