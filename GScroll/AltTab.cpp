#include "stdafx.h"
#include <windows.h>
#include <soundfile.h>
#include <time.h>
#include <notify.h>
#include <atltime.h>
#include <regext.h>
#include <tlhelp32.h>
#include <cpl.h>   // includes the common control header
#include <Imm.h>
#include "sensor.h"
#include "utils.h"
#include "resource.h"
#include "AltTab.h"
#include "TouchHandler.h"

#define WM_SHOWALTTAB      (WM_USER + 1)
#define WM_HIDEALTTAB      (WM_USER + 2)
#define WM_SELECTALTTAB    (WM_USER + 3)

void ShowError(WCHAR* wzMessage, HRESULT hRet)
{
	WCHAR wzError[50];
	_ltow(hRet, wzError, 16);

	MessageBox(NULL, wzMessage, wzError, MB_OK);
}

CAltTab::CAltTab(void)
{
    InitializeCriticalSection(&g_imgCriticalSection);
    g_hInstance = GetModuleHandle(NULL);

    // these are appswitch variables
    g_dwAppSwitchAutoClose = 0;
    g_bAppSwitchEnableShrinkAnimation = true;
    g_bAppSwitchEnableScrollAnimation = true;
    g_bAppSwitchEnableThumbnails = true;

    LoadSettings();

    memset(&g_wndClassAltTab, 0, sizeof(WNDCLASS));
    g_wndClassAltTab.lpszClassName = L"mobilesrcAltTab";
    g_wndClassAltTab.lpfnWndProc = &AltTabProc;
    g_wndClassAltTab.style = 0;
    g_wndClassAltTab.hInstance = g_hInstance;
    g_wndClassAltTab.hIcon = NULL;
    ATOM a = RegisterClassW(&g_wndClassAltTab);

    g_hwndAltTab = CreateWindowExW
    (
        WS_EX_TOPMOST,
        g_wndClassAltTab.lpszClassName,
        L"",
        WS_POPUP | WS_CLIPSIBLINGS,
        0,g_sipInfo.rcVisibleDesktop.top,g_width,g_height, NULL, NULL, g_hInstance, NULL
    );

    RECT rect;
    GetWindowRect(g_hwndAltTab, &rect);

    SetWindowLong(g_hwndAltTab, GWL_USERDATA, (LONG)this);

    DDCAPS ddCaps;
    DDCAPS ddHelCaps;
    DDSURFACEDESC ddsd;
    HRESULT hRet;

    hRet = DirectDrawCreate(NULL, &g_pDD, NULL);

    if (hRet != DD_OK)
    {
        // we failed
		ShowError(L"Failed DirectDrawCreate", hRet);
    }

    // Get exclusive mode
    hRet = g_pDD->SetCooperativeLevel(g_hwndAltTab, DDSCL_NORMAL );
    if (hRet != DD_OK)
    {
        // we failed
		ShowError(L"Failed g_pDD->SetCooperativeLevel", hRet);
    }

    hRet = g_pDD->GetCaps(&ddCaps, &ddHelCaps);
    if (hRet != DD_OK)
    {
        // we failed
		ShowError(L"Failed g_pDD->GetCaps", hRet);
    }

    if (!(ddCaps.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)) 
    {
        // we failed
		ShowError(L"Failed ddCaps.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE", hRet);
    }

    int maxDimension = max(g_width, g_height);

    // Create the primary surface with 1 back buffer
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    hRet = g_pDD->CreateSurface(&ddsd, &g_pDDSPrimary, NULL);
    if (hRet != DD_OK)
    {
        // we failed
		ShowError(L"Failed g_pDD->CreateSurface g_pDDSPrimary", hRet);
    }

    // Create a offscreen bitmap.
    ddsd.dwFlags = DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.dwHeight = maxDimension;
    ddsd.dwWidth = maxDimension;
    hRet = g_pDD->CreateSurface(&ddsd, &g_pDDSBackground, NULL);
    if (hRet != DD_OK)
    {
        // we failed
		ShowError(L"Failed g_pDD->CreateSurface g_pDDSBackground", hRet);
    }

    hRet = g_pDD->CreateSurface(&ddsd, &g_pDDSBuffer, NULL);
    if (hRet != DD_OK)
    {
        // we failed
		ShowError(L"Failed g_pDD->CreateSurface g_pDDSBuffer", hRet);
    }
    
    hRet = g_pDD->CreateSurface(&ddsd, &g_pDDSAnimate, NULL);
    if (hRet != DD_OK)
    {
        // we failed
		ShowError(L"Failed g_pDD->CreateSurface g_pDDSAnimate", hRet);
    }

    int maxLabelDimension = max(max(g_buttonWidth, g_labelWidth), max(g_buttonHeight, g_labelHeight));

    hRet = g_pDD->CreateClipper(0, &g_pDDClipper, NULL);
    if (hRet != DD_OK)
    {
        // we failed
		ShowError(L"Failed g_pDD->CreateClipper", hRet);
    }

    hRet=g_pDDClipper->SetHWnd(0, g_hwndAltTab);
    if (hRet != DD_OK)
    {
        // we failed
		ShowError(L"Failed g_pDDClipper->SetHWnd", hRet);
    }

    hRet=g_pDDSPrimary->SetClipper(g_pDDClipper);
    if (hRet != DD_OK)
    {
        // we failed
		ShowError(L"Failed g_pDDSPrimary->SetClipper", hRet);
    }

    g_hThumbThread = NULL;
    hFinishedEvent = NULL;
}

void CAltTab::Initialize()
{
    g_hThumbThread = CreateThread(NULL, 0, &CAltTab::PollThumbnails, this, 0, 0);
    hFinishedEvent = CreateEvent(NULL, FALSE, FALSE, L"CAltTab");
}

CAltTab::~CAltTab(void)
{
    ReleaseAllObjects();
    CleanupAppSwitchSettings();
    CleanupThumbnails();

    CloseHandle(hFinishedEvent);
    if (g_hThumbThread)
    {
        TerminateThread(g_hThumbThread, 0);
        CloseHandle(g_hThumbThread);
        g_hThumbThread = NULL;
    }
    DeleteCriticalSection(&g_imgCriticalSection);
}

void CAltTab::LoadSettings()
{
    LoadAppSwitchDisplaySettings();
    LoadAppSwitchSettings();
}

void CAltTab::CleanupThumbnails()
{
    PROCIMGMAP::iterator iter = g_imgMap.begin();
    while (iter != g_imgMap.end())
    {
        if (iter->second)
        {
            DeleteObject(iter->second);
            iter->second = NULL;
        }

        iter++;
    }
    PROCICONMAP::iterator iterIcon = g_iconMap.begin();
    while (iterIcon != g_iconMap.end())
    {
        if (iterIcon->second)
        {
            DestroyIcon(iterIcon->second);
            iterIcon->second = NULL;
        }

        iterIcon++;
    }
    g_iconMap.clear();
    g_imgMap.clear();
    g_procMap.clear();
    g_timeMap.clear();
}

void CAltTab::LoadAppSwitchDisplaySettings()
{
    ZeroMemory(&g_sipInfo, sizeof(g_sipInfo));
    g_sipInfo.cbSize = sizeof(g_sipInfo);
    SHSipInfo(SPI_GETSIPINFO, 0, &g_sipInfo, 0);

    g_width = GetSystemMetrics(SM_CXSCREEN);
    g_height = GetSystemMetrics(SM_CYSCREEN);
    g_bigThumbHeight = (int)(g_height / 2.5);
    g_bigThumbWidth = (int)(g_width / 2.5);
    g_bigThumbTop = (g_height - g_bigThumbHeight) / 2;
    g_bigThumbLeft = (g_width - g_bigThumbWidth) / 2;
    g_smallThumbHeight = g_height / 4;
    g_smallThumbWidth = g_width / 4;

    int labelTop = g_height / 24;

    g_labelWidth = g_width - 80;
    g_labelHeight = g_height / 10;
    g_labelSpacing = g_height / 25;
    g_labelLeft = (g_width - g_labelWidth) >> 1;
    g_labelFont = g_height / 16;
    g_buttonWidth = g_width - 30;
    g_buttonHeight = g_height / 9;
    g_buttonSpacing = g_height / 25;
    g_buttonLeft = (g_width - g_buttonWidth) >> 1;
    g_buttonFont = g_height / 21;
    g_dwAnimateInterval = 50;
    g_dwDeceleration = 75;

    g_vertShift = g_bigThumbTop - (labelTop + g_labelSpacing + g_labelHeight);
    g_bigThumbTop -= g_vertShift;
    g_bigThumbMid = g_bigThumbTop + (g_bigThumbHeight / 2);
    g_labelTop = g_bigThumbTop - g_labelSpacing - g_labelHeight;
    g_buttonTop = g_bigThumbTop + g_bigThumbHeight + g_buttonSpacing;
    g_height = (g_height - g_sipInfo.rcVisibleDesktop.top);
}

