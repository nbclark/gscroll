#include "stdafx.h"
#include <windows.h>
#include <atltime.h>
#include <tlhelp32.h>
#include "resource.h"
#include "utils.h"
#include "TouchHandler.h"

#define WM_SHOWTOUCHHANDLER     (WM_USER + 1)
#define WM_HIDETOUCHHANDLER     (WM_USER + 2)

CTouchHandler::CTouchHandler(void)
{
    g_iInitRet = 0;
    g_bInAltTab = false;
    g_bMouseMode = false;
    g_bUseCursorKeys = false;
    g_iMouseX = g_iMouseY = -1;
    g_iMouseAccel = 1;

    g_hInstance = GetModuleHandle(NULL);

    g_bIgnoreCurrentWindow = false;
    g_hWndActive = NULL;
    g_dwProcessIdActive = -1;
    g_bNormalized = false;
    g_bInActiveCall = false;

    ResetTouchData(true);
    LoadSettings(true);
    
    g_def_bSmoothScroll = g_bSmoothScroll = false;

    memset(&g_wndClassTouchHandler, 0, sizeof(WNDCLASS));
    g_wndClassTouchHandler.lpszClassName = L"mobilesrcTouchHandler";
    g_wndClassTouchHandler.lpfnWndProc = &TouchHandlerProc;
    g_wndClassTouchHandler.style = 0;
    g_wndClassTouchHandler.hInstance = g_hInstance;
    g_wndClassTouchHandler.hIcon = NULL;
    ATOM a = RegisterClassW(&g_wndClassTouchHandler);

    g_hwndTouchHandler = CreateWindowExW
    (
        WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        g_wndClassTouchHandler.lpszClassName,
        L"",
        WS_POPUP | WS_CLIPSIBLINGS,
        100,100,32,32, NULL, NULL, g_hInstance, NULL
    );

    SetWindowLong(g_hwndTouchHandler, GWL_USERDATA, (LONG)this);
}

void CTouchHandler::Initialize()
{
    g_AltTab.Initialize();
}

CTouchHandler::~CTouchHandler(void)
{
    CleanupLaunchSettings();
}

void CTouchHandler::CleanupLaunchSettings()
{
    hash_map<WCHAR*, WCHAR*>::iterator iter = g_wzLaunchList.begin();

    while (iter != g_wzLaunchList.end())
    {
        if (iter->first)
        {
            delete[] iter->first;
        }
        if (iter->second)
        {
            delete[] iter->second;
            delete[] iter->second;
        }

        iter++;
    }
    g_wzLaunchList.clear();
}

void CTouchHandler::LoadLaunchSettings()
{
    CleanupLaunchSettings();

    HKEY hKey;
    HRESULT hRes = RegOpenKeyEx(HKEY_CURRENT_USER, REG_GScrollKey, 0, KEY_READ, &hKey);
    if (hRes == ERROR_SUCCESS)
    {
        HKEY hKeyLaunch;
        hRes = RegOpenKeyEx(hKey, REG_LaunchKey, 0, KEY_READ, &hKeyLaunch);
        if (hRes == ERROR_SUCCESS)
        {
            DWORD dwNotify, dwSize, dwType;
            dwSize = sizeof(DWORD);
            RegQueryValueExW(hKeyLaunch, REG_Launch_Notify, 0, &dwType, (LPBYTE)&dwNotify, &dwSize);
            g_bLaunchNotify = (dwNotify != 0);

            HKEY hKeySequences;
            hRes = RegOpenKeyEx(hKeyLaunch, REG_Launch_SequencesKey, 0, KEY_READ | KEY_QUERY_VALUE, &hKeySequences);
            if (hRes == ERROR_SUCCESS)
            {
                int i = 0;
                while (true)
                {
                    WCHAR* wzSeqName = new WCHAR[10];
                    DWORD dwSeqName = 10;
                    WCHAR* wzValName = new WCHAR[200];
                    DWORD dwValName = 200;


                    if (ERROR_SUCCESS == RegEnumValue(hKeySequences, i, wzSeqName, &dwSeqName, 0, 0, (LPBYTE)wzValName, &dwValName))
                    {
                        g_wzLaunchList.insert(pair<WCHAR*, WCHAR*>(wzSeqName, wzValName));
                    }
                    else
                    {
                        delete[] wzSeqName;
                        delete[] wzValName;
                        break;
                    }
                    i++;
                }
                RegCloseKey(hKeySequences);
            }
            RegCloseKey(hKeyLaunch);
        }
        RegCloseKey(hKey);
    }
}

void CTouchHandler::LoadSettings(bool bDefault)
{
    LoadGeneralSettings(true);
    LoadLaunchSettings();
    
    g_AltTab.LoadSettings();
}

