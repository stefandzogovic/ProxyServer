#include "Header.h"


int idservisa = 1;				     //id servisa pocevsi od 1 
int proxytodriverthreadshutdown = 0; //globalni mutex za ProxyToDriverThread
int ddrthreadshutdown = 0;			 //globalni mutex za DDR

CRITICAL_SECTION cs; //Globalna promenljiva, kriticna sekcija

queue* red_proxy;
List* listclienthead;
List* listservicehead;


/*
	Kreaira novi prazan red i vraca pokazivac na njega.
*/
struct queue *queueCreate()
{

	struct queue *q;

	q = (queue*)malloc(sizeof(struct queue));

	q->head = q->tail = 0;

	return q;
}

/*
	Funkcija za kreiranje parova redova, za odgovarajuci servis. Inicijalno su sva polja postavljena na nule, ili
	NULL (respektivno).
*/
struct queuepair *queuePairCreate()
{

	struct queuepair *q;

	q = (queuepair*)malloc(sizeof(struct queuepair));

	q->idservisa = 0;
	q->shutdown = 0;
	q->queuerecvfromserv = NULL;
	q->queuesendtoserv = NULL;
	q->handle = 0;
	return q;
}

/* add a new value to back of queue */
void enq(struct queue *q, char *value)
{
	struct elt *e;

	e = (elt*)malloc(sizeof(struct elt));
	assert(e);

	e->value = value;

	/* Because I will be the tail, nobody is behind me */
	e->next = 0;

	if (q->head == 0) {
		/* If the queue was empty, I become the head */
		q->head = e;
	}
	else {
		/* Otherwise I get in line after the old tail */
		q->tail->next = e;
	}

	/* I become the new tail */
	q->tail = e;
}

//jednostavna provera da li je red prazan
int queueEmpty(const struct queue *q)
{
	return (q->head == 0 || q->head == NULL);
}

/* remove and return value from front of queue */
void deq(struct queue *q)
{
	struct elt *e;

	assert(!queueEmpty(q));

	/* patch out first element */
	e = q->head;
	q->head = q->head->next;

	free(e);

}
//isto kao deq samo sto se vraca pokazivac na vrednost dequeovanog elementa
char* deq2(struct queue *q, char *poruka)
{
	struct elt *e;

	assert(!queueEmpty(q));

	memcpy(poruka, q->head->value, DEFAULT_BUFLEN);
	//poruka = q->head->value;


	/* patch out first element */
	e = q->head;
	q->head = e->next;
	//char *temp_por = poruka;
	//temp_por = temp_por + (NUMBER_OF_SERVICES + 2) * 4;

	free(e);

	return poruka;
}

/* print contents of queue on a single line, head first */
void queuePrint(struct queue *q)
{
	struct elt *e;
	int brojac = 0;
	for (e = q->head; e != 0; e = e->next) {
		brojac++;
	}
	printf("%d", brojac);
	putchar('\n');
}

/* free a queue and all of its elements */
void queueDestroy(struct queue *q)
{
	while (!queueEmpty(q)) {
		deq(q);
	}

	free(q);
	q = NULL;
}

//unistava par redova i oslobadja memoriju
void queuepairDestroy(struct queuepair *q)
{
	free(q);
	q = NULL;
}

/*
	Funkcija kao parametre prima ID servera (ili klijenta), soket za komunikaciju, i pokazivac na listu podignutih servera, ili klijenata.
	Memorija se zauzima dinamicki za svaki novi elemenat liste upotrebom funkcije malloc,
	i redom se popunjavaju polja novog elementa liste, kao i povezivanje novog elementa sa starim (postavljanje na kraj),
	ili njegovo postavljanje na pocetak liste.
	Soket za komunikaciju se koristi da bi proksi znao kojem klijentu/serveru prosledjuje poruke.
*/
void ListAdd(int number, SOCKET s, DWORD id, HANDLE h, List **head)
{
	List* el;
	el = (List*)malloc(sizeof(List));
	el->num = number;
	el->s = s;
	el->threadID = id;
	el->clienth = h;
	el->next = NULL;
	el->ready = 0;
	el->ugasi = 0;
	el->brojizabranihservisa = 0;
	if (*head == NULL) {
		*head = el;
	}
	else {
		List *temp = *head;
		while (temp->next != NULL) {
			temp = temp->next;
		}
		temp->next = el;
	}
}
/*
	Funkcija vraca koliko postoji elemanata u odgovarajucoj listi (parametar pokazuje koja je lista u pitanju).
*/
int ListCount(List* head)
{
	List *temp = head;
	int ret = 0;
	while (temp) {
		ret++;
		temp = temp->next;
	}
	return ret;
}

