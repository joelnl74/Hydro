#pragma once
#include "hypch.h"
#include "Hydro/Utils/PlatformUtils.h"
#include "Hydro/Core/Application.h"

#include <commdlg.h>
#include <ShlObj_core.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Hydro {

	float Time::GetTime()
	{
		return glfwGetTime();
	}

	static int CALLBACK BrowseFolderCallback(
		HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
	{
		if (uMsg == BFFM_INITIALIZED) {
			LPCTSTR path = reinterpret_cast<LPCTSTR>(lpData);
			::SendMessage(hwnd, BFFM_SETSELECTION, true, (LPARAM)path);
		}
		
		return 0;
	}

	std::string FileDialogs::OpenFile(const char* filter)
	{
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };
		CHAR currentDir[256] = { 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		if (GetCurrentDirectoryA(256, currentDir))
			ofn.lpstrInitialDir = currentDir;
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileNameA(&ofn) == TRUE)
			return ofn.lpstrFile;

		return std::string();

	}

	std::string FileDialogs::OpenDirectory()
	{
		TCHAR path[MAX_PATH];
		WCHAR currentDir[256] = { 0 };

		GetCurrentDirectoryW(256, currentDir);
		LPCTSTR lpctstrVar = currentDir;

		BROWSEINFOA bi = { 0 };
		bi.lpszTitle = ("Select folder for project creation");
		bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
		bi.lParam = reinterpret_cast<LPARAM>(lpctstrVar);
		bi.lpfn = BrowseFolderCallback;

		LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);

		if (pidl != 0)
		{
			//get the name of the folder and put it in path
			SHGetPathFromIDList(pidl, path);

			//free memory used
			IMalloc* imalloc = 0;
			
			if (SUCCEEDED(SHGetMalloc(&imalloc)))
			{
				imalloc->Free(pidl);
				imalloc->Release();
			}

			std::wstring wstrPath(path);
			std::string strPath(wstrPath.begin(), wstrPath.end());  

			return strPath;
		}

		return "";
	}

	std::string FileDialogs::SaveFile(const char* filter)
	{
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };
		CHAR currentDir[256] = { 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		if (GetCurrentDirectoryA(256, currentDir))
			ofn.lpstrInitialDir = currentDir;
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

		// Sets the default extension by extracting it from the filter
		ofn.lpstrDefExt = strchr(filter, '\0') + 1;

		if (GetSaveFileNameA(&ofn) == TRUE)
			return ofn.lpstrFile;

		return std::string();
	}

}