void CAltTab::LoadAppSwitchSettings()
{
    CleanupAppSwitchSettings();

    HKEY hKey;
    DWORD dwSize = sizeof(DWORD);
    DWORD dwType = 0, dwValue = 0;
    HRESULT hRes = RegOpenKeyEx(HKEY_CURRENT_USER, REG_GScrollKey, 0, KEY_READ, &hKey);
    if (hRes == ERROR_SUCCESS)
    {
        HKEY hKeyAppSwitch;
        hRes = RegOpenKeyEx(hKey, REG_AppSwitchKey, 0, KEY_READ | KEY_ENUMERATE_SUB_KEYS, &hKeyAppSwitch);
        if (hRes == ERROR_SUCCESS)
        {
            // Load stuff here
            RegQueryValueExW(hKeyAppSwitch, REG_AppSwitch_AutoCloseDelay, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
            this->g_dwAppSwitchAutoClose = dwValue;
            RegQueryValueExW(hKeyAppSwitch, REG_AppSwitch_EnableShrinkAnimation, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
            this->g_bAppSwitchEnableShrinkAnimation = (dwValue != 0);
            RegQueryValueExW(hKeyAppSwitch, REG_AppSwitch_EnableScrollAnimation, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
            this->g_bAppSwitchEnableScrollAnimation = (dwValue != 0);
            RegQueryValueExW(hKeyAppSwitch, REG_AppSwitch_EnableThumbnails, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
            this->g_bAppSwitchEnableThumbnails = (dwValue != 0);

            HKEY hKeyProcesses;
            hRes = RegOpenKeyEx(hKeyAppSwitch, REG_AppSwitch_ProcessKey, 0, KEY_READ | KEY_ENUMERATE_SUB_KEYS, &hKeyProcesses);
            if (hRes == ERROR_SUCCESS)
            {
                int i = 0;

                HKEY hKeyProcess;

                while (true)
                {
                    bool bSucceeded = false;
                    AppSwitchProcess* pAppProcess = new AppSwitchProcess();
                    WCHAR* wzProcName = new WCHAR[200];
                    DWORD dwProcName = 200;
                    hRes = RegEnumKeyEx(hKeyProcesses, i++, wzProcName, &dwProcName, 0, 0, 0, 0);

                    if (hRes == ERROR_SUCCESS)
                    {
                        hRes = RegOpenKeyEx(hKeyProcesses, wzProcName, 0, KEY_READ | KEY_ENUMERATE_SUB_KEYS, &hKeyProcess);

                        if (hRes == ERROR_SUCCESS)
                        {
                            dwSize = sizeof(DWORD);
                            // we have our hKey -- it's either the default, or per process

                            RegQueryValueExW(hKeyProcess, REG_AppSwitch_Process_Ignore, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
                            pAppProcess->bIgnore = (dwValue != 0);
                            RegQueryValueExW(hKeyProcess, REG_AppSwitch_Process_CanClose, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
                            pAppProcess->bCanClose = (dwValue != 0);
                            RegQueryValueExW(hKeyProcess, REG_AppSwitch_Process_CanMinimize, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
                            pAppProcess->bCanMinimize = (dwValue != 0);

                            g_wzProcessList.insert(pair<WCHAR*, AppSwitchProcess*>(wzProcName, pAppProcess));
                            RegCloseKey(hKeyProcess);

                            bSucceeded = true;
                        }
                    }
                    if (!bSucceeded)
                    {
                        delete[] wzProcName;
                        delete pAppProcess;
                        break;
                    }
                }
                RegCloseKey(hKeyProcesses);
            }
            RegCloseKey(hKeyAppSwitch);
        }
        RegCloseKey(hKey);
    }
}


void CAltTab::CleanupAppSwitchSettings()
{
    hash_map<WCHAR*, AppSwitchProcess*>::iterator iter = g_wzProcessList.begin();

    while (iter != g_wzProcessList.end())
    {
        if (iter->first)
        {
            delete[] iter->first;
        }
        if (iter->second)
        {
            delete iter->second;
        }

        iter++;
    }
    g_wzProcessList.clear();
}

bool CAltTab::CloseProcess(HWND hwndTop, WCHAR* wzProcess, WCHAR* wzClass)
{
    if (this->g_wzProcessList.find(wzProcess) != this->g_wzProcessList.end())
    {
        if (!this->g_wzProcessList[wzProcess]->bCanClose)
        {
            MinimizeProcess(hwndTop, wzProcess, wzClass);
            return false;
        }
    }
    if (this->g_wzProcessList.find(wzClass) != this->g_wzProcessList.end())
    {
        if (!this->g_wzProcessList[wzClass]->bCanClose)
        {
            MinimizeProcess(hwndTop, wzProcess, wzClass);
            return false;
        }
    }
    // Close the process here
    DWORD dwProcessId;
    GetWindowThreadProcessId(hwndTop, &dwProcessId);

    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, dwProcessId);
    if (hProc)
    {
        TerminateProcess(hProc, 0);
        CloseHandle(hProc);
        return true;
    }
    return false;
}

bool CAltTab::MinimizeProcess(HWND hwndTop, WCHAR* wzProcess, WCHAR* wzClass)
{
    if (this->g_wzProcessList.find(wzProcess) != this->g_wzProcessList.end())
    {
        if (!this->g_wzProcessList[wzProcess]->bCanMinimize)
        {
            return false;
        }
    }
    if (this->g_wzProcessList.find(wzClass) != this->g_wzProcessList.end())
    {
        if (!this->g_wzProcessList[wzClass]->bCanMinimize)
        {
            return false;
        }
    }
    // Minimize the process here
    ShowWindow(hwndTop, SW_MINIMIZE);
    return true;
}

void CAltTab::Show(int iDir, void* pTouchHandler)
{
    MSG msg;
    g_pTouchHandler = pTouchHandler;
    g_iDir = iDir;

    // Show the window here
    LoadAppSwitchDisplaySettings();
    SHSipPreference(GetForegroundWindow(), SIP_FORCEDOWN);
    SHSipPreference(g_hwndAltTab, SIP_FORCEDOWN);
    SendMessage(g_hwndAltTab, WM_SHOWALTTAB, 0, iDir);
    while (WAIT_TIMEOUT == WaitForSingleObject(hFinishedEvent, 5))
    {
        while ( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) )
        {
            GetMessage( &msg, NULL, 0, 0 );
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
		if (GetForegroundWindow() != g_hwndAltTab)
		{
            ShowWindow(g_hwndAltTab, SW_HIDE);
            SendMessage(g_hwndAltTab, WM_HIDEALTTAB, 0, 0);
			SetFocus(NULL);
		}
    }
    Sleep(0);
}

void CAltTab::ReleaseAllObjects(void)
{
    if (g_pDDSBuffer != NULL)
    {
        g_pDDSBuffer->Release();
        g_pDDSBuffer = NULL;
    }
    if (g_pDDSPrimary != NULL)
    {
        g_pDDSPrimary->Release();
        g_pDDSPrimary = NULL;
    }
    if (g_pDDSBackground != NULL)
    {
        g_pDDSBackground->Release();
        g_pDDSBackground = NULL;
    }
    if (g_pDDSAnimate != NULL)
    {
        g_pDDSAnimate->Release();
        g_pDDSAnimate = NULL;
    }
    if (g_pDD != NULL)
    {
        g_pDD->Release();
        g_pDD = NULL;
    }
}

HRESULT CAltTab::Flip(HWND hwnd)
{
    HRESULT ddrval;
    RECT rcRectSrc;
    RECT rcRectDest;
    POINT p;

    // if we're windowed do the blit, else just Flip
    if (true)
    {
        // first we need to figure out where on the primary surface our window lives
        p.x = 0; p.y = 0;
        ClientToScreen(hwnd, &p);
        GetClientRect(hwnd, &rcRectDest);
        OffsetRect(&rcRectDest, p.x, p.y);
        GetClientRect(hwnd, &rcRectSrc);

        ddrval = g_pDDSPrimary->Blt( &rcRectDest, g_pDDSBuffer, &rcRectSrc, 0, NULL);

    }
    return ddrval;
}

HRESULT CAltTab::RestoreAll()
{
    HRESULT hRet;

    hRet = g_pDDSPrimary->Restore();
    if (hRet == DD_OK)
    {
        hRet = g_pDDSBackground->Restore();
        if (hRet == DD_OK)
        {
            hRet = g_pDDSAnimate->Restore();
            if (hRet == DD_OK)
            {
                InitSurfaces();
            }
			else
			{
				// we failed
				ShowError(L"Failed g_pDDSAnimate->Restore", hRet);
			}
        }
		else
		{
			// we failed
			ShowError(L"Failed g_pDDSBackground->Restore", hRet);
		}
    }
	else
    {
        // we failed
		ShowError(L"Failed g_pDDSPrimary->Restore", hRet);
    }
    return hRet;
}

BOOL CAltTab::InitSurfaces()
{
    // Let's take a screenshot here
    HDC hdcDesk = GetDC(HWND_DESKTOP);
                    
    HDC hdcAnimate = CreateCompatibleDC(hdcDesk);
    HBITMAP hAnimate = CreateCompatibleBitmap(hdcDesk, g_width, g_height);
    HBITMAP hbmpAnimateOld = (HBITMAP)SelectObject(hdcAnimate, hAnimate);

    BitBlt(hdcAnimate, 0, 0, g_width, g_height, hdcDesk, g_sipInfo.rcVisibleDesktop.left, g_sipInfo.rcVisibleDesktop.top, SRCCOPY);

    DeleteDC(hdcDesk);

    // TODO: Get the back handle here
    HRESULT hr;
    HDC hdc;
    if ((hr = g_pDDSBackground->GetDC(&hdc)) == DD_OK)
    {
        RECT windowRect;
        windowRect.left = windowRect.top = 0;
        windowRect.right = g_width;
        windowRect.bottom = g_height;

        FillRect(hdc, &windowRect, (HBRUSH)GetStockObject(BLACK_BRUSH));

        BLENDFUNCTION blendFunc;
        blendFunc.AlphaFormat = 0;
        blendFunc.BlendFlags = 0;
        blendFunc.BlendOp = AC_SRC_OVER;
        blendFunc.SourceConstantAlpha = 50;

        AlphaBlend(hdc, 0, 0, g_width, g_height, hdcAnimate, 0, 0, g_width, g_height, blendFunc);

        g_pDDSBackground->ReleaseDC(hdc);
    }
	else
	{
		ShowError(L"Failed g_pDDSBackground->GetDC", hr);
	}
    if ((hr = g_pDDSAnimate->GetDC(&hdc)) == DD_OK)
    {
        BitBlt(hdc, 0, 0, g_width, g_height, hdcAnimate, 0, 0, SRCCOPY);

        g_pDDSAnimate->ReleaseDC(hdc);
    }
	else
	{
		ShowError(L"Failed g_pDDSAnimate->GetDC", hr);
	}

    SelectObject(hdcAnimate, hbmpAnimateOld);
    DeleteObject(hAnimate);
    DeleteDC(hdcAnimate);

    return TRUE;
}

inline void DrawThumbnail(HDC hdc, HDC hdcThumb, HBITMAP hbmpWindow, HICON hIconWindow, HWND hwndParent, int thumbLeft, int thumbTop, int thumbWidth, int thumbHeight, int padding, DWORD dwColor, byte dwOpaque, bool bDrawIcon)
{
    BLENDFUNCTION blendFunc;
    blendFunc.AlphaFormat = 0;
    blendFunc.BlendFlags = 0;
    blendFunc.BlendOp = AC_SRC_OVER;
    blendFunc.SourceConstantAlpha = dwOpaque;

    RECT rect;
    BITMAP bitmap;
    int iconWidth = GetSystemMetrics(SM_CXICON);
    int iconHeight = GetSystemMetrics(SM_CYICON);

    thumbLeft += padding*2;
    thumbTop += padding*2;
    thumbWidth -= padding*4;
    thumbHeight -= padding*4;

    rect.left = thumbLeft - padding;
    rect.right = thumbLeft + thumbWidth + padding;
    rect.top = thumbTop - padding;
    rect.bottom = thumbTop + thumbHeight + padding;
    
    HGDIOBJ hBrushOld = SelectObject(hdc, (HBRUSH)GetStockObject(dwColor));

    if (hbmpWindow)
    {
        Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    }
    else
    {
        RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 10, 10);
    }
    SelectObject(hdc, hBrushOld);

    if (hbmpWindow)
    {
        ::GetObject(hbmpWindow, sizeof(BITMAP), &bitmap);
        HGDIOBJ hbmpOld = SelectObject(hdcThumb, hbmpWindow);
        BOOL bValue = AlphaBlend(hdc, thumbLeft, thumbTop, thumbWidth, thumbHeight, hdcThumb, 0, 0, bitmap.bmWidth, bitmap.bmHeight, blendFunc);
        SelectObject(hdcThumb, hbmpOld);
    }
    if (!hbmpWindow || bDrawIcon)
    {
        bool bNeedsDestroy = false;
        if (!hIconWindow)
        {
            hIconWindow = LoadIconW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
            bNeedsDestroy = true;
        }

        if (!hbmpWindow)
        {
            int borderWidth = 2;
            rect.left += borderWidth;
            rect.top += borderWidth;
            rect.right -= borderWidth;
            rect.bottom -= borderWidth;
            
            hBrushOld = SelectObject(hdc, (HBRUSH)GetStockObject(dwColor ^ 4));
            RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 10, 10);
            SelectObject(hdc, hBrushOld);
        }

        DrawIcon(hdc, rect.left + (rect.right - rect.left - iconWidth) / 2, rect.top + (rect.bottom - rect.top - iconHeight) / 2, hIconWindow);

        if (bNeedsDestroy)
        {
            DestroyIcon(hIconWindow);
        }
    }
}

node* CAltTab::BuildNodeList(int* piNodeLen)
{
    node* pHeadNode = NULL;
    node* pCurr = NULL;
    uint uiMaxProcs = 20;

    DWORD* foundProcs = new DWORD[uiMaxProcs];

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS, 0);
    PROCESSENTRY32 pe = {0};
    pe.dwSize = sizeof(PROCESSENTRY32);

    CTouchHandler* pTouchHandler = (CTouchHandler*)this->g_pTouchHandler;

    int count = 0;
    if (hSnapshot != INVALID_HANDLE_VALUE)
    {
        HWND hwndCurrent = GetForegroundWindow();
        while (NULL != hwndCurrent)
        {
            long lStyle = GetWindowLong(hwndCurrent, GWL_STYLE);
            if (0 == count || (0 == (WS_MINIMIZEBOX & lStyle) && IsWindowVisible(hwndCurrent) && GetWindowTextLength(hwndCurrent) > 0))
            {
                DWORD dwProcessId = 0;
                GetWindowThreadProcessId(hwndCurrent, &dwProcessId);
                if (Process32First(hSnapshot, &pe))
                {
                    do
                    {
                        if (pe.th32ProcessID == dwProcessId)
                        {
                            bool bFound = false;
                            for (int i = 0; i < count; ++i)
                            {
                                if (foundProcs[i] == dwProcessId)
                                {
                                    bFound = true;
                                    break;
                                }
                            }
                            if (bFound)
                            {
                                break;
                            }

                            if (count > 0)
                            {
                                if (this->g_wzProcessList.find(pe.szExeFile) != this->g_wzProcessList.end())
                                {
                                    if (this->g_wzProcessList[pe.szExeFile]->bIgnore)
                                    {
                                        break;
                                    }
                                }
                            }

                            node* pTemp = new node();

                            if (pHeadNode == NULL)
                            {
                                pHeadNode = pTemp;
                            }
                            GetWindowText(hwndCurrent, pTemp->wzText, 20);
                            pTemp->hWnd = hwndCurrent;
                            pTemp->dwProcessId = dwProcessId;
                            pTemp->prev = pCurr;
                            pTemp->next = NULL;

                            EnterCriticalSection(&g_imgCriticalSection);
                            if (g_imgMap.find(dwProcessId) != g_imgMap.end())
                            {
                                pTemp->hBmp = g_imgMap[dwProcessId];
                                pTemp->hIcon = NULL;
                            }
							bool bFoundIcon = false;
                            if (g_iconMap.find(dwProcessId) != g_iconMap.end())
                            {
                                pTemp->hIcon = g_iconMap[dwProcessId];
								bFoundIcon = true;
                            }
							if (!bFoundIcon && !pTemp->hIcon)
							{
								WCHAR wzClassName[MAX_PATH], wzWndText[MAX_PATH];
								GetClassName(hwndCurrent, wzClassName, MAX_PATH);
								GetWindowText(hwndCurrent, wzWndText, MAX_PATH);

								HMODULE hProcess = (HMODULE)OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pe.th32ProcessID);
								WCHAR wzExe[MAX_PATH];
								GetModuleFileName(hProcess, wzExe, MAX_PATH);
								CloseHandle(hProcess);

								pTemp->hIcon = GetApplicationIcon(wzExe, false, pe.szExeFile, wzWndText, wzClassName);
								g_iconMap.insert(pair<DWORD, HICON>(pe.th32ProcessID, pTemp->hIcon));
							}
                            LeaveCriticalSection(&g_imgCriticalSection);

                            if (pCurr)
                            {
                                pCurr->next = pTemp;
                            }
                            pCurr = pTemp;

                            foundProcs[count] = dwProcessId;

                            count++;
                            (*piNodeLen)++;
                            break;
                        }
                    } while (Process32Next(hSnapshot, &pe));
                }
                if (count == uiMaxProcs)
                {
                    break;
                }
            }
            hwndCurrent = GetWindow(hwndCurrent, GW_HWNDNEXT);
        }
        CloseToolhelp32Snapshot(hSnapshot);
    }

    // at the end we need to take the last one and tie it to the head

    if (pCurr)
    {
        pCurr->next = pHeadNode;
        pHeadNode->prev = pCurr;
    }

    delete[] foundProcs;

    return pHeadNode;
}

void CAltTab::EnsureCapture(HWND hwndCurrent, bool bForce)
{
    static DWORD dwLastUpdate = 0;
    if (!g_bAppSwitchEnableThumbnails)
    {
        return;
    }
    
    EnterCriticalSection(&g_imgCriticalSection);
    if ((GetTickCount() - dwLastUpdate) >= THUBMNAIL_REFRESH_INTERNVAL)
    {
        dwLastUpdate = GetTickCount();
        list<DWORD> procList;

        // Loop through existing thumbs and remove closed procs
        PROCIMGMAP::iterator iter = g_imgMap.begin();

        while (iter != g_imgMap.end())
        {
            DWORD dwProcess = iter->first;
            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, false, dwProcess);

            if (!hProc)
            {
                // the process no longer exists
                procList.push_back(iter->first);
            }
            else
            {
                CloseHandle(hProc);
            }

            iter++;
        }

        if (procList.size() > 0)
        {
            list<DWORD>::iterator oldProcIter = procList.begin();
            while (oldProcIter != procList.end())
            {
                DWORD dwProcess = *oldProcIter;

                if (g_imgMap[dwProcess])
                {
                    DeleteObject(g_imgMap[dwProcess]);
                }
                if (g_iconMap[dwProcess])
                {
                    DestroyIcon(g_iconMap[dwProcess]);
                }
                g_imgMap.erase(dwProcess);
                g_iconMap.erase(dwProcess);
                g_timeMap.erase(dwProcess);
                g_procMap.erase(dwProcess);

                oldProcIter++;
            }
        }
    }

    DWORD dwProcessId = 0;
    GetWindowThreadProcessId(hwndCurrent, &dwProcessId);

    HWND wndTest = GetDesktopWindow();

    bool needsImage = false;
    if (g_imgMap.find(dwProcessId) == g_imgMap.end())
    {
        // add it here
        WCHAR wzClassName[MAX_PATH], wzWndText[MAX_PATH];
        GetClassName(hwndCurrent, wzClassName, MAX_PATH);
        GetWindowText(hwndCurrent, wzWndText, MAX_PATH);

        HMODULE hProcess = (HMODULE)OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, dwProcessId);
        WCHAR wzExe[MAX_PATH];
        GetModuleFileName(hProcess, wzExe, MAX_PATH);
        CloseHandle(hProcess);

        HICON hIcon = GetApplicationIcon(wzExe, false, wzExe, wzWndText, wzClassName);

        g_iconMap.insert(pair<DWORD, HICON>(dwProcessId, hIcon));
        g_imgMap.insert(pair<DWORD, HBITMAP>(dwProcessId, NULL));
        g_timeMap.insert(pair<DWORD, DWORD>(dwProcessId, GetTickCount()));
        g_procMap.insert(pair<DWORD, HWND>(dwProcessId, hwndCurrent));
        needsImage = true;
    }
    DWORD dwTime = g_timeMap[dwProcessId];
    if (bForce || ((GetTickCount() - dwTime) >= THUBMNAIL_REFRESH_INTERNVAL) || (g_procMap[dwProcessId] != hwndCurrent))
    {
        DeleteObject(g_imgMap[dwProcessId]);
        g_imgMap[dwProcessId] = NULL;
        g_timeMap[dwProcessId] = GetTickCount();

        needsImage = true;
    }
    g_procMap[dwProcessId] = hwndCurrent;

    if (needsImage)
    {
        HWND hwndDesk = HWND_DESKTOP;

        // take the screenshot here
        HDC hdcFore = GetDC(hwndDesk);
        HDC hdcThumb = CreateCompatibleDC(hdcFore);

        int width = (g_sipInfo.rcVisibleDesktop.right - g_sipInfo.rcVisibleDesktop.left);
        int height = (g_sipInfo.rcVisibleDesktop.bottom - g_sipInfo.rcVisibleDesktop.top);

        HBITMAP hbmpThumb = CreateCompatibleBitmap(hdcFore, width / 3, height / 3);
        HGDIOBJ hbmpOld = SelectObject(hdcThumb, hbmpThumb);

        BOOL bValue = StretchBlt(hdcThumb, 0, 0, width / 3, height / 3, hdcFore, g_sipInfo.rcVisibleDesktop.left, g_sipInfo.rcVisibleDesktop.top, width, height, SRCCOPY);

        SelectObject(hdcThumb, hbmpOld);
        DeleteDC(hdcThumb);
        DeleteDC(hdcFore);

        g_imgMap[dwProcessId] = hbmpThumb;
    }
    LeaveCriticalSection(&g_imgCriticalSection);
}

