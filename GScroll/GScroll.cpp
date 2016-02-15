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
#include "GScroll.h"
#include "Sensor.h"
#include "RPN.h"
#include "resource.h"
#include "touchhandler.h"

// Modes
// 0 = disabled
// 1 = scroll only
// 2 = keydown only
// 3 = smart mode

void ActiveApplicationCallback(HREGNOTIFY hNotifyActiveApplication, DWORD dwUserData, const PBYTE pData, const UINT cbData)
{
    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLFOCUS);
    SetEvent(hEvent);
    CloseHandle(hEvent);
}

void ActiveCallCountCallback(HREGNOTIFY hNotifyActiveApplication, DWORD dwUserData, const PBYTE pData, const UINT cbData)
{
    if (!pData)
    {
        g_ScrollApp->g_bInActiveCall = false;
    }
    else
    {
        DWORD dwData = *((DWORD*)pData);
        g_ScrollApp->g_bInActiveCall = (dwData != 0);
    }
}

DWORD WINAPI HandleGPoll(LPVOID lpParameter)
{
    SENSORDATATILT sensorData = {0}, normalizedData = {0};
    HANDLE hSensor = NULL;
    DWORD dwEvent;
    DWORD dwLastTick = 0;

    HANDLE hEvent[2];
    hEvent[0] = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLDOWN);
    hEvent[1] = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLEXIT);

    while (WAIT_ABANDONED_0 != (dwEvent = WaitForMultipleObjects(2, hEvent, FALSE, INFINITE)))
    {
        if (dwEvent == WAIT_OBJECT_0)
        {
            if (NULL == hSensor)
            {
                SensorInit(SENSOR_TILT_PORT, &hSensor);

                if (NULL == hSensor)
                {
                    MessageBox(NULL, L"Error Opening GSensor", L"Error Opening GSensor", MB_OK);
                    ResetEvent(hEvent[1]);
                }
            }
            if (hSensor)
            {
                if (g_ScrollApp->g_bNormalized)
                {
                    // only poll every 50 seconds
                    if ((GetTickCount() - dwLastTick) > 50)
                    {
                        SensorGetDataTilt(hSensor, &sensorData);
                        dwLastTick = GetTickCount();
                    }

                    long shXDiff = sensorData.TiltX - normalizedData.TiltX;
                    long shYDiff = sensorData.TiltY - normalizedData.TiltY;
                    
                    DEVMODE deviceMode;
    
                    memset(&deviceMode, NULL, sizeof(DEVMODE));
                    deviceMode.dmSize = sizeof(DEVMODE);
                    deviceMode.dmFields = DM_DISPLAYORIENTATION;

                    int orientation = 0;

                    if (DISP_CHANGE_SUCCESSFUL == ChangeDisplaySettingsEx(NULL, &deviceMode, NULL, CDS_TEST, NULL))
                    {
                        orientation = deviceMode.dmDisplayOrientation;
                    }
                    switch (orientation)
                    {
                        case 0 :
                            {
                            }
                            break;
                        case DMDO_180 :
                            {
                                shXDiff = -shXDiff;
                                shYDiff = -shYDiff;
                            }
                            break;
                        case DMDO_90 :
                            {
                                int temp = shXDiff;
                                shXDiff = -shYDiff;
                                shYDiff = temp;
                            }
                            break;
                        case DMDO_270 :
                            {
                                int temp = shXDiff;
                                shXDiff = shYDiff;
                                shYDiff = -temp;
                            }
                            break;
                    }

                    long uiSleep = 100;
                    const UINT uiMinSleep = 5;

                    if (shXDiff > GSCROLL_TILT_THRESHOLD)
                    {
                        uiSleep = (GSCROLL_TILT_SLEEP_INTERVAL / (UINT)(shXDiff / GSCROLL_TILT_STEP));

                        g_ScrollApp->ScrollRight(GetForegroundWindow(), false, false, g_ScrollApp->g_dwTiltMode, 1);
                    }
                    if (shXDiff < -GSCROLL_TILT_THRESHOLD)
                    {
                        uiSleep = (GSCROLL_TILT_SLEEP_INTERVAL / (UINT)(-shXDiff / GSCROLL_TILT_STEP));

                        g_ScrollApp->ScrollLeft(GetForegroundWindow(), false, false, g_ScrollApp->g_dwTiltMode, 1);
                    }
                    if (shYDiff > GSCROLL_TILT_THRESHOLD)
                    {
                        uiSleep = (GSCROLL_TILT_SLEEP_INTERVAL / (UINT)(shYDiff / GSCROLL_TILT_STEP));

                        g_ScrollApp->ScrollDown(GetForegroundWindow(), false, false, g_ScrollApp->g_dwTiltMode, 1);
                    }
                    if (shYDiff < -GSCROLL_TILT_THRESHOLD)
                    {
                        uiSleep = (GSCROLL_TILT_SLEEP_INTERVAL / (UINT)(-shYDiff / GSCROLL_TILT_STEP));

                        g_ScrollApp->ScrollUp(GetForegroundWindow(), false, false, g_ScrollApp->g_dwTiltMode, 1);
                    }

                    Sleep(max(uiMinSleep, uiSleep));
                }
                else
                {
                    g_ScrollApp->g_bNormalized = true;
                    SensorGetDataTilt(hSensor, &normalizedData);

                    Sleep(50);
                }
            }
            else
            {
                ResetEvent(hEvent[0]);
            }
        }
        else
        {
            break;
        }
    }
    CloseHandle(hEvent[0]);
    CloseHandle(hEvent[1]);

    if (hSensor)
    {
        SensorUninit(hSensor);
        hSensor = NULL;
    }

    return 0;
}

