#ifndef IPC_FORMAT_H
#define IPC_FORMAT_H

#include <stdint.h>
#include <linux/limits.h>
#include <semaphore.h>

#define MAX_WORKERS 16
#define MAX_JOBS_QUEUE 50000
#define MAX_RESULTS_QUEUE 50000
#define DB_PART_COMPLETE 0
#define DB_FULL_COMPLETE 1
#define DB_START 2

typedef struct {
    char magic[4];
    uint32_t format_version; 
    uint32_t complete; 
    uint32_t file_record_count; 
    uint32_t worker_count;
} DBHeader;

typedef struct {
    char dir_path[PATH_MAX];
    int depth;
} Job;

typedef struct {
    char path[PATH_MAX];
    uint32_t size;
    uint64_t mtime;
    mode_t mode;
    uid_t uid;
    gid_t gid;
    unsigned char sha256_hash[32];
} FileRecord;

typedef struct {
    Job buffer[MAX_JOBS_QUEUE];
    int head;
    int tail;
    sem_t sem_mutex;
    sem_t sem_empty;
    sem_t sem_full;
} JobQueue;

typedef struct {
    FileRecord buffer[MAX_RESULTS_QUEUE];
    int head;
    int tail;
    sem_t sem_mutex;
    sem_t sem_empty;
    sem_t sem_full;
} ResultChannel;

typedef struct {
    uint32_t worker_id;
    pid_t pid;
    int exit_status;
    uint32_t jobs_processed;
    uint32_t files_emitted;
    uint64_t bytes_emitted;
    uint64_t wall_time_ms;
    uint64_t user_cpu_us;
    uint64_t sys_cpu_us;
} WorkerStats;

typedef struct {
    char magic[4];
    uint32_t is_running;
    uint32_t active_jobs;
    uint32_t simulate_work_ms;
    sem_t global_mutex;
    JobQueue job_queue;
    ResultChannel result_channels[MAX_WORKERS];
    WorkerStats worker_stats[MAX_WORKERS];
} IPC_SharedData;

#endif