DWORD WINAPI CAltTab::PollThumbnails(LPVOID lpParameter)
{
    CAltTab* p_this = (CAltTab*)lpParameter;
    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLEXIT);
    DWORD dwEvent = 0;

    while (WAIT_ABANDONED_0 != (dwEvent = WaitForSingleObject(hEvent, THUBMNAIL_POLL_INTERNVAL)))
    {
        // TODO, need to cleanup bitmaps every so often
        if (dwEvent == WAIT_TIMEOUT)
        {
            // TODO, put a critical section around creation of bitmaps
            HWND hwndCurrent = GetForegroundWindow();

            long lStyle = GetWindowLong(hwndCurrent, GWL_STYLE);
            if ((0 == (WS_MINIMIZEBOX & lStyle) && IsWindowVisible(hwndCurrent) && GetWindowTextLength(hwndCurrent) > 0))
            {
                p_this->EnsureCapture(hwndCurrent, false);
            }
        }
        else
        {
            break;
        }
    }
    CloseHandle(hEvent);

    return 0;
}

bool CloseActiveProgram(void* pVoid, HWND hwndDlg, node** ppNode)
{
    CAltTab* pAltTab = (CAltTab*)pVoid;
    node* pNode = *ppNode;
    bool bSingle = (pNode->next == pNode);
    WCHAR wzProcess[MAX_PATH], wzClassName[50];
    ::GetProcessName(pNode->dwProcessId, wzProcess);
    ::GetClassName(pNode->hWnd, wzClassName, 50);

    bool bClosed = pAltTab->CloseProcess(pNode->hWnd, wzProcess, wzClassName);
    if (bSingle)
    {
        ShowWindow(hwndDlg, SW_HIDE);
    }
    else if (bClosed)
    {
        node* pPrev = pNode->prev;
        *ppNode = pNode->next;

        pPrev->next = pNode->next;
        pNode->next->prev = pPrev;

        delete pNode;
    }
    return !bSingle;
}

