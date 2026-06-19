#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN


#include <iostream>
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
using namespace std;

#pragma comment(lib, "WS2_32.lib")

#define MTU		1500
#define MAX_CONNECTIONS		3

SOCKET clients[MAX_CONNECTIONS] = {};
DWORD dwThreadIDs[MAX_CONNECTIONS] = {};
HANDLE hThreads[MAX_CONNECTIONS] = {};
INT g_ActiveClients = 0;

VOID ClientHandle(SOCKET client_socket);

void main()
{
	setlocale(LC_ALL, "");
	cout << "SERVER" << endl;

	INT iResult = 0;
	//1) Init WinSOCK:
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	//2) Параметры подключени:
	addrinfo hints;
	addrinfo* binder;		//параметры, с которыми запустится Сервер
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	iResult = getaddrinfo(NULL, "27015", &hints, &binder); //NULL означает 0.0.0.0, т.е. сокет булет прослушивать все IP-адреса
	if (iResult != 0)
	{
		cout << "getaddrinfo() failed with error: " << iResult << endl;
		WSACleanup();
		return;
	}

	//3) Создаем сокет, который будет прослушивать канал (LISTENING) и принимать подключения от клиентов
	SOCKET listen_socket = socket(binder->ai_family, binder->ai_socktype, binder->ai_protocol);
	if (listen_socket == INVALID_SOCKET)
	{
		cout << "SOCKET creation failed with error: " << WSAGetLastError() << endl;
		freeaddrinfo(binder);
		WSACleanup();
		return;
	}

	//4) Bind SOCKET - Привязываем сокет к IP-адреса и портам, которые он будет слушать
	iResult = bind(listen_socket, binder->ai_addr, binder->ai_addrlen);
	freeaddrinfo(binder);
	if (iResult == SOCKET_ERROR)
	{
		cout << "Bind failed with error:" << WSAGetLastError() << endl;
		closesocket(listen_socket);
		WSACleanup();
		return;
	}

	//5) Запускаеи прослушивание:
	if (listen(listen_socket, MAX_CONNECTIONS) == SOCKET_ERROR) //1 - максимальное количество подключений
	{
		cout << "Listen failed with erroe: " << WSAGetLastError() << endl;
		closesocket(listen_socket);
		WSACleanup();
		return;
	}

	do
	{
		//6) Accept connection - Обработка входящих соединений
		SOCKADDR_IN client_addr;
		INT client_addrlen = sizeof(client_addr);
		SOCKET client_socket = accept(listen_socket, (SOCKADDR*)&client_addr, &client_addrlen);		//ожидает запрос от клиента
		if (client_socket == INVALID_SOCKET) cout << "Accept failed with error: " << WSAGetLastError() << endl;
		else cout << "CONNECTED ON " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << endl;

		if (g_ActiveClients < MAX_CONNECTIONS)
		{
			//7,8) Получение и отправка данных, Разрыв соединения:
			//ClientHandle(client_socket);
			clients[g_ActiveClients] = client_socket;
			hThreads[g_ActiveClients] = CreateThread
			(
				NULL,			//Атрибуты безопасности
				NULL,			//Stack size. Если 0, то все потоки будут использовать стек своего родительского процесса
				(LPTHREAD_START_ROUTINE)ClientHandle,	//Указатель на функцию, которая будет выполняться в потоке
				(LPVOID)client_socket,	//Параметр (lParam), передаваемый в функцию. Функция, запускаемая в потоке может принимать не более одного параметра.
				//Если функция, запускаемая в потоке, не принимает параметров, то на это место передается NULL.
				NULL,			//Флаги создания потока
				&dwThreadIDs[g_ActiveClients]
			);
			g_ActiveClients++;
		}
		else
		{
			CHAR szDeclineMessage[] = "Подключение невозможно, поскольку все места заняты, попробуйте позже.";
			iResult = send(client_socket, szDeclineMessage, strlen(szDeclineMessage), 0);
			if (iResult == SOCKET_ERROR) cout << "send Error: " << WSAGetLastError() << endl;
			iResult = shutdown(client_socket, SD_BOTH);
			if (iResult == SOCKET_ERROR) cout << "shutdown Error: " << WSAGetLastError() << endl;
		}
	} while (true);

	//9) Release resourses:
	closesocket(listen_socket);
	WSACleanup();
}

INT GetClientIndex(DWORD dwThreadID)			//Находим индекс текущего клиента
{
	for (int i = 0; i < g_ActiveClients; i++)
	{
		if (dwThreadIDs[i] == dwThreadID) return i;
	}
}
VOID Shift(INT index)
{
	for (int i = index; i < g_ActiveClients; i++)
	{
		clients[i] = clients[i + 1];
		dwThreadIDs[i] = dwThreadIDs[i + 1];
		hThreads[i] = hThreads[i + 1];
	}
	clients[MAX_CONNECTIONS - 1] = NULL;
	dwThreadIDs[MAX_CONNECTIONS - 1] = NULL;
	hThreads[MAX_CONNECTIONS - 1] = NULL;
	g_ActiveClients--;
}
VOID Broadcast(CHAR sz_message[], SOCKADDR_IN addr)
{
	CHAR addrInfo[22] = {};
	sprintf(addrInfo, "[%s:%i] => ", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
	for (int i = 0; i < g_ActiveClients; i++)
	{
		CHAR buffer[MTU] = {};
		strcpy(buffer, addrInfo);
		strcat(buffer, sz_message);
		send(clients[i], buffer, strlen(buffer), 0);
	}
}

VOID ClientHandle(SOCKET client_socket)
{
	INT iResult = 0;
	//7) Получение и отправка данных:
	CHAR send_buffer[MTU] = "Hello Client!!!";
	CHAR recv_buffer[MTU] = {};

	SOCKADDR_IN client_addr;
	INT client_addrlen = sizeof(client_addr);
	getpeername(client_socket, (SOCKADDR*)&client_addr, &client_addrlen);		//возвращает IP и порт удаленного адреса (клиента); getsockname() - возвращает локальный адрес (в данном случае - сервера)

	do
	{
		ZeroMemory(recv_buffer, MTU);
		iResult = recv(client_socket, recv_buffer, MTU, 0);			//Ожидает получение от клиента
		if (iResult > 0)
		{
			cout << iResult << " Bytes received, Message: " << recv_buffer << endl;
			Broadcast(recv_buffer, client_addr);
		}
		else if (iResult == 0) cout << "Nothing received from client" << endl;
		else cout << "Receive failed with error: " << WSAGetLastError() << endl;
	} while (iResult > 0);

	//8) Разрываем соединение с клиентом:
	iResult = shutdown(client_socket, SD_BOTH);
	if (iResult == SOCKET_ERROR) cout << "shutdown failed with error: " << WSAGetLastError() << endl;
	closesocket(client_socket);
	Shift(GetClientIndex(GetCurrentThreadId()));
	ExitThread(0);
}