#include "stdafx.h"

#include <windows.h>

#include <shared/Detours/src/detours.h>

#include <d3d11.h>

#include <tchar.h>

LONG error = NO_ERROR;
HMODULE hD3d11Dll = NULL;

int g_Override = 0;

HANDLE g_hFbSurface;
HANDLE g_hFbDepthSurface;


typedef HRESULT (STDMETHODCALLTYPE * CreateRenderTargetView_t)(
	ID3D11Device * This,
	/* [annotation] */
	_In_  ID3D11Resource *pResource,
	/* [annotation] */
	_In_opt_  const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
	/* [annotation] */
	_COM_Outptr_opt_  ID3D11RenderTargetView **ppRTView);

CreateRenderTargetView_t True_CreateRenderTargetView;

HRESULT STDMETHODCALLTYPE My_CreateRenderTargetView(
	ID3D11Device * This,
	/* [annotation] */
	_In_  ID3D11Resource *pResource,
	/* [annotation] */
	_In_opt_  const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
	/* [annotation] */
	_COM_Outptr_opt_  ID3D11RenderTargetView **ppRTView)
{
	if (0 < g_Override) {

		TCHAR myFormat[33];

		_stprintf_s(myFormat, _T("pDesc->Format = 0x%08x"), pDesc->Format);

		MessageBox(NULL, myFormat, _T("My_CreateRenderTargetView"), MB_OK | MB_ICONINFORMATION);
	}

	ID3D11Resource * resource = nullptr;

	switch (g_Override)
	{
	case 1:
		++g_Override;
		return This->OpenSharedResource(g_hFbSurface, __uuidof(ID3D11Resource), (void **)&resource);
	case 2:
		g_Override = 0;
		return This->OpenSharedResource(g_hFbDepthSurface, __uuidof(ID3D11Resource), (void **)&resource);
	}

	return True_CreateRenderTargetView(
		This,
		resource ? resource : pResource,
		pDesc,
		ppRTView
	);
}


typedef HRESULT(WINAPI * D3D11CreateDevice_t)(
	_In_opt_        IDXGIAdapter        *pAdapter,
	D3D_DRIVER_TYPE     DriverType,
	HMODULE             Software,
	UINT                Flags,
	_In_opt_  const D3D_FEATURE_LEVEL   *pFeatureLevels,
	UINT                FeatureLevels,
	UINT                SDKVersion,
	_Out_opt_       ID3D11Device        **ppDevice,
	_Out_opt_       D3D_FEATURE_LEVEL   *pFeatureLevel,
	_Out_opt_       ID3D11DeviceContext **ppImmediateContext
);

D3D11CreateDevice_t TrueD3D11CreateDevice = NULL;

HRESULT WINAPI MyD3D11CreateDevice(
	_In_opt_        IDXGIAdapter        *pAdapter,
	D3D_DRIVER_TYPE     DriverType,
	HMODULE             Software,
	UINT                Flags,
	_In_opt_  const D3D_FEATURE_LEVEL   *pFeatureLevels,
	UINT                FeatureLevels,
	UINT                SDKVersion,
	_Out_opt_       ID3D11Device        **ppDevice,
	_Out_opt_       D3D_FEATURE_LEVEL   *pFeatureLevel,
	_Out_opt_       ID3D11DeviceContext **ppImmediateContext
) {
	HRESULT result = TrueD3D11CreateDevice(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);

	if (IS_ERROR(result) || NULL == ppDevice)
	{
		error = E_FAIL;
		return result;
	}

	if (NULL == True_CreateRenderTargetView)
	{
		void ** pCreateRenderTargetView = (void **)((*(char **)(*ppDevice)) + sizeof(void *) * 9);

		True_CreateRenderTargetView = (CreateRenderTargetView_t)*pCreateRenderTargetView;

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)True_CreateRenderTargetView, My_CreateRenderTargetView);
		error = DetourTransactionCommit();
	}

	return result;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		{
			hD3d11Dll = LoadLibrary(_T("d3d11.dll"));

			TrueD3D11CreateDevice = (D3D11CreateDevice_t)GetProcAddress(hD3d11Dll,"D3D11CreateDevice");

			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)TrueD3D11CreateDevice, MyD3D11CreateDevice);
			error = DetourTransactionCommit();

		}
		break;
    case DLL_THREAD_ATTACH:
		break;
    case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		{
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourDetach(&(PVOID&)TrueD3D11CreateDevice, MyD3D11CreateDevice);
			error = DetourTransactionCommit();

			FreeLibrary(hD3d11Dll);
		}
        break;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) bool AfxHookUnityStatus(void) {
	return NO_ERROR == error;
}


extern "C" __declspec(dllexport) void AfxHookUnityBeginCreateRenderTexture(HANDLE fbSharedHandle, HANDLE fbDepthSharedHandle)
{
	g_hFbSurface = fbSharedHandle;
	g_hFbDepthSurface = fbSharedHandle;

	g_Override = 1;
}

extern "C" __declspec(dllexport) void AfxHookUnityEndCreateRenderTexture()
{
	g_Override = 0;
}
