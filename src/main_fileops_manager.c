#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include "ipc_format.h"
#include "ring_buffer.h"

volatile sig_atomic_t flag_status_req = 0;
volatile sig_atomic_t flag_shutdown_req = 0;

void manager_signal_handler(int sig) {
    if (sig == SIGUSR1) 
        flag_status_req = 1;
   else if (sig == SIGINT || sig == SIGTERM)
        flag_shutdown_req = 1;
}

int main(int argc, char *argv[]) {
    char *root_dir = NULL;
    int num_workers = 0;
    char *ipc_path = "data/ipc.mmap";
    char *db_path = "data/inventory.db";
    int max_depth = -1;
    int simulate_work_ms = 0;
    int graceful_timeout = 5;
    char *pid_file_path = NULL;
    pid_t worker_pids[MAX_WORKERS];
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) root_dir = argv[++i];
        else if (strcmp(argv[i], "--workers") == 0 && i + 1 < argc) num_workers = atoi(argv[++i]);
        else if (strcmp(argv[i], "--ipc") == 0 && i + 1 < argc) ipc_path = argv[++i];
        else if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) db_path = argv[++i];
        else if (strcmp(argv[i], "--max-depth") == 0 && i + 1 < argc) max_depth = atoi(argv[++i]);
        else if (strcmp(argv[i], "--simulate-work-ms") == 0 && i + 1 < argc) simulate_work_ms = atoi(argv[++i]);
        else if (strcmp(argv[i], "--graceful-timeout") == 0 && i + 1 < argc) graceful_timeout = atoi(argv[++i]);
        else if (strcmp(argv[i], "--pid-file") == 0 && i + 1 < argc) pid_file_path = argv[++i];
    }
    int verify_mode = 0;
    int dump_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verify") == 0) verify_mode = 1;
        else if (strcmp(argv[i], "--dump") == 0) dump_mode = 1;
    }
    if (verify_mode || dump_mode) {
        int fd = open(db_path, O_RDONLY);
        if (fd < 0) { 
            perror("Eroare deschidere DB pentru citire"); 
            return 1; 
        }
        DBHeader header;
        if (read(fd, &header, sizeof(DBHeader)) != sizeof(DBHeader)) {
            fprintf(stderr, "Eroare: Baza de date este corupta sau prea mica.\n");
            close(fd); 
            return 1;
        }

        if (verify_mode) {
            if (strncmp(header.magic, "INV4", 4) == 0 && header.format_version == 1 && (header.complete == DB_PART_COMPLETE || header.complete == DB_FULL_COMPLETE)) {
                printf("Baza de date este valida.\n");
            } else {
                fprintf(stderr, "Baza de date INVALIDA!\n");
            }
        }

        if (dump_mode) {
            printf("magic %.*s\n", 4, header.magic);
            printf("version %u\n", header.format_version);
            printf("complete %u\n", header.complete);
            printf("file_record_count %u\n", header.file_record_count);
            printf("worker_count %u\n", header.worker_count);
        }

        close(fd);
        return 0;
    }
    else if (!root_dir || num_workers < 1) {
        fprintf(stderr, "Eroare: --root si --workers >= 1 sunt obligatorii.\n");
        return 1;
    }
    if (pid_file_path != NULL) {
        FILE *pid_file = fopen(pid_file_path, "w");
        if (pid_file) {
            fprintf(pid_file, "%d\n", getpid());
            fclose(pid_file);
            printf("Manager: PID %d salvat in %s\n", getpid(), pid_file_path);
        } else {
            perror("Manager: Eroare la crearea fisierului PID");
        }
    }
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = manager_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    int fd_ipc = open(ipc_path, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd_ipc < 0) {
        perror("Eroare creare fisier IPC");
        return 1;
    }
    if (ftruncate(fd_ipc, sizeof(IPC_SharedData)) == -1) {
        perror("Eroare ftruncate");
        return 1;
    }
    IPC_SharedData *shared_data = mmap(NULL, sizeof(IPC_SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, fd_ipc, 0);
    if (shared_data == MAP_FAILED) {
        perror("Eroare mmap");
        return 1;
    }
    close(fd_ipc);
    
    memset(shared_data, 0, sizeof(IPC_SharedData));
    strncpy(shared_data->magic, "IPC4", 4);
    shared_data->is_running = 1;
    shared_data->active_jobs = 1;
    shared_data->simulate_work_ms = simulate_work_ms;
    sem_init(&shared_data->global_mutex, 1, 1);
    init_job_queue(&shared_data->job_queue);
    for (int i = 0; i < num_workers; i++) {
        init_result_channel(&shared_data->result_channels[i]); 
    }
    
    Job initial_job;
    memset(&initial_job, 0, sizeof(Job));
    if (realpath(root_dir, initial_job.dir_path) == NULL) {
        perror("Eroare realpath root_dir");
        return 1;
    }
    initial_job.depth = 0;
    push_job(&shared_data->job_queue, &initial_job);
    int control_pipe[2];
    if (pipe(control_pipe) == -1) {
        perror("Eroare creare pipe de control");
        return 1;
    }
    for (int i = 0; i < num_workers; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            close(control_pipe[0]);
            char worker_id_str[16];
            char max_depth_str[16];
            char control_fd_str[16];
            snprintf(control_fd_str, sizeof(control_fd_str), "%d", control_pipe[1]);
            snprintf(worker_id_str, sizeof(worker_id_str), "%d", i);
            snprintf(max_depth_str, sizeof(max_depth_str), "%d", max_depth);
            execl("bin/fileops_worker", "fileops_worker", "-worker-id", worker_id_str, "--ipc", ipc_path, "--max-depth", max_depth_str, "--control-fd", control_fd_str, NULL);
            perror("Eroare execl");
            exit(1); 
        }
        else if (pid > 0) {
            worker_pids[i] = pid;
        }
        else  {
            perror("Eroare fork");
        }
    }
    close(control_pipe[1]);
    int flags = fcntl(control_pipe[0], F_GETFL, 0);
    fcntl(control_pipe[0], F_SETFL, flags | O_NONBLOCK);
    printf("Manager a lansat %d workeri. Incepem colectarea...\n", num_workers);

    char tmp_db_path[PATH_MAX];
    snprintf(tmp_db_path, PATH_MAX, "%s.tmp", db_path);
    int fd_db = open(tmp_db_path, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd_db < 0) { perror("Eroare creare temp DB"); return 1; }
    DBHeader initial_header;
    memset(&initial_header, 0, sizeof(DBHeader));
    strncpy(initial_header.magic, "INV4", 4);
    initial_header.format_version = 1;
    initial_header.complete = DB_START;
    write(fd_db, &initial_header, sizeof(DBHeader));
    uint32_t total_files_written = 0;
    int all_done = 0;
    int  retry = 0;
    while (!all_done) {
        char pipe_buf[4096];
        int bytes_read;
        while ((bytes_read = read(control_pipe[0], pipe_buf, sizeof(pipe_buf) - 1)) > 0) {
            pipe_buf[bytes_read] = '\0';
            printf("Manager a primit mesaj: %s", pipe_buf);
        }
        if (flag_status_req) {
            flag_status_req = 0;
            int jobs_in_q; 
            sem_getvalue(&shared_data->job_queue.sem_full, &jobs_in_q);
            uint64_t total_bytes_live = 0;
            for (int w = 0; w < num_workers; w++) {
                total_bytes_live += shared_data->worker_stats[w].bytes_emitted;
            }
            printf("STATUS queued_jobs=%d active_jobs=%d files=%u bytes=%lu workers_alive=%d complete=0\n", jobs_in_q, shared_data->active_jobs, total_files_written, total_bytes_live, num_workers);
            fflush(stdout);
        }
        static int shutdown_requested = 0;
        static time_t shutdown_start_time = 0;
        if (flag_shutdown_req && !shutdown_requested) {
            printf("Manager: Shutdown cerut. Trimitere SIGTERM la workeri. Asteptam %d secunde...\n", graceful_timeout);
            shutdown_requested = 1;
            shutdown_start_time = time(NULL);
            shared_data->is_running = 0;
            for (int w = 0; w < num_workers; w++) {
                kill(worker_pids[w], SIGTERM);
            }
        }
        if (shutdown_requested) {
            if (time(NULL) - shutdown_start_time >= graceful_timeout) {
                printf("Manager: Timeout expirat! Trimitere SIGKILL restantiere...\n");
                for (int w = 0; w < num_workers; w++) {
                    kill(worker_pids[w], SIGKILL);
                }
                all_done = 1;
            }
        }
        int activity_in_this_cycle = 0;
        for (int i = 0; i < num_workers; i++) {
            ResultChannel *ch = &shared_data->result_channels[i];
            FileRecord rec;
            if (try_pop_result(ch, &rec)) {
                write(fd_db, &rec, sizeof(FileRecord));
                total_files_written++;
                activity_in_this_cycle = 1;
            }
        }
        sem_wait(&shared_data->global_mutex);
        int jobs_in_queue;
        sem_getvalue(&shared_data->job_queue.sem_full, &jobs_in_queue);
        if (jobs_in_queue == 0 && shared_data->active_jobs == 0 && activity_in_this_cycle == 0) {
            all_done = 1; 
            shared_data->is_running = 0;
        }
        sem_post(&shared_data->global_mutex);
        if (!activity_in_this_cycle && !all_done) {
            usleep(1000); 
        }
    }
    printf("Toate directoarele au fost scanate. Oprire workeri...\n");
    for (int i = 0; i < num_workers; i++) {
        Job poison_pill;
        memset(&poison_pill, 0, sizeof(Job)); 
        push_job(&shared_data->job_queue, &poison_pill);
    }
    for (int i = 0; i < num_workers; i++) {
        int status;
        pid_t child_pid = waitpid(-1, &status, 0);
        for (int w = 0; w < num_workers; w++) {
            if (shared_data->worker_stats[w].pid == child_pid) {
                if (WIFEXITED(status)) {
                    shared_data->worker_stats[w].exit_status = WEXITSTATUS(status);
                }
            }
        }
    }
    for (int i = 0; i < num_workers; i++) {
        write(fd_db, &shared_data->worker_stats[i], sizeof(WorkerStats));
    }
    DBHeader final_header;
    strncpy(final_header.magic, "INV4", 4);
    final_header.format_version = 1;
    final_header.complete = flag_shutdown_req ? DB_PART_COMPLETE : DB_FULL_COMPLETE;
    final_header.file_record_count = total_files_written;
    final_header.worker_count = num_workers;
    lseek(fd_db, 0, SEEK_SET);
    write(fd_db, &final_header, sizeof(DBHeader));
    close(fd_db);
    if (rename(tmp_db_path, db_path) == 0) {
        printf("Baza de date a fost generata cu succes: %s\n", db_path);
    } else {
        perror("Eroare la redenumirea bazei de date (rename)");
    }
    return 0;
}