bool CloseAllButActiveProgram(void* pVoid, HWND hwndDlg, node** ppNode)
{
    CAltTab* pAltTab = (CAltTab*)pVoid;
    node* pNode = *ppNode;
    bool bSingle = (pNode->next == pNode);

    if (bSingle)
    {
        ShowWindow(hwndDlg, SW_HIDE);
    }
    else
    {
        node* pIter = pNode->next;

        while (pIter != pNode)
        {
            bSingle = CloseActiveProgram(pVoid, hwndDlg, &pIter) || bSingle;
            pIter = pIter->next;
        }
    }
    return true;
}

bool HideTaskSwitch(void* pVoid, HWND hwndDlg, node** ppNode)
{
    ShowWindow(hwndDlg, SW_HIDE);
    return false;
}

inline void CAltTab::DrawUpdatedFrame(HWND hwndDlg)
{
    HRESULT hRet;
    while (TRUE)
    {
        hRet = this->Flip(hwndDlg);
        if (hRet == DD_OK)
        {
            break;
        }
        if (hRet == DDERR_SURFACELOST)
        {
            hRet = this->RestoreAll();
            if (hRet != DD_OK)
            {
                break;
            }
        }
        if (hRet == DDERR_SURFACEBUSY)
        {
			g_pDDSPrimary->Unlock(NULL);
            break;
        }
        if (hRet != DDERR_WASSTILLDRAWING)
        {
            break;
        }
    }
}