/*
	Funkcija prima 2 parametra: ID klijenta ili servisa sa kojim se komunicira, kao i pokazivac na odgovarajucu listu
	(listu podignutih klijenata, ili servisa) kroz koju se iterira u cilju popunjavanja odgovarajucih polja liste.
	Funkcija se koristi za pristupanje elementima liste. U slucaju da elemenat nije pronadjen, vraca NULL; ukoliko jeste
	povratna vrednost je pokazivac na trazeni element.
*/
List* ListElementAt(int index, List *head)
{
	if (ListCount(head) > 0) {
		List *temp = head;

		while (temp->num != index) {
			temp = temp->next;
		}
		return temp;
	}
	return NULL;
}

/*
	Funkcija select omogucava pracenje stanja pisanja, citanja, i gresaka na jednom ili vise soketa.
	Parametri funkcije su soket koji se obradjuje i promenljiva read; u slucaju da se proverava mogucnost citanja
	ovaj parametar ce biti postavljen na true (operacije recv, recvfrom, accept, close);
	ukoliko je taj parametar false vrsi se provera mogucnosti pisanja (operacije send, sendto, connect).
	U while petlji funkcije vrsi se inicijalizacija seta, dodavanje desktriptora soketa u set, podesavanje
	maksimalnog dozvoljenog vremena koje ce funkcija select cekati da se neki od dogadjaja desi, i na kraju
	pozivanje implementirane funkcije select sa odgovarajucim parametrima, u zavisnosti da li se podaci salju ili primaju.
*/
void Select(SOCKET socket, bool read) {
	while (true) {
		// Initialize select parameters
		FD_SET set;
		timeval timeVal;
		FD_ZERO(&set);
		// Add socket we will wait to read from
		FD_SET(socket, &set);
		// Set timeouts to zero since we want select to return
		// instantaneously
		timeVal.tv_sec = 0;
		timeVal.tv_usec = 0;
		int iResult;

		if (read) {
			iResult = select(0 /* ignored */, &set, NULL, NULL, &timeVal);
		}
		else {
			iResult = select(0 /* ignored */, NULL, &set, NULL, &timeVal);
		}

		if (iResult < 0) {
			printf("Select failed");
		}
		else if (iResult == 0) {
			Sleep(100);
			continue;
		}

		return;
	}
}


//Nit namenjena za svaki pokrenuti servis, koja slusa na svom Socketu, prihvata poruku i prosledjuje je na
//drugi red (red namenjen da prihvata poruke od servisa i salje klijentu) u okviru para redova na drajveru klijenta
//kome je poruka namenjena.
DWORD WINAPI ServiceCommunicateThread(LPVOID lpParam) 
{
	int iResult = 0;
	int idserv = *((int*)lpParam);
	free(lpParam);
	char *buffer = (char*)calloc(DEFAULT_BUFLEN, sizeof(char));
	int *pok = (int*)buffer;
	List *servis = ListElementAt(idserv, listservicehead);

	while (true)
	{ 
		if (servis->ugasi == 0)
		{
			if (servis->ready == 1)
			{
				SOCKET socket = servis->s;
				Select(socket, true);

				iResult = recv(socket, buffer, DEFAULT_BUFLEN, 0);
				if (iResult > 0)
				{
					int idklijenta = (int)(ntohl(*pok));

					List *client = ListElementAt(idklijenta, listclienthead);
					if (client->ready == 1)
					{
						for (int j = 0; j < client->brojizabranihservisa; j++)
						{
							if (client->drajver->pokazivaci[j]->idservisa == servis->num)
							{
								EnterCriticalSection(&cs);
								enq(client->drajver->pokazivaci[j]->queuerecvfromserv, buffer);
								LeaveCriticalSection(&cs);
								break;
							}
						}
					}

				}
			}
		}
		else
			break;
	}

	free(buffer);
	return 0;
}

