# proiect_pintos

## Proiect PINTOS la SOA

###### Studenti

- Raluca Popa
- Filip Popa
- Ovidiu Miches
- Florin Suciu

## Threads

   - Alarm Clock
   - Priority Scheduling
   - Advanced Scheduler

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