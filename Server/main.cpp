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
	if (listen(listen_socket, 1) == SOCKET_ERROR) //1 - максимальное количество подключений
	{
		cout << "Listen failed with erroe: " << WSAGetLastError() << endl;
		closesocket(listen_socket);
		WSACleanup();
		return;
	}

	//6) Accept connection - Обработка входящих соединений
	SOCKET client_socket = accept(listen_socket, NULL, NULL);		//ожидает запрос от клиента
	if (client_socket == INVALID_SOCKET) cout << "Accept failed with error: " << WSAGetLastError() << endl;

	//7) Получение и отправка данных:
	CHAR send_buffer[MTU] = "Hello Client!!!";
	CHAR recv_buffer[MTU] = {};

	do
	{
		iResult = recv(client_socket, recv_buffer, MTU, 0);			//Ожидает получение от клиента
		if (iResult > 0)
		{
			cout << iResult << " Bytes received, Message: " << recv_buffer << endl;
			INT iSendResult = send(client_socket, send_buffer, strlen(send_buffer), 0);
			if (iSendResult == SOCKET_ERROR)
			{
				cout << "Send failed with error: " << WSAGetLastError() << endl;
				closesocket(client_socket);
			}
			else cout << iSendResult << " Bytes send" << endl;
		}
		else if (iResult == 0) cout << "Nothing received from client" << endl;
		else cout << "Receive failed with error: " << WSAGetLastError() << endl;
	} while (iResult > 0);

	//8) Разрываем соединение с клиентом:
	iResult = shutdown(client_socket, SD_BOTH);
	if (iResult == SOCKET_ERROR) cout << "shutdown failed with error: " << WSAGetLastError() << endl;
	closesocket(client_socket);

	//9) Release resourses:
	closesocket(listen_socket);
	WSACleanup();
}