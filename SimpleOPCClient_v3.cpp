// Simple OPC Client
//
// This is a modified version of the "Simple OPC Client" originally
// developed by Philippe Gras (CERN) for demonstrating the basic techniques
// involved in the development of an OPC DA client.
//
// The modifications are the introduction of two C++ classes to allow the
// the client to ask for callback notifications from the OPC server, and
// the corresponding introduction of a message comsumption loop in the
// main program to allow the client to process those notifications. The
// C++ classes implement the OPC DA 1.0 IAdviseSink and the OPC DA 2.0
// IOPCDataCallback client interfaces, and in turn were adapted from the
// KEPWARE´s  OPC client sample code. A few wrapper functions to initiate
// and to cancel the notifications were also developed.
//
// The original Simple OPC Client code can still be found (as of this date)
// in
//        http://pgras.home.cern.ch/pgras/OPCClientTutorial/
//
//
// Luiz T. S. Mendes - DELT/UFMG - 15 Sept 2011
// luizt at cpdee.ufmg.br
//

#include <atlbase.h>    // required for using the "_T" macro
#include <iostream>
#include <ObjIdl.h>

#include "opcda.h"
#include "opcerror.h"
#include "SimpleOPCClient_v3.h"
#include "SOCAdviseSink.h"
#include "SOCDataCallback.h"
#include "SOCWrapperFunctions.h"

#include <thread>
#include <mutex>
#include "ThreadTCPClient.h"
#include "ThreadTeclado.h"
#include "memoria.h"
// Resumo das modificações nos arquivos disponibilizado -> funçoes alteradas praa trabalharem com vetores e nao items
// funçao adicional pra escrever no servidor OPC quando os setpoints sao disponibilizados 
// funçao de ondatachange modificada pra printar todos os valroes seguindo um padrao 
// grande parte da main foi apagada, e o tempo foi alterado pra 1000ms 
// decidiu-se manter o padrao do arquivo e tentar manter a maioria das variaveis com nomes semelhantes as que tavam antes no codigo 
// esse arquivo agr também inicia as threads e cria a memoria global utilizada para comunicação entre as threads e o OPC Client

using namespace std;

#define OPC_SERVER_NAME L"Matrikon.OPC.Simulation.1"
#define VT VT_R4
#define NUM_ITEMS 7


//#define REMOTE_SERVER_NAME L"your_path"

// Global variables

// The OPC DA Spec requires that some constants be registered in order to use
// them. The one below refers to the OPC DA 1.0 IDataObject interface.
UINT OPC_DATA_TIME = RegisterClipboardFormat (_T("OPCSTMFORMATDATATIME"));

wchar_t ITEM_ID[]=L"Saw-toothed Waves.Real4";
//vetor de strings com os nomes dos itens a serem lidos/escritos no servidor OPC
const wchar_t* ITEM_IDS[NUM_ITEMS] = {
	L"Saw-toothed Waves.Real4",
	L"Square Waves.Real4",      
	L"Triangle Waves.Real4",    
	L"Random.Real4",            
	L"Bucket Brigade.Int4", 
	L"Bucket Brigade.Real4",    
	L"Bucket Brigade.Real8"     
};
//tipo de dado de cada item, 4 leitura 3 escrta
const VARTYPE ITEM_VTS[NUM_ITEMS] = {
	VT_R4, VT_R4, VT_R4, VT_R4, 
	VT_I4, VT_R4, VT_R8         
};
//trocou de void pra int pq agr retorna 0 
int main(void)
{
	IOPCServer* pIOPCServer = NULL;   //pointer to IOPServer interface
	IOPCItemMgt* pIOPCItemMgt = NULL; //pointer to IOPCItemMgt interface

	OPCHANDLE hServerGroup;
	OPCHANDLE hServerItems[NUM_ITEMS];
	memoria memoria;
	int i;
	char buf[100];

	// Have to be done before using microsoft COM library:
	printf("Initializing the COM environment...\n");
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	_ASSERT(!FAILED(hr));
	// Inicialização das threads

	thread TCPClient(threadClient, &memoria);
	thread Teclado(threadTeclado, &memoria);
	pIOPCServer = InstantiateServer(OPC_SERVER_NAME);

	AddTheGroup(pIOPCServer, pIOPCItemMgt, hServerGroup);


	AddTheItems(pIOPCItemMgt, hServerItems);
	IConnectionPoint* pIConnectionPoint = NULL;
	DWORD dwCookie = 0;
	SOCDataCallback* pSOCDataCallback = new SOCDataCallback(&memoria);
	pSOCDataCallback->AddRef();
	SetDataCallback(pIOPCItemMgt, pSOCDataCallback, pIConnectionPoint, &dwCookie);
	SetGroupActive(pIOPCItemMgt);
	while (!memoria.encerrar) {
		if (memoria.novosSetPoints) {
			memoria.novosSetPoints = false; 

			memoria.mtxDados.lock();
			int spPressao = memoria.spPressaoCamara;
			float spTemp = memoria.spTempCamara;
			double spArgonio = memoria.spPressaoArgonio;
			memoria.mtxDados.unlock();

			WriteSetPoints(pIOPCItemMgt, hServerItems, spPressao, spTemp, spArgonio);
			memoria.mtxSaida.lock();
			printf("[OPC WRITE] Set-Points escritos no servidor OPC: Pressao: %d | Temp: %.1f | Argonio: %.1lf\n",
				spPressao, spTemp, spArgonio);
			memoria.mtxSaida.unlock();

		}
	}
	CancelDataCallback(pIConnectionPoint, dwCookie);
	pSOCDataCallback->Release();

	RemoveItems(pIOPCItemMgt, hServerItems);
	pIOPCItemMgt->Release();
	RemoveGroup(pIOPCServer, hServerGroup);
	pIOPCServer->Release();

	if (TCPClient.joinable()) TCPClient.join();
	if (Teclado.joinable()) Teclado.join();

	CoUninitialize();
	return 0;
}