bool CTouchHandler::LoadGeneralSettings(bool bDefault)
{
    DWORD dwProcessId, dwType, dwValue, dwSize;
    HWND hwndActive = GetForegroundWindow();
    bool bChanged = false;

    if ((hwndActive != g_hWndActive) || bDefault)
    {
        if (!bDefault)
        {
            bChanged = true;
            g_hWndActive = hwndActive;
        }
        else
        {
            g_hWndActive = NULL;
        }
        ResetTouchData(true);
        GetWindowThreadProcessId(g_hWndActive, &dwProcessId);

        if (dwProcessId != g_dwProcessIdActive)
        {
            // Load the settings for this process here...
            if (!bDefault)
            {
                g_dwProcessIdActive = dwProcessId;
            }
            WCHAR wzProcessName[MAX_PATH];
            GetProcessName(dwProcessId, wzProcessName);

            HKEY hKey;
            HRESULT hRes = RegOpenKeyEx(HKEY_CURRENT_USER, REG_GScrollKey, 0, KEY_READ, &hKey);
            if (hRes == ERROR_SUCCESS)
            {
                if (!bDefault)
                {
                    HKEY hKeyPerProcess;
                    hRes = RegOpenKeyEx(hKey, REG_PerProcessKey, 0, KEY_READ, &hKeyPerProcess);
                    if (hRes == ERROR_SUCCESS)
                    {
                        HKEY hKeySettings;
                        hRes = RegOpenKeyEx(hKeyPerProcess, wzProcessName, 0, KEY_READ, &hKeySettings);
                        if (hRes == ERROR_SUCCESS)
                        {
                            RegCloseKey(hKey);
                            hKey = hKeySettings;
                        }
                        RegCloseKey(hKeyPerProcess);
                    }
                }

                dwSize = sizeof(DWORD);
                // we have our hKey -- it's either the default, or per process

                RegQueryValueExW(hKey, REG_SwipeMode, 0, &dwType, (LPBYTE)&g_dwSwipeMode, &dwSize);
                RegQueryValueExW(hKey, REG_TiltMode, 0, &dwType, (LPBYTE)&g_dwTiltMode, &dwSize);

                RegQueryValueExW(hKey, REG_Disabled, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
                g_bDisabled = (dwValue != 0);

                RegQueryValueExW(hKey, REG_DisableLaunchGestures, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
                g_disableLaunchGestures = (dwValue != 0);

                RegQueryValueExW(hKey, REG_EnableScrollWheel, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
                g_bScrollWheel = (dwValue != 0);

                RegQueryValueExW(hKey, REG_EnablePrecisionMode, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
                g_bPrecisionMode = (dwValue != 0);

                RegQueryValueExW(hKey, REG_EnableGameMode, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
                g_bGameMode = (dwValue != 0);

                RegQueryValueExW(hKey, REG_EnableSmoothScroll, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
                g_bSmoothScroll = (dwValue != 0);
                
                RegQueryValueExW(hKey, REG_GameModeKeyRepeat, 0, &dwType, (LPBYTE)&g_dwGameModeRepeat, &dwSize);
                RegQueryValueExW(hKey, REG_PrecisionModeScrollSize, 0, &dwType, (LPBYTE)&g_dwPrecisionModeScrollSize, &dwSize);
                RegQueryValueExW(hKey, REG_PrecisionModeNavigateSize, 0, &dwType, (LPBYTE)&g_dwPrecisionModeNavigateSize, &dwSize);

				if (bDefault)
				{
					RegQueryValueExW(hKey, REG_TapMinInterval, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
					GSCROLL_TAP_MININTERVAL = (dwValue) ? dwValue : GSCROLL_TAP_MININTERVAL;
					RegQueryValueExW(hKey, REG_TapMaxInterval, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
					GSCROLL_TAP_MAXINTERVAL = (dwValue) ? dwValue : GSCROLL_TAP_MAXINTERVAL;
					RegQueryValueExW(hKey, REG_SwipeMinInterval, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
					GSCROLL_SWIPE_MININTERVAL = (dwValue) ? dwValue : GSCROLL_SWIPE_MININTERVAL;
					RegQueryValueExW(hKey, REG_DblTapMaxInterval, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
					GSCROLL_DOUBLETAP_MAXINTERVAL = (dwValue) ? dwValue : GSCROLL_DOUBLETAP_MAXINTERVAL;
					RegQueryValueExW(hKey, REG_DblTapMinInterval, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
					GSCROLL_DOUBLETAP_MININTERVAL = (dwValue) ? dwValue : GSCROLL_DOUBLETAP_MININTERVAL;
					RegQueryValueExW(hKey, REG_PrecisionInterval, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
					GSCROLL_PRECISON_INTERVAL = (dwValue) ? dwValue : GSCROLL_PRECISON_INTERVAL;
					RegQueryValueExW(hKey, REG_TiltStep, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
					GSCROLL_TILT_STEP = (dwValue) ? dwValue : GSCROLL_TILT_STEP;
					RegQueryValueExW(hKey, REG_TiltSleep, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
					GSCROLL_TILT_SLEEP_INTERVAL = (dwValue) ? dwValue : GSCROLL_TILT_SLEEP_INTERVAL;
					RegQueryValueExW(hKey, REG_TiltThreshold, 0, &dwType, (LPBYTE)&dwValue, &dwSize);
					GSCROLL_TILT_THRESHOLD = (dwValue) ? dwValue : GSCROLL_TILT_THRESHOLD;
				}
                RegCloseKey(hKey);
            }
        }
    }
    if (bDefault)
    {
        g_def_dwSwipeMode = g_dwSwipeMode;
        g_def_dwGameModeRepeat = g_dwGameModeRepeat;
        g_def_dwPrecisionModeScrollSize = g_dwPrecisionModeScrollSize;
        g_def_dwPrecisionModeNavigateSize = g_dwPrecisionModeNavigateSize;
        g_def_bDisabled = g_bDisabled;
        g_def_bScrollWheel = g_bScrollWheel;
        g_def_bPrecisionMode = g_bPrecisionMode;
        g_def_bGameMode = g_bGameMode;
    }
    return bChanged;
}


void CTouchHandler::ProcessMouseData(HWND hwnd, HANDLE hNavDevice, HMODULE hSensorMod, HTCTOUCH_WPARAM* touchW, HTCTOUCH_LPARAM* touchL, bool bTimer)
{
    static int _prevX = -1, _prevY = -1;
    // Only do this if we are in mouse mode

    HANDLE hEventCursor = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLCURSOR);
    ResetEvent(hEventCursor);

    if (!touchW->Up)
    {
        if (touchW->xPosRP != 255)
        {
            g_iMouseX = touchW->xPosRP + 50;
            g_iMouseY = touchW->yPosRP + 50;
        }
        else if (touchL->xPosLP != 255)
        {
            g_iMouseX = touchL->xPosLP;
            g_iMouseY = touchL->yPosLP;
        }
        else
        {
            g_iMouseX = g_iMouseY = -1;
        }
        if (-1 != g_iMouseX)
        {
            SetEvent(hEventCursor);
        }
    }
    CloseHandle(hEventCursor);
    ProcessTouchData(*touchW, *touchL, touchW->Where, touchL->WheelAngle, false);
}

int CTouchHandler::GetOrientation()
{
    DEVMODE deviceMode;

    memset(&deviceMode, NULL, sizeof(DEVMODE));
    deviceMode.dmSize = sizeof(DEVMODE);
    deviceMode.dmFields = DM_DISPLAYORIENTATION;

    int orientation = 0;

    if (DISP_CHANGE_SUCCESSFUL == ChangeDisplaySettingsEx(NULL, &deviceMode, NULL, CDS_TEST, NULL))
    {
        orientation = deviceMode.dmDisplayOrientation;
    }

    return orientation;
}

DWORD CTouchHandler::GetDPadKey(short shDir)
{
    // 0 = left, 1 = up, 2 = right, 3 = down
    // VK_RIGHT
    int orientation = GetOrientation();
    switch (orientation)
    {
        case 0 :
            {
            }
            break;
        case DMDO_180 :
            {
                shDir = (shDir+2) % 4;
            }
            break;
        case DMDO_90 :
            {
                shDir = (shDir+3) % 4;
            }
            break;
        case DMDO_270 :
            {
                shDir = (shDir+1) % 4;
            }
            break;
    }
    switch (shDir)
    {
        case 0 :
            {
                return VK_LEFT;
            }
        case 1 :
            {
                return VK_UP;
            }
        case 2 :
            {
                return VK_RIGHT;
            }
        case 3 :
            {
                return VK_DOWN;
            }
    }
    return 0;
}

void CTouchHandler::Scroll(HWND hwnd, bool bPage, bool bForceKeyDown, int iIterationsX, int iIterationsY, short dwMode)
{
    int orientation = GetOrientation();
    switch (orientation)
    {
        case 0 :
            {
            }
            break;
        case DMDO_180 :
            {
                iIterationsX = -iIterationsX;
                iIterationsY = -iIterationsY;
            }
            break;
        case DMDO_90 :
            {
                int temp = iIterationsX;
                iIterationsX = -iIterationsY;
                iIterationsY = temp;
            }
            break;
        case DMDO_270 :
            {
                int temp = iIterationsX;
                iIterationsX = iIterationsY;
                iIterationsY = -temp;
            }
            break;
    }

    if (iIterationsX != 0)
    {
        if (iIterationsX > 0)
        {
            ScrollRight(hwnd, bPage, bForceKeyDown, dwMode, iIterationsX);
        }
        else
        {
            ScrollLeft(hwnd, bPage, bForceKeyDown, dwMode, -iIterationsX);
        }
    }
    if (iIterationsY != 0)
    {
        if (iIterationsY > 0)
        {
            ScrollDown(hwnd, bPage, bForceKeyDown, dwMode, iIterationsY);
        }
        else
        {
            ScrollUp(hwnd, bPage, bForceKeyDown, dwMode, -iIterationsY);
        }
    }
}

void CTouchHandler::ResetTouchData(bool bSettingsChanged)
{
    if (bSettingsChanged)
    {
        g_previousPositionX = -1, g_previousPositionY = -1;
        g_dwFirstDownTick = 0;
        g_bWasDown = false;
    }

    g_dwDownRegion = -1;
    g_dwDownTick = 0;
    g_dwKeyRepeat = 0;
    g_bNoUpHandle = false;
    g_previousPositionX = -1;
    g_previousPositionY = -1;
    g_bMadeDecision = false;
    g_bKeyWasPressed = false;
}

void CTouchHandler::ProcessTouchData(HTCTOUCH_WPARAM touchW, HTCTOUCH_LPARAM touchL, byte wheelNav, byte wheelDeg, bool bTouchOnly)
{
    static bool _bGameModeDown = false;
    static short _shGameModeDir = 0;
    this->g_bNormalized = false;

    if (!this->g_bInActiveCall)
    {
        // Check if any known keys are pressed
        if
        (
            GetAsyncKeyState(VK_ACTION) ||
            GetAsyncKeyState(VK_UP) ||
            GetAsyncKeyState(VK_DOWN) ||
            GetAsyncKeyState(VK_LEFT) ||
            GetAsyncKeyState(VK_RIGHT)
        )
        {
            g_bKeyWasPressed = !_bGameModeDown;
        }

        short dwSwipeMode = g_dwSwipeMode;

        bool bCenter = (touchL.xPosLP == 255 && touchL.yPosLP == 255) && (touchW.xPosRP == 255 && touchW.yPosRP == 255);
        bool bZero = (touchL.xPosLP == 0 && touchL.yPosLP == 0) && (touchW.xPosRP == 0 && touchW.yPosRP == 0);
        bool bLeftSide = (touchL.xPosLP != 255 && touchL.yPosLP != 255);
        bool bIsDown = (touchW.Up == 0);

        if (bZero)
        {
            return;
        }

        int xPos = (bLeftSide) ? touchL.xPosLP : touchW.xPosRP;
        int yPos = (bLeftSide) ? touchL.yPosLP : touchW.yPosRP;

        int iIterationSize = g_dwPrecisionModeScrollSize;

        if (bTouchOnly)
        {
            dwSwipeMode = GScroll_TiltMode_DPadOnly;
        }
        else
        {
            if (g_dwSwipeMode == GScroll_TiltMode_LeftScrollRightDir)
            {
                dwSwipeMode = (bLeftSide) ? GScroll_TiltMode_ScrollOnly : GScroll_TiltMode_DPadOnly;
            }
            if (g_dwSwipeMode == GScroll_TiltMode_LeftDirRightScroll)
            {
                dwSwipeMode = (bLeftSide) ? GScroll_TiltMode_DPadOnly : GScroll_TiltMode_ScrollOnly;
            }
        }
        if (dwSwipeMode == GScroll_TiltMode_DPadOnly)
        {
            iIterationSize = g_dwPrecisionModeNavigateSize;
        }

        short dwRegion = (bLeftSide) ? 0 : 2;
        if (yPos > (GSCROLL_PANEL_HEIGHT / 2))
        {
            dwRegion++;
        }
        if (bCenter)
        {
            dwRegion = 4;
        }

        // Finger is down
        if (bIsDown)
        {
            // do nothing if a key is pressed
            if (!g_bKeyWasPressed)
            {
                // We have already decided that we want to handle moving touches
                if (g_bNoUpHandle)
                {
                    if (this->g_bPrecisionMode && !bCenter)
                    {
                        int xDiff = xPos - g_previousPositionX;
                        int yDiff = yPos - g_previousPositionY;

                        int iIterationsX = (xDiff) / iIterationSize;
                        int iIterationsY = (yDiff) / iIterationSize;

                        if (abs(xDiff) > abs(yDiff))
                        {
                            iIterationsY = 0;
                        }
                        else
                        {
                            iIterationsX = 0;
                        }
                        // Scroll the X & Y
                        if (iIterationsX || iIterationsY)
                        {
                            Scroll(GetForegroundWindow(), false, false, iIterationsX, iIterationsY, dwSwipeMode);
                            g_previousPositionY = yPos;
                            g_previousPositionX = xPos;
                        }
                        if (iIterationsX != 0)
                        {
                            g_previousPositionX = xPos;
                        }
                        if (iIterationsY != 0)
                        {
                            g_previousPositionY = yPos;
                        }
                    }
                    // TODO: if we are in game mode or maybe anything, we should make the wheel bigger...
                    else if (bCenter && g_bGameMode && (wheelNav & HTCNAV_WHEEL_MASK_DIR)) // need to hide this behind game-mode flag
                    {
                        double scaledWheelDeg = 360.0 * wheelDeg / HTCNAV_WHEEL_DEG;

                        short shDir = 0;
                        if (scaledWheelDeg > 315 || scaledWheelDeg < 45)
                        {
                            // left
                            shDir = 0;
                        }
                        else if (scaledWheelDeg < 135)
                        {
                            // up
                            shDir = 1;
                        }
                        else if (scaledWheelDeg < 225)
                        {
                            // right
                            shDir = 2;
                        }
                        else
                        {
                            // down
                            shDir = 3;
                        }

                        DWORD dwTick = GetTickCount();
                        DWORD dwDiff = (dwTick - g_dwKeyRepeat);
                        if (((dwTick - g_dwKeyRepeat) > g_dwGameModeRepeat))
                        {
                            DWORD dwKey = GetDPadKey(shDir);
                            if (_bGameModeDown)
                            {
                                //if (_shGameModeDir != shDir)
                                {
                                    // send the key up
                                }
                                //else
                                {
                                    //return;
                                }
                            }
                            g_dwKeyRepeat = dwTick;
                            _bGameModeDown = true;
                            _shGameModeDir = shDir;
                            keybd_event((BYTE)dwKey, 0, 0, 0);
                            keybd_event((BYTE)dwKey, 0, KEYEVENTF_KEYUP, 0);
                        }
                        else
                        {
                            Sleep(0);
                        }
                        //if (/*(g_dwKeyRepeat == 0) || */((dwTick - g_dwKeyRepeat) > g_dwGameModeRepeat))
                        //{
                        //    if (shDir == 0)
                        //    {
                        //        // left
                        //        Scroll(this->g_hWndActive, false, true, -1, 0, 2);
                        //    }
                        //    else if (shDir == 1)
                        //    {
                        //        // up
                        //        Scroll(this->g_hWndActive, false, true, 0, -1, 2);
                        //    }
                        //    else if (shDir == 1)
                        //    {
                        //        // right
                        //        Scroll(this->g_hWndActive, false, true, 1, 0, 2);
                        //    }
                        //    else
                        //    {
                        //        // down
                        //        Scroll(this->g_hWndActive, false, true, 0, 1, 2);
                        //    }
                        //    g_dwKeyRepeat = dwTick;
                        //}
                    }
                }
                // We have decided that we want to handle down and up and not moving touches
                else if (g_bMadeDecision)
                {
                    return;
                }
                else
                {
                    // we haven't yet decided how to handle the touches
                    if (g_previousPositionX == -1 || g_previousPositionY == -1)
                    {
                        g_previousPositionX = xPos;
                        g_previousPositionY = yPos;
                        g_dwDownTick = GetTickCount();
                        g_dwDownRegion = dwRegion;
                    }
                    else
                    {
                        if (bTouchOnly)
                        {
                            g_bMadeDecision = true;
                            g_bNoUpHandle = false;
                        }
                        else
                        {
                            DWORD dwTick = GetTickCount();
                            if ((dwTick - g_dwDownTick) > GSCROLL_PRECISON_INTERVAL)
                            {
                                g_bMadeDecision = true;

                                // If we are high precision, scroll normally
                                if (this->g_bPrecisionMode || this->g_bGameMode)
                                {
                                    g_bNoUpHandle = true;
                                }
                                else
                                {
                                    g_bNoUpHandle = false;
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            if (_bGameModeDown)
            {
                DWORD dwKey = GetDPadKey(_shGameModeDir);
                //keybd_event((BYTE)dwKey, 0, KEYEVENTF_KEYUP, 0);
                _bGameModeDown = false;
            }
            else if (!g_bNoUpHandle && !g_bKeyWasPressed)
            {
                int xDiff = xPos - g_previousPositionX;
                int yDiff = yPos - g_previousPositionY;

                if ((dwRegion == g_dwDownRegion) && abs(xDiff) < GSCROLL_SWIPE_MININTERVAL && abs(yDiff) < GSCROLL_SWIPE_MININTERVAL)
                {
                    if (!bTouchOnly)
                    {
                        DWORD dwTick = GetTickCount();
                        DWORD dwDiff = (dwTick - g_dwDownTick);
                        DWORD dwFirstDiff = (dwTick - g_dwFirstDownTick);
                        if (dwDiff < GSCROLL_TAP_MAXINTERVAL && dwDiff > GSCROLL_TAP_MININTERVAL)
                        {
                            // we have a tap here
                            DWORD dwMaxInterval = (IsWindowVisible(g_hwndTouchHandler)) ? (GSCROLL_DOUBLETAP_MAXINTERVAL*2) : GSCROLL_DOUBLETAP_MAXINTERVAL;
                            if (dwFirstDiff > dwMaxInterval || dwFirstDiff < GSCROLL_DOUBLETAP_MININTERVAL)
                            {
                                // we have a tap, but it took too long
                                ClearTaps();
                            }
                            this->RegisterTap(dwRegion);
                            g_dwFirstDownTick = g_dwDownTick;
                        }
                    }
                }
                else // this was a quick scroll
                {
                    if (!bCenter && g_previousPositionX != 0xFF && g_previousPositionY != 0xFF)
                    {
                        // Our regions don't match OR or x&y diffs are too large. We want to scroll normally if we decided to do that
                        if (abs(xDiff) > GSCROLL_SWIPE_MININTERVAL || abs(yDiff) > GSCROLL_SWIPE_MININTERVAL)
                        {
                            if (abs(xDiff) > abs(yDiff))
                            {
                                yDiff = 0;
                            }
                            else
                            {
                                xDiff = 0;
                            }
                            Scroll(GetForegroundWindow(), true, false, (abs(xDiff) > 10) ? (xDiff / abs(xDiff)) : 0, (abs(yDiff) > 10) ? (yDiff / abs(yDiff)) : 0, dwSwipeMode);
                        }
                    }
                }
            }

            // We are done here
            ResetTouchData(false);
        }
        g_bWasDown = bIsDown;
    }
}

BOOL CALLBACK EnumChildWindowsProc(HWND hwnd, LPARAM lParam)
{
    long lStyle = GetWindowLong(hwnd, GWL_STYLE);
    HWND* phWndScrollable = (HWND*)lParam;

    if ((lStyle & (WS_VSCROLL | WS_HSCROLL)) && (lStyle & WS_VISIBLE))
    {
        *phWndScrollable = hwnd;

        return FALSE;
    }
    else if (lStyle & WS_VISIBLE)
    {
        WCHAR wzClass[50];
        GetClassName(hwnd, wzClass, 50);

        if (0 == wcscmp(wzClass, L"ScrollBar"))
        {
            Sleep(0);
        }
    }

    return TRUE;
}

bool CTouchHandler::GetProcessName(DWORD dwProcessId, WCHAR wzProcessName[MAX_PATH])
{
    return ::GetProcessName(dwProcessId, wzProcessName);
}

void CTouchHandler::SetScrollSettings(HWND hWndForeground, HWND* phWndScrollable, bool* pbVScroll, bool* pbHScroll)
{
    *phWndScrollable = NULL;
    *pbVScroll = false;
    *pbHScroll = false;

    long lStyle = GetWindowLong(hWndForeground, GWL_STYLE);

    if (g_bSmoothScroll)
    {
        *phWndScrollable = hWndForeground;
    }
    else if ((lStyle & (WS_VSCROLL | WS_HSCROLL)) && (lStyle & WS_VISIBLE))
    {
        *phWndScrollable = hWndForeground;

        *pbVScroll = 0 != (lStyle & WS_VSCROLL);
        *pbHScroll = 0 != (lStyle & WS_HSCROLL);
    }
    else
    {
		if (g_bSmoothScroll)
		{
			*phWndScrollable = hWndForeground;
		}
        else if (!*phWndScrollable)
        {
            ::EnumChildWindows(hWndForeground, EnumChildWindowsProc, (LPARAM)phWndScrollable);
            
            lStyle = GetWindowLong(*phWndScrollable, GWL_STYLE);
            *pbVScroll = 0 != (lStyle & WS_VSCROLL);
            *pbHScroll = 0 != (lStyle & WS_HSCROLL);
        }
    }
}

void CTouchHandler::ScrollWithMouse(HWND hWnd, DWORD dwScrollDir, DWORD dwScrollPage, bool bPage)
{
    int xDiff = 0;
    int yDiff = 0;

    if (dwScrollDir == WM_HSCROLL)
    {
        if (dwScrollPage == SB_PAGEDOWN)
        {
            xDiff = (bPage) ? 1000 : 100;
        }
        else
        {
            xDiff = (bPage) ? -1000 : -100;
        }
    }
    else
    {
        if (dwScrollPage == SB_PAGEDOWN)
        {
            yDiff = (bPage) ? 1000 : 100;
        }
        else
        {
            yDiff = (bPage) ? -1000 : -100;
        }
    }

    const int nIterations = 3;
    RECT rect;
    GetClientRect(hWnd, &rect);

    int x = rect.left + (rect.right - rect.left) / 2;
    int y = rect.top + (rect.bottom - rect.top) / 2;

    int maxX = (rect.right - rect.left);
    int maxY = (rect.bottom - rect.top);

    if (bPage)
    {
        maxX /= 2;
        maxY /= 2;
    }
    else
    {
        maxX /= 4;
        maxY /= 4;
    }

    int xDistance = max(-maxX, min(xDiff, maxX)) / nIterations;
    int yDistance = max(-maxY, min(yDiff, maxY)) / nIterations;

    SendMessage(hWnd, WM_LBUTTONDOWN, 1, MAKELONG(x, y));

    for (int i = 0; i < nIterations; ++i)
    {
        x -= xDistance;
        y -= yDistance;
        SendMessage(hWnd, WM_MOUSEMOVE, 1, MAKELONG(x, y));
        if (i < nIterations - 1)
        {
            Sleep(20);
        }
    }
    SendMessage(hWnd, WM_LBUTTONUP, 1, MAKELONG(x, y));
}

void CTouchHandler::HandleKeyPress(HWND hWnd, bool bPage, bool bCanScroll, bool bCanScrollAny, DWORD dwScrollDir, DWORD dwScrollPage, DWORD dwScrollLine, DWORD dwKey, DWORD dwMode, short iMultiplier)
{
    if (dwMode)
    {
        // keep it awake
        SystemIdleTimerReset();
    }
    switch (dwMode)
    {
    case 0:
        {
            return;
        }
    case 1:
        {
            if (bCanScroll && hWnd)
            {
                for (int i = 0; i < iMultiplier; ++i)
                {
                    int x = SendMessage(hWnd, dwScrollDir, (bPage) ? dwScrollPage : dwScrollLine, (LPARAM)hWnd);
					Sleep(0);
                }
            }
            else if (hWnd && !bCanScrollAny)
            {
                ScrollWithMouse(hWnd, dwScrollDir, dwScrollPage, bPage);
            }
            return;
        }
    case 2:
        {
            keybd_event((BYTE)dwKey, 0, 0, 0);
            keybd_event((BYTE)dwKey, 0, KEYEVENTF_KEYUP, 0);
            return;
        }
    case 3:
        {
            if (bCanScroll && hWnd)
            {
                for (int i = 0; i < iMultiplier; ++i)
                {
                    SendMessage(hWnd, dwScrollDir, (bPage) ? dwScrollPage : dwScrollLine, (LPARAM)hWnd);
                }
            }
            else if (hWnd && !bCanScrollAny)
            {
                ScrollWithMouse(hWnd, dwScrollDir, dwScrollPage, bPage);
            }
            else
            {
                keybd_event((BYTE)dwKey, 0, 0, 0);
                keybd_event((BYTE)dwKey, 0, KEYEVENTF_KEYUP, 0);
            }
            return;
        }
    }
}

void CTouchHandler::LaunchApplication(WCHAR* wzPath)
{
	bool bHandled = true;
    if (this->g_bLaunchNotify)
    {
        LedOn(1);
        Sleep(50);
        LedOff(1);
    }

    if (wzPath && wcslen(wzPath) > 0)
    {
        if (wcsstr(wzPath, L"::"))
        {
            WCHAR* wzCommand = wzPath+2;
                
                if (wzCommand == wcsstr(wzCommand, L"type "))
                {
                    WCHAR* wzTypeText = wzCommand + wcslen(L"type ");

                    // Type here
                    int uiLen = wcslen(wzTypeText);

                    UINT uiKeyState[1];
                    UINT uiKeyDown[1];
                    UINT KeyStateDownFlag= 0x0080;
                    UINT KeyShiftDeadFlag= 0x20000;

                    for (int i = 0; i < uiLen; ++i)
                    {
                        uiKeyDown[0] = (UINT)wzTypeText[i];
                        uiKeyState[0] = KeyStateDownFlag;
                        PostKeybdMessage((HWND)-1, NULL, KeyStateDownFlag, 1, &uiKeyState[0], &uiKeyDown[0]);
                        uiKeyState[0] = KeyShiftDeadFlag;
                        PostKeybdMessage((HWND)-1, NULL, KeyShiftDeadFlag, 1, &uiKeyState[0], &uiKeyDown[0]);
                    }
                }
                else if (0 == wcsicmp(L"cursor", wzCommand))
                {
                    g_bMouseMode = true;
                }
                else if (0 == wcsicmp(L"ok", wzCommand))
                {
                    HWND hwnd = GetForegroundWindow();
                    SendMessage(hwnd, WM_COMMAND, IDOK, 0);
                    SendMessage(hwnd, WM_COMMAND, IDCANCEL, 0);
                }
                else if (0 == wcsicmp(L"appswitch", wzCommand))
                {
                    this->AltTab(GetForegroundWindow(), 0);
                }
                else if (0 == wcsicmp(L"appswitchnext", wzCommand))
                {
                    this->AltTab(GetForegroundWindow(), 1);
                }
                else if (0 == wcsicmp(L"appswitchprev", wzCommand))
                {
                    this->AltTab(GetForegroundWindow(), -1);
                }
                else if (0 == wcsicmp(L"leftsoft", wzCommand))
                {
                    keybd_event(VK_TSOFT1, 0, 0, 0);
                    keybd_event(VK_TSOFT1, 0, KEYEVENTF_KEYUP, 0);
                }
                else if (0 == wcsicmp(L"rightsoft", wzCommand))
                {
                    keybd_event(VK_TSOFT2, 0, 0, 0);
                    keybd_event(VK_TSOFT2, 0, KEYEVENTF_KEYUP, 0);
                }
                else if (0 == wcsicmp(L"taptilt", wzCommand))
                {
                    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLDOWN);
                    SetEvent(hEvent);
                    CloseHandle(hEvent);
                }
                else if (0 == wcsicmp(L"lock", wzCommand))
                {
                    SHDeviceLockAndPrompt();
                }
                else if (0 == wcsicmp(L"start", wzCommand))
                {
                    keybd_event(VK_LWIN, 0, 0, 0);
                    keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
                }
                else if (0 == wcsicmp(L"minimize", wzCommand))
                {
                    WCHAR wzProcess[200], wzClassName[50];
                    HWND hwnd = GetForegroundWindow();
                    DWORD dwProcessId = 0;
                    GetWindowThreadProcessId(hwnd, &dwProcessId);
                    GetProcessName(dwProcessId, wzProcess);
                    GetClassName(hwnd, wzClassName, 50);

                    g_AltTab.MinimizeProcess(hwnd, wzProcess, wzClassName);
                }
                else if (0 == wcsicmp(L"close", wzCommand))
                {
                    WCHAR wzProcess[200], wzClassName[50];
                    HWND hwnd = GetForegroundWindow();
                    DWORD dwProcessId = 0;
                    GetWindowThreadProcessId(hwnd, &dwProcessId);
                    GetProcessName(dwProcessId, wzProcess);
                    GetClassName(hwnd, wzClassName, 50);

                    g_AltTab.CloseProcess(hwnd, wzProcess, wzClassName);
                }
        }
        else
        {
            FILE* hFile = _wfopen(wzPath, L"r");

            if (hFile)
            {
                fclose(hFile);
                
                // Notify of application launch
                LaunchProgram(wzPath, L"");
            }
            else
            {
				bHandled = false;
            }
        }
    }
    else
    {
		bHandled = false;
    }

	if (!bHandled)
	{
		// Link not found
		if (IDYES == MessageBox(NULL, L"The program assigned to launch from this hot key could not be found. Would you like to change it now?", L"Launch Target Not Found", MB_YESNO | MB_TOPMOST))
		{
			LaunchProgram(L"\\Windows\\ctlpnl.exe", L"GScrollConfig.cpl");
		}
	}
}

void CTouchHandler::ScrollRight(HWND hWnd, bool bPage, bool bForceKeyDown, DWORD dwMode, short iMultiplier)
{
    HWND hWndScrollable = hWnd;
    bool bHScroll = false, bVScroll = false;
    SetScrollSettings(hWnd, &hWndScrollable, &bVScroll, &bHScroll);

    HandleKeyPress(hWndScrollable, bPage, bHScroll, bHScroll || bVScroll, WM_HSCROLL, SB_PAGEDOWN, SB_LINEDOWN, VK_RIGHT, dwMode, iMultiplier);
}
void CTouchHandler::ScrollLeft(HWND hWnd, bool bPage, bool bForceKeyDown, DWORD dwMode, short iMultiplier)
{
    HWND hWndScrollable = hWnd;
    bool bHScroll = false, bVScroll = false;
    SetScrollSettings(hWnd, &hWndScrollable, &bVScroll, &bHScroll);

    HandleKeyPress(hWndScrollable, bPage, bHScroll, bHScroll || bVScroll, WM_HSCROLL, SB_PAGEUP, SB_LINEUP, VK_LEFT, dwMode, iMultiplier);
}
void CTouchHandler::ScrollDown(HWND hWnd, bool bPage, bool bForceKeyDown, DWORD dwMode, short iMultiplier)
{
    HWND hWndScrollable = hWnd;
    bool bHScroll = false, bVScroll = false;
    SetScrollSettings(hWnd, &hWndScrollable, &bVScroll, &bHScroll);

    HandleKeyPress(hWndScrollable, bPage, bVScroll, bHScroll || bVScroll, WM_VSCROLL, SB_PAGEDOWN, SB_LINEDOWN, VK_DOWN, dwMode, iMultiplier);
}
void CTouchHandler::ScrollUp(HWND hWnd, bool bPage, bool bForceKeyDown, DWORD dwMode, short iMultiplier)
{
    HWND hWndScrollable = hWnd;
    bool bHScroll = false, bVScroll = false;
    SetScrollSettings(hWnd, &hWndScrollable, &bVScroll, &bHScroll);

    HandleKeyPress(hWndScrollable, bPage, bVScroll, bHScroll || bVScroll, WM_VSCROLL, SB_PAGEUP, SB_LINEUP, VK_UP, dwMode, iMultiplier);
}

bool CTouchHandler::AltTab(HWND hWnd, int iDir)
{
    if (g_bInAltTab)
    {
        return false;
    }
    else
    {
        g_bInAltTab = true;
        int iResult = 0;
        
        g_AltTab.Show(iDir, this);

        g_bInAltTab = false;

        return true;
    }
}

void CTouchHandler::RegisterTap(short iTap)
{
	if (g_disableLaunchGestures)
	{
		return;
	}
    g_typeList.push_back(iTap);

    WCHAR wzLaunch[10];
    ZeroMemory(wzLaunch, sizeof(WCHAR)*10);

    int i = 0;
    list<short>::iterator iter = g_typeList.begin();
    while (iter != g_typeList.end())
    {
        wzLaunch[i++] = (WCHAR)((short)L'0' + *iter);
        iter++;
    }

    bool bExists = false;
    if (false && g_wzLaunchList.find(wzLaunch) != g_wzLaunchList.end())
    {
        bExists = true;
        // Execute it here
        Show();
        Sleep(250);
        ClearTaps();
        LaunchApplication(g_wzLaunchList[wzLaunch]);
        g_typeList.clear();
    }
    else
    {
        Show();
    }
}

void CTouchHandler::ClearTaps()
{
    g_typeList.clear();
    Hide();
}

void CTouchHandler::Hide()
{
    SendMessage(g_hwndTouchHandler, WM_HIDETOUCHHANDLER, 0, 0);
}

void CTouchHandler::Show()
{
    if (g_typeList.size() > 0)
    {
        // Show the window here
        BOOL isVisible = IsWindowVisible(g_hwndTouchHandler);
        SendMessage(g_hwndTouchHandler, WM_SHOWTOUCHHANDLER, 0, 0);
    }
    Sleep(0);
}

#define ICON_IMAGE_SIZE 96
#define ICON_GRID_SIZE (ICON_IMAGE_SIZE+8)
#define ICON_RECT_SIZE (ICON_GRID_SIZE+8)
#define ICON_OPTION_HEIGHT 52
#define ICON_OPTION_WIDTH 400
#define ICON_OPTION_ICON 48

LRESULT CTouchHandler::TouchHandlerProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{
    CTouchHandler* p_this = (CTouchHandler*)GetWindowLong(hwndDlg, GWL_USERDATA);
    static HDC hdcBack = NULL;
    static HBITMAP hbmpBack = NULL;
    static HGDIOBJ hbmpBackOld = NULL;

    static HBITMAP hBmpHome = NULL;
    static HBITMAP hBmpBack = NULL;
    static HBITMAP hBmpEnd = NULL;
    static HBITMAP hBmpCall = NULL;
    static HBITMAP hBmpWheel = NULL;

    static HBITMAP hBmpHome_Sm = NULL;
    static HBITMAP hBmpBack_Sm = NULL;
    static HBITMAP hBmpEnd_Sm = NULL;
    static HBITMAP hBmpCall_Sm = NULL;
    static HBITMAP hBmpWheel_Sm = NULL;

    static int _width = ICON_RECT_SIZE;
    static int _height = ICON_RECT_SIZE;
    static WCHAR _wzLaunch[10];
    static int _iCount = 0;
    static int _iMatchCount = 0;
    static WCHAR* _wzApp = NULL;

	static HRGN _hRgn = NULL;

    switch (message) 
    {
		case (WM_USER+4) :
		{
			p_this->AltTab(GetForegroundWindow(), 0);
		}
		return 1;
        case WM_CREATE:
            {
                SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, ICON_RECT_SIZE, ICON_RECT_SIZE, SWP_HIDEWINDOW | SWP_NOACTIVATE);
                //SetTimer(hwndDlg, 1, 500, NULL);
            }
            break;
        case WM_DESTROY :
            {
                if (hBmpHome)
                {
                    DeleteObject(hBmpHome);
                    DeleteObject(hBmpBack);
                    DeleteObject(hBmpCall);
                    DeleteObject(hBmpEnd);
                    DeleteObject(hBmpWheel);
                }
            }
            break;
        case WM_SHOWTOUCHHANDLER :
            {
                if (!hBmpHome)
                {
                    hBmpHome = LoadBitmap(p_this->g_hInstance, MAKEINTRESOURCE(IDB_HOME));
                    hBmpBack = LoadBitmap(p_this->g_hInstance, MAKEINTRESOURCE(IDB_BACK));
                    hBmpCall = LoadBitmap(p_this->g_hInstance, MAKEINTRESOURCE(IDB_CALL));
                    hBmpEnd = LoadBitmap(p_this->g_hInstance, MAKEINTRESOURCE(IDB_END));
                    hBmpWheel = LoadBitmap(p_this->g_hInstance, MAKEINTRESOURCE(IDB_WHEEL));
                    hBmpHome_Sm = LoadBitmap(p_this->g_hInstance, MAKEINTRESOURCE(IDB_SM_HOME));
                    hBmpBack_Sm = LoadBitmap(p_this->g_hInstance, MAKEINTRESOURCE(IDB_SM_BACK));
                    hBmpCall_Sm = LoadBitmap(p_this->g_hInstance, MAKEINTRESOURCE(IDB_SM_CALL));
                    hBmpEnd_Sm = LoadBitmap(p_this->g_hInstance, MAKEINTRESOURCE(IDB_SM_END));
                    hBmpWheel_Sm = LoadBitmap(p_this->g_hInstance, MAKEINTRESOURCE(IDB_SM_WHEEL));
                }
				if (!p_this)
				{
					Sleep(0);
				}
                int width = GetSystemMetrics(SM_CXSCREEN);
                int height = GetSystemMetrics(SM_CYSCREEN);
                _iCount = p_this->g_typeList.size();

                // calculate matches
                list<short>::iterator iter = p_this->g_typeList.begin();

                _iMatchCount = 0;
                int i = 0;
                ZeroMemory(_wzLaunch, 10 * sizeof(WCHAR));
                while (iter != p_this->g_typeList.end())
                {
                    _wzLaunch[i++] = (WCHAR)((short)L'0' + *iter);
                    iter++;
                }
                hash_map<WCHAR*, WCHAR*>::iterator hashIter = p_this->g_wzLaunchList.begin();

                while (hashIter != p_this->g_wzLaunchList.end())
                {
                    bool bNoMatch = false;
                    if (wcslen(hashIter->first) < wcslen(_wzLaunch))
                    {
                        bNoMatch = true;
                    }
                    else
                    {
                        for (int j = 0; j < i; ++j)
                        {
                            if (hashIter->first[j] && hashIter->first[j] != _wzLaunch[j])
                            {
                                bNoMatch = true;
                                break;
                            }
                        }
                    }
                    if (!bNoMatch)
                    {
                        _iMatchCount++;
                    }
                    hashIter++;
                }

                _wzApp = NULL;
                if (p_this->g_wzLaunchList.find(_wzLaunch) != p_this->g_wzLaunchList.end())
                {
                    _wzApp = p_this->g_wzLaunchList[_wzLaunch];
                }

				if (p_this->g_typeList.size() == 1 && _iMatchCount == 0)
				{
					p_this->ClearTaps();
				}
				else
				{
					int iMatchCount = _iMatchCount;
					_iMatchCount = max(_iMatchCount, 1);
					int winWidth = max(ICON_RECT_SIZE*_iCount, ICON_OPTION_WIDTH);
					int winHeight = ICON_RECT_SIZE + (_iMatchCount * ICON_OPTION_HEIGHT);
					int thumbWidth = ICON_RECT_SIZE*_iCount;
					int xDiff = (winWidth - (thumbWidth)) / 2;

					if (_hRgn)
					{
						SetWindowRgn(hwndDlg, NULL, FALSE);
						DeleteObject(_hRgn);
						_hRgn = NULL;
					}
					_hRgn = CreateRectRgn(xDiff, 0, xDiff + ICON_RECT_SIZE*_iCount, ICON_RECT_SIZE);
					HRGN hRgn2 = CreateRectRgn(0, ICON_RECT_SIZE, winWidth, winHeight);
					CombineRgn(_hRgn, _hRgn, hRgn2, RGN_OR);
					DeleteObject(hRgn2);
					SetWindowRgn(hwndDlg, _hRgn, FALSE);
					//DeleteObject(hRgn);

					SetWindowPos(hwndDlg, HWND_TOPMOST, (width - winWidth) / 2, (height - ICON_RECT_SIZE - ICON_OPTION_HEIGHT) / 2, winWidth, winHeight, SWP_SHOWWINDOW);
					InvalidateRect(hwndDlg, NULL, TRUE);
					UpdateWindow(hwndDlg);

					if (iMatchCount == 1 && _wzApp)
					{
						SetTimer(hwndDlg, 1, GSCROLL_DOUBLETAP_MAXINTERVAL / 4, 0);
					}
					else if (iMatchCount == 1 || _wzApp)
					{
						SetTimer(hwndDlg, 1, GSCROLL_DOUBLETAP_MAXINTERVAL, 0);
					}
					else if (iMatchCount == 0)
					{
						SetTimer(hwndDlg, 1, GSCROLL_DOUBLETAP_MAXINTERVAL, 0);
					}
					else
					{
						SetTimer(hwndDlg, 1, GSCROLL_DOUBLETAP_MAXINTERVAL * 2, 0);
					}
				}
            }
            break;
        case WM_TIMER :
            {
                p_this->ClearTaps();
                if (_wzApp)
                {
                    p_this->LaunchApplication(_wzApp);
                }
            }
            break;
        case WM_HIDETOUCHHANDLER :
            {
                ShowWindow(hwndDlg, SW_HIDE);
                KillTimer(hwndDlg, 1);
                if (hbmpBack)
                {
                    SelectObject(hdcBack, hbmpBackOld);
                    DeleteObject(hbmpBack);
                    hbmpBack = NULL;
                }
                if (hdcBack)
                {
                    DeleteDC(hdcBack);
                    hdcBack = NULL;
                }
            }
            break;
        case WM_PAINT :
            {
                RECT windowRect;
                GetWindowRect(hwndDlg, &windowRect);

                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwndDlg, &ps);
                // draw in images and text here

                int width = windowRect.right - windowRect.left;
                int height = (windowRect.bottom - windowRect.top);
                int thumbWidth = ICON_RECT_SIZE*_iCount;
                int xDiff = (width - (thumbWidth)) / 2;

                HDC hdcBuffer = CreateCompatibleDC(hdc);
                HBITMAP hbmpBuffer = CreateCompatibleBitmap(hdc, width, height);
                HGDIOBJ hOldBuffer = SelectObject(hdcBuffer, hbmpBuffer);

                StretchBlt(hdcBuffer, 0, 0, width, height, hdcBack, windowRect.left, windowRect.top, width, height, SRCCOPY);

                // draw icons here
                list<short>::iterator iter;

                int padding = (ICON_RECT_SIZE - ICON_IMAGE_SIZE) / 2;
                int left = padding + xDiff;
                int top = padding;

                HDC hdcIcon = CreateCompatibleDC(hdcBuffer);
                HDC hdcGrid = CreateCompatibleDC(hdcBuffer);
                HBITMAP hbmpGrid = CreateCompatibleBitmap(hdcBuffer, ICON_GRID_SIZE, ICON_GRID_SIZE);
                HGDIOBJ hGridOld = SelectObject(hdcGrid, hbmpGrid);

                RECT fillRect;
                fillRect.top = 0;
                fillRect.left = xDiff;
                fillRect.right = fillRect.left+thumbWidth;
                fillRect.bottom = ICON_RECT_SIZE;
                FillRect(hdcBuffer, &fillRect, (HBRUSH)GetStockObject(BLACK_BRUSH));

                fillRect.left = 0;
                fillRect.right = width;
                fillRect.top = fillRect.bottom;
                fillRect.bottom = fillRect.bottom + ICON_OPTION_HEIGHT * _iMatchCount;
                FillRect(hdcBuffer, &fillRect, (HBRUSH)GetStockObject(WHITE_BRUSH));

                BLENDFUNCTION blendFunc;
                blendFunc.AlphaFormat = 0;
                blendFunc.BlendFlags = 0;
                blendFunc.BlendOp = AC_SRC_OVER;
                blendFunc.SourceConstantAlpha = 50;

                AlphaBlend(hdcBuffer, 0, 0, width, height, hdcBack, windowRect.left, 0, width, height, blendFunc);

                SelectObject(hdcBuffer, GetStockObject(NULL_BRUSH));
                SelectObject(hdcBuffer, GetStockObject(BLACK_PEN));
                Rectangle(hdcBuffer, fillRect.left, fillRect.top, fillRect.right, fillRect.bottom);

                RECT rectGrid;
                rectGrid.left = rectGrid.top = 0;
                rectGrid.bottom = ICON_GRID_SIZE;
                rectGrid.right = ICON_GRID_SIZE;
                FillRect(hdcGrid, &rectGrid, (HBRUSH)GetStockObject(WHITE_BRUSH));

                //BLENDFUNCTION blendFunc;
                blendFunc.AlphaFormat = 0;
                blendFunc.BlendFlags = 0;
                blendFunc.BlendOp = AC_SRC_OVER;
                blendFunc.SourceConstantAlpha = 25;

                int gridPadding = (ICON_GRID_SIZE - ICON_IMAGE_SIZE) / 2;

                for (iter = p_this->g_typeList.begin(); iter != p_this->g_typeList.end(); ++iter)
                {
                    HBITMAP hBmp = NULL;
                    switch (*iter)
                    {
                        case 0 :
                        {
                            hBmp = hBmpHome;
                        }
                        break;
                        case 1 :
                        {
                            hBmp = hBmpCall;
                        }
                        break;
                        case 2 :
                        {
                            hBmp = hBmpBack;
                        }
                        break;
                        case 3 :
                        {
                            hBmp = hBmpEnd;
                        }
                        break;
                        case 4 :
                        {
                            hBmp = hBmpWheel;
                        }
                        break;
                    }

                    // Alpha blend rect here...
                    AlphaBlend(hdcBuffer, left - gridPadding, top - gridPadding, ICON_GRID_SIZE, ICON_GRID_SIZE, hdcGrid, 0, 0, ICON_GRID_SIZE, ICON_GRID_SIZE, blendFunc);

                    // Draw Icon here
                    HGDIOBJ hOld = SelectObject(hdcIcon, hBmp);
                    TransparentBlt(hdcBuffer, left, top, ICON_IMAGE_SIZE, ICON_IMAGE_SIZE, hdcIcon, 0, 0, ICON_IMAGE_SIZE, ICON_IMAGE_SIZE, RGB(0,0,0));
                    SelectObject(hdcIcon, hOld);
                    left += ICON_RECT_SIZE;
                }

                SelectObject(hdcGrid, hGridOld);
                DeleteObject(hbmpGrid);
                DeleteDC(hdcGrid);

                RECT textRect;
                textRect.top = fillRect.top;
                textRect.left = 5;
                textRect.right = width - 5;
                textRect.bottom = textRect.top + ICON_OPTION_HEIGHT;

                LOGFONT lf;
                ZeroMemory(&lf, sizeof(lf));
                lf.lfHeight = -(ICON_OPTION_HEIGHT/2);
                lf.lfWeight = FW_BOLD;
                lf.lfCharSet = DEFAULT_CHARSET;
                HFONT hFont = CreateFontIndirect(&lf);

                SetBkMode(hdcBuffer, TRANSPARENT);
                SetBkColor(hdcBuffer, TRANSPARENT);
                HGDIOBJ hOldFont = SelectObject(hdcBuffer, hFont);
                //DrawText(hdcBuffer, pNodeActive->wzText, wcslen(pNodeActive->wzText), &rect, DT_VCENTER | DT_CENTER);

                int iDrawn = 0;
                int i = _iCount;
                hash_map<WCHAR*, WCHAR*>::iterator hashIter = p_this->g_wzLaunchList.begin();
                while (hashIter != p_this->g_wzLaunchList.end())
                {
                    bool bNoMatch = false;
                    int j;
                    if (wcslen(hashIter->first) < wcslen(_wzLaunch))
                    {
                        bNoMatch = true;
                    }
                    else
                    {
                        for (j = 0; j < i; ++j)
                        {
                            if (hashIter->first[j] && hashIter->first[j] != _wzLaunch[j])
                            {
                                bNoMatch = true;
                                break;
                            }
                        }
                    }

                    if (!bNoMatch)
                    {
                        j = wcslen(hashIter->first);
                        left = textRect.right - (ICON_OPTION_ICON * j);
                        top = textRect.top + (ICON_OPTION_HEIGHT - ICON_OPTION_ICON) / 2;

                        WCHAR* wzIter = hashIter->first;

                        while (wzIter[0])
                        {
                            HBITMAP hBmp;
                            switch (wzIter[0])
                            {
                                case '0' :
                                {
                                    hBmp = hBmpHome_Sm;
                                }
                                break;
                                case '1' :
                                {
                                    hBmp = hBmpCall_Sm;
                                }
                                break;
                                case '2' :
                                {
                                    hBmp = hBmpBack_Sm;
                                }
                                break;
                                case '3' :
                                {
                                    hBmp = hBmpEnd_Sm;
                                }
                                break;
                                case '4' :
                                {
                                    hBmp = hBmpWheel_Sm;
                                }
                                break;
                            }
                            HGDIOBJ hOld = SelectObject(hdcIcon, hBmp);
                            TransparentBlt(hdcBuffer, left, top, ICON_OPTION_ICON, ICON_OPTION_ICON, hdcIcon, 0, 0, ICON_OPTION_ICON, ICON_OPTION_ICON, RGB(0xff,0xff,0xff));
                            SelectObject(hdcIcon, hOld);
                            wzIter++;
                            left += ICON_OPTION_ICON;
                        }

                        WCHAR wzAction[MAX_PATH];
                        wcscpy(wzAction, hashIter->second);
                        WCHAR* wzText = wcsrchr(wzAction, '\\');
                        if (!wzText)
                        {
                            wzText = wzAction;
                        }
                        else
                        {
                            wzText++;
                            WCHAR* wzDot = wcsrchr(wzText, '.');
                            if (wzDot)
                            {
                                wzDot[0] = 0;
                            }
                        }
                        DrawText(hdcBuffer, wzText, wcslen(wzText), &textRect, DT_VCENTER | DT_END_ELLIPSIS);

                        iDrawn++;
                        textRect.top += ICON_OPTION_HEIGHT;
                        textRect.bottom += ICON_OPTION_HEIGHT;
                    }
                    hashIter++;
                }
                if (!iDrawn)
                {
                    DrawText(hdcBuffer, L"--No Matches--", wcslen(L"--No Matches--"), &textRect, DT_VCENTER | DT_CENTER);
                }
                SelectObject(hdcBuffer, hOldFont);
                DeleteObject(hFont);
                DeleteDC(hdcIcon);

                //RECT fillRect;

                BitBlt(hdc, 0, 0, width, height, hdcBuffer, 0, 0, SRCCOPY);

                SelectObject(hdcBuffer, hOldBuffer);
                DeleteObject(hbmpBuffer);
                DeleteDC(hdcBuffer);
                EndPaint(hwndDlg, &ps);

                DeleteDC(hdc);
            }
            return 1;
        case WM_ERASEBKGND:
            {
                RECT windowRect;
                GetWindowRect(hwndDlg, &windowRect);

                if (NULL == hdcBack)
                {
                    int width = GetSystemMetrics(SM_CXSCREEN);
                    //int height = GetSystemMetrics(SM_CYSCREEN);
                    int height = (windowRect.bottom - windowRect.top);

                    HWND hwndDesktop = HWND_DESKTOP;

                    HDC hdcDesk = GetDC(hwndDesktop);
                    hdcBack = CreateCompatibleDC(hdcDesk);
                    hbmpBack = CreateCompatibleBitmap(hdcDesk, width, height);
                    hbmpBackOld = SelectObject(hdcBack, hbmpBack);

                    int thumbWidth = ICON_RECT_SIZE*_iCount;
                    int xDiff = (width - (thumbWidth)) / 2;

                    StretchBlt(hdcBack, 0, 0, width, height, hdcDesk, 0, windowRect.top, width, height, SRCCOPY);

                    // Draw highlight areas here
                    DeleteDC(hdcDesk);
                }
            }
            return 1;
    }
    return DefWindowProc(hwndDlg, message, wParam, lParam);
}