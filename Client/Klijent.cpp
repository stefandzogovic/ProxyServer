#include "Header.h"

int brojservisa = 0;
int izabranbrojservisa = 0;
int idClienta = 0;
char *izabraniservisizaconnect = NULL;

SOCKET connectSocket = INVALID_SOCKET;
char buffer[DEFAULT_BUFLEN];


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

/*
	Funkcija pomocu koje klijent odabira servise sa kojim ce komunicirati. Poruka buffer sadrzi 4 bajta, i sastoji se od:
	na prvom mestu ID klijenta, a ostala 3 bajta su ID servisa sa kojima klijent moze komunicirati. Ukoliko su podignuta
	3 servisa, svi bajti do kraja ce biti popunjeni; u slucaju da su podignuta samo 2 servisa, posle ID drugog bice nule
	(kao NULL terminator kod stringova). Pomeranjem (iteriranjem kroz poruku, klijent vidi koji su mu servisi dostupni).
	Kada klijent izabere servise od interesa, poruka se vraca nazad proksiju, u odgovarajucem izgledu.
*/
char* Menu(SOCKET connectsocket, char *buffer)
{
	char *x = buffer;
	char *izabraniservisi = (char*)calloc(16, sizeof(char)); // 20 bajta 
	idClienta = htonl((int)*x);
	while (true)
	{
		x = x + 4;
		int p = (int)(*x);
		if (p == 0)
		{
			break;
		}
		printf("Servis ID: %d\n", *x);
		brojservisa++;
	}
	while (true)
	{
		printf("Izaberi broj servisa na koji zelis da se konektujes. (1 - %d)\n", brojservisa);
		scanf("%d", &izabranbrojservisa);
		if (izabranbrojservisa <= 0 || izabranbrojservisa > brojservisa)
		{
			continue;
		}
		else
		{
			int temp = 0;
			while (temp < izabranbrojservisa)
			{
				printf("Izaberi servis(e) po ID-u. Za odabir vise servisa staviti razmak izmedju.\n");

				int id = 0;
				scanf("%d", &id);
				id = htonl(id);
				memcpy(izabraniservisi + temp * 4, &(id), sizeof(id));
				temp++;
			}
			return izabraniservisi;
		}
	}
}

/*
	Servise koje je u prethodnoj funkciji klijent odabrao, ova funkcija prosledjuje nazad proksiju.
	Parametri su soket, i poruka u kojoj se nalaze izabrani servisi. Poruka se sastoji od 4 bajta:
	prva 3 su ID servisa koje je klijent izabrao (ako je, na primer, izabrao 2 servisa, prva 2 bajta bice
	popunjena, ostala ce biti nule).
*/
void Connect(SOCKET connectSocket, char *izabraniservisi)
{
	int iResult = 0;
	iResult = send(connectSocket, izabraniservisi, brojservisa * 4, 0); //(brojservisa + 1) * 4

	if (iResult == SOCKET_ERROR)
	{
		int i = 0;
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(connectSocket);
		WSACleanup();
		scanf("%d", &i);
		return;
	}
	Sleep(1000);
}

/*
Funkcija koja sluzi za komunikaciju sa serverom(proxy-jem). Posalje poruku i ceka na recv.
*/
DWORD WINAPI ClientSendData(LPVOID lpParam)
{
	int brojac = 0;
	int iResult = 0;
	while (true)
	{
		brojac = 0;
		int brojbajta = rand() % (DEFAULT_BUFLEN + 1 - 100) + 100;
		char *tempbuffer = (char*)calloc(brojbajta, sizeof(char));
		memcpy(tempbuffer, &(idClienta), 4);
		memcpy(tempbuffer + 4, izabraniservisizaconnect, 16);
		memcpy(tempbuffer + 4 + 16, buffer + 4 + 16, brojbajta - 20);
		iResult = send(connectSocket, tempbuffer, DEFAULT_BUFLEN, 0);

		if (iResult == SOCKET_ERROR)
		{
			printf("send failed with error: %d\n", WSAGetLastError());
		}

		printf("Bytes Sent: %ld\n", iResult);
		while (true)
		{
			iResult = recv(connectSocket, buffer, DEFAULT_BUFLEN, 0);
			if (iResult > 0)
			{
				printf("Poruka primljena");
				break;
			}
			else
			{
				brojac++;
				if (brojac == 10000)
					break;
			}
		}

		free(tempbuffer);
		Sleep(1500);
	}
}