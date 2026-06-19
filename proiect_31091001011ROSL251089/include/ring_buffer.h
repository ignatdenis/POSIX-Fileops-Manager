#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "ipc_format.h"

void init_job_queue(JobQueue *q);
void push_job(JobQueue *q, const Job *j);
void pop_job(JobQueue *q, Job *out_j);
void init_result_channel(ResultChannel *ch);
void push_result(ResultChannel *ch, const FileRecord *rec);
int try_pop_result(ResultChannel *ch, FileRecord *out_rec);

#endif