/*
	Nit koja sluzi da klijent izabere servise sa kojima zeli da komunicira odnosno razmenjuje poruke.
	Parametar koji se prosledjuje jeste ID klijenta. U momentu kada klijent vrsi odabir
	servisa, vrsi se i instanciranje odgovarajucih drajvera i redova (skladista podataka za servise).
	Poruke koje se razmenjuju se sadrze od 4 bajta; poruka koju proksi salje klijentu na prvom mestu sadrzi
	klijentov ID, a na ostalim mestima ID-eve dostupnih servisa za razmenu poruka, dok poruka koju salje klijent
	na prva 3 mesta takodje sadrzi servisne ID-eve (sa kojima klijent zeli da komunicira), ali se na poslednjem mestu
	nalaze nule (predstavljaju NULL terminator). Primer: ukoliko je klijent	odabrao 2 servisa, samo ce prva 2 bajta
	biti popunjena, ostala ce biti nula); u slucaju da je izabrao sva 3 servisa za komunikaciju, prva 3 mesta sadrze
	odgovarajuce ID-eve, a na poslednjem mestu su nule, koje oznacavaju kraj poruke (kao '\0' - koji pokazuje
	kraj stringa).

*/
DWORD WINAPI ClientChooseServicesThread(LPVOID lpParam) {

	printf("connected");

	int idclienta = *((int*)lpParam);
	free(lpParam);   // oslobodimo pokazivac
	char *buffer = (char*)calloc((NUMBER_OF_SERVICES + 1) * 4, sizeof(char));

	if (ListElementAt(idclienta, listclienthead)->ugasi == 0)
	{

		drajver *drajver = (struct drajver*)malloc(sizeof(struct drajver));
		ListElementAt(idclienta, listclienthead)->drajver = drajver;

		int iResult = 0;


		EnterCriticalSection(&cs);
		SOCKET acceptedSocket = ListElementAt(idclienta, listclienthead)->s;
		LeaveCriticalSection(&cs);

		Select(acceptedSocket, false);

		List* temphead = listservicehead;
		int id_servisa[NUMBER_OF_SERVICES + 1]; //calloc
		id_servisa[0] = idclienta;

		for (int i = 1; i <= ListCount(listservicehead); i++)
		{
			id_servisa[i] = temphead->num;
			temphead = temphead->next;
		}


		iResult = send(acceptedSocket, (char*)id_servisa, sizeof(id_servisa), 0);
		//free calloc
		Select(acceptedSocket, true);

		while (true)
		{
			iResult = recv(acceptedSocket, buffer, (NUMBER_OF_SERVICES + 1) * 4, 0); // + 1 zbog nule 
			if (iResult > 0)
			{
				int brojac = 0;
				printf("\nIzabrani servisi klijenta sa ID: %d\n-------------------------\n", idclienta);
				int *temp = (int*)buffer;
				while (true)
				{
					int idserv = (int)(ntohl(*temp));

					if (idserv == 0)
					{
						break;
					}

					HANDLE handle1;
					DWORD dword1;
					queuepair *drajverqueuepair = queuePairCreate();
					drajverqueuepair->idservisa = idserv;
					queue *drajverqueue1 = queueCreate();
					queue *drajverqueue2 = queueCreate();
					drajverqueuepair->queuerecvfromserv = drajverqueue2;
					drajverqueuepair->queuesendtoserv = drajverqueue1;
					ListElementAt(idclienta, listclienthead)->drajver->pokazivaci[brojac] = drajverqueuepair;
					handle1 = CreateThread(NULL, 0, &DriverQueuePairSendThread, drajverqueuepair, 0, &dword1);
					ListElementAt(idserv, listservicehead)->ready = 1;
					ListElementAt(idclienta, listclienthead)->drajver->pokazivaci[brojac]->handle = handle1;
					//handle2 = CreateThread(NULL, 0, &DriverQueuePairRecvThread, drajverqueuepair, 0, &dword2);
					temp = temp + 1;

					printf("Server ID: %d\n-------------------------\n", idserv);
					brojac++;
				}
				EnterCriticalSection(&cs);
				ListElementAt(idclienta, listclienthead)->brojizabranihservisa = brojac;
				ListElementAt(idclienta, listclienthead)->ready = 1;
				LeaveCriticalSection(&cs);
				break;
			}
		}
	}
	free(buffer);
	return 0;
}

