#pragma once
#include <Windows.h>
#include <string>
#include "DXComponentTea.h"

namespace AmazonHook
{
	class MainModule
	{
	private:
		static std::string *moduleDirectory;

	public:
		static const wchar_t* WindowName;

		static HWND WindowHandle;
		static HMODULE Module;

		static int fpsLimit;
		static int fpsLimitSet;
		static bool inputDisable;
		static intptr_t baseAddress;
		static bool showUi;
		static std::string GetModuleDirectory();
		static POINT GetWindowBounds();
		static AmazonHook::Vtea::DXComponentTea dxcomptea;

		static void Initialize();
	};
}