inline void CAltTab::AbortAutoClose(HWND hwndDlg)
{
    KillTimer(hwndDlg, 2);
}

inline void CAltTab::ResetAutoClose(HWND hwndDlg)
{
    AbortAutoClose(hwndDlg);
    if (this->g_dwAppSwitchAutoClose > 0)
    {
        SetTimer(hwndDlg, 2, 1000 * this->g_dwAppSwitchAutoClose, NULL);
    }
}

LRESULT CAltTab::AltTabProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{
    CAltTab* p_this = (CAltTab*)GetWindowLong(hwndDlg, GWL_USERDATA);
    HRESULT hRet;
    static int _x = 0;
    static int _y = 0;
    static int _iAccel = 0;
    static int _iAccelStart = 0;
    static int _iDecel = 1;
    static bool _bMouseDown = false;
    static bool _bMouseMove = false;
    static node* _pHead = NULL;
    static node* _pActive = NULL;
    static node* _pUnderMouse = NULL;
    static int _iDistance = 0;
    static int _iDistanceNeeded = 0;
    static int _iMouseDistance = 0;
    static int _iNodLen = 0;
    static DWORD _dwDownTick = 0;

    static PFN_ClickAction _pClickAction = NULL;

    // 1 = in, 2 = out, 3 = scroll
    static int _iAnimation = 0;
    static int _iIteration = 0;
    static int _iIterations = 10;
    static int _iInitialDir = 0;
    static bool _isAnimating = false;
    static bool _isLoaded = false;
    static double _decelRate = 0.0;
    static DWORD _dwLastAnimation = 0;
    static DWORD _dwEstimatedTime = 0;

    // img stuff
    static HDC _hdcThumb = NULL;

    // sensor info
    static HMODULE hSensorMod = NULL;

    switch (message) 
    {
        // no idea what this message is...
		case 0xe :
		{
			HWND hwnd = GetFocus();
			Sleep(0);
		}
		break;
        case 0x0000c008 :
        {
			//ShowError(L"Hiding 0x0000c008", 0x0000c008);
            //ShowWindow(hwndDlg, SW_HIDE);
            //SendMessage(hwndDlg, WM_HIDEALTTAB, 0, 0);
        }
        break;
        case WM_SHOWALTTAB:
            {
                HDC hdc = GetDC(NULL);
                _hdcThumb = CreateCompatibleDC(hdc);
                DeleteDC(hdc);

                if (p_this->g_bAppSwitchEnableShrinkAnimation)
                {
                    _iIterations = 10;
                }
                else
                {
                    _iIterations = 0;
                }
                if (p_this->g_bAppSwitchEnableScrollAnimation)
                {
                    _decelRate = p_this->g_dwDeceleration / 100.0;
                }
                else
                {
                    _decelRate = 0;
                }
                _dwEstimatedTime = _iIterations * p_this->g_dwAnimateInterval;

                p_this->EnsureCapture(GetForegroundWindow(), true);
                _pHead = p_this->BuildNodeList(&_iNodLen);
                p_this->InitSurfaces();

                _isLoaded = true;
                
                _iAnimation = 1;
                ShowWindow(hwndDlg, SW_SHOWNORMAL);
                SetForegroundWindow(hwndDlg);
                SetFocus(hwndDlg);

                _iInitialDir = (int)lParam;

                // fade in
                _iIteration = _iIterations;
                if (p_this->g_bAppSwitchEnableShrinkAnimation)
                {
                    _iIteration = 0;
                }
                SetTimer(hwndDlg, 1, p_this->g_dwAnimateInterval, NULL);
            }
            break;
        case WM_ACTIVATE :
            {
                if (!LOWORD(wParam))
                {
                    if (_iAnimation == 1)
                    {
                        Sleep(0);
                    }
                    SendMessage(hwndDlg, WM_HIDEALTTAB, 0, 0);
                }
            }
            break;
        case WM_KILLFOCUS :
            {
                Sleep(0);
            }
            break;
        case WM_HTC_TOUCH :
            {
                HTCTOUCH_WPARAM touchW;
                HTCTOUCH_LPARAM touchL;

                memcpy(&touchW, &wParam, sizeof touchW);
                memcpy(&touchL, &lParam, sizeof touchL);

                //((CTouchHandler*)(p_this->g_pTouchHandler))->ProcessTouchData(touchW, touchL, touchW.Where, touchL.WheelAngle, true);
            }
            break;
        case WM_SETFOCUS :
            {
                hSensorMod = SensorGestureInit(hSensorMod, hwndDlg, true, true);

                if (_iAnimation == 3 && p_this->g_dwAppSwitchAutoClose > 0)
                {
                    SetTimer(hwndDlg, 2, 1000 * p_this->g_dwAppSwitchAutoClose, NULL);
                }
            }
            break;
        case WM_HIDEALTTAB :
            {
                //ShowWindow(hwndDlg, SW_HIDE);
                if (_isLoaded)
                {
                    _isLoaded = false;

                    _x = _y = 0;

                    HWND hwndTaskBar = FindWindow(L"HHTaskBar",NULL);
                    ::RedrawWindow(hwndTaskBar,NULL,NULL,RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);

                    KillTimer(hwndDlg, 1);
                    KillTimer(hwndDlg, 2);

                    _iIteration = 0;
                    _iDistance = _iDecel = _iDistanceNeeded = 0;

                    SetEvent(p_this->hFinishedEvent);
                    if (hSensorMod)
                    {
                        SensorGestureUninit(hSensorMod, hwndDlg);
                        hSensorMod = NULL;
                    }
                    if (_hdcThumb)
                    {
                        DeleteDC(_hdcThumb);
                        _hdcThumb = NULL;
                    }
                    // Cleanup here
                    if (_pHead)
                    {
                        // keep us from looping
                        _pHead->prev->next = NULL;

                        node* pCurr = _pHead;
                        do
                        {
                            node* pTemp = pCurr;
                            pCurr = pTemp->next;

                            if (pTemp->hIcon)
                            {
                                pTemp->hIcon = NULL;
                            }

                            delete pTemp;
                        } while (pCurr);
                        _pHead = NULL;
                    }
                }
            }
            return 1;
        case WM_SELECTALTTAB :
            {
                if (_pActive)
                {
                    _pHead = _pActive;
                }
                // let's select this guy
                if (p_this->g_bAppSwitchEnableShrinkAnimation)
                {
                    KillTimer(hwndDlg, 1);
                    _iAnimation = 2;
                    _iIteration = 0;
                    SetTimer(hwndDlg, 1, p_this->g_dwAnimateInterval, NULL);
                }
                else
                {
                    SetForegroundWindow(_pHead->hWnd);
                    ShowWindow(hwndDlg, SW_HIDE);
                }
            }
            return 1;
        case WM_KEYDOWN :
            {
                if (_iAnimation == 3)
                {
                    // Handle any non-accelerated key commands
                    switch (wParam)
                    {
                        case VK_ACTION:
                            {
                                p_this->AbortAutoClose(hwndDlg);
                                SendMessage(hwndDlg, WM_SELECTALTTAB, 0, 0);
                            }
                            break;
                        case VK_UP :
                        case VK_LEFT :
                            {
                                p_this->AbortAutoClose(hwndDlg);
                                KillTimer(hwndDlg, 1);
                                _iDistanceNeeded += p_this->g_smallThumbWidth;
                                SetTimer(hwndDlg, 1, p_this->g_dwAnimateInterval, NULL);
                            }
                            break;
                        case VK_DOWN :
                        case VK_RIGHT :
                            {
                                p_this->AbortAutoClose(hwndDlg);
                                KillTimer(hwndDlg, 1);
                                _iDistanceNeeded += -p_this->g_smallThumbWidth;
                                SetTimer(hwndDlg, 1, p_this->g_dwAnimateInterval, NULL);
                            }
                            break;
                    }
                }
            }
            return 1;
        case WM_LBUTTONDOWN :
            {
                // if the timer is active...we need to adjust our x offset here
                
                if (_iAnimation == 3)
                {
                    p_this->AbortAutoClose(hwndDlg);
                    int x = _x;
                    int y = _y;
                    _x = LOWORD(lParam);
                    _y = HIWORD(lParam);

                    _bMouseMove = false;
                    _pClickAction = NULL;

                    if (_y >= p_this->g_bigThumbTop)
                    {
                        _bMouseDown = true;

                        if (_y <= (p_this->g_bigThumbTop + p_this->g_bigThumbHeight))
                        {
                            _bMouseMove = true;
                            KillTimer(hwndDlg, 1);
                            if (_pActive)
                            {
                                _pHead = _pActive;
                            }

                            _dwDownTick = GetTickCount();
                        }
                    }
                    else
                    {
                        _pClickAction = &HideTaskSwitch;
                    }
                    if (!_bMouseMove && _isAnimating)
                    {
                        _x = x;
                        _y = y;
                        _bMouseDown = false;
                    }
                }
            }
            break;
        case WM_LBUTTONUP :
            {
                if (_iAnimation == 3)
                {
                    int x = LOWORD(lParam);
                    int y = HIWORD(lParam);

                    if (_bMouseMove)
                    {
                        DWORD dwUpTick = GetTickCount();
                        DWORD dwDiff = (dwUpTick - _dwDownTick);

                        int xDiff = (x - _x);

                        if (dwDiff < 500 && abs(xDiff) >= 20)
                        {
                            // between -3 & 3
                            int count = max(-10, min(10, xDiff / p_this->g_smallThumbWidth));

                            if (count == 0)
                            {
                                if (xDiff < 0)
                                {
                                    count = -1;
                                }
                                else
                                {
                                    count = 1;
                                }
                            }
                            else
                            {
                                count = count << 1;
                            }
                            _iDistanceNeeded = count*p_this->g_smallThumbWidth;

                            // _iDistance is how far we need to go to get to our current item
                            if (count > 0)
                            {
                                // if we are moving right...
                                if ((_iDistanceNeeded < 0) == (_iDistance < 0))
                                {
                                    _iDistanceNeeded += abs(_iDistance);
                                }
                                else
                                {
                                    _iDistanceNeeded -= abs(_iDistance);
                                }
                                _iDecel = x - _x;
                            }
                            else
                            {
                                // if we are moving left...
                                if ((_iDistanceNeeded < 0) == (_iDistance < 0))
                                {
                                    _iDistanceNeeded -= abs(_iDistance);
                                }
                                else
                                {
                                    _iDistanceNeeded += abs(_iDistance);
                                }
                                _iDecel = x - _x;
                            }

                            _isAnimating = true;
                            SetTimer(hwndDlg, 1, p_this->g_dwAnimateInterval, NULL);
                            Sleep(0);
                            // Start a timer here
                        }
                        else if (abs(xDiff) < 20 && _pUnderMouse && !_isAnimating)
                        {
                            // we had a click...
                            // get the item at that point
                            if (_pUnderMouse == _pActive)
                            {
                                SendMessage(hwndDlg, WM_KEYDOWN, VK_ACTION, 0);
                            }
                            else
                            {
                                _iDecel = x - _x;
                                _iDistanceNeeded = _iMouseDistance;
                                SetTimer(hwndDlg, 1, p_this->g_dwAnimateInterval, NULL);
                            }
                        }
                        else if (abs(xDiff) < 20)
                        {
                            // we click elsewhere -- close it
                            ShowWindow(hwndDlg, SW_HIDE);
                        }
                        else
                        {
                            // Add acceleration here - we want to snap to the active tab
                            _iDecel = x - _x;
                            _iDistanceNeeded = _iDistance;
                            SetTimer(hwndDlg, 1, p_this->g_dwAnimateInterval, NULL);
                        }
                    }
                    else if (_pClickAction != NULL)
                    {
                        if (_pClickAction(p_this, hwndDlg, &_pHead))
                        {
                            p_this->UpdateFrameScroll(_hdcThumb, hwndDlg, 0, 0, 0, 0, _pHead, &_pActive, &_pUnderMouse, &_pClickAction, &_iDistance, &_iMouseDistance, true);
                            p_this->DrawUpdatedFrame(hwndDlg);
                        }
                    }

                    _bMouseMove = false;
                    _bMouseDown = false;
                }
                _pClickAction = NULL;
            }
            break;
        case WM_MOUSEMOVE :
            {
                if (_iAnimation == 3 && _bMouseDown)
                {
                    int x = LOWORD(lParam);
                    int y = HIWORD(lParam);

                    int xDiff = 0;
                    int yDiff = 0;

                    if (_bMouseMove)
                    {
                        xDiff = (x - _x);
                        yDiff = (y - _y);
                    }

                    if (_bMouseMove || !_isAnimating)
                    {
                        p_this->UpdateFrameScroll(_hdcThumb, hwndDlg, xDiff, yDiff, x, y, _pHead, &_pActive, &_pUnderMouse, &_pClickAction, &_iDistance, &_iMouseDistance, true);
                        p_this->DrawUpdatedFrame(hwndDlg);
                    }
                }
            }
            break;
        case WM_TIMER :
            {
                // Update and flip surfaces
                if (wParam == 2)
                {
                    // auto-select here
                    p_this->AbortAutoClose(hwndDlg);
                    SendMessage(hwndDlg, WM_SELECTALTTAB, 0, 0);
                }
                else if (1 == wParam && _iAnimation > 0)
                {
                    int x;
                    DWORD dwTick = GetTickCount();
                    DWORD dwDiff = dwTick - _dwLastAnimation;
                    bool bSkipFrame = (dwDiff < 500) && (_dwLastAnimation > 0) && (dwDiff / p_this->g_dwAnimateInterval)> 1;
                    _dwLastAnimation = dwTick;

                    if (bSkipFrame)
                    {
                        Sleep(0);
                    }

                    switch (_iAnimation)
                    {
                    case 1 :
                        {
                            if (_iIteration <= _iIterations)
                            {
                                _iIteration++;
                                p_this->UpdateFrameAnimateIn(_iIterations, _iIteration);
                            }
                            else
                            {
                                _iAnimation = 3;
                                KillTimer(hwndDlg, 1);
                                _isAnimating = false;
                                // Slide left or right here depending on scroll

                                if (_iInitialDir < 0)
                                {
                                    SendMessage(hwndDlg, WM_KEYDOWN, VK_LEFT, 0);
                                }
                                else if (_iInitialDir > 0)
                                {
                                    SendMessage(hwndDlg, WM_KEYDOWN, VK_RIGHT, 0);
                                }
                                else
                                {
                                    p_this->UpdateFrameScroll(_hdcThumb, hwndDlg, p_this->g_smallThumbWidth * _iInitialDir, 0, _x, _y, _pHead, &_pActive, &_pUnderMouse, &_pClickAction, &x, &x, true);
                                }
                            }
                        }
                        break;
                    case 2 :
                        {
                            if (_iIteration <= _iIterations)
                            {
                                _iIteration++;
                                p_this->UpdateFrameAnimateOut(_hdcThumb, _iIterations, _iIterations-_iIteration, (_pActive) ? _pActive : _pHead);
                            }
                            else
                            {
                                // Hide window here
                                KillTimer(hwndDlg, 1);
                                _isAnimating = false;
                                SetForegroundWindow(_pHead->hWnd);
                                ShowWindow(hwndDlg, SW_HIDE);

                                return 1;
                            }
                        }
                        break;
                    case 3 :
                        {
                            // We are scrolling here
                            if (_iDistanceNeeded == 0)
                            {
                                _iDecel = 0;
                                KillTimer(hwndDlg, 1);
                                _isAnimating = false;
                                p_this->UpdateFrameScroll(_hdcThumb, hwndDlg, 0, 0, _x, _y, _pHead, &_pActive, &_pUnderMouse, &_pClickAction, &_iDistance, &_iMouseDistance, true);

                                p_this->ResetAutoClose(hwndDlg);
                            }
                            else
                            {
                                double decelRate = _decelRate;

                                if (bSkipFrame)
                                {
                                    decelRate = _decelRate * _decelRate;
                                }
                                _iDecel += (_iDistanceNeeded - (int)(_iDistanceNeeded * decelRate));
                                // todo add logic here

                                bool bFinalStop = (abs(_iDistanceNeeded) < p_this->g_smallThumbWidth / 2) || (abs(_iDecel) < p_this->g_smallThumbWidth / 2) || (decelRate == 0);

                                if (bFinalStop)
                                {
                                    Sleep(0);
                                }

                                p_this->UpdateFrameScroll(_hdcThumb, hwndDlg, _iDecel, 0, _x, _y, _pHead, &_pActive, &_pUnderMouse, &_pClickAction, &_iDistance, &_iMouseDistance, bFinalStop);
                                _iDistanceNeeded = (int)(_iDistanceNeeded * decelRate);

                                if (_iDistanceNeeded == 0)
                                {
                                    _pHead = _pActive;
                                }
                            }
                        }
                    }
                    
                    p_this->DrawUpdatedFrame(hwndDlg);
                }
            }
            return 1;
        case WM_ERASEBKGND:
            {
            }
            return 1;
    }
    return DefWindowProc(hwndDlg, message, wParam, lParam);
}


