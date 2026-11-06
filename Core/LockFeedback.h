#ifndef LOCKFEEDBACK_H
#define LOCKFEEDBACK_H

#define FEEDBACK_NORMAL 1
#define FEEDBACK_SUCCESS 2
#define FEEDBACK_ERROR 3
#define FEEDBACK_LIMIT 4

void FeedbackInit();
void FeedbackExecute();
void FeedbackRequest(unsigned char event);
void TaskFeedback();

#endif