void Test(HTCTOUCH_RAWDATA * pData, int nSize, UINT xInt)
{
	NKDbgPrintfW(L"data");
	//HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, GSCROLL_DATA_EVENT);
	//SetEvent(hEvent);
	//CloseHandle(hEvent);
	DWORD junk;
	HTCTOUCH_RAWDATA rawData;
	static HTCTOUCH_WPARAM wParam;
	static HTCTOUCH_LPARAM lParam;
	static SHORT wheelDeg = -1;
	ZeroMemory(&rawData, sizeof(rawData));

	HANDLE hNavDevice = CreateFile(_T("NAV1:"),        // drive 
	 0,                    // no access to the drive
	 FILE_SHARE_READ, 
	 NULL,                // default security attributes
	 OPEN_EXISTING,        // disposition
	 0,                    // file attributes
	 NULL);                // do not copy file attributes

	BOOL bResult = DeviceIoControl
	(
		hNavDevice,  // device to be queried
		HTCNAV_GET_DATA_IOCTRL,  // operation to perform
		&rawData, sizeof(rawData), // input buffer
		NULL, 0,     // output buffer
		&junk,                 // # bytes returned
		(LPOVERLAPPED) NULL
	);  // synchronous I/O*/
	
	wParam.Up = !((rawData.action != 0) && (rawData.wheelDeg != 0));

	if (!wParam.Up)
	{
		wParam.xPosRP = (BYTE)rawData.xPosRP;
		wParam.yPosRP = (BYTE)rawData.yPosRP;

		lParam.xPosLP = (BYTE)rawData.xPosLP;
		lParam.yPosLP = (BYTE)rawData.yPosLP;
	}

	g_ScrollApp->ProcessTouchData(wParam, lParam, rawData.action, (byte)rawData.wheelDeg, false);
	
	CloseHandle(hNavDevice);
}

