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
	typedef HRESULT(__stdcall* Present_t)(IDirect3DDevice9* device, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion);
	typedef DWORD(__fastcall* FUN_00b700a0)(int param_1);
	typedef DWORD(__fastcall* FUN_00b700b0)(int param_1);

	//timer hax
	typedef unsigned int(__cdecl* FUN_5C32A0)(unsigned int a1);
	typedef int(__thiscall* FUN_6F5CD0)(__m128i *thisobj);
	typedef int(__fastcall* FUN_00cf34b0)(void*, char*, int, int);

	typedef int* (*FUN_00b69190)(int* param_1, int* param2);
	static FUN_00b700a0 f00b700a0;
	static FUN_00b700b0 f00b700b0;
	static FUN_00b69190 f00b69190;
	static FUN_00cf34b0 f00cf34b0;
	static FUN_5C32A0 f5C32A0;
	static FUN_6F5CD0 f6F5CD0;
	static Direct3DCreate9_t OriginalDirect3DCreate9;
	static CreateDevice_t OriginalCreateDevice;
	static EndScene_t OriginalEndScene;
	static Present_t OriginalPresent;
	static IDirect3D9* ID3D9;
	static IDirect3DDevice9* D3DDevice;

	static bool Windowed = false;
	static bool Vsync = false;
	static bool Frame = false;
	static bool Debug = false;

	float Acceleration = 4;

	static int Scaling = 1;
	static int ResWidth = 0;
	static int ResHeight = 0;
	static int Verbosity = 0;
	static int RefreshRate = 60;

	static int WindowedWidth = -1;
	static int WindowedHeight = -1;

	static WNDPROC oWndProc;
	bool print = false;

	int addresses[9999];
	bool hookall = false;
	static int areturn = 0x00CEDBD4;
	DXComponentTea::DXComponentTea()
	{

	}


	DXComponentTea::~DXComponentTea()
	{

	}

	void clearscr() {
		COORD topLeft = { 0, 0 };
		HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleCursorPosition(console, topLeft);
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

	DWORD __fastcall FUN00cf34b0(void* param_1, char* param_1_00, int param_2, DWORD param_3)
	{
		printf("%s", param_1_00);
		return f00cf34b0(param_1, param_1_00, param_2, param_3);
	}

	int f0xb691e0 = 0;
	int f0xb691d7 = 0;
	DWORD __fastcall FUN00b700a0(int param_1)
	{
		auto returnaddr = (intptr_t)_ReturnAddress() - MainModule::baseAddress + 0x400C00;
		int after = 0;
		after = f00b700a0(param_1);
		if (Verbosity >= 4)
			printf("[AmazonHook] wut a0\n %p=%d, %p, %p\n", param_1, after, _ReturnAddress(), returnaddr);
		if (MainModule::WindowHandle != NULL)
		{
			if (returnaddr == 0xb691e0)
			{
				//if (f0xb691e0 == 0)
				//if (after != ResHeight)
				if (ResHeight == 0)
				{
					auto winsize = MainModule::GetWindowBounds();
					after = (int)((float)after * ((float)after / (float)winsize.y));
				}
				/*else
					after = (int)((float)after * ((float)after / (float)ResHeight));*/
				f0xb691e0++;
				if (Verbosity >= 4)
					printf("[AmazonHook] f0xb691e0 %d\n", f0xb691e0);
			}
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
		{
			if (returnaddr == 0xb691d7)
			{
				//if (f0xb691d7 == 0)
					//if (after != ResWidth)
				if (ResWidth == 0)
				{
					auto winsize = MainModule::GetWindowBounds();
					after = (int)((float)after * ((float)after / (float)winsize.x));
				}
				/*else
					after = (int)((float)after * ((float)after / (float)ResWidth));*/
				f0xb691d7++;
				if (Verbosity >= 4)
					printf("[AmazonHook] f0xb691d7 %d\n", f0xb691d7);
			}
		}
		return after;
	}
	static bool once = false;
	int* FUN00b69190(int* param_1, int* param_2)
	{
		printf("hello");
		auto result = f00b69190(param_1, param_2);
		return result;
	}

	HRESULT __stdcall EndSceneHOOK(IDirect3DDevice9* device)
	{
		print = false;
		//maybe for imgui later
		if (Verbosity >= 3)
			printf("[AmazonHook] EndScene! %p\n", _ReturnAddress());

		f0xb691e0 = 0;
		f0xb691d7 = 0;
		//clearscr();
		
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

		/*ok = SetWindowPos(
			hwnd,
			HWND_TOP,
			rect.left,
			rect.top,
			rect.right - rect.left,
			rect.bottom - rect.top,
			SWP_FRAMECHANGED | SWP_NOMOVE);*/

		ok = SetWindowPos(
			hwnd,
			HWND_TOP,
			0,
			0,
			WindowedWidth,
			WindowedHeight,
			SWP_FRAMECHANGED | SWP_NOMOVE);


		if (!ok)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			printf("[AmazonHook] SetWindowPos(%p) failed: %x\n", hwnd, (int)hr);

			return hr;
		}

		return S_OK;
	}

	static RECT newrect;

	HRESULT __stdcall PresentHOOK(IDirect3DDevice9* d3d9, const RECT* pSourceRect,
		const RECT* pDestRect,
		HWND          hDestWindowOverride,
		const RGNDATA* pDirtyRegion
	)
	{
		auto current = MainModule::GetWindowBounds();
		return OriginalPresent(d3d9, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	}

	HRESULT __stdcall CreateDeviceHOOK(IDirect3D9* d3d9, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** ppReturnedDeviceInterface)
	{
		pp->FullScreen_RefreshRateInHz = RefreshRate;

		if (Windowed)
		{
			if (Scaling == 2)
			{
				//pp->BackBufferWidth = ResWidth;
				//pp->BackBufferHeight = ResHeight;
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
			pp->SwapEffect = D3DSWAPEFFECT_COPY;
		}

		if (Frame && Windowed)
		{
			gfx_frame_window(hFocusWindow);
		}
		else {

			if (Windowed)
			{
				auto ok = SetWindowPos(
					hFocusWindow,
					HWND_TOP,
					0,
					0,
					WindowedWidth,
					WindowedHeight,
					SWP_FRAMECHANGED | SWP_NOMOVE);

				if (!ok)
				{
					auto hr = HRESULT_FROM_WIN32(GetLastError());
					printf("[AmazonHook] SetWindowPos(%p) failed: %x\n", hFocusWindow, (int)hr);

					return hr;
				}
			}
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

	void __stdcall FSleep(DWORD miliseconds)
	{
		if (miliseconds > 499)
			printf("Sleep %p %d\n", _ReturnAddress(), miliseconds);
		OSleep(miliseconds);
		return;
	}
	unsigned int __cdecl FUN5C32A0(unsigned int a1)
	{
		auto returnaddr = (intptr_t)_ReturnAddress() - MainModule::baseAddress + 0x400C00;
		printf("timer??? %p \n", returnaddr);

		auto returnvalue = a1 / 0x3e8;
		printf("%d %d \n", a1, returnvalue);
		a1 = 99;
		returnvalue = 99;
		return returnvalue;
	}
	static char** timerobjvalue = 0x00;
	int __cdecl FUN6F5CD0(__m128i* thisobj)
	{
		auto returnaddr = (intptr_t)_ReturnAddress() - MainModule::baseAddress + 0x400C00;
		return f6F5CD0(thisobj);
	}

	void print128_num(__m128i var)
	{
		int64_t* v64val = (int64_t*)&var;
		printf("%.16llx %.16llx\n", v64val[1], v64val[0]);
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

			if (amazonConfig.TryGetValue("RefreshRate", &value))
			{
				RefreshRate = std::stoi(*value);
			}

			if (amazonConfig.TryGetValue("WindowedWidth", &value))
			{
				WindowedWidth = std::stoi(*value);
			}

			if (amazonConfig.TryGetValue("WindowedHeight", &value))
			{
				WindowedHeight = std::stoi(*value);
			}

			if (amazonConfig.TryGetValue("Debug", &value))
			{
				if (*value == oneStr)
				{
					Debug = true;
				}
			}
		}

		RECT desktop;
		const HWND hDesktop = GetDesktopWindow();
		// Get the size of screen to the variable desktop
		GetWindowRect(hDesktop, &desktop);

		if (WindowedWidth == -1)
		{
			WindowedWidth = desktop.right;
		}

		if (WindowedHeight == -1)
		{
			WindowedHeight = desktop.bottom;
		}

		printf("[AmazonHook] Detouring Direct3DCreate9 %p\n", &ODirect3DCreate9);
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)ODirect3DCreate9, (PVOID)hooked_Direct3DCreate9);
		DetourTransactionCommit();

		f00b700a0 = (FUN_00b700a0)(MainModule::baseAddress + 0x00b700a0 - 0x400C00);
		f00b700b0 = (FUN_00b700b0)(MainModule::baseAddress + 0x00b700b0 - 0x400C00);
		f00b69190 = (FUN_00b69190)(MainModule::baseAddress + 0x00b69190 - 0x400C00);

		f5C32A0 = (FUN_5C32A0)(MainModule::baseAddress + 0x5C32A0);
		f6F5CD0 = (FUN_6F5CD0)(MainModule::baseAddress + 0x6F5CD0);

		if (Debug)
		{
			printf("[DivaImGui] Detouring Sleep %p\n", OSleep);
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)OSleep, (PVOID)FSleep);
			DetourTransactionCommit();

			printf("[AmazonHook] Detouring 5C32A0  %p\n", f5C32A0);
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)f5C32A0, (PVOID)FUN5C32A0);
			DetourTransactionCommit();
		}

		if (Scaling == 3)
		{
			printf("[AmazonHook] Detouring 00b69190 %p\n", f00b700b0);
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)f00b69190, (PVOID)FUN00b69190);
			DetourTransactionCommit();
		}

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
