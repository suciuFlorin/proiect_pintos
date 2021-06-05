# proiect_pintos

## Proiect PINTOS la SOA

###### Studenti

- Raluca Popa
- Filip Popa
- Ovidiu Miches
- Florin Suciu

## 1. Threads (Florin)

- Alarm Clock 
- Priority Scheduling 
- Advanced Scheduler

## 2. User Programs (Raluca, Ovidiu, Florin, Filip)

- Process Termination Messages
- Argument Passing
- System Calls
- Denying Writes to Executables

## 3. Virtual Memory (Filip)

- Paging
- Stack Growth
- Memory Mapped Files
- Accessing User Memory

## 4. File Systems (Nu a fost acoperit)

- Indexed and Extensible Files
- Subdirectories
- Buffer Cache
- Synchronization

# 1. Threads

# Alarm Clock

## Fisiere afectate

```
 thread.h
 timer.c
```

## Adaugari la structura de Thread-uri

  ```struct semaphore sema;``` - Un semafor ce poate fi manipulat pentru a pune threaduri in sleep si sa le trezim.

 ```int64_t sleep_duration;``` - Cat de mult trebuie sa fie in sleep thread-ul exprimat prin tacturi de la butarea OS-ului.

```struct list_elem timer_sleep_elem;``` - O lista de elemente pentru a contoriza thread-urile puse in sleep din timer.c.

> Intr-un call al functiei `timer_sleep()` se vor intampla urmatoarele.

Ne-am gandit sa asignam fiecarui thread o durata de sleep si un semafor care se initializeaza cu valoarea 0.
Pe urma adaugam thread-urile care sunt in sleep intr-o lista de procese si decrementam semaforul pentru a pune thread-ul in modul sleep.

> Intrerrupt handler se ocupa cu?

Procesul de validare care verifica potentialele thread-uri care se pot trezi si le trezeste.

> Performanta Intrerrupt handler-ului

Lista de thread-uri este sortata iar intrerrupt handler-ul va verifica de la varful listei si pe urma cele cu cea mai mica durata de somn (sleep). Daca un thread nu este pregatit sa se trezeasca (wake up) atunci se ignora restul listei pana la urmatoarea verificare.


## Sincronizarea

> Cum evitam problemele de sincronizare cand se face call la mai multe thread-uri?

Functia `timer_sleep()` este blocata pentru a asigura doar modificarea listei de thread-uri doar o singura data pe call. Singura operatie din `timer_sleep()` ce poate creea probleme de sincronizare este `list_insert_ordered()`.

> Cum evitam desincronizarea thread-urilor atunci cand intervine `timer intrerrupt` intr-un call `timer_sleep()`?

Zona de creere a desincronizarii este intre adaugarea unui thread la lista de thread-uri care dorm dar si la decrementarea semaforului din thread. La fiecare adaugare in lista de thread-uri se adauga si durata de somn `sleep_duration` iar `timer_intrerrupt` va inlatura thread-ul din lista doar daca este pregatit de trezire (wake-up).

## Notite

Design-ul a fost gandit ca sa evitam blocarea call-ului de `timer_intrerrupt` cu ajutorul semaforului dar si a listei de thread-uri puse in sleep.

# Priority Scheduling

## Fisiere afectate

```
 thread.h
```

## Adaugari

`struct list donated_priorities;`   - Lista prioritatilor donate la Thread

`struct list priority_recipients;`  - Lista de thread-uri la care thread-ul curent a donat prioritate

`struct list_elem pri_elem;`        - Lista ce tine cont de prioritatile donate.

`struct list_elem recp_elem;`       - Lista ce tine cont de thread-ul curent cu o lista de recipienti.

> Structura de date folosita pentru urmarirea donatiilor de prioritati

Este formata dintr-o lista ordonata de thread-uri care au primit donatii de prioritati si niste liste aditionale pentru a sti ce thread primeste de la oricare thread o donatie sau mai multe de prioritati.