DWORD WINAPI HandleCapacative(LPVOID lpParameter)
{
    bool bDemoMode = false;
    DWORD dwStartTime = GetTickCount();
    const DWORD dwEvalTime = 1000 * 60 * 15; // 15 minutes demo

    DWORD dwDisable = 0;
    GetFromRegistry(REG_Disabled, &dwDisable);

    if (!IsDebuggerPresent())
    {
        if (dwDisable) { return -1; }
        if (!CheckIfRegistered(REG_RegstrationCode, RPN_CVALUE, RPN_VARIANT))
        {
            // enter demo mode
            bDemoMode = true;
        }
    }
	else
	{
		dwDisable = false;
	}

	if (!dwDisable)
	{
		HANDLE hEventExit = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLEXIT);
		HANDLE hEventSettings = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLSETTINGS);
		HANDLE hEventDown = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLDOWN);
		HANDLE hEvents[3];
		hEvents[0] = hEventExit;
		hEvents[1] = hEventSettings;
		//hEvents[2] = hEventDown;

		DWORD dwEvent = NULL;
		int resetCount = 0;

		DWORD dwOffPeakSleepTime = 150;
		DWORD dwMinSleepTime = 50;
		DWORD dwSleepTime = dwMinSleepTime;

		HMODULE hHTCAPI = SensorRawInit(NULL, g_ScrollApp->g_bScrollWheel);
		HANDLE hNavDevice = CreateFile(_T("NAV1:"),        // drive 
		 0,                    // no access to the drive
		 FILE_SHARE_READ, 
		 NULL,                // default security attributes
		 OPEN_EXISTING,        // disposition
		 0,                    // file attributes
		 NULL);                // do not copy file attributes

		bool bWasDown = false;
		HTCTOUCH_WPARAM wParam;
		HTCTOUCH_LPARAM lParam;

		ZeroMemory(&wParam, sizeof(HTCTOUCH_WPARAM));
		ZeroMemory(&lParam, sizeof(HTCTOUCH_LPARAM));

		bool bGameMode = true;
		HTCTOUCH_RAWDATA prevData;
		ZeroMemory(&prevData, sizeof(prevData));

		int iterations = 0;
		int iRepeat = 0;
		int iOff = 0;

		bool bLastProcess = false;
		int i = 0;

		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

		HWND hwndFore = NULL;
		while (WAIT_ABANDONED_0 != (dwEvent = WaitForMultipleObjects(2, hEvents, FALSE, dwSleepTime)))
		{
			HWND hwndTemp = GetForegroundWindow();
			iterations += dwSleepTime;
			if (iterations > 10 || hwndFore != hwndTemp)
			{
				iterations = 0;
				SensorGestureInit(hHTCAPI, GetForegroundWindow(), g_ScrollApp->g_bScrollWheel, hwndTemp != hwndFore);
				hwndFore = hwndTemp;
			}
			DWORD dwCurrTime = GetTickCount();
			if (bDemoMode && (dwCurrTime < dwStartTime || dwCurrTime > (dwStartTime + dwEvalTime)))
			{
				if (IDYES == MessageBox(NULL, L"Your evaluation period has expired. Would you like to visit the GScroll website for registration information?", L"GScroll Evaluation Expired", MB_YESNO | MB_ICONASTERISK))
				{
					LaunchProgram(L"http://www.mobilesrc.com/gscroll2.aspx", L"");
				}
				break;
			}
			bool bProcess = false;
			//We should probably timeout here and do a foreground window check -- just to be safe
			if (dwEvent == WAIT_OBJECT_0) // Exit
			{
				break;
			}
			else if (dwEvent == WAIT_OBJECT_0 + 1) // settings
			{
				g_ScrollApp->LoadSettings(true);
				ResetEvent(hEventSettings);

				if (g_ScrollApp->g_def_bDisabled)
				{
					break;
				}
			}
			else if (true || dwEvent == WAIT_OBJECT_0 + 2)
			{
				if (!g_ScrollApp->g_bDisabled || IsDebuggerPresent())
				{
					DWORD junk;
					HTCTOUCH_RAWDATA rawData;
					ZeroMemory(&rawData, sizeof(rawData));

					BOOL bResult = DeviceIoControl
					(
						hNavDevice,  // device to be queried
						HTCNAV_GET_DATA_IOCTRL,  // operation to perform
						&rawData, sizeof(rawData), // input buffer
						NULL, 0,     // output buffer
						&junk,                 // # bytes returned
						(LPOVERLAPPED) NULL
					);  // synchronous I/O

					if
					(
						(rawData.action == prevData.action) &&
						(rawData.wheelDeg == prevData.wheelDeg) &&
						(rawData.xPosLP == prevData.xPosLP) &&
						(rawData.xPosRP == prevData.xPosRP) &&
						(rawData.yPosLP == prevData.yPosLP) &&
						(rawData.yPosRP == prevData.yPosRP)
					)
					{
						iRepeat++;
						Sleep(0);
					   // continue;
					}
					else
					{
						iRepeat = 0;
					}

					memcpy(&prevData, &rawData, sizeof(HTCTOUCH_RAWDATA));

					if (rawData.action)
					{
						Sleep(0);
					}

					bool bIsDown = (rawData.action != 0) && (rawData.wheelDeg != 0);
					bool bCenter = (rawData.xPosLP == 255 && rawData.yPosLP == 255) && (rawData.xPosRP == 255 && rawData.yPosRP == 255);
					bool bLeftSide = (rawData.xPosLP != 255 && rawData.yPosLP != 255);


					if (iRepeat > 5 && rawData.action && !bCenter)
					{
						Sleep(0);
					}

					if (g_ScrollApp->g_bGameMode && bIsDown)
					{
						if (!bCenter)
						{
							if (bLeftSide && rawData.xPosLP > (GSCROLL_PANEL_WIDTH - GSCROLL_GAMEMODE_PADDING))
							{
								rawData.xPosLP = 0xFF;
								rawData.yPosLP = 0xFF;

								rawData.action = HTCNAV_WHEEL_MASK_DIR;
								rawData.wheelDeg = HTCNAV_WHEEL_LEFT;
							}
							else if (!bLeftSide && rawData.xPosRP < GSCROLL_GAMEMODE_PADDING)
							{
								rawData.xPosRP = 0xFF;
								rawData.yPosRP = 0xFF;

								rawData.action = HTCNAV_WHEEL_MASK_DIR;
								rawData.wheelDeg = HTCNAV_WHEEL_RIGHT;
							}
						}
					}

					// The values on up are all 0, so we need to remember the last good down position
					if (bIsDown)
					{
						wParam.xPosRP = (BYTE)rawData.xPosRP;
						wParam.yPosRP = (BYTE)rawData.yPosRP;

						lParam.xPosLP = (BYTE)rawData.xPosLP;
						lParam.yPosLP = (BYTE)rawData.yPosLP;
						iOff = 0;
					}
					else
					{
						iOff++;
						//ResetEvent(hEventDown);
					}

					if (g_ScrollApp->g_bDisabled && !IsDebuggerPresent())
					{
						dwSleepTime = 500;
					}
					else
					{
						dwSleepTime = dwMinSleepTime;
						if (iOff > 100)
						{
							dwSleepTime = dwOffPeakSleepTime;
						}
					}

					if (bIsDown != bWasDown || bIsDown)
					{
						// Only load settings before we do something
						if (g_ScrollApp->LoadGeneralSettings(false))
						{
						    ZeroMemory(&wParam, sizeof(wParam));
						    ZeroMemory(&lParam, sizeof(lParam));
						}

						bWasDown = bIsDown;
						wParam.Up = !bIsDown;

						if (wParam.Up)
						{
							ResetEvent(hEventDown);
						}

						if (g_ScrollApp->g_bDisabled && !IsDebuggerPresent())
						{
							// drop the query time
							dwSleepTime = 500;
						}
						else
						{
							// up the query time
							dwSleepTime = dwMinSleepTime;
							g_ScrollApp->ProcessTouchData(wParam, lParam, rawData.action, (byte)rawData.wheelDeg, false);
						}
					}
				}
				else
				{
					// we are disabled...

					// Check if we have new settings
					g_ScrollApp->LoadGeneralSettings(false);
					if (!g_ScrollApp->g_bDisabled)
					{
						dwSleepTime = dwMinSleepTime;
					}
				}
			}
			else
			{
				Sleep(0);
			}
		}

		MessageBox(GetForegroundWindow(), L"GScroll is Exiting", L"GScroll Exiting", MB_OK);
		SetEvent(hEventExit);

		CloseHandle(hEventExit);
		CloseHandle(hEventSettings);
		CloseHandle(hEventDown);
		CloseHandle(hNavDevice);
	}

	PostMessage(g_ScrollApp->g_hwndTouchHandler, WM_QUIT, 0, 0);

	return 0;
}
#define WM_LAUNCHALTTAB	   (WM_USER + 4)

