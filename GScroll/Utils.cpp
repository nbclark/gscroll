#include "stdafx.h"
#include "utils.h"
#include <nled.h>
#include <tlhelp32.h>

void LedOn(int id)
{ 
    NLED_SETTINGS_INFO settings; 
    settings.LedNum= id; 
    settings.OffOnBlink= 1; 
    NLedSetDevice(NLED_SETTINGS_INFO_ID, &settings); 
}

void LedOff(int id)
{
    NLED_SETTINGS_INFO settings; 
    settings.LedNum= id; 
    settings.OffOnBlink= 0; 
    NLedSetDevice(NLED_SETTINGS_INFO_ID, &settings); 
}

BOOL LaunchProgram(LPCTSTR lpFile, LPCTSTR lpParameters)
{
    SHELLEXECUTEINFO shInfo;

    shInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    shInfo.dwHotKey = 0;
    shInfo.fMask = 0;
    shInfo.hIcon = NULL;
    shInfo.hInstApp = NULL;
    shInfo.hProcess = NULL;
    shInfo.lpDirectory = NULL;
    shInfo.lpIDList = NULL;
    shInfo.lpParameters = lpParameters;
    shInfo.lpVerb = NULL;
    shInfo.nShow = SW_SHOW;
    shInfo.lpFile = lpFile;

    return ShellExecuteEx(&shInfo);
}

HRESULT GetFromRegistry(HKEY rootKey, TCHAR* szRegistryKey, TCHAR* szRegValueName, WCHAR wzValue[MAX_PATH])
{
    HKEY    hKey;
    DWORD    dwSize;
    DWORD    dwType;
    HRESULT    hRes;

    hRes = RegOpenKeyExW(rootKey,szRegistryKey,0,KEY_READ,&hKey);
    if (hRes == ERROR_SUCCESS)
    {
        if (wzValue)
        {
            if (hRes == ERROR_SUCCESS)
            {
                dwSize = sizeof(WCHAR) * MAX_PATH;
                dwType = REG_SZ;

                hRes = RegQueryValueExW(hKey,szRegValueName,0,&dwType,(LPBYTE)wzValue,&dwSize);

                if (hRes != ERROR_SUCCESS)
                {
                    wzValue[0] = 0;
                }
            }
        }
        RegCloseKey(hKey);
    }

    return hRes;
}

HRESULT GetFromRegistry(TCHAR* szRegValueName, WCHAR wzValue[MAX_PATH])
{
    return GetFromRegistry(HKEY_CURRENT_USER, REG_GScrollKey, szRegValueName, wzValue);
}

HRESULT GetFromRegistry(HKEY rootKey, TCHAR* szRegistryKey, TCHAR* szRegValueName, DWORD *pdwValue)
{
    HKEY    hKey;
    DWORD    dwSize;
    DWORD    dwType;
    HRESULT    hRes;

    if (pdwValue)
    {
        *pdwValue = 0;
    }

    hRes = RegOpenKeyExW(rootKey,szRegistryKey,0,KEY_READ,&hKey);
    if (hRes == ERROR_SUCCESS)
    {
        if (pdwValue)
        {
            dwSize = sizeof(DWORD);

            if (hRes == ERROR_SUCCESS)
            {
                hRes = RegQueryValueExW(hKey,szRegValueName,0,&dwType,(LPBYTE)pdwValue,&dwSize);
            }
        }
        RegCloseKey(hKey);
    }

    return hRes;
}

HRESULT GetFromRegistry(TCHAR* szRegValueName, DWORD *pdwValue)
{
    return GetFromRegistry(HKEY_CURRENT_USER, REG_GScrollKey, szRegValueName, pdwValue);
}

HRESULT SetToRegistry(TCHAR* szRegValueName, DWORD dwValue)
{
    HKEY    hKey;
    DWORD    dwDisp;
    HRESULT    hRes;

    TCHAR    *szRegistryKey;

    szRegistryKey    = REG_GScrollKey;
    
    hRes = RegCreateKeyExW(HKEY_CURRENT_USER, szRegistryKey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, &dwDisp);
    if (hRes != ERROR_SUCCESS)
    {
        return hRes;
    }
    
    hRes = RegSetValueExW(hKey,szRegValueName,0,REG_DWORD,(LPBYTE)&dwValue,sizeof(DWORD));

    RegCloseKey(hKey);                    
    
    return hRes;
}

