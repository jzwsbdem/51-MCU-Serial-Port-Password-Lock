#include "Timer.h"
#include <REGX52.h>
#include <intrins.h>
#include "LEDDisplay.h"
#include "Core/LockFeedback.h"

typedef struct {
    unsigned long next;
    unsigned int period;
    TaskFunction function;
} TimerTask;

static TimerTask xdata tasks[MAX_TASKS];
static volatile unsigned long ticks;
static unsigned char fastDivider;
static unsigned char tickPhase;
static bit processing;
unsigned char taskAddFailures;

void Timer0Init()
{
    unsigned char i;

    TR0 = 0;
    ET0 = 0;
    TR2 = 0;
    ET2 = 0;
    ticks = 0;
    taskAddFailures = 0;
    fastDivider = 0;
    tickPhase = 0;
    processing = 0;
    for (i = 0; i < MAX_TASKS; i++)
    {
        tasks[i].function = 0;
        tasks[i].period = 0;
        tasks[i].next = 0;
    }
    T2CON = 0;
    T2MOD = 0;
    RCAP2L = TL2 = 0x66;
    RCAP2H = TH2 = 0xFC;
    PT2 = 0;
    ET2 = 1;
    TR2 = 1;
}

//使用Timer2进一步提高准确度
void Timer0Routine() INTERRUPT(5)
{
    TF2 = 0;
    if (++tickPhase == 5) tickPhase = 0;
    RCAP2L = (tickPhase == 1 || tickPhase == 3) ? 0x67 : 0x66;
    ticks++;
    if (++fastDivider == 2)
    {
        fastDivider = 0;
        ShowLoop();
        TaskFeedback();
    }
}

unsigned long TaskGetTick()
{
    unsigned long now;
    bit enabled;

    enabled = EA;
    EA = 0;
    now = ticks;
    EA = enabled;
    return now;
}

unsigned long TaskGetNextTick(TaskFunction function)
{
    unsigned char i;

    if (function == 0) return 0;
    for (i = 0; i < MAX_TASKS; i++)
        if (tasks[i].function == function)
            return tasks[i].next;
    return 0;
}

void TaskProcess()
{
    static unsigned char xdata i;
    static unsigned long xdata now;
    static TaskFunction xdata function;

    if (processing) return;
    processing = 1;
    now = TaskGetTick();
    for (i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i].function == 0)
            continue;
        if ((long)(now - tasks[i].next) < 0)
            continue;
        tasks[i].next += tasks[i].period;
        function = tasks[i].function;
        if (function != 0)
            function();
    }
    processing = 0;
}

unsigned char TaskAdd(unsigned int period, TaskFunction function)
{
    unsigned char i;

    if (period == 0 || function == 0)
        return 0;
    for (i = 0; i < MAX_TASKS; i++)
        if (tasks[i].function == function)
            return 1;
    for (i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i].function == 0)
        {
            tasks[i].period = period;
            tasks[i].next = TaskGetTick() + period;
            tasks[i].function = function;
            return 1;
        }
    }
    if (taskAddFailures != 255) taskAddFailures++;
    return 0;
}

void TaskDelete(TaskFunction function)
{
    unsigned char i;

    for (i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i].function == function)
        {
            tasks[i].function = 0;
            tasks[i].period = 0;
            tasks[i].next = 0;
        }
    }
}

unsigned int TaskGetPeriod(TaskFunction function)
{
    unsigned char i;

    if (function == 0) return 0;
    for (i = 0; i < MAX_TASKS; i++)
        if (tasks[i].function == function)
            return tasks[i].period;
    return 0;
}

void TaskSetPeriod(unsigned int period, TaskFunction function)
{
    unsigned char i;

    if (period == 0 || function == 0)
        return;
    for (i = 0; i < MAX_TASKS; i++)
        if (tasks[i].function == function)
        {
            tasks[i].period = period;
            tasks[i].next = TaskGetTick() + period;
            return;
        }
}

unsigned char TaskExist(TaskFunction function)
{
    unsigned char i;

    if (function == 0) return 0;

    for (i = 0; i < MAX_TASKS; i++)
        if (tasks[i].function == function)
            return 1;
    return 0;
}

unsigned char TaskCount()
{
    unsigned char i, count = 0;

    for (i = 0; i < MAX_TASKS; i++)
        if (tasks[i].function != 0)
            count++;
    return count;
}

void Delay(unsigned int ms)
{
    while (ms--)
    {
        unsigned char i, j;
        _nop_();
        _nop_();
        i = 11;
        j = 190;
        do { while (--j); } while (--i);
    }
}
