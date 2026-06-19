1. Descrierea Structurii Binare:

     -toate fisierele .db sunt formate din:
     A. Header: - orice fisier baza de date incepe cu header de dimensiune fixa (DBHEADER)
                - campuri:  -magic (char[4]) 
                                            - tine minte tipul fisierului IDX (pt fisiere) sau PRCI (pt procese)
                                            - un vector de caractere de lungime 4 este deajuns pentru a stoca IDX sau PRCI
                           -format_version (uint32_t) 
                                            - indica versiunea structurii interne
                           - snapshot_id (uint32_t) 
                                            - ID unic pentru sesiunea curenta de captura (folosind time(NULL))
                                            - garanteaza ca toate instantele concurente stiu ca fac parte din acelasi snapshot
                           - state (uint32_t) 
                                            - monitorizeaza starea bazei de date: 0 pentru OPEN, 1 pentru SEALED
                           - active_writers (uint32_t) 
                                            - contorizeaza cate procese scriu simultan in fisier
                                            - daca ajunge la 0, ultima instanta care se termina marcheaza snapshotul ca sealed
                           - record_count (uint32_t) 
                                            - numarul total de inregistrari din baza de date
                                            - limita de 4.2 miliarde este suficienta pentru orice utilizare practica
                           - root_path (char[PATH_MAX]) - retine calea absoluta de unde a inceput indexarea
                                                        - foloseste o limita standard impusa de kernel-ul Linux (<linux/limits.h>) pentru compatibilitate
                                                        
     B. Inregistrari Fisiere: - o structura de dimensiune fixa (FileRecord) ce stocheaza metadatele fisierelor indexate 
                - campuri:
                          - path (char[PATH_MAX]) - retine calea absoluta pentru cautare si comparare 
                                            - foloseste MAX_PATH impus de kernel pentru a stoca corect oricare cale, indiferent de adancime 
                          - type (uint8_t) - retine tipul intrarii (1=Regular, 2=Dir, 3=Symlink, 4=FIFO) 
                                           - un intreg pe 8 biti este suficient deoarece nu exista mai mult de 255 de tipuri POSIX
                          - size (uint32_t) - retine dimensiunea fisierului regulat in octeti (0 pentru celelalte tipuri) 
                                            - definit pe 64 de biti deoarece un fisier poate depasi 4 GB
                          - mtime (uint64_t) - retine timpul ultimei modificari 
                                             - definit pe 64 de biti pentru a respecta standardul time_t modern
                          - checksum (uint32_t) - retine suma de control (XOR) pentru continutul fisierelor regulate 
                                                - 32 de biti ofera un spatiu suficient pentru modificari de continut intre snapshot-uri
                          - st_dev (uint64_t) - retine ID-ul partitiei / device-ului conform sistemului 
                                              - pastreaza formatul pe 64 de biti din structura stat originala
                          - st_ino (uint64_t) - retine Inod-ul fisierului
                                              - pastreaza formatul pe 64 de biti din structura stat originala
     C. Inregistrari Procese: - o structura de dimensiune fixa (ProcRecord) ce stocheaza detalii extrase din directorul /proc 
                 - campuri:
                          - pid, ppid (uint32_t) - retin ID-ul procesului, respectiv al procesului parinte 
                                                 - tipul de 32 de biti acopera limita kernel-ului
                          - state (char) - retine starea curenta a procesului (0 pentru open si 1 pentru sealed)
                          - comm (char[MAX_COMM_LEN]) - retine numele executabilului 
                                                      - limita standard a kernel-ului linux
                          - cmdline (char[MAX_CMDLINE_LEN]) - retine linia de comanda (argumentele procesului la rulare) 
                                                            - limita standard a kernel-ului Linux
                          - rss (uint32_t) - retine memoria consumata de proces
                                           - 32 de biti acopera procese care consuma pana la 4gb de ram
                          - cpu_time (uint32_t) - retine timpul CPU consumat (calculat ca suma dintre utime si stime) 
                                                - 32 de biti asigura inregistrarea corecta a ciclurilor procesor pentru aplicatiile care ruleaza de foarte mult timp
                          - process_status (uint8_t) -indicator de stare intern (0=PENDING, 1=DONE, 2=DEAD, 3=PROCESSING) 
                          
                
                
