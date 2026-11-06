#ifndef TIMER_H
#define TIMER_H

#ifdef __VSCODE_C51__
#define INTERRUPT(x)
#else
#define INTERRUPT(x) interrupt x
#endif

#define MAX_TASKS 15

typedef void (*TaskFunction)();

extern unsigned char taskAddFailures;

void Timer0Init();
void Timer0Routine();
unsigned long TaskGetTick();
unsigned long TaskGetNextTick(TaskFunction function);
void TaskProcess();
unsigned char TaskAdd(unsigned int period, TaskFunction function);
void TaskDelete(TaskFunction function);
void TaskSetPeriod(unsigned int period, TaskFunction function);
unsigned int TaskGetPeriod(TaskFunction function);
unsigned char TaskExist(TaskFunction function);
unsigned char TaskCount();
void Delay(unsigned int ms);

#endif
