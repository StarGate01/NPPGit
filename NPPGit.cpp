///----------------------------------------------------------------------------
/// Copyright (c) 2008-2010 
/// Brandon Cannaday
/// Paranoid Ferret Productions (support@paranoidferret.com)
///
/// This program is free software; you can redistribute it and/or
/// modify it under the terms of the GNU General Public License
/// as published by the Free Software Foundation; either
/// version 2 of the License, or (at your option) any later version.
///
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
/// GNU General Public License for more details.
///
/// You should have received a copy of the GNU General Public License
/// along with this program; if not, write to the Free Software
/// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
///----------------------------------------------------------------------------

#include "stdafx.h"
#include "PluginInterface.h"
#include <locale>
#include <codecvt>
#include <string>
#include <vector>

const TCHAR PLUGIN_NAME[] = TEXT("Git");

static NppData nppData;
static std::vector<FuncItem> funcItems;

#define EXECMODE_ALLOPENFILES 0
#define EXECMODE_SIGLEFILE 1
#define EXECMODE_ALLFILES 2

//
// Required plugin methods
//
extern "C" __declspec(dllexport) BOOL isUnicode()
{
	return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData)
{
	nppData = notpadPlusData;
}

extern "C" __declspec(dllexport) const TCHAR * getName()
{
	return PLUGIN_NAME;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification *notifyCode)
{
	
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

extern "C" __declspec(dllexport) FuncItem * getFuncsArray(int *nbF)
{
	*nbF = funcItems.size();
	return &funcItems.front();
}

///
/// Gets the path to the current file.
///
/// @return Current file path.
///
std::wstring getCurrentFile()
{
	TCHAR path[MAX_PATH];
	::SendMessage(nppData._nppHandle, NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)path); 
	return std::wstring(path);
}

///
/// Gets the path to the current directory.
///
/// @return Current file path.
///
std::wstring getCurrentDirectory()
{
	TCHAR path[MAX_PATH];
	::SendMessage(nppData._nppHandle, NPPM_GETCURRENTDIRECTORY, MAX_PATH, (LPARAM)path);
	return std::wstring(path);
}

///
/// Gets the full path to every opened file.
///
/// @param numFiles [out] Returns the number of opened files.
///
/// @return Vector of filenames.
///
std::vector<std::wstring> getAllFiles(const std::wstring filter)
{
   int numFiles = (::SendMessage(nppData._nppHandle, NPPM_GETNBOPENFILES, 0, ALL_OPEN_FILES)) - 1;
   TCHAR** files = new TCHAR*[numFiles];
   for(int i = 0; i < numFiles; i++) files[i] = new TCHAR[MAX_PATH];
   ::SendMessage(nppData._nppHandle, NPPM_GETOPENFILENAMES, (WPARAM)files, (LPARAM)numFiles);
   std::vector<std::wstring> filePaths;
   for (int i = 0; i < numFiles; i++)
   {
	  std::wstring cfile(files[i]);
	  if(cfile.substr(0, filter.size()) == filter) filePaths.push_back(files[i]);
   }
   return filePaths;
}

///
/// Gets the path to the TortioseGit executable from the registry.
///
/// @param loc [out] Location of Tortoise executable
///
/// @return Whether or not the path was successfully retrieved.
///         If false, Tortoise is most likely not installed.
bool getTortoiseLocation(std::wstring &loc)
{
	HKEY hKey;
	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Software\\TortoiseGit"), 0,
		KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
	{
		return false;
	}

	TCHAR procPath[MAX_PATH];
	DWORD length = MAX_PATH;

   // Modified Douglas Phillips <doug@sbscomp.com> 2008-12-29 to 
   // support 32-bit and Non-Vista operating Systems.
	if(RegQueryValueEx( 
         hKey, 
		   TEXT("ProcPath"), 
		   NULL, 
		   NULL, 
		   (LPBYTE)procPath, 
		   &length) != ERROR_SUCCESS)
	{
		return false;
	}

	loc = loc.append(procPath);
	return true;
}

///
/// Launches TortoiseGit using the supplied command
///
/// @param Command line string to execute.
///
/// @return Whether or not Tortoise could be launched.
///
bool launchTortoise(std::wstring &command)
{
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	memset(&si, 0, sizeof(si));
	memset(&pi, 0, sizeof(pi));
	si.cb = sizeof(si);

	return CreateProcess(
		NULL,
		const_cast<LPWSTR>(command.c_str()),
		NULL,
		NULL,
		FALSE,
		CREATE_DEFAULT_ERROR_MODE,
		NULL,
		NULL,
		&si,
		&pi) != 0;
}

///
/// Finds the .git (root) directory
/// @param path Path of the current directory
///
bool getGitDirectory(std::wstring &path)
{
	while (true)
	{
		std::wstring gitPath = path + TEXT("\\.git");
		DWORD ftyp = GetFileAttributes(gitPath.c_str());
		if (ftyp != INVALID_FILE_ATTRIBUTES) return true;
		if (std::count(path.begin(), path.end(), '\\') == 0) return false;
		std::wstring newpath(path.substr(0, path.find_last_of(TEXT("\\"))).c_str());
		path = newpath;
	}
}

