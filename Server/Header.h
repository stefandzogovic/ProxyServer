
#include <ws2tcpip.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#define NUMBER_OF_SERVICES 3
#define SERVER_SLEEP_TIME 50
#define DEFAULT_BUFLEN 150
#define MAX_SIZE 100
#define NUMBER_OF_SERVICES 3
#define DEFAULT_PORT "27016"
#define DEFAULT_PORT_SERVICE 27017
#pragma warning(disable:4996) 
#pragma comment(lib, "Ws2_32.lib")


// Jedan element u redu, sa pokazivacima na naredni/prethodni element, kao i vrednoscu same poruke.
typedef struct elt {
	struct elt *next;
	char* value;
}elt;

typedef struct queue {
	struct elt *head;  /* dequeue this next */
	struct elt *tail;  /* enqueue after this */
}queue;

/*
	Struktura tipa red. Sadrzi polja ID servisa ciji su redovi, i 2 reda, jedan za prijem, a drugi za slanje
	tj. skidanje elemenata sa reda.
*/
typedef struct queuepair
{
	int idservisa;
	HANDLE handle;
	int shutdown;
	struct queue *queuesendtoserv;
	struct queue *queuerecvfromserv;
}queuepair;

/*
	Struktura tipa liste, koja sadrzi sledeca polja :
		num - predstavlja jedinstveni ID, servera ili klijenta, u zavisnosti koja je lista u pitanju;
		s - soket za komunikaciju;
		threadID - ID niti koja salje ili prima podatke, takodje u zavisnosti od svrhe liste;
		ready - ovaj parametar obavestava proksi da li je klijent izabrao servise za komunikaciju
				(da li je spreman za dalju komunikaciju).Koristi se samo kod klijentske liste;
		brojizabranihservisa - broj servisa koje je klijent izabrao za komunikaciju, takodje se koristi samo za
								klijentske	liste;
		*drajver - pokazivac na strukturu Drajver, sadrzi pokazivace na odgovarajuce parove redova koji sluze kao
					skladiste podataka servisa.Koristi se samo u serverskoj listi;
		*next - pokazivac na naredni elemenat u listi.
*/
typedef struct list {
	int num;
	SOCKET s;
	DWORD threadID;
	HANDLE clienth;
	int ugasi = 0;
	int ready;
	int brojizabranihservisa;
	struct drajver *drajver;
	struct list * next;
} List;

/*
	Struktura drajver sadrzi pokazivace na parove redova; instanci drajvera postoji onoliko koliko je klijenta podignuto,
	dok se parovi redova instanciraju samo za one servise sa kojim klijent(i) odluce da komuniciraju.
	Primer: ukoliko jedan klijent zeli da komunicira sa sva 3 servisa, napravice se instanca drajvera sa 3 para redova;
	i slucaju da se podigne jos jedan klijent, i on izabere 2 servisa za komunikaciju, instancirace se jos jedan drajver
	sa dva (nova) para redova.
*/
typedef struct drajver
{
	struct queuepair *pokazivaci[NUMBER_OF_SERVICES];
}Drajver;


void Select(SOCKET socket, bool read);
List* ListElementAt(int index, List *head);
void ListAdd(int number, SOCKET s, DWORD id, HANDLE h, List **head);
int ListCount(List* head);
int queueEmpty(const struct queue *q);
void queuePrint(struct queue *q);
struct queue *queueCreate();
struct queuepair *queuePairCreate();
void enq(struct queue *q, char* value);
void queueDestroy(struct queue *q);
void deq(struct queue *q);
char* deq2(struct queue *q, char *poruka);
DWORD WINAPI DriverQueuePairSendThread(LPVOID lpParam);
DWORD WINAPI ClientChooseServicesThread(LPVOID lpParam);
DWORD WINAPI ClientCommunicateThread(LPVOID lpParam);
DWORD WINAPI ServiceCommunicateThread(LPVOID lpParam);
DWORD WINAPI ProxyToDriverThread(LPVOID lpParam);
DWORD WINAPI DRRThread(LPVOID lpParam);
void ClientShutdown(List* client);
bool InitializeWindowsSockets();
