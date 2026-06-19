DOCUMENTATIE CONTROL PLANE - TEMA 5

1. Canal de Comunicare Pipe
Sistemul utilizeaza un pipe anonim creat in manager inainte de fork.
Workerii mostenesc file descriptor-ul si inchid capatul de citire.
Managerul inchide capatul de scriere si seteaza capatul de citire in mod neblocant (O_NONBLOCK) pentru a citi asincron mesajele fara sa blocheze fluxul principal de date.

2. Formatul Mesajelor T5MSG
Mesajele trimise de workeri catre manager sunt text, atomice (un singur apel write, dimensiune sub PIPE_BUF) si se termina cu newline.
Format general: T5MSG type=TIP worker_id=ID [argumente_suplimentare]
Exemplu la finalizare: T5MSG type=WORKER_EXITING worker_id=2 reason=shutdown

3. Raportarea Statusului (SIGUSR1)
La receptionarea semnalului SIGUSR1, managerul seteaza un flag atomic. In bucla principala, flag-ul este detectat si se afiseaza linia de status standardizata:
STATUS queued_jobs=X active_jobs=X files=X bytes=X workers_alive=X complete=0
Datele despre bytes sunt agregate live din structurile de statistici ale workerilor aflate in mmap.

4. Gestionarea Semnalelor
SIGUSR1: Seteaza flag pentru afisare status.
SIGINT / SIGTERM: Declanșeaza oprirea controlata a aplicatiei.
SIGCHLD: Permite curatarea completa a proceselor fiu prin apeluri waitpid, prevenind aparitia proceselor zombie.
In handlerele de semnal se seteaza doar variabile globale de tip volatile sig_atomic_t.

5. Shutdown Gratios
La primirea SIGINT sau SIGTERM, managerul:
- Seteaza is_running la 0 in mmap pentru a opri distribuirea de joburi noi.
- Trimite SIGTERM catre toti workerii salvati in vectorul de PID-uri.
- Asteapta confirmarea sau expirarea timpului de gratie definit prin --graceful-timeout.
- Daca timeout-ul expira, trimite SIGKILL workerilor activi.
- Efectueaza waitpid pentru toate procesele worker.

6. Semantica complete=0
Daca procesul este intrerupt prin semnal, headerul bazei de date este salvat in mod atomic (prin scriere in fisier .tmp si rename) avand campul complete setat la constanta DB_PART_COMPLETE (valoarea 0). Aceasta indica o inventariere valida structural, dar incompleta. Comanda --verify accepta acest status ca fiind valid.
