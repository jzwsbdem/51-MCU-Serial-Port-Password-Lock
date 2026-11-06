#ifndef LOCKSAVER_H
#define LOCKSAVER_H

#include <REGX52.h>

void LockSaverInit();
bit LockSaverLoad();
bit LockSaverBusy();
bit LockSaverFailed();
bit LockSaverReadFailed();
void TaskSavePending();
void TriggerSavePassword();
void TriggerSaveWrongCount();
void TriggerSaveState();

#endif
