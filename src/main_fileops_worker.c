#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/resource.h>
#include <openssl/evp.h>
#include "ipc_format.h"
#include "ring_buffer.h"
#include <signal.h>

volatile sig_atomic_t worker_shutdown_req = 0;

void worker_signal_handler(int sig) {
    if (sig == SIGTERM) {
        worker_shutdown_req = 1;
    }
}

void send_control_message(int fd, int worker_id, const char *type, const char *extra_args) {
    char buffer[512];
    int len;
    if (extra_args != NULL) {
        len = snprintf(buffer, sizeof(buffer), "T5MSG type=%s worker_id=%d %s\n", type, worker_id, extra_args);
    } else {
        len = snprintf(buffer, sizeof(buffer), "T5MSG type=%s worker_id=%d\n", type, worker_id);
    }
    write(fd, buffer, len);
}

void calculate_sha256(const char *path, unsigned char output_hash[32]) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        memset(output_hash, 0, 32);
        return;
    }
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        EVP_DigestUpdate(mdctx, buffer, bytes_read);
    }
    unsigned int md_len;
    EVP_DigestFinal_ex(mdctx, output_hash, &md_len);
    EVP_MD_CTX_free(mdctx);
    close(fd);
}

int main(int argc, char *argv[]) {
    int worker_id = -1;
    char *ipc_path = NULL;
    int max_depth = -1;
    int control_fd = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-worker-id") == 0 && i + 1 < argc) worker_id = atoi(argv[++i]);
        else if (strcmp(argv[i], "--ipc") == 0 && i + 1 < argc) ipc_path = argv[++i];
        else if (strcmp(argv[i], "--max-depth") == 0 && i + 1 < argc) max_depth = atoi(argv[++i]);
        else if (strcmp(argv[i], "--control-fd") == 0 && i + 1 < argc) control_fd = atoi(argv[++i]);
    }
    if (worker_id == -1 || !ipc_path || control_fd == -1) {
        fprintf(stderr, "Eroare: Argumente worker invalide.\n");
        return 1;
    }
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = worker_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa, NULL);
    
    int fd_ipc = open(ipc_path, O_RDWR);
    if (fd_ipc < 0) return 1;
    IPC_SharedData *shared = mmap(NULL, sizeof(IPC_SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, fd_ipc, 0);
    close(fd_ipc);
    if (shared == MAP_FAILED) return 1;
    WorkerStats *my_stats = &shared->worker_stats[worker_id];
    my_stats->worker_id = worker_id;
    my_stats->pid = getpid();
    while (shared->is_running && !worker_shutdown_req) {
        Job current_job;
        pop_job(&shared->job_queue, &current_job);
        if (strlen(current_job.dir_path) == 0) break;
        if (shared->simulate_work_ms > 0) {
            usleep(shared->simulate_work_ms * 1000);
        }
        DIR *dir = opendir(current_job.dir_path);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
                char full_path[PATH_MAX + 256];
                snprintf(full_path, sizeof(full_path), "%s/%s", current_job.dir_path, entry->d_name);
                struct stat st;
                if (lstat(full_path, &st) < 0) continue;
                if (S_ISLNK(st.st_mode)) continue; 
                if (S_ISDIR(st.st_mode)) {
                    if (max_depth == -1 || current_job.depth < max_depth) {
                        Job new_job;
                        strncpy(new_job.dir_path, full_path, PATH_MAX);
                        new_job.depth = current_job.depth + 1;
                        sem_wait(&shared->global_mutex);
                        shared->active_jobs++;
                        sem_post(&shared->global_mutex);    
                        push_job(&shared->job_queue, &new_job); 
                    }
                }
                else if (S_ISREG(st.st_mode)) {
                    FileRecord rec;
                    memset(&rec, 0, sizeof(FileRecord));
                    strncpy(rec.path, full_path, PATH_MAX);
                    rec.size = st.st_size;
                    rec.mtime = st.st_mtime;
                    rec.mode = st.st_mode;
                    rec.uid = st.st_uid;
                    rec.gid = st.st_gid;
                    calculate_sha256(full_path, rec.sha256_hash);
                    push_result(&shared->result_channels[worker_id], &rec);
                    my_stats->files_emitted++;
                    my_stats->bytes_emitted += st.st_size;
                }
            }
            closedir(dir);
        }
        my_stats->jobs_processed++;
        sem_wait(&shared->global_mutex);
        shared->active_jobs--;
        sem_post(&shared->global_mutex);
    }
    send_control_message(control_fd, worker_id, "WORKER_EXITING", "reason=shutdown");
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    my_stats->user_cpu_us = usage.ru_utime.tv_sec * 1000000 + usage.ru_utime.tv_usec;
    my_stats->sys_cpu_us = usage.ru_stime.tv_sec * 1000000 + usage.ru_stime.tv_usec;
    my_stats->exit_status = 0;
    return 0;
}