////////////////////////////////////////////////////////////////////
// Instantiate the IOPCServer interface of the OPCServer
// having the name ServerName. Return a pointer to this interface
//
IOPCServer* InstantiateServer(wchar_t ServerName[])
{
	CLSID CLSID_OPCServer;
	HRESULT hr;

	// get the CLSID from the OPC Server Name:
	hr = CLSIDFromString(ServerName, &CLSID_OPCServer);
	_ASSERT(!FAILED(hr));


	//queue of the class instances to create
	LONG cmq = 1; // nbr of class instance to create.
	MULTI_QI queue[1] =
		{{&IID_IOPCServer,
		NULL,
		0}};

	//Server info:
	//COSERVERINFO CoServerInfo =
    //{
	//	/*dwReserved1*/ 0,
	//	/*pwszName*/ REMOTE_SERVER_NAME,
	//	/*COAUTHINFO*/  NULL,
	//	/*dwReserved2*/ 0
    //}; 

	// create an instance of the IOPCServer
	hr = CoCreateInstanceEx(CLSID_OPCServer, NULL, CLSCTX_SERVER,
		/*&CoServerInfo*/NULL, cmq, queue);
	_ASSERT(!hr);

	// return a pointer to the IOPCServer interface:
	return(IOPCServer*) queue[0].pItf;
}


/////////////////////////////////////////////////////////////////////
// Add group "Group1" to the Server whose IOPCServer interface
// is pointed by pIOPCServer. 
// Returns a pointer to the IOPCItemMgt interface of the added group
// and a server opc handle to the added group.
//
void AddTheGroup(IOPCServer* pIOPCServer, IOPCItemMgt* &pIOPCItemMgt, 
				 OPCHANDLE& hServerGroup)
{
	DWORD dwUpdateRate = 0;
	OPCHANDLE hClientGroup = 0;

	//alteração pra bater os 1000ms requerido 
	HRESULT hr = pIOPCServer->AddGroup(
		L"Group1", FALSE, 1000, hClientGroup, 0, 0, 0,
		&hServerGroup, &dwUpdateRate, IID_IOPCItemMgt, (IUnknown**)&pIOPCItemMgt);
	_ASSERT(!FAILED(hr));
}



//////////////////////////////////////////////////////////////////
// Add the Item ITEM_ID to the group whose IOPCItemMgt interface
// is pointed by pIOPCItemMgt pointer. Return a server opc handle
// to the item.
 