HRESULT SetStringToRegistry(TCHAR* szRegValueName, WCHAR* wzValue)
{
    HKEY    hKey;
    DWORD    dwDisp;
    HRESULT    hRes;

    TCHAR    *szRegistryKey;

    szRegistryKey    = REG_GScrollKey;
    
    hRes = RegCreateKeyExW(HKEY_CURRENT_USER, szRegistryKey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, &dwDisp);
    if (hRes != ERROR_SUCCESS)
    {
        return hRes;
    }

    int cCharacters =  wcslen(wzValue)+1;

    if (!wzValue)
    {
        wzValue = L"";
    }
    hRes = RegSetValueExW(hKey,szRegValueName,0,REG_SZ,(LPBYTE)wzValue, cCharacters * sizeof(WCHAR));

    RegCloseKey(hKey);                    
    
    return hRes;
}

WCHAR* TrimEndSpaces(WCHAR* szEXEPath2)
{
    if (szEXEPath2)
    {
        int iLen = wcslen(szEXEPath2) - 1;

        while (iLen >=0 && szEXEPath2[iLen] == L' ')
        {
            szEXEPath2[iLen] = L'\0';
        }
    }
    return szEXEPath2;
}

WCHAR* TrimSpaces(WCHAR* szEXEPath2)
{
    if (szEXEPath2)
    {
        if (szEXEPath2[0] == L'"')
        {
            int len = wcslen(szEXEPath2);
            szEXEPath2[len-1] = L'\0';
            szEXEPath2++;
        }
        else
        {
            WCHAR* szSpace = wcschr(szEXEPath2, L' ');

            if (szSpace)
            {
                szSpace[0] = L'\0';
            }
        }
    }
    return szEXEPath2;
}

bool FindLnkIcon(WCHAR szFoundPath[MAX_PATH], WCHAR* szPath, WCHAR* wzProcessName)
{
    bool bResult = false;
    WCHAR szFilteredPath[MAX_PATH];
    wcscpy(szFilteredPath, szPath);
    wcscat(szFilteredPath, L"*");
    WIN32_FIND_DATAW data;
    HANDLE hFile = FindFirstFile(szFilteredPath, &data);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				// recurse
				WCHAR szNewPath[MAX_PATH];
				wcscpy(szNewPath, szPath);
				wcscat(szNewPath, data.cFileName);
				wcscat(szNewPath, L"\\");

				bResult = bResult || FindLnkIcon(szFoundPath, szNewPath, wzProcessName);

				break;
			}
			else
			{
				WCHAR* wzFile = wcsstr(data.cFileName, wzProcessName);
				if (wzFile)
				{
					wcscpy(szFoundPath, szPath);
					wcscat(szFoundPath, data.cFileName);

					bResult = true;
					break;
				}
			}
		} while (FindNextFile(hFile, &data));
	}
	FindClose(hFile);

	return bResult;
}