> Cum ne asiguram ca ordinea de prioritate a thread-urilor asteapta semaforul, o blocare sa o conditie pentru a se trezi?

Cand un thread incearca sa aplice la blocare, semafor sau trezire/adormire facem o comparatie a prioritatilor pentru a determina daca thread-ul nou are o prioritate mai mare decat un thread deja existent. In cazul in care are o prioritate mai mare decat thread-ul curent acesta este nevoit sa cedeze actiunea thread-ului prioritat adaugandu-se in lista de asteptare pana cand conditia asteptata devine disponibila.

> Secventa evenimentelor petrecute la call-ul `lock_acquire()` in cazul donari de prioritate?

Cand un thread nou incearca sa asigure lock-ul prima data se verifica daca lock-ul este folosit de alt thread, in cazul in care lock-ul nu este disponibil se verifica prioritatea iar daca prioritatea thread-ului nou este mai mare decat a thread-ului curent atunci thread-ul curent doneaza prioritatea thread-ului nou si intra in lista de asteptare.

> B5: Secventa de evenimente la apelarea `lock_release()` in cazul in care se cere un lock la care asteapta un proces cu o prioritate mai mare?

Cand un thread incearca sa elibereze un clock acesta verifica daca a primit o donatie de prioritate de la orice thread existent iar in cazul in care a primit o donatie de prioritate acesta o inlatura din lista de donatii, prioritati si din lista de recipienti. Thread-ul pe urma deblockeaza lock-ul pentru a putea fi preluat de alt thread.

## Sincronizarea

> Potentialele desincronizari la rularea `thread_set_priority()` si cum evitam aceste desincronizari?

O potentiala desincronizare in `thread_set_priority()` poate aparea atunci cand se incearca preluarea prioritatii donate de la inceputul listei. Desincronizarea se poate produce atunci cand incercam sa preluam accessul de la varful liste in timp ce se adauga/scoate o donatie de prioritati. Pentru a mitiga aceasta situatie facem inaccesibile intreruperile inainte de acesarea si modificarea listei de prioritati.

## Notite

Mentinem functionalitatea deja existenta a proiectului pintos iar toate listele sunt sortate dupa prioritate in vederea realizarii unei performante mai bune.

# Advanced Scheduler

Nu exista adaugari

# 2. User Programs

// de adaugat

# 3. Virtual Memory

# PAGING


### ALGORITM

> Vom descrie in cateva randuri codul folosit pentru localizarea fisierului cadru, daca exista unul, ce contine informatii despre o anume pagina
Fiecare structură de pagină are un număr de membri asociați, inclusiv structura cadru care conține datele sale fizice. Structura cadrului conține un pointer către adresa virtuală a nucleului care deține datele sale și o referință la pagina care o deține. Când pagina este creată inițial, cadrul său este setat la NULL - nu primește un cadru până nu este alocat unul prin intermediul funcției `frame_alloc_and_lock()` din `frame.c` (numită de funcția do_page_in ()).

Procesul de găsire a unui cadru liber în memorie este realizat de `frame_alloc_and_lock()`. Face mai multe încercări de a asigura o regiune liberă de memorie în care să aloce noul cadru. Dacă nu există nicio bucată de memorie de dimensiunea unui cadru, atunci un cadru existent trebuie evacuat pentru a face loc celui nou. După găsirea / crearea unui nou cadru, cadrul este returnat și asociat cu pagina care l-a solicitat `(p-> frame = frame și f-> page = page)`. Dacă dintr-un anumit motiv `frame_alloc_and_lock()` nu reușește să găsească un cadru existent de evacuat, NULL este returnat și nu este alocat niciun cadru.

### SINCRONIZARE

