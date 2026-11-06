#include "LockFeedback.h"
#include <REGX52.h>

sbit buzzer = P2^5;
sbit led = P2^6;

static volatile unsigned char requested;
static unsigned char xdata active, ticksLeft, pulses;
static bit gap;

void FeedbackInit()
{
    requested = active = ticksLeft = pulses = 0;
    gap = 0;
    buzzer = 0;
    led = 1;
}

void FeedbackRequest(unsigned char event)
{
    if (event > requested) requested = event;
}

void FeedbackExecute()
{
    FeedbackRequest(FEEDBACK_NORMAL);
}

void TaskFeedback()
{
    unsigned char event;

    event = requested;
    requested = 0;
    if (event > active)
    {
        active = event;
        pulses = event == FEEDBACK_LIMIT ? 3 : (event == FEEDBACK_SUCCESS ? 2 : 1);
        ticksLeft = event == FEEDBACK_LIMIT ? 50 : (event == FEEDBACK_SUCCESS ? 25 : (event == FEEDBACK_ERROR ? 100 : 15));
        gap = 0;
        buzzer = 1;
        led = 0;
        return;
    }
    if (active == 0) return;
    if (--ticksLeft == 0)
    {
        if (!gap && --pulses == 0)
        {
            active = 0;
            buzzer = 0;
            led = 1;
            return;
        }
        gap = !gap;
        ticksLeft = active == FEEDBACK_LIMIT ? 50 : 25;
        led = gap ? 1 : 0;
        buzzer = gap ? 0 : 1;
    }
    else if (!gap)
        buzzer = !buzzer;
}