///
/// Builds and executes command line string to send to CreateProcess
/// See http://tortoisesvn.net/docs/release/TortoiseSVN_en/tsvn-automation.html
/// for TortoiseSVN command line parameters.
///
/// @param cmd Command name to execute.
/// @param all Execute command on all files, or just the current file.
///
void ExecCommand(const std::wstring &cmd, int mode = EXECMODE_ALLFILES)
{
	std::wstring tortoiseLoc;
	bool tortoiseInstalled = getTortoiseLocation(tortoiseLoc);
	if(!tortoiseInstalled)
	{
		MessageBox(NULL, TEXT("Could not locate TortoiseGit"), TEXT("Git Error"), 0);
		return;
	}
	std::wstring gitPath = getCurrentDirectory();
	if (!getGitDirectory(gitPath)) 
	{
		MessageBox(NULL, TEXT("Could not find .git directory"), TEXT("Git error"), 0);
		return;
	}
	std::wstring command = tortoiseLoc;
	command += TEXT(" /command:") + cmd + TEXT(" /path:\"");
	if (mode != EXECMODE_ALLFILES)
	{
		std::vector<std::wstring> files;
		if (mode == EXECMODE_ALLOPENFILES) files = getAllFiles(gitPath);
		else if (mode == EXECMODE_SIGLEFILE) files.push_back(getCurrentFile());
		for (std::vector<std::wstring>::iterator itr = files.begin(); itr != files.end(); itr++)
		{
			command += (*itr);
			if (itr != files.end() - 1) command += TEXT("*");
		}
	}
	else
	{
		command += gitPath;
	}
	command += TEXT("\" /closeonend:2");
	if(!launchTortoise(command)) MessageBox(NULL, TEXT("Could not launch TortoiseGit"), TEXT("Git error"), 0);
}

void AddCommand(PFUNCPLUGINCMD func, LPCTSTR sMenuItemCaption,	UCHAR cShortCut)
{
	FuncItem item;

	item._pFunc = func;
	lstrcpy(item._itemName, sMenuItemCaption);
	item._init2Check = false;
	item._pShKey = new ShortcutKey;
	item._pShKey->_isAlt = true;
	item._pShKey->_isCtrl = true;
	item._pShKey->_isShift = false;
	item._pShKey->_key = cShortCut;

	funcItems.push_back(item);
}

////////////////////////////////////////////////////////////////////////////
///
/// Execution commands:
///
void commitFile()
{
   ExecCommand(TEXT("commit"), EXECMODE_SIGLEFILE);
}

void commitAllFiles()
{
	ExecCommand(TEXT("commit"));
}

void commitAllOpenFiles()
{
   ExecCommand(TEXT("commit"), EXECMODE_ALLOPENFILES);
}

void addFile()
{
   ExecCommand(TEXT("add"), EXECMODE_SIGLEFILE);
}

void diffFile()
{
   ExecCommand(TEXT("diff"), EXECMODE_SIGLEFILE);
}

void revertFile()
{
   ExecCommand(TEXT("revert"), EXECMODE_SIGLEFILE);
}

void revertAllFiles()
{
	ExecCommand(TEXT("revert"));
}

void revertAllOpenFiles()
{
   ExecCommand(TEXT("revert"), EXECMODE_ALLOPENFILES);
}

void showFileLog()
{
    ExecCommand(TEXT("log"), EXECMODE_SIGLEFILE);
}

void showAllFileLog()
{
	ExecCommand(TEXT("log"));
}

void showAllOpenFileLog()
{
	ExecCommand(TEXT("log"), EXECMODE_ALLOPENFILES);
}

void pushRep()
{
	ExecCommand(TEXT("push"));
}

void pullRep()
{
	ExecCommand(TEXT("pull"));
}

////////////////////////////////////////////////////////////////////////////
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			AddCommand(commitFile,			TEXT("Commit Project File"),				0);
			AddCommand(commitAllFiles,		TEXT("Commit All Project Files"),			0);
			AddCommand(commitAllOpenFiles,	TEXT("Commit All Open Project Files"),		0);
			AddCommand(addFile,				TEXT("Add File To Project"),				0);
			AddCommand(diffFile,			TEXT("Diff Project File"),					0);
			AddCommand(revertFile,			TEXT("Revert Project File"),				0);
			AddCommand(revertAllFiles,		TEXT("Revert All Project Files"),			0);
			AddCommand(revertAllOpenFiles,	TEXT("Revert All Open Project Files"),		0);
			AddCommand(showFileLog,			TEXT("Show Project File Log"),				0);
			AddCommand(showAllFileLog,		TEXT("Show All Project File Log"),			0);
			AddCommand(showAllOpenFileLog,	TEXT("Show All Open Project File Log"),		0);
			AddCommand(pushRep,				TEXT("Push Project To Repository"),			0);
			AddCommand(pullRep,				TEXT("Pull Project From Repository"),		0);
			break;

		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}

