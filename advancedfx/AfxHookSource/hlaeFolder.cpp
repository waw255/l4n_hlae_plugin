#include "stdafx.h"

#include "hlaeFolder.h"

#include <shared/StringTools.h>
#include <shared/FileTools.h>

#include <Windows.h>
#include <Shlobj.h>
#include <string>

std::wstring g_HlaeFolderW(L"");
std::string g_HlaeFolder("");

void CalculateHlaeFolderOnce();

namespace {

bool IsAbsolutePath(const std::wstring& path)
{
	return (2 <= path.length() && L':' == path[1]) ||
		(2 <= path.length() && L'\\' == path[0] && L'\\' == path[1]);
}

std::wstring EnsureTrailingSlash(std::wstring path)
{
	if (path.empty() || L'\\' == path.back())
		return path;

	path.push_back(L'\\');
	return path;
}

std::wstring MakeAbsolutePath(const std::wstring& path)
{
	wchar_t buffer[32768]{};
	const DWORD length = GetFullPathNameW(
		path.c_str(), ARRAYSIZE(buffer), buffer, nullptr);
	if (0 == length || ARRAYSIZE(buffer) <= length)
		return path;

	return std::wstring(buffer, length);
}

bool GetModuleDirectory(HMODULE* outModule, std::wstring* outDirectory)
{
	if (nullptr == outModule || nullptr == outDirectory)
		return false;

	HMODULE module = nullptr;
	if (!GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
				GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&CalculateHlaeFolderOnce), &module))
		return false;

	wchar_t fileName[32768]{};
	const DWORD length = GetModuleFileNameW(module, fileName, ARRAYSIZE(fileName));
	if (0 == length || ARRAYSIZE(fileName) <= length)
		return false;

	std::wstring directory(fileName, length);
	const size_t separator = directory.find_last_of(L'\\');
	if (std::wstring::npos == separator)
		return false;

	directory.resize(separator + 1);
	*outModule = module;
	*outDirectory = directory;
	return true;
}

#ifdef AFX_L4N_PLUGIN
std::wstring GetConfiguredL4nRoot(const std::wstring& moduleDirectory)
{
	const std::wstring configPath = moduleDirectory + L"l4n_hlae_plugin.ini";
	wchar_t configuredRoot[32768]{};
	const DWORD length = GetPrivateProfileStringW(
		L"HLAE", L"HlaeRoot", L"", configuredRoot,
		ARRAYSIZE(configuredRoot), configPath.c_str());

	std::wstring root(configuredRoot, length);
	if (root.empty())
		root = L"..\\..\\..\\l4n_hlae_core";
	if (!IsAbsolutePath(root))
		root = moduleDirectory + root;

	return EnsureTrailingSlash(MakeAbsolutePath(root));
}
#endif

}  // namespace

void CalculateHlaeFolderOnce()
{
	static bool firstRun = true;
	if (firstRun)
	{
		firstRun = false;
	}
	else
		return;

	HMODULE module = nullptr;
	std::wstring moduleDirectory;
	if (!GetModuleDirectory(&module, &moduleDirectory))
		return;

#ifdef AFX_L4N_PLUGIN
	g_HlaeFolderW = GetConfiguredL4nRoot(moduleDirectory);
#else
	g_HlaeFolderW = EnsureTrailingSlash(moduleDirectory);
#ifdef _WIN64
	// Strip the x64 folder for the standard HLAE layout.
	const size_t separator = g_HlaeFolderW.find_last_of(L'\\',
		g_HlaeFolderW.length() > 1 ? g_HlaeFolderW.length() - 2 : 0);
	if (std::wstring::npos != separator)
		g_HlaeFolderW.resize(separator + 1);
#endif
#endif

	WideStringToUTF8String(g_HlaeFolderW.c_str(), g_HlaeFolder);

	return;
}

const char * GetHlaeFolder()
{
	CalculateHlaeFolderOnce();

	return g_HlaeFolder.c_str();
}

const wchar_t * GetHlaeFolderW()
{
	CalculateHlaeFolderOnce();

	return g_HlaeFolderW.c_str();
}


std::wstring g_HlaeRoamingAppDataFolderW(L"");
std::string g_HlaeRoamingAppDataFolder("");

void CalculateHlaeRoamingAppDataFolderOnce()
{
	static bool firstRun = true;
	if (firstRun)
	{
		firstRun = false;
	}
	else
		return;

	PWSTR path = nullptr;

	if (S_OK == SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT,0,&path))
	{
		g_HlaeRoamingAppDataFolderW = path;

		size_t fp = g_HlaeRoamingAppDataFolderW.find_last_of(L'\\');
		if (std::string::npos == fp || g_HlaeRoamingAppDataFolderW.length() != fp)
		{
			g_HlaeRoamingAppDataFolderW.resize(g_HlaeRoamingAppDataFolderW.length() + 1, L'\\');
		}

		g_HlaeRoamingAppDataFolderW += L"HLAE\\";

		WideStringToUTF8String(g_HlaeRoamingAppDataFolderW.c_str(), g_HlaeRoamingAppDataFolder);

		CreatePath(g_HlaeRoamingAppDataFolderW.c_str(), std::wstring());
	}

	CoTaskMemFree(path);

	return;
}

const char* GetHlaeRoamingAppDataFolder()
{
	CalculateHlaeRoamingAppDataFolderOnce();

	return g_HlaeRoamingAppDataFolder.c_str();
}

const wchar_t* GetHlaeRoamingAppDataFolderW()
{
	CalculateHlaeRoamingAppDataFolderOnce();

	return g_HlaeRoamingAppDataFolderW.c_str();
}
