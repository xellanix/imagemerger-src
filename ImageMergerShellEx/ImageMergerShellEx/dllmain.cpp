#include "MergeWithImageMerger.h"
#include <windows.h>
#include <Guiddef.h>
#include "ClassFactory.h"           // For the class factory
#include "Reg.h"
#include "common.h"

HINSTANCE g_hInst;
long g_cDllRef = 0;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
		case DLL_PROCESS_ATTACH:
			// Hold the instance of this DLL module, we will use it to get the 
			// path of the DLL to register the component.
			g_hInst = hModule;
			DisableThreadLibraryCalls(hModule);
			break;
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}

STDAPI DllCanUnloadNow(void)
{
	return g_cDllRef > 0 ? S_FALSE : S_OK;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
	HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;

	if (IsEqualCLSID(CLSID_ImageMergerShellEx, rclsid))
	{
		hr = E_OUTOFMEMORY;

		ClassFactory* pClassFactory = new ClassFactory();
		if (pClassFactory)
		{
			hr = pClassFactory->QueryInterface(riid, ppv);
			pClassFactory->Release();
		}
	}

	return hr;
}

STDAPI DllRegisterServer(void)
{
	HRESULT hr;

	wchar_t szModule[MAX_PATH];
	if (GetModuleFileName(g_hInst, szModule, ARRAYSIZE(szModule)) == 0)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	// Register the component.
	hr = RegisterInprocServer(szModule, CLSID_ImageMergerShellEx,
							  L_Friendly_Class_Name,
							  L"Apartment");
	if (SUCCEEDED(hr))
	{
		// Register the context menu handler. The context menu handler is 
		// associated with the any file class.
		// Control the visibility in QueryContextMenu
		hr = RegisterShellExtContextMenuHandler(L"*",
												CLSID_ImageMergerShellEx,
												L_Friendly_Menu_Name);
	}

	return hr;
}

STDAPI DllUnregisterServer(void)
{
	HRESULT hr = S_OK;

	wchar_t szModule[MAX_PATH];
	if (GetModuleFileName(g_hInst, szModule, ARRAYSIZE(szModule)) == 0)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	// Unregister the component.
	hr = UnregisterInprocServer(CLSID_ImageMergerShellEx);
	if (SUCCEEDED(hr))
	{
		// Unregister the context menu handler.
		hr = UnregisterShellExtContextMenuHandler(L"*",
												  CLSID_ImageMergerShellEx);
	}

	return hr;
}