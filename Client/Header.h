#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#pragma warning(disable:4996) 
#pragma comment(lib, "Ws2_32.lib")


#define DEFAULT_BUFLEN 150
#define DEFAULT_PORT 27016
#define NUMBER_OF_SERVICES 3


char* Menu(SOCKET connectsocket, char *buffer);
void Connect(SOCKET, char *serviceIds);
DWORD WINAPI ClientSendData(LPVOID lpParam);
bool InitializeWindowsSockets();