/*
	Nit koja sluzi za primanje poruka od strane klijenta. Parametar ready oznacava da je klijent spreman,
	odnosno da je izabrao servise sa kojima zeli da komunicira. Poruke koje proksi primi skladiste se u
	redu ""red_proxy".
*/
DWORD WINAPI ClientCommunicateThread(LPVOID lpParam)
{
	int idclienta = *((int*)lpParam);
	int iResult = 0;
	char *buffer = (char*)calloc(DEFAULT_BUFLEN, sizeof(char));
	List *klijent = ListElementAt(idclienta, listclienthead);
	SOCKET socket = klijent->s;
	free(lpParam);
	while (true)
	{
		if (klijent->ugasi == 0)
		{
			if (klijent->ready == 1)
			{
				Select(socket, true);

				iResult = recv(socket, buffer, DEFAULT_BUFLEN, 0);
				if (iResult > 0)
				{
					EnterCriticalSection(&cs);
					printf("enqueued message from client %d\n", klijent->num);
					enq(red_proxy, buffer);
					LeaveCriticalSection(&cs);
				}
				else if (iResult == -1)
				{
					printf("iskljucio se klijent");
					//ClientShutdown(klijent);
					klijent->ready = 0;
					break;
				}
			}
		}
		else
		{
			break;
		}
	}

	free(buffer);

	return 0;
}


/*
	Ova nit sluzi da preuzme poruke koje se nalaze na klijentovom (red_proxy) redu.
	U zavisnosti od toga koliko je klijent izabrao servisa za komunikaciju, ova nit ce skidati toliko "kopija"
	te poruke, i stavljati ih u drajvere, konkretno u poseban red koji sluzi za primanje klijentovih poruka.
	Simultano se poruka brise sa klijentskog reda. Klijentska poruka ispisuje se na konzolu.
*/
DWORD WINAPI ProxyToDriverThread(LPVOID lpParam)
{
	int *poruka_temp;
	int brojac = 0;
	int id = 0;
	char *poruka = (char*)calloc(DEFAULT_BUFLEN, sizeof(char));
	int timeout = 0;
	while (true)
	{
		if (proxytodriverthreadshutdown == 0)
		{
			if (!queueEmpty(red_proxy))
			{
				timeout = 0;

				EnterCriticalSection(&cs);
				poruka = deq2(red_proxy, poruka);
				LeaveCriticalSection(&cs);
				poruka_temp = (int*)poruka;
				id = (int)(ntohl(*poruka_temp));
				printf("dequeued message from client ID: %d\n--------------------\n", id);
				while (brojac < NUMBER_OF_SERVICES) {
					poruka_temp = poruka_temp + 1;
					int temp = (int)(ntohl(*poruka_temp));
					if (temp == 0)
					{
						break;
					}

					brojac++;
				}
				for (int i = 0; i < brojac; i++)
				{
					EnterCriticalSection(&cs);
					enq(ListElementAt(id, listclienthead)->drajver->pokazivaci[i]->queuesendtoserv, poruka);
					LeaveCriticalSection(&cs);
				}
				brojac = 0;
			}
		}
		else
		{
			break;
		}
	}
	free(poruka);
	return 0;
}


