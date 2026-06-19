Structura Fisierului
Baza de date este un fisier binar scris in aceasta ordine:
1. DBHeader: magic ("INV4"), versiune (1), flag complete, numar total de fisiere (file_record_count) si numar de workeri (worker_count).
2. File Records: Un vector de file_record_count elemente. Fiecare detine path, size, mtime, permisiuni, uid/gid si hash-ul SHA256.
3. Worker Stats: Un vector de worker_count elemente. Conține date despre performanta fiecarui proces (pid, fisiere procesate, timp CPU user/sys obtinut cu getrusage).

Conditii de validitate (--verify)
Managerul considera baza de date valida daca fisierul incepe cu magic == "INV4", are format_version == 1, iar complete == 1 (indica o terminare normala, fara intreruperi).

Siguranta si scriere atomica
Workerii nu ating fisierul final. Doar managerul construieste baza de date.
Pentru a garanta scrierea atomica, datele sunt scrise intr-un fișier .tmp. Abia dupa ce totul este gata si verificat, se apelează rename() pentru a inlocui vechea baza de date cu cea noua.
