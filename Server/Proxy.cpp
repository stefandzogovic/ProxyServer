#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "Header.h"


bool InitializeWindowsSockets();


int main()
{
	// Socket used for listening for new clients 
	SOCKET listenSocket = INVALID_SOCKET;
	// Socket used for communication with client
	SOCKET acceptedSocket = INVALID_SOCKET;

	SOCKET connectSocket = INVALID_SOCKET;

	extern List* listclienthead;
	extern List* listservicehead;
	extern CRITICAL_SECTION cs;
	extern queue* red_proxy;
	extern int ddrthreadshutdown;
	extern int proxytodriverthreadshutdown;
	extern int idservisa;
	// variable used to store function return value
	int iResult;
	// Buffer used for storing incoming data
 
	red_proxy = queueCreate();
	char *idservisasend = (char*)malloc(4);

	int id = 0;

	InitializeCriticalSection(&cs);

	if (InitializeWindowsSockets() == false)
	{
		// we won't log anything since it will be logged
		// by InitializeWindowsSockets() function
		return 1;
	}

	connectSocket = socket(AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP);

	if (connectSocket == INVALID_SOCKET)
	{
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	// Prepare address information structures
	addrinfo *resultingAddress = NULL;
	addrinfo hints;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;       // IPv4 address
	hints.ai_socktype = SOCK_STREAM; // Provide reliable data streaming
	hints.ai_protocol = IPPROTO_TCP; // Use TCP protocol
	hints.ai_flags = AI_PASSIVE;     // 

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &resultingAddress);
	if (iResult != 0)
	{
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	listenSocket = socket(AF_INET,      // IPv4 address famly
		SOCK_STREAM,  // stream socket
		IPPROTO_TCP); // TCP

	if (listenSocket == INVALID_SOCKET)
	{
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(resultingAddress);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket - bind port number and local address 
	// to socket
	iResult = bind(listenSocket, resultingAddress->ai_addr, (int)resultingAddress->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(resultingAddress);
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}

	// Since we don't need resultingAddress any more, free it
	freeaddrinfo(resultingAddress);

	// Set listenSocket in listening mode
	iResult = listen(listenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR)
	{
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}

	unsigned long int nonBlockingMode = 1;
	iResult = ioctlsocket(listenSocket, FIONBIO, &nonBlockingMode);

	if (iResult == SOCKET_ERROR)
	{
		printf("ioctlsocket failed with error: %ld\n", WSAGetLastError());
		return 1;
	}

	printf("Proxy initialized, waiting for %d services.\n", NUMBER_OF_SERVICES);


	do
	{
		// Wait for clients and accept client connections.
		// Returning value is acceptedSocket used for further
		// Client<->Server communication. This version of
		// server will handle only one client.

		// set whole buffer to zero

		Select(listenSocket, true);

		acceptedSocket = accept(listenSocket, NULL, NULL);



		if (acceptedSocket == INVALID_SOCKET)
		{
			continue;
		}
		else
		{
			HANDLE hThread2;
			DWORD dw2;
			int *param = (int*)malloc(sizeof(int));
			Select(acceptedSocket, false);
			int idservisatemp = htonl(idservisa);
			idservisatemp = htonl(idservisa);
			memcpy(idservisasend, &(idservisatemp), 4);
			iResult = send(acceptedSocket, idservisasend, 4, 0);
			*param = idservisa;
			hThread2 = CreateThread(NULL, 0, &ServiceCommunicateThread, param, 0, &dw2);
			ListAdd(idservisa, acceptedSocket, dw2, hThread2, &listservicehead);

			idservisa++;
		}
		// here is where server shutdown logic could be placed

	} while (idservisa < NUMBER_OF_SERVICES + 1);


	printf("Connectovano je %d servisa.", NUMBER_OF_SERVICES);

	DWORD dw1;
	DWORD dw3;
	HANDLE hThread1;
	HANDLE hThread3;
	hThread1 = CreateThread(NULL, 0, &ProxyToDriverThread, NULL, 0, &dw1);
	hThread3 = CreateThread(NULL, 0, &DRRThread, NULL, 0, &dw3);


	while(true)
	{

		// Wait for clients and accept client connections.
		// Returning value is acceptedSocket used for further
		// Client<->Server communication. This version of
		// server will handle only one client.

		// set whole buffer to zero

		Select(listenSocket, true);

		acceptedSocket = accept(listenSocket, NULL, NULL);

		if (id == 10)
		{
			Sleep(5000);
			break;
		}

		if (acceptedSocket == INVALID_SOCKET)
		{
			continue;
		}
		else
		{
			unsigned long int nonBlockingMode = 1;
			iResult = ioctlsocket(acceptedSocket, FIONBIO, &nonBlockingMode);
			DWORD dw, dw1;
			HANDLE hThread, hThread1;
			int *param = (int*)malloc(sizeof(int));
			int *param1 = (int*)malloc(sizeof(int));
			*param = id;
			*param1 = id;
			hThread = CreateThread(NULL, 0, &ClientChooseServicesThread, param, 0, &dw);
			hThread1 = CreateThread(NULL, 0, &ClientCommunicateThread, param1, 0, &dw1);
			ListAdd(id, acceptedSocket, dw1, hThread1, &listclienthead);
			id++;
		}

		// here is where server shutdown loguc could be placed

	}

	// shutdown the connection since we're done

	//LOGIKA ZA SHUTDOWN
#pragma region LOGIKAZASHUTDOWN

	free(idservisasend);
	ddrthreadshutdown = 1;
	proxytodriverthreadshutdown = 1;
	WaitForSingleObject(hThread1, INFINITE);

	CloseHandle(hThread1);
	CloseHandle(hThread3);
	
	for (int i = 1; i < ListCount(listservicehead); i++)
	{
		List *servis = (ListElementAt(i, listservicehead));
		shutdown(servis->s, 2);
		servis->ready = 0;
		closesocket(servis->s);
		servis->ugasi = 1;
		CloseHandle(servis->clienth);
	}

	int cnt = ListCount(listclienthead);
	for (int i = 0; i < cnt; i++)
	{
		List *klijent = (ListElementAt(i, listclienthead));
		shutdown(klijent->s, 2);
		closesocket(klijent->s);
		WaitForSingleObject(klijent->clienth, INFINITE);
		CloseHandle(klijent->clienth);
		klijent->ugasi = 1;
		for (int j = 0; j < klijent->brojizabranihservisa; j++)
		{
			klijent->drajver->pokazivaci[j]->shutdown = 1;
			WaitForSingleObject(klijent->drajver->pokazivaci[j]->handle, INFINITE);
			CloseHandle(klijent->drajver->pokazivaci[j]->handle);
		}
	}

	List *tempclienthead = listclienthead;
	List *tempclienthead2 = listclienthead->next;

	for (int i = 0; i < cnt; i++)
	{
		ClientShutdown(ListElementAt(i, tempclienthead));
		tempclienthead = tempclienthead2;
		if (tempclienthead == NULL)
		{
			break;
		}
		tempclienthead2 = tempclienthead2->next;

	}

	List *tempservicehead = listservicehead;
	List *tempservicehead2 = listservicehead->next;

	for (int i = 1; i <= NUMBER_OF_SERVICES; i++)
	{
		free(ListElementAt(i, tempservicehead));
		tempservicehead = tempservicehead2;
		if (tempservicehead == NULL)
		{
			break;
		}
		tempservicehead2 = tempservicehead2->next;

	}

	queueDestroy(red_proxy);
	
	iResult = shutdown(acceptedSocket, SD_SEND);

	// cleanup
	closesocket(listenSocket);
	closesocket(acceptedSocket);
	WSACleanup();
	DeleteCriticalSection(&cs);
#pragma endregion LOGIKAZASHUTDOWN
	int i = 0;
	scanf("%d", &i);

	return 0;
}