> Când două procese de utilizator au nevoie de un nou cadru în același timp, cum sunt evitate cursele?
Căutarea în tabelul de cadre (de obicei pentru a găsi un cadru liber) este limitată la un singur proces la un moment dat prin intermediul unui blocaj numit scan_lock. Nu există două procese care pot asigura același cadru simultan, iar condițiile de cursă sunt evitate. În plus, fiecare cadru individual conține propriul blocaj (f-> lock) care indică dacă este sau nu ocupat.

# PAGING CATRE SI DE PE DISC

### ALGORITM

> Când este necesar un cadru, dar niciunul nu este liber, trebuie să fie un cadru evacuat. Vom descrie algoritmul utilizat pentru a alege un cadru de evacuat.

Dacă toate cadrele sunt ocupate, trebuie să stabilim un cadru bun de evacuat. Ținând cont de cache și locatie, obiectivul nostru este de a evacua acele cadre care au fost accesate cel mai recent - algoritmul pentru a face acest lucru este implementat în `try_frame_alloc_and_lock()` în `„frame.c”`.

În cazul în care cadrul căutat nu are nici o pagină asociată cu acesta, dobândim imediat acel cadru. În caz contrar, obținem primul cadru care nu a fost accesat recent. Dacă toate cadrele au fost accesate recent, atunci vom repeta peste fiecare cadru. De data aceasta, este foarte probabil ca un cadru valid să fie achiziționat deoarece funcția `page_accessed_recently()` modifică starea de acces a unui cadru la apelare. Dacă din orice motiv a doua iterație nu produce cadre valide, atunci NULL este returnat și niciun cadru nu este evacuat.

> Vom explica modul in care am gandit algoritmul sa decida daca o eroare de pagina pentru o adresa virtuală nevalidă ar trebui să determine extinderea stivei în pagina care a dat greș.

Există două verificări importante care trebuie făcute înainte ca o pagină să fie alocată. În primul rând, adresa paginii trebuie să se afle în spațiul alocat stivei (care este implicit 1 MB). În al doilea rând, adresa paginii  trebuie să se afle în limita a 32 de octeți de firele `user_esp`. Facem acest lucru pentru a ține cont de comenzile care gestionează memoria stivei, inclusiv comenzile `PUSH` și `PUSHA` care vor accesa cel mult 32 de octeți dincolo de indicatorul stivei.

# MEMORY MAPPED FILES

### ALGORITM

> Descrieți modul în care fișierele mapate de memorie se integrează în subsistemul de memorie virtuală.Explicați cum defecțiunile paginii și procesele de evacuare diferă între paginile de schimb și alte pagini

Fișierele mapate de memorie sunt încapsulate într-o structură numită `mapare` situată în
, `syscall.c`. Fiecare mapare conține o referință la adresa sa din memorie și fișierul pe care
îl mapează. Fiecare fir conține o listă a tuturor fișierelor mapate la acel fir, care poate
fi utilizat pentru a gestiona fișierele care sunt prezente direct în memorie. În caz contrar,
paginile care conțin informații despre fișierele mapate de memorie sunt gestionate la fel ca 
orice altă pagină.


Procesul de defecțiune și evacuare a paginii diferă ușor pentru paginile aparținând fișierelor
mapate cu memorie. Paginile care nu au legătură cu fișierele sunt mutate într-o partiție swap
la evacuare, indiferent dacă pagina este murdară sau nu. Când sunt evacuate, paginile de fișier
cartografiate cu memorie trebuie redactate în fișier numai dacă sunt modificate. În caz contrar,
nu este necesară scrierea - partiția de swap este evitată împreună pentru fișierele mapate cu
memorie.


> Explicați cum determinați dacă o nouă mapare de fișiere se suprapune unui segment existent.


Pagini pentru o nouă mapare a fișierelor sunt alocate numai dacă se găsesc pagini libere și
neaccepate. Funcția `page_allocated()` are acces la toate mapările de fișiere și va refuza să
aloce orice spațiu care este deja ocupat. Dacă un nou fișier încearcă să încalce spațiul deja
mapat, acesta este imediat nemapat și procesul eșuează.

