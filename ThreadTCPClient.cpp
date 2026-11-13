#include <iostream>
#include "ThreadTCPClient.h"
#include "memoria.h"
#include <winsock2.h>
#include <stdio.h>
#include <mutex>
#include <string>
#include <cstdio>
#define TAMBUF 128
#define PORTA_SERVIDOR 2540
#define IP_SERVIDOR "127.0.0.1"

#pragma comment(lib, "Ws2_32.lib")
std::string formataNumeroNoPadrao(float valor) {
	char temp[32];
	snprintf(temp, sizeof(temp), "%06.1f", valor);
	return std::string(temp).substr(0, 6);
}
int incrementa(int seqAtual) {
	return (seqAtual + 1) % 1000000;
}
int threadClient(memoria* memoria) {
	WSADATA wsaData;
	SOCKET s;
	SOCKADDR_IN ServerAddr;
	char bufEnvio[TAMBUF];
	char bufRecepcao[TAMBUF];
	
	char* ipaddr, buf[TAMBUF], msg[TAMBUF + 1];
	int  port, status;
	ipaddr = IP_SERVIDOR;
	port = PORTA_SERVIDOR;
	status = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (status != 0) {
		memoria->mtxSaida.lock();
		printf("socket: Falha na inicializacao do Winsock 2! Erro = %d\n", WSAGetLastError());
		memoria->mtxSaida.unlock();
		return -1;
	}

	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(PORTA_SERVIDOR);
	ServerAddr.sin_addr.s_addr = inet_addr(IP_SERVIDOR);

	while (!memoria->encerrar) {
		s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s == INVALID_SOCKET) {
			memoria->mtxSaida.lock();
			printf("Falha ao criar socket. Tentando novamente em 2s\n");
			memoria->mtxSaida.unlock();
			//espera 2 segundos e tenta novamente
			Sleep(2000);
			continue;
		}
		status = connect(s, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr));
		if (status == SOCKET_ERROR) {
			memoria->mtxSaida.lock();
			printf("Falha ao conectar ao servidor %s:%d. Tentando reconectar em 2s\n", IP_SERVIDOR, PORTA_SERVIDOR);
			memoria->mtxSaida.unlock();
			closesocket(s);
			Sleep(2000);
			continue;
		}
		memoria->mtxSaida.lock();
		printf(" TCP Conectado ao Computador de Processo (porta 2540)!\n");
		memoria->mtxSaida.unlock();
		int numSequencial = 1;//zera o sequencial sempre que reconeta 
		//zera a variavel caso tenha sido escrito algo enquanto tava off
		memoria->pedirSetPoints = false;
		DWORD ultimoEnvio =	GetTickCount64();
		while (!memoria->encerrar) {

			if (memoria->pedirSetPoints) {
				memoria->pedirSetPoints = false; 

				snprintf(bufEnvio, sizeof(bufEnvio), "%06d$155", numSequencial);

				status = send(s, bufEnvio, (int)strlen(bufEnvio), 0);
				memoria->mtxSaida.lock();
				printf("Socket pro VMS: Solicitacao de Set-Points enviada: %s\n", bufEnvio);
				memoria->mtxSaida.unlock();
				memset(bufRecepcao, 0, sizeof(bufRecepcao));
				status = recv(s, bufRecepcao, sizeof(bufRecepcao) - 1, 0);
				memoria->mtxSaida.lock();
				printf("VMS pro Socket: Mensagem de Set-Points recebida (200): %s\n", bufRecepcao);
				memoria->mtxSaida.unlock();
				int seqRecebido = 0, codMsg = 0, spPressao = 0;
				float spTemp = 0.0f;
				double spArgonio = 0.0;
				if (sscanf(bufRecepcao, "%d$%d$%d$%f$%lf", &seqRecebido, &codMsg, &spPressao, &spTemp, &spArgonio) == 5 && codMsg == 200) {
					numSequencial = incrementa(seqRecebido);
					memoria->mtxDados.lock();
					memoria->spPressaoCamara = spPressao;
					memoria->spTempCamara = spTemp;
					memoria->spPressaoArgonio = spArgonio;
					memoria->mtxDados.unlock();

					memoria->novosSetPoints = true;

					snprintf(bufEnvio, sizeof(bufEnvio), "%06d$055", numSequencial);
					status = send(s, bufEnvio, (int)strlen(bufEnvio), 0);
					memoria->mtxSaida.lock();
					printf("Ack Enviado: %s\n", bufEnvio);
					memoria->mtxSaida.unlock();
					numSequencial = incrementa(numSequencial);
				}
			}
			DWORD agora = GetTickCount64();
			if (agora - ultimoEnvio >= 2000) {
				ultimoEnvio = agora;
				memoria->mtxDados.lock();
				float tCamara = memoria->tempCamara;
				float pCamara = memoria->pressaoCamara;
				float pArgonio = memoria->pressaoArgonio;
				float tPanela = memoria->tempPanela;
				memoria->mtxDados.unlock();
				//passar sequencial pra string e limitar a 6
				std::string seqStr = std::to_string(numSequencial);
				seqStr = std::string(6 - seqStr.length(), '0') + seqStr;
				std::string mensagem = seqStr + "$255$" +
					formataNumeroNoPadrao(tCamara) + "$" +
					formataNumeroNoPadrao(pCamara) + "$" +
					formataNumeroNoPadrao(pArgonio) + "$" +
					formataNumeroNoPadrao(tPanela);
				status = send(s, mensagem.c_str(), (int)mensagem.length(), 0);
				memoria->mtxSaida.lock();
				printf("Socket pro VMS: Dados de Processo enviados: %s\n", mensagem.c_str());
				memoria->mtxSaida.unlock();
				memset(bufRecepcao, 0, sizeof(bufRecepcao));
                status = recv(s, bufRecepcao, sizeof(bufRecepcao) - 1, 0);
				memoria->mtxSaida.lock();
				printf("VMP pro Socket: ACK recebido: %s\n", bufRecepcao);
				memoria->mtxSaida.unlock();
				std::string mensagemRecebida(bufRecepcao);
				int seqAck = std::stoi(mensagemRecebida.substr(0, mensagemRecebida.find('$')));
				numSequencial = incrementa(seqAck);//muda pra sequencia que veio 
			}
		}
		closesocket(s);
		s = INVALID_SOCKET;
		//se tiver caido e nao tiver sido pedido para encerrar, espera 2 segundos e tenta reconectar
		if (!memoria->encerrar) {
			Sleep(2000);
		}
	}
	WSACleanup();
	return 0;
};