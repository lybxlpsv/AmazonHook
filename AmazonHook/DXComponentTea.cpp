#include <Windows.h>
#include "DXComponentTea.h"

#include <stdio.h>

#include <chrono>
#include <thread>

#include "MainModule.h"
#include "parser.hpp"
#include "FileSystem/ConfigFile.h"
#include "detours/detours.h"
#include <d3d9.h>
#include <vtablehook.h>
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)

#pragma comment(lib, "d3d9.lib")

namespace AmazonHook::Vtea
{
	typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
	static bool initialize = false;
	int DXComponentTea::gamever = 0;

	static IDirect3D9* (WINAPI* ODirect3DCreate9)(UINT SDKVersion) = Direct3DCreate9;
	static VOID(WINAPI* OSleep)(DWORD dwMilliseconds) = Sleep;

	typedef IDirect3D9* (WINAPI* Direct3DCreate9_t)(UINT SDKVersion);
	typedef HRESULT(__stdcall* CreateDevice_t)(IDirect3D9* d3d9, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface);
	typedef HRESULT(__stdcall* EndScene_t)(IDirect3DDevice9* device);
	typedef HRESULT(__stdcall* Present_t)(IDirect3DDevice9* device, const RECT* pSourceRect, const RECT* pDestRect,	HWND hDestWindowOverride, const RGNDATA* pDirtyRegion);
	typedef DWORD(__fastcall* FUN_00b700a0)(int param_1);
	typedef DWORD(__fastcall* FUN_00b700b0)(int param_1);
	static FUN_00b700a0 f00b700a0;
	static FUN_00b700b0 f00b700b0;
	static Direct3DCreate9_t OriginalDirect3DCreate9;
	static CreateDevice_t OriginalCreateDevice;
	static EndScene_t OriginalEndScene;
	static Present_t OriginalPresent;
	static IDirect3D9* ID3D9;
	static IDirect3DDevice9* D3DDevice;

	static bool Windowed = false;
	static bool Vsync = false;
	static bool Frame = false;

	static int Scaling = 1;
	static int ResWidth = 0;
	static int ResHeight = 0;
	static int Verbosity = 0;
	static WNDPROC oWndProc;

	DXComponentTea::DXComponentTea()
	{

	}


	DXComponentTea::~DXComponentTea()
	{

	}

	void InjectCode(void* address, const std::vector<uint8_t> data)
	{
		const size_t byteCount = data.size() * sizeof(uint8_t);

		DWORD oldProtect;
		VirtualProtect(address, byteCount, PAGE_EXECUTE_READWRITE, &oldProtect);
		memcpy(address, data.data(), byteCount);
		VirtualProtect(address, byteCount, oldProtect, nullptr);
	}

	void Update()
	{

	}

	DWORD __fastcall FUN00b700a0(int param_1)
	{
		auto returnaddr = (intptr_t)_ReturnAddress() - MainModule::baseAddress + 0x400C00;
		int after = 0;
		after = f00b700a0(param_1);
		if (Verbosity >= 4)
			printf("[AmazonHook] wut a0\n %p=%d, %p, %04x\n", param_1, after, _ReturnAddress(), returnaddr);
		if (MainModule::WindowHandle != NULL)
			if (returnaddr == 0xb691e0)
			{
				if (ResHeight == 0)
				{
					auto winsize = MainModule::GetWindowBounds();
					after = (int)((float)after * ((float)after / (float)winsize.y));
				}
				else
					after = (int)((float)after * ((float)after / (float)ResWidth));
			}
		return after;
	}

	DWORD __fastcall FUN00b700b0(int param_1)
	{
		auto returnaddr = (intptr_t)_ReturnAddress() - MainModule::baseAddress + 0x400C00;
		int after = f00b700b0(param_1);
		if (Verbosity >= 4)
			printf("[AmazonHook] wut b0\n %p=%d, %p, %p\n", param_1, after, _ReturnAddress(), returnaddr);
		if (MainModule::WindowHandle != NULL)
			if (returnaddr == 0xb691d7)
			{
				if (ResHeight == 0)
				{
					auto winsize = MainModule::GetWindowBounds();
					after = (int)((float)after * ((float)after / (float)winsize.x));
				}
				else
					after = (int)((float)after * ((float)after / (float)ResHeight));
			}
		return after;
	}

	HRESULT __stdcall EndSceneHOOK(IDirect3DDevice9* device)
	{
		//maybe for imgui later
		if (Verbosity >= 3)
			printf("[AmazonHook] EndScene! %p\n", _ReturnAddress());
		return OriginalEndScene(device);
	}

	//based on djh's
	static HRESULT gfx_frame_window(HWND hwnd)
	{
		HRESULT hr;
		DWORD error;
		LONG style;
		RECT rect;
		BOOL ok;

		SetLastError(ERROR_SUCCESS);
		style = GetWindowLongW(hwnd, GWL_STYLE);
		error = GetLastError();

		if (error != ERROR_SUCCESS)
		{
			hr = HRESULT_FROM_WIN32(error);
			printf("[AmazonHook] GetWindowLongPtrW(%p, GWL_STYLE) failed: %x\n",
				hwnd,
				(int)hr);

			return hr;
		}

		ok = GetClientRect(hwnd, &rect);

		if (!ok)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			printf("[AmazonHook] GetClientRect(%p) failed: %x\n", hwnd, (int)hr);

			return hr;
		}

		style |= WS_BORDER | WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_THICKFRAME;
		ok = AdjustWindowRect(&rect, style, FALSE);