void CAltTab::UpdateFrameAnimateIn(int iAnimateIterations, int iIteration)
{
    DDBLTFX ddbltfx;

    memset(&ddbltfx, 0, sizeof(ddbltfx));
    ddbltfx.dwSize = sizeof(ddbltfx);
    ddbltfx.dwROP = SRCCOPY;

    HRESULT hRet = g_pDDSBuffer->Blt(NULL, g_pDDSBackground, NULL, DDBLT_ROP, &ddbltfx);

    int anWidth = (int)(g_bigThumbWidth + 1.0 * (iAnimateIterations - iIteration) * (g_width - g_bigThumbWidth) / iAnimateIterations);
    int anHeight = (int)(g_bigThumbHeight + 1.0 * (iAnimateIterations - iIteration) * (g_height - g_bigThumbHeight) / iAnimateIterations);

    int x = (g_bigThumbLeft + g_bigThumbWidth / 2) - (anWidth / 2);
    int y = ((g_bigThumbTop - (g_sipInfo.rcVisibleDesktop.top / 2)) * 1.0 * (iIteration) / iAnimateIterations);

    RECT rect;
    rect.left = x;
    rect.right = x + anWidth;
    rect.top = y;
    rect.bottom = y + anHeight;

    RECT srcRect;
    srcRect.left = 0;
    srcRect.right = g_width;
    srcRect.top = 0;
    srcRect.bottom = g_height;

     hRet = g_pDDSBuffer->Blt(&rect, g_pDDSAnimate, &srcRect, DDBLT_ROP, &ddbltfx);
}

