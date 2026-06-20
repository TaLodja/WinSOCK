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
#include <FormatLastError.h>
using namespace std;

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "FormatLastError.lib")

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
	DWORD dwError = 0;
	CHAR szError[USHRT_MAX + 1] = {};
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
	dwError = WSAGetLastError();
	if (listen_socket == INVALID_SOCKET)
	{
		cout << "SOCKET creation failed with error: " << dwError << endl;
		cout << FormatLastError(szError, dwError) << endl;
		freeaddrinfo(binder);
		WSACleanup();
		return;
	}

	//4) Bind SOCKET - Привязываем сокет к IP-адреса и портам, которые он будет слушать
	iResult = bind(listen_socket, binder->ai_addr, binder->ai_addrlen);
	dwError = WSAGetLastError();
	freeaddrinfo(binder);
	if (iResult == SOCKET_ERROR)
	{
		cout << "Bind failed with error:" << dwError << endl;
		cout << FormatLastError(szError, dwError) << endl;
		closesocket(listen_socket);
		WSACleanup();
		return;
	}

	//5) Запускаеи прослушивание:
	if (listen(listen_socket, MAX_CONNECTIONS) == SOCKET_ERROR) //1 - максимальное количество подключений
	{
		dwError = WSAGetLastError();
		cout << "Listen failed with erroe: " << dwError << endl;
		cout << FormatLastError(szError, dwError) << endl;
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
		dwError = WSAGetLastError();
		if (client_socket == INVALID_SOCKET)
		{
			cout << "Accept failed with error: " << dwError << endl;
			cout << FormatLastError(szError, dwError) << endl;
		}
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
			dwError = WSAGetLastError();
			if (iResult == SOCKET_ERROR)
			{
				cout << "send Error: " << dwError << endl;
				cout << FormatLastError(szError, dwError) << endl;
			}
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
VOID Broadcast(CHAR sz_message[], DWORD dwID)
{
	for (int i = 0; i < g_ActiveClients; i++)
	{
		if (dwThreadIDs[i] != dwID) send(clients[i], sz_message, strlen(sz_message), 0);
	}
}

VOID ClientHandle(SOCKET client_socket)
{
	SOCKADDR_IN client_address;
	client_address.sin_family = AF_INET;
	INT addrlen = sizeof(client_address);
	getpeername(client_socket, (SOCKADDR*)&client_address, &addrlen);
	CHAR sz_client_address[32] = {};
	//CHAR sz_client_connected[32] = {};
	sprintf(sz_client_address, "[%s:%d]", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

	INT iResult = 0;
	DWORD dwError = 0;
	CHAR szError[USHRT_MAX + 1] = {};

	//7) Получение и отправка данных:
	CHAR send_buffer[MTU] = "Hello Client!!!";
	CHAR recv_buffer[MTU] = {};

	do
	{
		ZeroMemory(recv_buffer, MTU);
		ZeroMemory(send_buffer, MTU);
		iResult = recv(client_socket, recv_buffer, MTU, 0);			//Ожидает получение от клиента
		dwError = WSAGetLastError();
		if (iResult > 0)
		{
			sprintf(send_buffer, "%s:\t%s", sz_client_address, recv_buffer);
			cout /*<< iResult << " Bytes received, Message: " */<< send_buffer << endl;
			Broadcast(send_buffer, GetCurrentThreadId());
			/*INT iSendResult = send(client_socket, recv_buffer, strlen(send_buffer), 0);
			if (iSendResult == SOCKET_ERROR)
			{
				cout << "Send failed with error: " << WSAGetLastError() << endl;
				closesocket(client_socket);
			}
			else cout << iSendResult << " Bytes send" << endl;*/
		}
		else if (iResult == 0) cout << "Nothing received from client" << endl;
		else
		{
			cout << "Receive failed with error: " << dwError << endl;
			cout << FormatLastError(szError, dwError) << endl;
		}
	} while (iResult > 0);

	//8) Разрываем соединение с клиентом:
	iResult = shutdown(client_socket, SD_BOTH);
	dwError = WSAGetLastError();
	if (iResult == SOCKET_ERROR)
	{
		cout << "shutdown failed with error: " << dwError << endl;
		cout << FormatLastError(szError, dwError) << endl;
	}
	closesocket(client_socket);
	//g_ActiveClients--;
	Shift(GetClientIndex(GetCurrentThreadId()));
	ExitThread(0);
}