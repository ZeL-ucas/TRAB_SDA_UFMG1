//
// C++ class to implement the OPC DA 2.0 IOPCDataCallback interface.
//
// Note that only the ::OnDataChangeMethod() is currently implemented
// here. This code is largely based on the KEPWARE´s sample client code.
//
// Luiz T. S. Mendes - DELT/UFMG - 13 Sept 2011
//

#include <stdio.h>
#include "SOCDataCallback.h"
#include "SOCWrapperFunctions.h"
#include "memoria.h"
extern UINT OPC_DATA_TIME;

//	Constructor.  Reference count is initialized to zero.
SOCDataCallback::SOCDataCallback (memoria* memoria) : m_cnRef (0)
	{
	m_cnRef = 0;
	m_interna = memoria;
	}

//	Destructor
SOCDataCallback::~SOCDataCallback ()
	{
	}

// IUnknown methods
HRESULT STDMETHODCALLTYPE SOCDataCallback::QueryInterface (REFIID riid, LPVOID *ppv)
{
	// Validate the pointer
	if (ppv == NULL)
		return E_POINTER;       // invalid pointer
	// Standard COM practice requires that we invalidate output arguments
	// if an error is encountered.  Let's assume an error for now and invalidate
	// ppInterface.  We will reset it to a valid interface pointer later if we
	// determine requested ID is valid:
	*ppv = NULL;
	if (riid == IID_IUnknown)
		*ppv = (IUnknown*) this;
	else if (riid == IID_IOPCDataCallback)
		*ppv = (IOPCDataCallback*) this;
	else
		return (E_NOINTERFACE); //unsupported interface

	// Success: increment the reference counter.
	AddRef ();                  
	return (S_OK);
}

ULONG STDMETHODCALLTYPE SOCDataCallback::AddRef (void)
{
    // Atomically increment the reference count and return
	// the value.
    return InterlockedIncrement((volatile LONG*)&m_cnRef);
}

ULONG STDMETHODCALLTYPE SOCDataCallback::Release (void)
{
	if (InterlockedDecrement((volatile LONG*)&m_cnRef) == 0){
		delete this;
		return 0;
	}
	else
	    return m_cnRef;
}

// OnDataChange method. This method is provided to handle notifications
// from an OPC Group for exception based (unsolicited) data changes and
// refreshes. Data for one or possibly more active items in the group
// will be provided.
//
// Parameters:
//	DWORD		dwTransID			Zero for normal OnDataChange events, 
//                                    non-zero for Refreshes.
//	OPCHANDLE	hGroup				Client group handle.
//	HRESULT		hrMasterQuality		S_OK if all qualities are GOOD, otherwise S_FALSE.
//	HRESULT		hrMasterError		S_OK if all errors are S_OK, otherwise S_FALSE.
//	DWORD		dwCount				Number of items in the lists that follow.
//	OPCHANDLE	*phClientItems		Item client handles.
//	VARIANT		*pvValues			Item data.
//	WORD		*pwQualities		Item qualities.
//	FILETIME	*pftTimeStamps		Item timestamps.
//	HRESULT		*pErrors			Item errors.
//
// Returns:
//	HRESULT - 
//		S_OK - Processing of advisement successful.
//		E_INVALIDARG - One of the arguments was invalid.
// **************************************************************************
HRESULT STDMETHODCALLTYPE SOCDataCallback::OnDataChange(
	DWORD dwTransID,
	OPCHANDLE hGroup,
	HRESULT hrMasterQuality,
	HRESULT hrMasterError,
	DWORD dwCount,
	OPCHANDLE* phClientItems,
	VARIANT* pvValues,
	WORD* pwQualities,
	FILETIME* pftTimeStamps,
	HRESULT* pErrors)
{
	FILETIME lft;
	SYSTEMTIME st;
	char szLocalDate[255], szLocalTime[255];
	bool status;
	char buffer[100];
	WORD quality;

	if (dwCount == 0 ||
		phClientItems == NULL ||
		pvValues == NULL ||
		pwQualities == NULL ||
		pftTimeStamps == NULL ||
		pErrors == NULL) {
		printf("IOPCDataCallback::OnDataChange: invalid arguments.\n");
		return (E_INVALIDARG);
	}

	// Percorre todos os itens que sofreram alteração neste ciclo de 1000 ms
	for (DWORD dwItem = 0; dwItem < dwCount; dwItem++)
	{
		status = VarToStr(pvValues[dwItem], buffer);
		if (status) {
			const char* nomeItem = "Desconhecido";

			if (m_interna != NULL && pvValues[dwItem].vt == VT_R4) {
				float valor = pvValues[dwItem].fltVal;

				m_interna->mtxDados.lock();
				switch (phClientItems[dwItem]) {
				case 1:
					m_interna->tempCamara = valor;
					nomeItem = "Temp. Camara (Saw-toothed)";
					break;
				case 2:
					m_interna->pressaoCamara = valor;
					nomeItem = "Pressao Camara (Square)";
					break;
				case 3:
					m_interna->pressaoArgonio = valor;
					nomeItem = "Pressao Argonio (Triangle)";
					break;
				case 4:
					m_interna->tempPanela = valor;
					nomeItem = "Temp. Panela (Random)";
					break;
				}
				m_interna->mtxDados.unlock();
			}

			quality = pwQualities[dwItem] & OPC_QUALITY_MASK;
			const char* strQuality = (quality == OPC_QUALITY_GOOD) ? "good" : "not good";

			FileTimeToLocalFileTime(&pftTimeStamps[dwItem], &lft);
			FileTimeToSystemTime(&lft, &st);
			GetDateFormat(LOCALE_SYSTEM_DEFAULT, DATE_SHORTDATE, &st, NULL, szLocalDate, 255);
			GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, &st, NULL, szLocalTime, 255);

			// 3. Exibe na console protegido pelo mutex de saída
			if (m_interna != NULL) m_interna->mtxSaida.lock();
			printf("[OPC READ] Item[%d] %s = %s | Quality: %s | Time: %s %s\n",
				phClientItems[dwItem], nomeItem, buffer, strQuality, szLocalDate, szLocalTime);
			if (m_interna != NULL) m_interna->mtxSaida.unlock();
		}
		else {
			if (m_interna != NULL) m_interna->mtxSaida.lock();
			printf("IOPCDataCallback: Unsupported item type\n");
			if (m_interna != NULL) m_interna->mtxSaida.unlock();
		}
	}

	return (S_OK);
}
// The remaining methods of IOPCDataCallback are not implemented here, so
// we just use dummy functions that simply return S_OK.
HRESULT STDMETHODCALLTYPE SOCDataCallback::OnReadComplete(
	DWORD dwTransID,
	OPCHANDLE hGroup,
	HRESULT hrMasterQuality,
	HRESULT hrMasterError,
	DWORD dwCount,
	OPCHANDLE *phClientItems,
	VARIANT *pvValues,
	WORD *pwQualities,
	FILETIME *pftTimeStamps,
	HRESULT *pErrors)
{
	return (S_OK);
}

HRESULT STDMETHODCALLTYPE SOCDataCallback::OnWriteComplete(
	DWORD dwTransID,
	OPCHANDLE hGroup,
	HRESULT hrMasterError,
	DWORD dwCount,
	OPCHANDLE *phClientItems,
	HRESULT *pErrors)
{
	return(S_OK);
}

HRESULT STDMETHODCALLTYPE SOCDataCallback::OnCancelComplete(
	DWORD dwTransID,
	OPCHANDLE hGroup)
{
	return(S_OK);
}