		if (!ok)
		{
			/* come on... */
			hr = HRESULT_FROM_WIN32(GetLastError());
			printf("[AmazonHook] AdjustWindowRect failed: %x\n", (int)hr);

			return hr;
		}

		/* This... always seems to set an error, even though it works? idk */
		SetWindowLongW(hwnd, GWL_STYLE, style);

		ok = SetWindowPos(
			hwnd,
			HWND_TOP,
			rect.left,
			rect.top,
			rect.right - rect.left,
			rect.bottom - rect.top,
			SWP_FRAMECHANGED | SWP_NOMOVE);

		if (!ok)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			printf("[AmazonHook] SetWindowPos(%p) failed: %x\n", hwnd, (int)hr);

			return hr;
		}

		return S_OK;
	}

	HRESULT __stdcall PresentHOOK(IDirect3DDevice9* d3d9, const RECT* pSourceRect,
		const RECT* pDestRect,
		HWND          hDestWindowOverride,
		const RGNDATA* pDirtyRegion
	)
	{
		return OriginalPresent(d3d9, pDestRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	}

	HRESULT __stdcall CreateDeviceHOOK(IDirect3D9* d3d9, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** ppReturnedDeviceInterface)
	{
		if (Windowed)
		{
			if (Scaling == 2)
			{
				pp->BackBufferWidth = 0;
				pp->BackBufferHeight = 0;
			}
			else {
				pp->BackBufferWidth = 1920;
				pp->BackBufferHeight = 1080;
			}
			pp->Windowed = TRUE;
			pp->FullScreen_RefreshRateInHz = 0;
		}

		if (!Vsync)
		{
			pp->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
			pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
		}

		if (Frame && Windowed)
		{
			gfx_frame_window(hFocusWindow);
		}

		MainModule::WindowHandle = hFocusWindow;

		HRESULT result = OriginalCreateDevice(d3d9, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pp, ppReturnedDeviceInterface);

		if (result != D3D_OK)
		{
			printf("[AmazonHook] CreateDevice Failed\n");
			return result;
		}

		IDirect3DDevice9* device = *ppReturnedDeviceInterface;
		
		void** pVTable = *(void***)(device);

		OriginalEndScene = (EndScene_t)pVTable[42];
		OriginalPresent = (Present_t)pVTable[17];

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)OriginalEndScene, (PVOID)EndSceneHOOK);
		DetourTransactionCommit();

		/*DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)OriginalPresent, (PVOID)PresentHOOK);
		DetourTransactionCommit();*/

		return result;
	}

	IDirect3D9* WINAPI hooked_Direct3DCreate9(UINT sdkversion)
	{
		IDirect3D9* ID3D9 = ODirect3DCreate9(sdkversion);
		void** pVTable = *(void***)(ID3D9);
		OriginalCreateDevice = (CreateDevice_t)pVTable[16];
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)OriginalCreateDevice, (PVOID)CreateDeviceHOOK);
		DetourTransactionCommit();
		printf("[AmazonHook] Direct3DCreate9 Hooked!\n");
		return ID3D9;
	}

	void DXComponentTea::Initialize()
	{
		AmazonHook::FileSystem::ConfigFile amazonConfig(MainModule::GetModuleDirectory(), "amazon.ini");
		bool success = amazonConfig.OpenRead();

		if (success) {
			std::string oneStr = "1";
			std::string zeroStr = "0";
			std::string* value;

			if (amazonConfig.TryGetValue("Windowed", &value))
			{
				if (*value == oneStr)
				{
					Windowed = true;
				}
			}

			if (amazonConfig.TryGetValue("Frame", &value))
			{
				if (*value == oneStr)
				{
					Frame = true;
				}
			}

			if (amazonConfig.TryGetValue("Vsync", &value))
			{
				if (*value == oneStr)
				{
					Vsync = true;
				}
			}

			if (amazonConfig.TryGetValue("Scaling", &value))
			{
				Scaling = std::stoi(*value);
			}

			if (amazonConfig.TryGetValue("ResWidth", &value))
			{
				ResWidth = std::stoi(*value);
				printf("[AmazonHook] ResWidth = %d\n", ResWidth);
			}

			if (amazonConfig.TryGetValue("ResHeight", &value))
			{
				ResHeight = std::stoi(*value);
				printf("[AmazonHook] ResHeight = %d\n", ResHeight);
			}

			if (amazonConfig.TryGetValue("Verbosity", &value))
			{
				Verbosity = std::stoi(*value);
			}
		}
		printf("[AmazonHook] Detouring Direct3DCreate9 %p\n", &ODirect3DCreate9);
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)ODirect3DCreate9, (PVOID)hooked_Direct3DCreate9);
		DetourTransactionCommit();

		f00b700a0 = (FUN_00b700a0)(MainModule::baseAddress + 0x00b700a0 - 0x400C00);
		f00b700b0 = (FUN_00b700b0)(MainModule::baseAddress + 0x00b700b0 - 0x400C00);

		if ((Scaling == 2) || ((!Windowed) && (Scaling == 1)))
		{
			printf("[AmazonHook] Detouring 00b700a0 %p\n", f00b700a0);
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)f00b700a0, (PVOID)FUN00b700a0);
			DetourTransactionCommit();

			printf("[AmazonHook] Detouring 00b700b0 %p\n", f00b700b0);
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)f00b700b0, (PVOID)FUN00b700b0);
			DetourTransactionCommit();
		}
	}
}