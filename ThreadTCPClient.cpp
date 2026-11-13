#include <iostream>
#include "ThreadTCPClient.h"
#include <winsock2.h>
#include <stdio.h>
#include <mutex>

#define TAMBUF 20

#pragma comment(lib, "Ws2_32.lib")

int threadClient(std::mutex *escrita) {

	escrita->lock();
	std::cout << "Thread iniciada. TAMBUF = " << TAMBUF << std::endl;
	escrita->unlock();

	WSADATA     wsaData;
	SOCKET      s;
	SOCKADDR_IN ServerAddr;

	char* ipaddr, buf[TAMBUF], msg[TAMBUF + 1];
	int         port, status;

	// Endereço IP + Porto para conexão TCP
	ipaddr = "127.0.0.1";
	port = 2540;

	// Inicializa Winsock versão 2.2
	status = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (status != 0) {

		escrita->lock();
		printf("Falha na inicializacao do Winsock 2! Erro  = %d\n", WSAGetLastError());
		escrita->unlock();
		WSACleanup();
		exit(0);
	}

	// Cria um novo socket para estabelecer a conexão.
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET) {
		status = WSAGetLastError();
		if (status == WSAENETDOWN)
		{
			escrita->lock();
			printf("Rede ou servidor de sockets inacessíveis!\n");
			escrita->unlock();
		}
		else
		{
			escrita->lock();
			printf("Falha na rede: codigo de erro = %d\n", status);
			escrita->unlock();
		}
		WSACleanup();
		exit(0);
	}

	// Inicializa a estrutura SOCKADDR_IN que será utilizada para
	// a conexão ao servidor.
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(port);
	ServerAddr.sin_addr.s_addr = inet_addr(ipaddr);

	// Estabelece a conexão com o servidor
	status = connect(s, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr));
	if (status == SOCKET_ERROR) {
		escrita->lock();
		printf("Falha na conexao ao servidor ! Erro  = %d\n", WSAGetLastError());
		escrita->unlock();
		WSACleanup();
		exit(0);
	}

	// Le a hora corrente do servidor


	// Fim do enlace: fecha o "socket" e encerra o ambiente Winsock 2.
		escrita->lock();
	printf("Encerrando a conexao ...\n");
		escrita->unlock();
	closesocket(s);
	WSACleanup();
		escrita->lock();
	printf("Conexao encerrada. Fim do programa.\n");
		escrita->unlock();

	return 0;
};