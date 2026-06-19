#include "ipc_format.h"
#include <stdio.h>

void init_job_queue(JobQueue *q) {
    q->head = 0;
    q->tail = 0;
    sem_init(&q->sem_mutex, 1, 1); 
    sem_init(&q->sem_empty, 1, MAX_JOBS_QUEUE); 
    sem_init(&q->sem_full,  1, 0); 
}

void push_job(JobQueue *q, const Job *j) {
    sem_wait(&q->sem_empty); 
    sem_wait(&q->sem_mutex); 
    q->buffer[q->tail] = *j;
    q->tail = (q->tail + 1) % MAX_JOBS_QUEUE; 
    sem_post(&q->sem_mutex); 
    sem_post(&q->sem_full);  
}

void pop_job(JobQueue *q, Job *out_j) {
    sem_wait(&q->sem_full);  
    sem_wait(&q->sem_mutex); 
    *out_j = q->buffer[q->head];
    q->head = (q->head + 1) % MAX_JOBS_QUEUE; 
    sem_post(&q->sem_mutex); 
    sem_post(&q->sem_empty); 
}

void init_result_channel(ResultChannel *ch) {
    ch->head = 0;
    ch->tail = 0;
    sem_init(&ch->sem_mutex, 1, 1);
    sem_init(&ch->sem_empty, 1, MAX_RESULTS_QUEUE);
    sem_init(&ch->sem_full, 1, 0);
}

void push_result(ResultChannel *ch, const FileRecord *rec) {
    sem_wait(&ch->sem_empty);
    sem_wait(&ch->sem_mutex);
    ch->buffer[ch->tail] = *rec;
    ch->tail = (ch->tail + 1) % MAX_RESULTS_QUEUE;
    sem_post(&ch->sem_mutex);
    sem_post(&ch->sem_full);
}

int try_pop_result(ResultChannel *ch, FileRecord *out_rec) {
    if (sem_trywait(&ch->sem_full) == 0) {
        sem_wait(&ch->sem_mutex);
        *out_rec = ch->buffer[ch->head];
        ch->head = (ch->head + 1) % MAX_RESULTS_QUEUE;
        sem_post(&ch->sem_mutex);
        sem_post(&ch->sem_empty);
        return 1;
    }
    return 0;
}