int _tmain(int argc, _TCHAR* argv[])
{
    DWORD dwDisable = 0;
    GetFromRegistry(REG_Disabled, &dwDisable);

    if (!IsDebuggerPresent())
    {
        if (dwDisable) { return -1; }
	}
	//HANDLE h = CreateEventW(NULL, TRUE, TRUE, L"HTCNAV_EVENT_DATAREADY");
	//Sleep(0);
	//SensorGestureInit(NULL, GetForegroundWindow(), true);
	//HANDLE h = OpenEvent(0, FALSE, L"HTCNAV_EVENT_DATAREADY");
	//return 0;
    MSG msg;
    /*
    // The beta won't check for keys but will expire after 30 days or something
    SYSTEMTIME curTime;
    GetSystemTime(&curTime);

    // May 15
    if (((curTime.wMonth > 4) && (curTime.wDay > 14)) || (curTime.wYear > 2009))
    {
        MessageBox(NULL, L"The GScroll beta period has expired. Visit http://www.mobilesrc.com/ for more information.", L"GScroll Beta Expired", MB_OK | MB_ICONWARNING);
        return 0;
    }
    //BOOL bSuccess = CeSetThreadPriority(GetCurrentThread(), 100);
	*/
    HANDLE hEvent = CreateEvent(NULL, FALSE, TRUE, EVENT_GSCROLL_MUTEX);

    if (!hEvent)
    {
        return -1;
    }
    if (ERROR_ALREADY_EXISTS == GetLastError())
    {
        // the event already exists
        CloseHandle(hEvent);

		HWND hwndAlt = FindWindow(L"mobilesrcTouchHandler", NULL);

        if (hwndAlt)
        {
			PostMessage(hwndAlt, WM_LAUNCHALTTAB, 0, 0);
        }

        return -2;
    }
	g_ScrollApp = new CTouchHandler();
    g_ScrollApp->Initialize();

    NOTIFICATIONCONDITION nc;
    HREGNOTIFY hNotifyActiveCallCount = NULL;
    // Receive a notification whenever that bit toggles.
    nc.ctComparisonType = REG_CT_ANYCHANGE;
    nc.TargetValue.dw = 0;

    RegistryNotifyCallback(SN_ACTIVECALLCOUNT_ROOT, SN_ACTIVECALLCOUNT_PATH, SN_ACTIVECALLCOUNT_VALUE, ActiveCallCountCallback, 0, NULL, &hNotifyActiveCallCount);
    
	HANDLE hEventExit = CreateEvent(NULL, TRUE, FALSE, EVENT_GSCROLLEXIT);
	HANDLE hEventSettings = CreateEvent(NULL, TRUE, FALSE, EVENT_GSCROLLSETTINGS);
	HANDLE hEventDown = CreateEvent(NULL, TRUE, FALSE, EVENT_GSCROLLDOWN);

    HANDLE hPollThread = CreateThread(NULL, 0, HandleGPoll, NULL, 0, 0);
    HANDLE hPollCapacative = CreateThread(NULL, 0, HandleCapacative, NULL, 0, 0);

    while ( GetMessage( &msg, g_ScrollApp->g_hwndTouchHandler, 0, 0 ) )
    {
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

    TerminateThread(hPollThread, 0);
    TerminateThread(hPollCapacative, 0);

	CloseHandle(hEventExit);
	CloseHandle(hEventSettings);
	CloseHandle(hEventDown);

    RegistryCloseNotification(hNotifyActiveCallCount);

	delete g_ScrollApp;

    return 0;
}