void CAltTab::UpdateFrameAnimateOut(HDC hdcThumb, int iAnimateIterations, int iIteration, node* pActiveNode)
{
    DDBLTFX ddbltfx;

    memset(&ddbltfx, 0, sizeof(ddbltfx));
    ddbltfx.dwSize = sizeof(ddbltfx);
    ddbltfx.dwROP = SRCCOPY;

    g_pDDSBuffer->Blt(NULL, g_pDDSBackground, NULL, DDBLT_ROP, &ddbltfx);

    int anWidth = (int)(g_bigThumbWidth + 1.0 * (iAnimateIterations - iIteration) * (g_width - g_bigThumbWidth) / iAnimateIterations);
    int anHeight = (int)(g_bigThumbHeight + 1.0 * (iAnimateIterations - iIteration) * (g_height - g_bigThumbHeight) / iAnimateIterations);

    //int x = g_width / 2;
    //int y = g_height / 2;
    int x = (g_bigThumbLeft + g_bigThumbWidth / 2) - (anWidth / 2);
    int y = ((g_bigThumbTop - (g_sipInfo.rcVisibleDesktop.top / 2)) * 1.0 * (iIteration) / iAnimateIterations);

    RECT rect;
    rect.left = x;
    rect.right = x + anWidth;
    rect.top = y;
    rect.bottom = y + anHeight;

    HDC hdc;
    g_pDDSBuffer->GetDC(&hdc);
    
    DrawThumbnail(hdc, hdcThumb, pActiveNode->hBmp, pActiveNode->hIcon, NULL, rect.left, rect.top, anWidth, anHeight, 5, WHITE_BRUSH, 255, true);

    g_pDDSBuffer->ReleaseDC(hdc);
}