/*
	Nit koja sluzi za slanje poruke od proksija ka odgovarajucem servisu.
*/
DWORD WINAPI DriverQueuePairSendThread(LPVOID lpParam)
{
	char *poruka = (char*)calloc(DEFAULT_BUFLEN, sizeof(char));

	queuepair *red = (queuepair*)(lpParam);
	SOCKET socket = ListElementAt(red->idservisa, listservicehead)->s;

	int iResult = 0;
	while (true)
	{
		if (red->shutdown == 0)
		{
			if (!queueEmpty(red->queuesendtoserv))
			{
				EnterCriticalSection(&cs);
				poruka = deq2(red->queuesendtoserv, poruka);
				LeaveCriticalSection(&cs);

				iResult = send(socket, poruka, DEFAULT_BUFLEN, 0);
			}
		}
		else
			break;
	}
	free(poruka);
	return 0;
}


/* Deficit Round Robin nit.
   Skida poruke sa svakog reda u nizu. Ukoliko postoji poruka koja je veca od kvantuma(broj maksimalne
   velicine poruke koju nit sme da skine sa reda), kvantum ce se povecati u sledecem krugu da bi mogao tu poruku da skine,
   ali ce u ovom krugu preskociti red.
   Ukoliko postoji vise poruka cija ukupna velicina u bajtima nije veca od kvantuma, nit treba da ih skine sve sa reda.*/
DWORD WINAPI DRRThread(LPVOID lpParam)
{
	int quantum = 10;
	int povecaj = 0;
	char *poruka = (char*)calloc(DEFAULT_BUFLEN, sizeof(char));
	while (true)
	{
		if (ddrthreadshutdown == 0)
		{
			povecaj = 0;

			for (int i = 0; i < ListCount(listclienthead); i++)
			{
				List* client = ListElementAt(i, listclienthead);
				if (client->ready == 1)
				{
					for (int j = 0; j < client->brojizabranihservisa; j++)
					{
						int temp = quantum;
						while (!queueEmpty(client->drajver->pokazivaci[j]->queuerecvfromserv))
						{
							if (strlen(client->drajver->pokazivaci[j]->queuerecvfromserv->head->value + 20) > quantum)
							{
								povecaj = 1;
								break;
							}
							else if (strlen(client->drajver->pokazivaci[j]->queuerecvfromserv->head->value + 20) > temp)
							{
								break;
							}
							else
							{
								temp = temp - strlen(client->drajver->pokazivaci[j]->queuerecvfromserv->head->value + 4);
								EnterCriticalSection(&cs);
								poruka = deq2(client->drajver->pokazivaci[j]->queuerecvfromserv, poruka);
								LeaveCriticalSection(&cs);
								printf("dequeued from driver whose client id is: %d\n", client->num);
								Select(client->s, false);
								send(client->s, poruka, DEFAULT_BUFLEN, 0);

							}
						}
					}
				}
			}
			if (povecaj == 1)
			{
				quantum += 10;
			}
		}
		else
			break;
	}
	/*while (true)
	{
		for (int i = 0; i < ListCount(listclienthead); i++)
		{
			List* client = ListElementAt(i, listclienthead);
			if (client->ready == 1)
			{
				Select(client->s, false);
				send(client->s, "jasdiosa", 4, 0);
			}
		}
	}*/
	free(poruka);
	return 0;
}

//Funkcija koja sluzi za gasenje klijenta i delociranje memorije
void ClientShutdown(List* client)
{
	client->threadID = 0;
	for (int i = 0; i < client->brojizabranihservisa; i++)
	{


		queueDestroy(client->drajver->pokazivaci[i]->queuerecvfromserv);

		queueDestroy(client->drajver->pokazivaci[i]->queuesendtoserv);

		queuepairDestroy(client->drajver->pokazivaci[i]);
	}

	free(client->drajver);
	client->drajver = NULL;
	free(client);
}


/*
	Funkcija za inicijalizaciju WinSock2 biblioteke, vraca true ukoliko je uspesno inicijalizovana, false u suprotnom.
*/
bool InitializeWindowsSockets()
{
	WSADATA wsaData;
	// Initialize windows sockets library for this process
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		printf("WSAStartup failed with error: %d\n", WSAGetLastError());
		return false;
	}
	return true;
}