void AddTheItems(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE hServerItems[NUM_ITEMS])
{
	HRESULT hr;
	OPCITEMDEF ItemArray[NUM_ITEMS];
	//cadsatra 7itens ao inves de 1 agr, só os 4 primeiros (0 a 3) sao leitura assincrona
	for (int i = 0; i < NUM_ITEMS; i++) {
		ItemArray[i].szAccessPath = L"";
		ItemArray[i].szItemID = (LPWSTR)ITEM_IDS[i];
		ItemArray[i].bActive = (i < 4) ? TRUE : FALSE;
		ItemArray[i].hClient = i + 1;
		ItemArray[i].dwBlobSize = 0;
		ItemArray[i].pBlob = NULL;
		ItemArray[i].vtRequestedDataType = ITEM_VTS[i];
		ItemArray[i].wReserved = 0;
	}

	OPCITEMRESULT* pAddResult = NULL;
	HRESULT* pErrors = NULL;

	hr = pIOPCItemMgt->AddItems(NUM_ITEMS, ItemArray, &pAddResult, &pErrors);
	if (hr != S_OK && hr != S_FALSE) {
		printf("[OPC ERRO] Falha na chamada AddItems. Codigo = %x\n", hr);
		exit(0);
	}

	for (int i = 0; i < NUM_ITEMS; i++) {
		if (FAILED(pErrors[i])) {
			printf("[OPC ERRO] Falha ao adicionar o item %d!\n", i);
		}
		else {
			hServerItems[i] = pAddResult[i].hServer;
		}
		if (pAddResult[i].pBlob != NULL) {
			CoTaskMemFree(pAddResult[i].pBlob);
		}
	}

	CoTaskMemFree(pAddResult);
	CoTaskMemFree(pErrors);
}

///////////////////////////////////////////////////////////////////////////////
// Read from device the value of the item having the "hServerItem" server 
// handle and belonging to the group whose one interface is pointed by
// pGroupIUnknown. The value is put in varValue. 
//
void ReadItems(IUnknown* pGroupIUnknown, OPCHANDLE hServerItem, VARIANT& varValue)
{
	// value of the item:
	OPCITEMSTATE* pValue = NULL;

	//get a pointer to the IOPCSyncIOInterface:
	IOPCSyncIO* pIOPCSyncIO;
	pGroupIUnknown->QueryInterface(__uuidof(pIOPCSyncIO), (void**) &pIOPCSyncIO);

	// read the item value from the device:
	HRESULT* pErrors = NULL; //to store error code(s)
	HRESULT hr = pIOPCSyncIO->Read(OPC_DS_DEVICE, 1, &hServerItem, &pValue, &pErrors);
	_ASSERT(!hr);
	_ASSERT(pValue!=NULL);

	varValue = pValue[0].vDataValue;

	//Release memeory allocated by the OPC server:
	CoTaskMemFree(pErrors);
	pErrors = NULL;

	CoTaskMemFree(pValue);
	pValue = NULL;

	// release the reference to the IOPCSyncIO interface:
	pIOPCSyncIO->Release();
}

///////////////////////////////////////////////////////////////////////////
// Remove the item whose server handle is hServerItem from the group
// whose IOPCItemMgt interface is pointed by pIOPCItemMgt
//
void RemoveItems(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE  hServerItems[NUM_ITEMS])
{	//funçao 
	HRESULT* pErrors = NULL;
	HRESULT hr = pIOPCItemMgt->RemoveItems(NUM_ITEMS, hServerItems, &pErrors);
	_ASSERT(!hr);

	if (pErrors != NULL) CoTaskMemFree(pErrors);
}

////////////////////////////////////////////////////////////////////////
// Remove the Group whose server handle is hServerGroup from the server
// whose IOPCServer interface is pointed by pIOPCServer
//
void RemoveGroup (IOPCServer* pIOPCServer, OPCHANDLE hServerGroup)
{
	// Remove the group:
	HRESULT hr = pIOPCServer->RemoveGroup(hServerGroup, FALSE);
	if (hr != S_OK){
		if (hr == OPC_S_INUSE)
			printf ("Failed to remove OPC group: object still has references to it.\n");
		else printf ("Failed to remove OPC group. Error code = %x\n", hr);
		exit(0);
	}
}

void WriteSetPoints(IUnknown* pGroupIUnknown, OPCHANDLE hServerItems[NUM_ITEMS],
	int spPressao, float spTemp, double spArgonio)
{
	IOPCSyncIO* pIOPCSyncIO = NULL;
	pGroupIUnknown->QueryInterface(__uuidof(pIOPCSyncIO), (void**)&pIOPCSyncIO);
	OPCHANDLE hItensEscrita[3] = {
		hServerItems[4],
		hServerItems[5], 
		hServerItems[6]  
	};

	VARIANT valores[3];
	VariantInit(&valores[0]);
	VariantInit(&valores[1]);
	VariantInit(&valores[2]);

	valores[0].vt = VT_I4;
	valores[0].lVal = spPressao;

	valores[1].vt = VT_R4;
	valores[1].fltVal = spTemp;

	valores[2].vt = VT_R8;
	valores[2].dblVal = spArgonio;

	HRESULT* pErrors = NULL;
	HRESULT hr = pIOPCSyncIO->Write(3, hItensEscrita, valores, &pErrors);
	pIOPCSyncIO->Release();
}