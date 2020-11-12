#include "Header.h"

// Initializes WinSock2 library
// Returns true if succeeded, false otherwise.


int __cdecl main(int argc, char **argv)
{
	// socket used to communicate with server
	SOCKET connectSocket = INVALID_SOCKET;
	// variable used to store function return value
	int iResult;
	int idServisa = 0;
	// message to send
	char buffer[DEFAULT_BUFLEN];

	char *izabraniservisi = (char*)calloc(16, sizeof(char));


	int i;

	if (InitializeWindowsSockets() == false)
	{
		// we won't log anything since it will be logged
		// by InitializeWindowsSockets() function
		return 1;
	}
	// create a socket
	connectSocket = socket(AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP);

	if (connectSocket == INVALID_SOCKET)
	{
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	// create and initialize address structure
	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
	serverAddress.sin_port = htons(DEFAULT_PORT);
	// connect to server specified in serverAddress and socket connectSocket
	if (connect(connectSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
	{
		printf("Unable to connect to server.\n");
		closesocket(connectSocket);
		WSACleanup();
	}

	char *idservisarecv = (char*)malloc(4);
	iResult = recv(connectSocket, idservisarecv, 4, 0);
	int *idserv = (int*)idservisarecv;
	idServisa = (int)(ntohl(*idserv));

	printf("Service ID: %d\n", idServisa);

	free(idservisarecv);


	// Send an prepared message with null terminator included

	while (true)
	{

		memset(buffer, 0, DEFAULT_BUFLEN);
		iResult = recv(connectSocket, buffer, DEFAULT_BUFLEN, 0);
		char *x = buffer;
		int p = 0;
		izabraniservisi = buffer;
		int idClienta = ((int)*x);
		p = htonl(idServisa);
		if (iResult != SOCKET_ERROR)
		{
			ispisporuke(buffer); 
			iResult = send(connectSocket, buffer, DEFAULT_BUFLEN, 0);
		}
	}

	// cleanup
	closesocket(connectSocket);
	WSACleanup();

	scanf("%d", &i);
	return 0;
}
