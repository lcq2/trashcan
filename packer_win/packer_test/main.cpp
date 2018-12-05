#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdint.h>
#include <stdio.h>
#include <delayimp.h>

#pragma code_seg(".text")
__declspec(allocate(".text")) static uint8_t g_loaderCode[4096 * 4] = { '_', '_', 'L', 'O', 'A', 'D', 'E', 'R'};
#pragma code_seg()

#pragma const_seg(".rdata")
__declspec(allocate(".rdata")) static uint8_t g_fakeIT[4096*2] = { '_', '_', 'F', 'A', 'K', 'E', 'I', 'T' };
#pragma const_seg()

#pragma data_seg(".data")
__declspec(allocate(".data")) static uint8_t g_loaderData[64] = { '_', '_', 'L', 'O', 'A', 'D', 'E', 'R', 'D' };
#pragma data_seg()

/*
extern "C" void NTAPI TlsCallBack(PVOID h, DWORD dwReason, PVOID pv);

#pragma data_seg(".CRT$XLB")
extern "C" PIMAGE_TLS_CALLBACK p_thread_callback = TlsCallBack;
#pragma data_seg()

#pragma comment(linker, "/INCLUDE:__tls_used")

extern "C" void NTAPI TlsCallBack(PVOID h, DWORD dwReason, PVOID pv)
{
	OutputDebugString(L"TLS Callback");
}


#pragma comment(lib, "DelayImp.lib")

FARPROC WINAPI delayHook(unsigned dliNotify, PDelayLoadInfo pdli)
{
	switch (dliNotify) {
	case dliStartProcessing:
		OutputDebugString(L"dliStartProcessing");
		// If you want to return control to the helper, return 0.
		// Otherwise, return a pointer to a FARPROC helper function
		// that will be used instead, thereby bypassing the rest 
		// of the helper.

		break;

	case dliNotePreLoadLibrary:
		OutputDebugString(L"dliNotePreLoadLibrary");

		// If you want to return control to the helper, return 0.
		// Otherwise, return your own HMODULE to be used by the 
		// helper instead of having it call LoadLibrary itself.

		break;

	case dliNotePreGetProcAddress:
		OutputDebugString(L"dliNotePreGetProcAddress");

		// If you want to return control to the helper, return 0.
		// If you choose you may supply your own FARPROC function 
		// address and bypass the helper's call to GetProcAddress.

		break;

	case dliFailLoadLib:
		OutputDebugString(L"dliFailLoadLib");
		// LoadLibrary failed.
		// If you don't want to handle this failure yourself, return 0.
		// In this case the helper will raise an exception 
		// (ERROR_MOD_NOT_FOUND) and exit.
		// If you want to handle the failure by loading an alternate 
		// DLL (for example), then return the HMODULE for 
		// the alternate DLL. The helper will continue execution with 
		// this alternate DLL and attempt to find the
		// requested entrypoint via GetProcAddress.

		break;

	case dliFailGetProc:
		OutputDebugString(L"dliFailGetProc");
		// GetProcAddress failed.
		// If you don't want to handle this failure yourself, return 0.
		// In this case the helper will raise an exception 
		// (ERROR_PROC_NOT_FOUND) and exit.
		// If you choose you may handle the failure by returning 
		// an alternate FARPROC function address.


		break;

	case dliNoteEndProcessing:
		OutputDebugString(L"dliNoteEndProcessing");
		// This notification is called after all processing is done. 
		// There is no opportunity for modifying the helper's behavior
		// at this point except by longjmp()/throw()/RaiseException. 
		// No return value is processed.

		break;

	default:

		return NULL;
	}

	return NULL;
}


//and then at global scope somewhere
PfnDliHook __pfnDliNotifyHook2 = delayHook;
*/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	g_loaderCode;
	g_fakeIT;
	g_loaderData;

	wchar_t buf[64] = { 0 };
	wsprintf(buf, L"%p", (ULONG_PTR *)hInstance);
	MessageBox(NULL, buf, L"Test", MB_OK | MB_ICONINFORMATION);
	return 0;
}