HICON GetApplicationIcon(WCHAR* szFilteredPath, bool bSmall, WCHAR* wzExeFile, WCHAR* wzWndText, WCHAR* wzClassName)
{
    WCHAR szEXEPath[MAX_PATH];
    HICON hIcon = NULL;

    if (wzExeFile && wzClassName)
    {
        WCHAR wzRegPath[MAX_PATH];
        HKEY hKey;

        wcscpy(wzRegPath, L"Software\\HTC\\TaskManager\\NoIconProcess");

        if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, wzRegPath, 0,KEY_READ,&hKey))
        {
            HKEY hKeyProc;
            if (ERROR_SUCCESS == RegOpenKeyEx(hKey, wzExeFile, 0,KEY_READ,&hKeyProc))
            {
                HKEY hKeyClass;
                if (ERROR_SUCCESS == RegOpenKeyEx(hKeyProc, L"NoClass", 0,KEY_READ,&hKeyClass))
                {
                    RegCloseKey(hKeyClass);
                }
                else
                {
                    if (ERROR_SUCCESS == RegOpenKeyEx(hKeyProc, wzClassName, 0,KEY_READ,&hKeyClass))
                    {
                        WCHAR wzRegValue[MAX_PATH];
                        DWORD dwRegPath = MAX_PATH;
                        DWORD dwRegValue = MAX_PATH * sizeof(WCHAR);
                        if (ERROR_SUCCESS == RegEnumValue(hKeyClass, 0, wzRegPath, &dwRegPath, NULL, NULL, (LPBYTE)wzRegValue, &dwRegValue))
                        {
                            ExtractIconEx(wzRegPath, _wtoi(wzRegValue), NULL, &hIcon, 1);

                            if (hIcon)
                            {
                                return hIcon;
                            }
                        }
                        RegCloseKey(hKeyClass);
                    }
                }
                RegCloseKey(hKeyProc);
            }
            RegCloseKey(hKey);
        }
    }

    if (wzWndText)
    {
        if (SHGetSpecialFolderPath(NULL, szEXEPath, CSIDL_PROGRAMS, FALSE))
        {
            TrimEndSpaces(wzWndText);
            wcscat(szEXEPath, L"\\");
            if (FindLnkIcon(szFilteredPath, szEXEPath, wzWndText))
            {
                Sleep(0);
            }
        }
    }

    WCHAR* wzExt = wcsrchr(szFilteredPath, L'.');

    if (wzExt && 0 == wcsicmp(wzExt, L".lnk"))
    {
        SHGetShortcutTarget(szFilteredPath, szEXEPath, MAX_PATH);

        WCHAR* szEXEPath2 = szEXEPath;
        szEXEPath2 = TrimSpaces(szEXEPath2);

        int iIndex = 0;
        if (!hIcon)
        {
            FILE* hFile = _wfopen(szFilteredPath, L"r");

            if (hFile)
            {
                char szLnkData[500];
                memset(szLnkData, 0, sizeof(szLnkData));
                fread(szLnkData, sizeof(char), sizeof(szLnkData), hFile);
                fclose(hFile);

                char* szQuestion = strchr(szLnkData, '?');

                if (szQuestion)
                {
                    szQuestion++;
                    char* szComma = strchr(szLnkData, ',');

                    if (szComma)
                    {
                        szComma[0] = '\0';
                        szComma++;

                        int cCharacters =  strlen(szQuestion)+1;
                        int iIcon = atoi(szComma);
                        WCHAR* pszW = (LPOLESTR)CoTaskMemAlloc(cCharacters*2);
                        MultiByteToWideChar(CP_ACP, 0, szQuestion, cCharacters, pszW, cCharacters);
                        ExtractIconEx(pszW, iIcon, (bSmall) ? NULL : &hIcon, (!bSmall) ? NULL : &hIcon, 1);
                        CoTaskMemFree(pszW);
                    }
                }
                else
                {
                    char* szColon = strstr(szLnkData, "#:");

                    if (szColon)
                    {
                        szColon++;

                        char* szNewLine = strchr(szLnkData, '\r');
                        if (szNewLine)
                        {
                            szNewLine[0] = '\0';
                        }
                        szNewLine = strchr(szLnkData, '\n');
                        if (szNewLine)
                        {
                            szNewLine[0] = '\0';
                        }

                        WCHAR wzRegPath[MAX_PATH];
                        int cCharacters =  strlen(szColon)+1;
                        WCHAR* pszW = (LPOLESTR)CoTaskMemAlloc(cCharacters*2);
                        MultiByteToWideChar(CP_ACP, 0, szColon, cCharacters, pszW, cCharacters);
                        
                        wcscpy(wzRegPath, L"Software\\Microsoft\\Shell\\Rai\\");
                        wcscat(wzRegPath, pszW);

                        CoTaskMemFree(pszW);

                        HKEY hKey;
                        
                        if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, wzRegPath, 0,KEY_READ,&hKey))
                        {
                            DWORD dwSize;
                            DWORD dwType;
                            dwSize = sizeof(wzRegPath);
                            dwType = REG_SZ;

                            RegQueryValueEx(hKey, L"1",0,&dwType,(LPBYTE)wzRegPath,&dwSize);

                            WCHAR* wzRegPath2 = TrimSpaces(wzRegPath);
                            ExtractIconEx(wzRegPath2, 0, (bSmall) ? NULL : &hIcon, (!bSmall) ? NULL : &hIcon, 1);
                        }
                    }
                }
            }
        }

        if (!hIcon)
        {
            ExtractIconEx(szEXEPath2, 0, (bSmall) ? NULL : &hIcon, (!bSmall) ? NULL : &hIcon, 1);
        }
    }
    else
    {
        ExtractIconEx(szFilteredPath, 0, (bSmall) ? NULL : &hIcon, (!bSmall) ? NULL : &hIcon, 1);
    }

    return hIcon;
}

bool RectContains(RECT* pRect, int xPos, int yPos)
{
    if (!pRect)
    {
        return false;
    }
    return ((pRect->left <= xPos) && (pRect->right >= xPos) && (pRect->top <= yPos) && (pRect->bottom >= yPos));
}

bool GetProcessName(DWORD dwProcessId, WCHAR wzProcessName[MAX_PATH])
{
    bool bFound = false;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS, 0);
    PROCESSENTRY32 pe = {0};
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (hSnapshot != INVALID_HANDLE_VALUE)
    {
        if (Process32First(hSnapshot, &pe))
        {
            do
            {
                if (pe.th32ProcessID == dwProcessId)
                {
                    wcscpy(wzProcessName, pe.szExeFile);
                    bFound = true;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe));
        }
        CloseToolhelp32Snapshot(hSnapshot);
    }

    return bFound;
}