void CAltTab::UpdateFrameScroll(HDC hdcThumb, HWND hWnd, int xDiff, int yDiff, int xPos, int yPos, node* pHead, node** pActive, node** pUnderMouse, PFN_ClickAction* pClickAction, int* piDistanceFromCenter, int* piMouseFromCenter, bool bFinalStop)
{
    HRESULT                     hRet;
    DDBLTFX                     ddbltfx;

    while (xDiff > g_smallThumbWidth)
    {
        pHead = pHead->prev;
        xDiff -= g_smallThumbWidth;
    }
    while (xDiff < -g_smallThumbWidth)
    {
        pHead = pHead->next;
        xDiff += g_smallThumbWidth;
    }

    *pUnderMouse = NULL;
    *pActive = NULL;
    *pClickAction = NULL;

    memset(&ddbltfx, 0, sizeof(ddbltfx));
    ddbltfx.dwSize = sizeof(ddbltfx);
    ddbltfx.dwROP = SRCCOPY;

    g_pDDSBuffer->Blt(NULL, g_pDDSBackground, NULL, DDBLT_ROP, &ddbltfx);

    int bigLeft = (g_width - g_smallThumbWidth) / 2;
    int bigRight = bigLeft + g_smallThumbWidth;
    int bigMid = g_width / 2;

    HDC hdc;
    g_pDDSBuffer->GetDC(&hdc);
    RECT activeRect;
    bool bHasActive = false;
    double percentAway = 1.0;

    for (int iDir = -1; iDir < 2; iDir+=2)
    {
        node* pNodeCurr = pHead;
        for (int i = 0; i < 6; ++i)
        {
            if (iDir > 0 && i == 0)
            {
                //
            }
            else
            {
                int xMid = bigMid + (g_smallThumbWidth * iDir * i);

                int x = xMid + xDiff;
                int y = g_bigThumbMid;

                if ((x + g_smallThumbWidth / 2) < 0 || (x - g_smallThumbWidth / 2) > g_width)
                {
                    break;
                }

                int anWidth = g_smallThumbWidth;
                int anHeight = g_smallThumbHeight;

                bool bSetActive = false;
                // in the middle
                if (x == bigMid)
                {
                    // we need to draw the big one last...
                    *piDistanceFromCenter = 0;
                    *pActive = pNodeCurr;
                    // we need to do a proportional draw
                    percentAway = 1.0;

                    anWidth = g_smallThumbWidth + (g_bigThumbWidth - g_smallThumbWidth) * percentAway;
                    anHeight = g_smallThumbHeight + (g_bigThumbWidth - g_smallThumbWidth) * percentAway;

                    bSetActive = true;
                }
                else if (abs(bigMid - x) < (g_smallThumbWidth / 2))
                {
                    *piDistanceFromCenter = (bigMid - x);
                    *pActive = pNodeCurr;
                    // we need to do a proportional draw
                    percentAway = max(0, 1.0 - (1.0 * abs(bigMid - x)) / (g_smallThumbWidth / 2));

                    anWidth = g_smallThumbWidth + (g_bigThumbWidth - g_smallThumbWidth) * percentAway;
                    anHeight = g_smallThumbHeight + (g_bigThumbWidth - g_smallThumbWidth) * percentAway;

                    bSetActive = true;
                }

                // this will animate the list sliding from side to side

                RECT rect;
                rect.left = x - anWidth/2;
                rect.top = y - anHeight/2;
                rect.right = rect.left + anWidth;
                rect.bottom = rect.top + anHeight;

                if (RectContains(&rect, xPos, yPos))
                {
                    *pUnderMouse = pNodeCurr;
                    *piMouseFromCenter = (bigMid - x);
                }

                if (rect.right > 0 && rect.left < g_width)
                {
                    if (bSetActive)
                    {
                        bHasActive = true;
                        memcpy(&activeRect, &rect, sizeof(RECT));
                    }
                    else
                    {
                        DrawThumbnail(hdc, hdcThumb, pNodeCurr->hBmp, pNodeCurr->hIcon, hWnd, rect.left, rect.top, anWidth, anHeight, 5, BLACK_BRUSH, 100, true);
                    }
                }
                else
                {
                    Sleep(0);
                }
            }
            pNodeCurr = (iDir < 0) ? pNodeCurr->prev : pNodeCurr->next;
        }
    }
    node* pNodeActive = *pActive;

    if (pNodeActive && bHasActive)
    {
        RECT rect;
        
        rect.left = g_labelLeft;
        rect.top = g_labelTop;
        rect.right = g_labelLeft + g_labelWidth;
        rect.bottom = g_labelTop + g_labelHeight;


        double labelPercent = percentAway;

        int labelYMid = (g_labelTop) + (g_labelHeight) / 2;
        int labelXMid = (g_labelLeft) + (g_labelWidth) / 2;
        int labelYHalf = (g_labelHeight) / 2;
        int labelXHalf = (g_labelWidth) / 2;

        COLORREF rgb = RGB(0xE5, 0xEA, 0xFF);
        HBRUSH hBrush = CreateSolidBrush(rgb);//RGB(0, 255, 0));
        HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);
        RoundRect(hdc, labelXMid - labelXHalf, labelYMid - labelYHalf, labelXMid + labelXHalf, labelYMid + labelYHalf, 20, 20);
        SelectObject(hdc, hOldBrush);
        DeleteObject(hBrush);

        LOGFONT lf;
        ZeroMemory(&lf, sizeof(lf));
        lf.lfHeight = -(g_labelFont);
        lf.lfWeight = FW_BOLD;
        lf.lfCharSet = DEFAULT_CHARSET;
        HFONT hFont = CreateFontIndirect(&lf);

        SetBkMode(hdc, TRANSPARENT);
        SetBkColor(hdc, TRANSPARENT);
        HGDIOBJ hOldFont = SelectObject(hdc, hFont);
        DrawText(hdc, pNodeActive->wzText, wcslen(pNodeActive->wzText), &rect, DT_VCENTER | DT_CENTER);
        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);

        // only draw if we are stopping
        if (labelPercent > 0.75 && bFinalStop)
        {
            labelPercent = (labelPercent - 0.75) * 4;

            labelYMid = g_buttonTop + (g_buttonHeight / 2);
            labelXMid = (g_buttonLeft) + (g_buttonWidth) / 2;
            labelYHalf = (g_buttonHeight) / 2 * labelPercent;
            labelXHalf = (g_buttonWidth) / 2 * labelPercent;

            COLORREF rgb = RGB(0xE5, 0xEA, 0xFF);
            COLORREF rgbSel = RGB(0x32, 0x54, 0xFF);
            HBRUSH hBrush = CreateSolidBrush(rgb);//RGB(0, 255, 0));
            HBRUSH hBrushSel = CreateSolidBrush(rgbSel);//RGB(0, 255, 0));
            HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);
            LOGFONT lfLabel;
            ZeroMemory(&lfLabel, sizeof(lfLabel));
            lfLabel.lfHeight = -(g_buttonFont*labelPercent);
            lfLabel.lfWeight = FW_SEMIBOLD;
            lfLabel.lfCharSet = DEFAULT_CHARSET;
            HFONT hFontLabel = CreateFontIndirect(&lfLabel);
            hOldFont = SelectObject(hdc, hFontLabel);

            rect.left = labelXMid - labelXHalf;
            rect.top = labelYMid - labelYHalf;
            rect.right = labelXMid + labelXHalf;
            rect.bottom = labelYMid + labelYHalf;

            if (RectContains(&rect, xPos, yPos))
            {
                *pClickAction = &CloseActiveProgram;
                SelectObject(hdc, hBrushSel);
            }
            else
            {
                SelectObject(hdc, hBrush);
            }
            RoundRect(hdc, labelXMid - labelXHalf, labelYMid - labelYHalf, labelXMid + labelXHalf, labelYMid + labelYHalf, 20, 20);
			WCHAR wzCloseProgram[100];
			StringCchCopy(wzCloseProgram, sizeof(WCHAR)*100, L"Close '");
			StringCchCat(wzCloseProgram, sizeof(WCHAR)*100, pNodeActive->wzText);
			StringCchCat(wzCloseProgram, sizeof(WCHAR)*100, L"'");
            DrawText(hdc, wzCloseProgram, wcslen(wzCloseProgram), &rect, DT_VCENTER | DT_CENTER);


            labelYMid += g_buttonHeight + g_buttonSpacing/2;

            rect.left = labelXMid - labelXHalf;
            rect.top = labelYMid - labelYHalf;
            rect.right = labelXMid + labelXHalf;
            rect.bottom = labelYMid + labelYHalf;
            
            if (RectContains(&rect, xPos, yPos))
            {
                *pClickAction = &CloseAllButActiveProgram;
                SelectObject(hdc, hBrushSel);
            }
            else
            {
                SelectObject(hdc, hBrush);
            }
            RoundRect(hdc, labelXMid - labelXHalf, labelYMid - labelYHalf, labelXMid + labelXHalf, labelYMid + labelYHalf, 20, 20);
            DrawText(hdc, L"Close All Other Programs", wcslen(L"Close All Other Programs"), &rect, DT_VCENTER | DT_CENTER);

            SelectObject(hdc, hOldBrush);
            DeleteObject(hBrush);
            DeleteObject(hBrushSel);
            
            SelectObject(hdc, hOldFont);
            DeleteObject(hFontLabel);
        }

        DrawThumbnail(hdc, hdcThumb, pNodeActive->hBmp, pNodeActive->hIcon, hWnd, activeRect.left, activeRect.top, activeRect.right - activeRect.left, activeRect.bottom - activeRect.top, 5, WHITE_BRUSH, 255, true);
    }
    else
    {
        Sleep(0);
    }
    g_pDDSBuffer->ReleaseDC(hdc);

    while (TRUE)
    {
        if (hRet == DD_OK)
            break;
        if (hRet == DDERR_SURFACELOST)
        {
            hRet = RestoreAll();
            if (hRet != DD_OK)
                break;
        }
        if (hRet != DDERR_WASSTILLDRAWING)
            break;
    }
}