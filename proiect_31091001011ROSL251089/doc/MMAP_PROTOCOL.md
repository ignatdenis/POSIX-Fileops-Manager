Formatul Memoriei Partajate (mmap cu MAP_SHARED)
Structura IPC_SharedData contine:
1. Header: sirul magic "IPC4", contoare de stare (is_running, active_jobs) si un mutex global.
2. Job Queue: Buffer circular (1024 elemente) pentru directoarele de scanat.
3. Result Channels: Vector de 16 buffere circulare (capacitate 1024 fiecare), cate unul pentru fiecare worker, pentru a trimite structurile FileRecord inapoi la manager.
4. Statistici: Vector in care workerii isi raporteaza timpii rusage la final.

Sincronizare
Toate cozile folosesc semafoare POSIX anonime setate pe process-shared=1. Fiecare coada are:
- sem_mutex: lock pentru modificarea sigura a indecsilor de scriere/citire.
- sem_empty: numara locurile libere.
- sem_full: numara elementele disponibile pentru citire.

Backpressure
Daca un canal de rezultate este plin, workerul este blocat de sem_wait(&sem_empty). Ramane blocat pana cand managerul consuma un rezultat și elibereaza un loc, prevenind pierderea sau suprascrierea datelor.

Limite alese
- Maxim 16 workeri.
- Maxim 1024 directoare in coada de job-uri.
- Maxim 1024 rezultate in asteptare per worker.