2. Fileops indexer:
     -procesul de indexare realizeaza parcurgerea recursiva a ierarhiei de directoare si salvarea metadatelor intr-un format binar stabil
     -etape: -attach: -fiecare instanta deschide fisierul .db si aplica un lacat (fcntl) pe header pentru a coordona accesul
                      -daca fisierul este nou, se genereaza un snapshot_id unic folosind time(NULL) si se seteaza starea pe OPEN
                      -daca snapshot-ul este deja deschis se incrementeaza campul active_writers
             -mersul prin arbore: -se folosesc functii ca opendir si readdir pentru a explora toate directoarele recursiv
                                  -si ignora directoarele "." si ".." pentru a preveni bucle infinite
             -extragerea datelor: - se foloseste apelul de sistem lstat pentru a colecta informatii: dimensiune, mtime, tip si identificatori binar (st_dev, st_ino)
                                  - pentru fisierele REGULAR se calculeaza checksum prin operatii XOR pe continutul citit in bucati de 4 octeti
             -protectie la dubluri: - inainte de scriere se aplica un lock de scriere pe regiunea inregistrarilor
                                    - programul cauta liniar calea absoluta, daca aceasta exista deja, record-ul este rescris la offset-ul curent pentru a evita dublurile
                                    - intrarile noi sunt adaugate la finalul fisierului, urmate de actualizarea campului record_count din header
             -detach: - dupa terminarea parcurgerii, fiecare instanta decrementeaza contorul active_writers 
                      - ultima instanta care ruleaza marcheaza snapshot-ul ca SEALED, sigiland astfel baza de date pentru viitoare comparatii
                      
3. Proccess snapshot:
       -preia informatiile despre procesele curente din sistem /proc si le stocheaza intr-o baza de date binara
       -etapele de executie: - attach: - instantele incearca deschiderea fisierului .db aplicand un lacat pe header
                                       -daca fisierul nu exista, prima instanta genereaza un snapshot_id, seteaza starea pe OPEN si scrie datele initiale
                                        -prima instanta parcurge directorul /proc si pre-incarca in baza de date doar PIDuri gasite in acel moment
                                        -aceste piduri se marcheaza cu PROC_PENDING
                                        -instantele concurente lansate ulterior reutilizeaza snapshot_id-ul si incrementeaza numarul de active_writers
                             -procesarea proceselor: -instantele concurente cauta in fisierul .db o inregistrare marcata ca PROC_PENDING
                                                     -la gasirea unui proces neprocesat instanta il marcheaza ca PROC_PROCESSING, isi salveaza offset-ul si elibereaza lacatul
                            -extragerea datelor: - programul citeste /proc/[pid]/stat pentru a obtine ppid, stare, rss si timpii utime/stime pentru a calcula cpu_time
                                                 -se extrag argumentele din /proc/[pid]/cmdline, inlocuind terminatorul null cu spatiu pentru a forma un sir continu
                                                 - daca procesul s-a inchis inainte sa poata fi citit, este capturat in baza de date cu starea PROC_DEAD, omarand astfel disparitia 
                                                 sa fara a invalida snapshot-ul
                             -salvarea cu protectie: - dupa colectarea tuturor datelor, instanta preia din nou lacatul exclusiv pe fisier navigheaza la offset-ul calculat anterior 
                                                    si suprascrie inregistrarea cu forma ei finala si completa, modificand statusul in PROC_DONE
                                                    
4. Data Base differences: -compara doua baze de date binare de acelasi tip si genereaza un raport text cu diferentele identificate
                          -deschide ambele fisiere si citeste structurile DBHeader pentru a verifica integritatea
                          -verifica daca ambele snapshot-uri sunt sealed, oprind executia daca datele sunt inca in starea OPEN
                          -verifica daca tipurile de baze de date coincid pe baza campului magic 
                          -daca fisierele nu au acelasi tip sau folosesc versiuni diferite ale formatului binar, programul se opreste
                          -incarcarea in memorie: - aloca dinamic memorie (malloc) in functie de dimensiunea record-urilor si valoarea record_count din header
                                                  -incarca toate inregistrarile in memorie printr-o singura operatie de citire
                           -comparari: -intrari disparute (STERS): Fisier care apare in baza veche dar nu cea nou
                                       -intrari aparute (NOU): Fisier care apare in baza noua dar nu cea veche
                                       -intrari modificate (MODIFICAT): Fisier care i-au fost modificate campurile type, size, mtime sau checksum
                                       
                                       -procese disparute (STERS): PID-urile care apar in snapshot-ul vechi dar nu in cel nou 
                                       -procese aparute (NOU): PID-urile care apar in snapshot-ul nou dar nu in cel vechi 
                                       -procese modificate semnificativ (MODIFICAT): PID-URILE care au diferente in campul state
                          
                                  
