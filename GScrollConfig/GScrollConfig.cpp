#include <aygshell.h>
#include <shellapi.h>
#include <windows.h>
#include <commctrl.h>   // includes the common control header
#include <string.h>   // includes the common control header
#include "DeviceResolutionAware.h"
#include "GScrollConfig.h"
#include "..\GScroll\Utils.h"
#include "..\GScroll\Sensor.h"
#include "..\GScroll\RPN.h"

#define NUM_APPLETS        1
#define    ID_POWEROFFMODE    0
#define    ID_POWERONMODE    1
#define MAX_APPLICATIONS 500
typedef struct tagApplets
{
    int icon;           // icon resource identifier
    int namestring;     // name-string resource identifier
    int descstring;     // description-string resource identifier
} APPLETS;

const APPLETS SystemApplets[] =
{
    {APPLET_ICON, APPLET_NAME, APPLET_DESC}
    // add more struct members here if supporting more than on applet
};

HINSTANCE    g_hInstance    = NULL;

////////////////////////////////////////////////////////
//    DllMain
//
////////////////////////////////////////////////////////
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
                     )
{
    if (DLL_PROCESS_ATTACH == ul_reason_for_call)
        g_hInstance = (HINSTANCE)hModule;

    return TRUE;
}


////////////////////////////////////////////////////////
//    Do initialization, eg memory allocations, etc.
//  return TRUE if all OK and FALSE if applet shouldn't
//  be loaded
//
////////////////////////////////////////////////////////
BOOL InitApplet(HWND hwndParent)
{
    return TRUE;
}


////////////////////////////////////////////////////////
//    Do cleanup here
//  
////////////////////////////////////////////////////////
void TermApplet()
{
    return;
}

////////////////////////////////////////////////////////
//    This is the entry point called by ctlpnl.exe
//  
////////////////////////////////////////////////////////
extern "C"
__declspec(dllexport)
LONG WINAPI CPlApplet(HWND hwndCPL, UINT uMsg, LONG lParam1, LONG lParam2)
{
    static int        iInitCount = 0;
    int                iApplet;
    static DWORD    dwThread = 0;

    switch (uMsg)
    {
        // First message sent. It is sent only once to
        // allow the dll to initialize it's applet(s)
        case CPL_INIT:
            if (!iInitCount)
            {
                if (!InitApplet(hwndCPL))
                    return FALSE;
            }
            iInitCount++;
            return TRUE;
            
        // Second message sent. Return the count of applets supported
        // by this dll
        case CPL_GETCOUNT:
            // Return the number of applets we support
            return (LONG)((sizeof(SystemApplets))/(sizeof(SystemApplets[0])));

        // Third message sent. Sent once for each applet supported by this dll.
        // The lParam1 contains the number that indicates which applet this is
        // for, from 0 to 1 less than the count of applets.
        // lParam2 is a NEWCPLINFO that should be filled with information about
        // this applet before returning
        case CPL_NEWINQUIRE:
            {
                LPNEWCPLINFO    lpNewCPlInfo;

                lpNewCPlInfo                = (LPNEWCPLINFO)lParam2;
                iApplet                        = (int)lParam1;
                lpNewCPlInfo->dwSize        = (DWORD)sizeof(NEWCPLINFO);
                lpNewCPlInfo->dwFlags        = 0;
                lpNewCPlInfo->dwHelpContext    = 0;
                lpNewCPlInfo->lData            = SystemApplets[iApplet].icon;
                lpNewCPlInfo->hIcon            = LoadIcon(g_hInstance,(LPCTSTR)MAKEINTRESOURCE(SystemApplets[iApplet].icon));
                lpNewCPlInfo->szHelpFile[0]    = '\0';

                LoadString(g_hInstance,SystemApplets[iApplet].namestring,lpNewCPlInfo->szName,32);
                LoadString(g_hInstance,SystemApplets[iApplet].descstring,lpNewCPlInfo->szInfo,64);
            }

            break;

        // This is sent whenever the user clicks an icon in Settings for one of
        // the applets supported by this dll. lParam1 contains the number indicating
        // which applet. Return 0 if applet successfully launched, non-zero otherwise
        case CPL_DBLCLK:
            {
                TCHAR szStatic[MAX_PATH], szStartup[MAX_PATH], szSettings[MAX_PATH];
				TCHAR* szSettingsPath = szSettings;
                if (SHGetSpecialFolderPath(NULL, szStartup, CSIDL_STARTUP, FALSE))
                {
                    _tcscat(szStartup, L"\\GScroll.lnk");
                    if (SHGetShortcutTarget(szStartup, szStatic, MAX_PATH))
                    {
                        StringCchCopy(szSettings, sizeof(szSettings), szStatic);
                        WCHAR* wzLastDot = wcsrchr(szSettings, L'\\');
                        if (wzLastDot)
                        {
                            wzLastDot[0] = '\0';
                        }
                        StringCchCat(szSettings, sizeof(szSettings), L"\\MobileSRC.GScrollSettings.exe");
						if (szSettings[0] == L'"')
						{
							szSettingsPath++;
						}
                    }
                }

                iApplet = (UINT)lParam1;

                PROCESS_INFORMATION pi;
                ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

                if (CreateProcess(szSettingsPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi))
                {
                    WaitForSingleObject(pi.hProcess, INFINITE);
                }

                HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLSETTINGS);
                if (hEvent)
                {
                    SetEvent(hEvent);
                    CloseHandle(hEvent);
                }
                else
                {
                    LaunchProgram(szStatic, NULL);
                }
            }
            return 1;
            
        // Sent once per applet, before CPL_EXIT
        case CPL_STOP:
            break;
            
        // Sent once before the dll is unloaded
        case CPL_EXIT:
            iInitCount--;
            if (!iInitCount)
                TermApplet();
            break;
            
        default:
            break;
    }

    return 0;
}