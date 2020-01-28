#include "MainModule.h"
#include <filesystem>
#include "FileSystem/ConfigFile.h"
#include "DXComponentTea.h"

namespace AmazonHook
{
	typedef std::filesystem::path fspath;

	std::string* MainModule::moduleDirectory;

	HWND MainModule::WindowHandle;
	HMODULE MainModule::Module;

	int MainModule::fpsLimit = 0;
	int MainModule::fpsLimitSet = 0;
	bool MainModule::inputDisable = false;
	bool MainModule::showUi = false;
	AmazonHook::Vtea::DXComponentTea MainModule::dxcomptea;

	intptr_t MainModule::baseAddress = 0;

	void InjectCode(void* address, const std::vector<uint8_t> data)
	{
		const size_t byteCount = data.size() * sizeof(uint8_t);

		DWORD oldProtect;
		VirtualProtect(address, byteCount, PAGE_EXECUTE_READWRITE, &oldProtect);
		memcpy(address, data.data(), byteCount);
		VirtualProtect(address, byteCount, oldProtect, nullptr);
	}

	void MainModule::Initialize()
	{
		AmazonHook::FileSystem::ConfigFile amazonConfig(AmazonHook::MainModule::GetModuleDirectory(), "amazon.ini");
		HMODULE chun = GetModuleHandle(NULL);
		baseAddress = (intptr_t)chun + 0xC00;
		DWORD oldProtect, bck;

		bool success = amazonConfig.OpenRead();
		if (!success)
		{
			printf("[AmazonHook] Unable to parse %s\n", "amazon.ini");
			return;
		}

		if (success) {
			std::string oneStr = "1";
			std::string zeroStr = "0";
			std::string* value;

			if (amazonConfig.TryGetValue("AllocConsole", &value))
			{
				if (*value == oneStr)
				{
					AllocConsole();
					freopen("CONOUT$", "w", stdout);
					printf("[AmazonHook] Console Enabled!\n");
				}
			}

			if (amazonConfig.TryGetValue("Logger", &value))
			{
				if (*value == oneStr)
				{
					freopen("logs.txt", "a", stdout);
					printf("[AmazonHook] Logger Enabled!\n");
				}
			}

			printf("[AmazonHook] Base Address = %p\n", baseAddress);

			if (amazonConfig.TryGetValue("allowlocalhostnetserver", &value))
			{
				if (*value == oneStr)
				{
					InjectCode((void*)(baseAddress + 0x8BC180), { 0x31, 0xC0, 0xC3 });
					InjectCode((void*)(baseAddress + 0x164D0E0), { 0x30, 0x2F, 0x38, 0x00 });
				}
			}

			if (amazonConfig.TryGetValue("disableshopcloselockout", &value))
			{
				if (*value == oneStr)
				{
					InjectCode((void*)(baseAddress + 0x8E3DD3), { 0xEB });
				}
			}
			if (amazonConfig.TryGetValue("disableupdatecheck", &value))
			{
				if (*value == oneStr)
				{
					InjectCode((void*)(baseAddress + 0x8AE690), { 0x31, 0xC0, 0xC3 });
				}
			}

			if (amazonConfig.TryGetValue("Wasapi", &value))
			{
				InjectCode((void*)(baseAddress + 0xC77D1A), { (byte)std::stoi(*value) });
			}

			if (amazonConfig.TryGetValue("Force2Ch", &value))
			{
				if (*value == oneStr)
					InjectCode((void*)(baseAddress + 0xC77DF1), { 0x90, 0x90 });
			}

			if (amazonConfig.TryGetValue("FpsLimit", &value))
			{
				if (*value == zeroStr)
					InjectCode((void*)(baseAddress + 0x1FA30), { 0xC3 });
			}

			if (amazonConfig.TryGetValue("GfxComponent", &value))
			{
				if (*value == oneStr)
					dxcomptea.Initialize();
			}

			if (amazonConfig.TryGetValue("FreezeSongSelectTimer", &value))
			{
				if (*value == oneStr)
				{
					InjectCode((void*)(baseAddress + 0x6FFC62), { 0xEB });
				}
			}
		}
	}

	std::string MainModule::GetModuleDirectory()
	{
		if (moduleDirectory == nullptr)
		{
			WCHAR modulePathBuffer[MAX_PATH];
			GetModuleFileNameW(MainModule::Module, modulePathBuffer, MAX_PATH);

			fspath configPath = fspath(modulePathBuffer).parent_path();
			moduleDirectory = new std::string(configPath.u8string());
		}

		return *moduleDirectory;
	}

	POINT MainModule::GetWindowBounds()
	{
		RECT windowRect;
		GetWindowRect(WindowHandle, &windowRect);
		POINT window;
		window.x = (windowRect.right - windowRect.left);
		window.y = (windowRect.bottom - windowRect.top);
		return window;
	}
}