#include "Header.h"

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

void ispisporuke(char *poruka)
{
	int *poruka_temp;
	poruka_temp = (int*)poruka;
	int idclienta = (int)(ntohl(*poruka_temp));
	printf("Client with ID: %d\n", idclienta);
	poruka_temp = poruka_temp + NUMBER_OF_SERVICES + 2;
	printf("Sent: %s\n--------------\n", (char*)poruka_temp);
}