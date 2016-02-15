#include "stdafx.h"
#include <windows.h>
#include <soundfile.h>
#include <time.h>
#include <notify.h>
#include <atltime.h>
#include <regext.h>
#include "GScroll.h"
#include "Sensor.h"
#include "RPN.h"

// Modes
// 0 = disabled
// 1 = scroll only
// 2 = keydown only
// 3 = smart mode

LRESULT CALLBACK SubClass(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND hwndMessage = NULL;
	static bool bSensorsInit = false;
	static bool bAlreadyInit = false;
	static bool bGotData = false;
	static HANDLE hEventDebug = NULL;
	static HANDLE hEventReturn = NULL;
	static HANDLE hEventDown = NULL;

	HANDLE hEvent;
	switch (uMsg)
	{
		case WM_HTC_TOUCH :
			{
				if (bSensorsInit)
				{
					bGotData = true;

					HTCTOUCH_WPARAM touchW;
					HTCTOUCH_LPARAM touchL;

					memcpy(&touchW, &wParam, sizeof touchW);
					memcpy(&touchL, &lParam, sizeof touchL);

					ProcessTouchData(hwnd, touchW, touchL);
					//g_ScrollApp.SetTouchData(hwnd, touchW, touchL, hEventDebug, hEventReturn, hEventDown);
				}
			}
			break;
		case WM_CLOSE :
		case WM_DESTROY :
		case WM_KILLFOCUS :
			{
				hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLFOCUS);
				SetEvent(hEvent);
				CloseHandle(hEvent);
			}
			break;
		case WM_INITSENSORS :
			{
				bool bFirstInit = (wParam & 2);

				if (bFirstInit && !bGotData)
				{
					bSensorsInit = false;
				}

				if (!bSensorsInit)
				{
					bGotData = false;

					//hEventDebug = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLDEBUG1);
					//hEventReturn = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLRETURN);

					if (NULL == hEventDown)
					{
						hEventDown = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLDOWN);
					}

					hwndMessage = (HWND)lParam;
					DWORD dwProc;
					bool bNavReady = (wParam & 1);
					bool hasNavSensor = false;
					bool hasOtherSensor = false;

					if (bNavReady)
					{
						hasNavSensor = true;
					}
					//else
					//{
					//	GetWindowThreadProcessId(hwnd, &dwProc);

					//	HANDLE hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE,	dwProc );
					//	MODULEENTRY32 pe32 = { 0 };
					//	pe32.dwSize = sizeof( MODULEENTRY32 );

					//	Module32First(hProcessSnap, &pe32);
					//	do
					//	{
					//		if (0 != wcsstr(_wcslwr(pe32.szModule), L"htcapi.dll"))
					//		{
					//			hasNavSensor = true;
					//			break;
					//		}
					//		else if (0 != wcsstr(_wcslwr(pe32.szModule), L"htcsensorsdk.dll"))
					//		{
					//			hasOtherSensor = true;
					//			break;
					//		}

					//	} while( Module32Next( hProcessSnap, &pe32 ) );
					//	CloseToolhelp32Snapshot( hProcessSnap );
					//}

					if (!SensorGestureInit(hwnd, hasNavSensor, hasOtherSensor))
					{
						return 1;
					}
					bSensorsInit = true;
				}
			}
			return -1;
		case WM_INITSENSORS2 :
			{
				return (bSensorsInit) ? 1 : 0;
			}
			break;
		case WM_UNINITSENSORS :
			{
				bGotData = false;
				if (hEventDebug)
				{
					CloseHandle(hEventDebug);
					hEventDebug = NULL;
				}
				if (hEventReturn)
				{
					CloseHandle(hEventReturn);
					hEventReturn = NULL;
				}
				if (hEventDown)
				{
					CloseHandle(hEventDown);
					hEventDown = NULL;
				}
				if (bSensorsInit)
				{
					//SensorGestureUninit();
				}
				bAlreadyInit = false;
				bSensorsInit = false;
			}
			return -1;
	}
	return CallWindowProc(g_ScrollApp.g_lOldProc, hwnd, uMsg, wParam, lParam);
}

inline void ProcessTouchData(HWND hwnd, HTCTOUCH_WPARAM touchW, HTCTOUCH_LPARAM touchL)
{
	static DWORD g_dwDownTick = 0;
	static DWORD g_dwLastTick = 0;
	static DWORD g_dwLastRegion = -1;
	static DWORD g_dwRegion = -1;
	static bool g_bSingleTap = false, g_bLeftSide = false;
	static int g_previousPositionX = -1, g_previousPositionY = -1;
	HANDLE hEvent;
	DWORD dwBytesRead = 0, dwFlags = 0;

	g_ScrollApp.g_bNormalized = false;

	if (!g_ScrollApp.g_bInActiveCall)
	{
		//PulseEvent(hEventReturn);

		bool bCenter = (touchL.xPosLP == 255 && touchL.yPosLP == 255) && (touchW.xPosRP == 255 && touchW.yPosRP == 255);
		bool bLeftSide = (touchL.xPosLP != 255 && touchL.yPosLP != 255);
		int xPos = (bLeftSide) ? touchL.xPosLP : touchW.xPosRP;
		int yPos = (bLeftSide) ? touchL.yPosLP : touchW.yPosRP;

		bool bHandled = false;
		if (touchW.Up == 1)
		{
			if (g_previousPositionX > -1 && g_previousPositionY > -1)
			{
				int xDiff = xPos - g_previousPositionX;
				int yDiff = yPos - g_previousPositionY;

				DWORD dwSwipeMode = g_ScrollApp.g_dwSwipeMode;

				if (g_ScrollApp.g_dwSwipeMode == 4)
				{
					dwSwipeMode = (g_bLeftSide) ? 1 : 2;
				}
				if (g_ScrollApp.g_dwSwipeMode == 5)
				{
					dwSwipeMode = (g_bLeftSide) ? 2 : 1;
				}

				if (g_bLeftSide && bCenter)
				{
					g_ScrollApp.ScrollRight(hwnd, true, true, dwSwipeMode);
					bHandled = true;
				}
				if (!g_bLeftSide && bCenter)
				{
					g_ScrollApp.ScrollLeft(hwnd, true, true, dwSwipeMode);
					bHandled = true;
				}

				if (g_bLeftSide == bLeftSide && !bCenter)
				{
					if (abs(xDiff) <= abs(yDiff))
					{
						if (yDiff > GSCROLL_SWIPE_SENSITIVITYMAX)
						{
							g_ScrollApp.ScrollDown(hwnd, true, true, dwSwipeMode);
							bHandled = true;
						}
						else if (yDiff < GSCROLL_SWIPE_SENSITIVITYMIN)
						{
							g_ScrollApp.ScrollUp(hwnd, true, true, dwSwipeMode);
							bHandled = true;
						}
					}
					else
					{
						if (xDiff > GSCROLL_SWIPE_SENSITIVITYMAX)
						{
							g_ScrollApp.ScrollRight(hwnd, true, true, dwSwipeMode);
							bHandled = true;
						}
						else if (xDiff < GSCROLL_SWIPE_SENSITIVITYMIN)
						{
							g_ScrollApp.ScrollLeft(hwnd, true, true, dwSwipeMode);
							bHandled = true;
						}
					}
				}
			}
			g_previousPositionY = g_previousPositionX = -1;
		}

		DWORD dwRegion = (bLeftSide) ? 0 : 2;
		if (yPos > 50)
		{
			dwRegion++;
		}
		if (bCenter)
		{
			dwRegion = 4;
		}

		DWORD dwTick = GetTickCount();
		if (touchW.Up == 0)
		{
			if (!bCenter)
			{
				g_bLeftSide = bLeftSide;
				g_previousPositionX = xPos;
				g_previousPositionY = yPos;
			}
			
			g_dwRegion = dwRegion;
			g_dwDownTick = dwTick;
		}
		else
		{
			g_previousPositionX = -1;
			g_previousPositionY = -1;
		}

		if (!bHandled)
		{
			if (touchW.Up == 1)
			{
				if (0 != g_dwDownTick && (dwTick - g_dwDownTick) < GSCROLL_TAP_INTERVAL)
				{
					if (dwRegion == g_dwRegion)
					{
						// we have our single tap here
						if (g_bSingleTap && g_dwLastRegion == dwRegion)
						{
							// check for double-tap
							if (0 != g_dwLastTick && (dwTick - g_dwLastTick) < GSCROLL_DOUBLETAP_INTERVAL)
							{
								switch (g_dwRegion)
								{
									case 0:
										{
											g_ScrollApp.LaunchApplication(REG_GSCROLLLAUNCH0);
										}
										break;
									case 1:
										{
											g_ScrollApp.LaunchApplication(REG_GSCROLLLAUNCH1);
										}
										break;
									case 2:
										{
											g_ScrollApp.LaunchApplication(REG_GSCROLLLAUNCH2);
										}
										break;
									case 3:
										{
											g_ScrollApp.LaunchApplication(REG_GSCROLLLAUNCH3);
										}
										break;
									case 4:
										{
											LedOn(1);
											Sleep(50);
											LedOff(1);
											hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLDOWN);
											SetEvent(hEvent);
											CloseHandle(hEvent);
										}
										break;
								}

								g_dwLastRegion = g_dwRegion = -1;
								g_dwLastTick = g_dwDownTick = 0;
								g_bSingleTap = false;
							}
							else if (0 != g_dwDownTick && (dwTick - g_dwDownTick) < GSCROLL_TAP_INTERVAL)
							{
								g_dwLastRegion = g_dwRegion = dwRegion;
								g_bSingleTap = true;
								g_dwLastTick = g_dwDownTick;
							}
							else
							{
								g_bSingleTap = false;
							}
						}
						else
						{
							g_dwLastRegion = g_dwRegion = dwRegion;
							g_bSingleTap = true;
							g_dwLastTick = g_dwDownTick;
						}
					}
					else
					{
						g_dwLastRegion = g_dwRegion = -1;
						g_dwLastTick = 0;
						g_bSingleTap = false;
					}
				}
				else
				{
					g_dwLastRegion = g_dwRegion = -1;
					g_dwLastTick = 0;
					g_bSingleTap = false;
				}
			}
		}
		else
		{
			g_dwLastRegion = g_dwRegion = -1;
			g_dwLastTick = 0;
			g_dwDownTick = 0;
			g_bSingleTap = false;
		}
	}
}

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
		g_ScrollApp.g_bInActiveCall = false;
	}
	else
	{
		DWORD dwData = *((DWORD*)pData);
		g_ScrollApp.g_bInActiveCall = (dwData != 0);
	}
}


DWORD WINAPI HandleGPoll(LPVOID lpParameter)
{
	SENSORDATATILT sensorData = {0}, normalizedData = {0};
	HANDLE hSensor = (HANDLE)lpParameter;
	DWORD dwEvent;
	DWORD dwLastTick = 0;

	HANDLE hEvent[2];
	hEvent[0] = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLDOWN);
	hEvent[1] = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_GSCROLLEXIT);
	while (WAIT_ABANDONED_0 != (dwEvent = WaitForMultipleObjects(2, hEvent, FALSE, INFINITE)))
	{
		if (dwEvent == WAIT_OBJECT_0)
		{
			if (hSensor && g_ScrollApp.g_dwTiltMode)
			{
				if (g_ScrollApp.g_bNormalized)
				{
					// only poll every 50 seconds
					if ((GetTickCount() - dwLastTick) > 50)
					{
						SensorGetDataTilt(hSensor, &sensorData);
						dwLastTick = GetTickCount();
					}

					long shXDiff = sensorData.TiltX - normalizedData.TiltX;
					long shYDiff = sensorData.TiltY - normalizedData.TiltY;

					long uiSleep = 100;
					const UINT uiMinSleep = 5;

					if (shXDiff > GSCROLL_THRESHOLD)
					{
						uiSleep = (GSCROLL_SLEEP_INTERVAL / (UINT)(shXDiff / GSCROLL_TICK_SIZE));

						g_ScrollApp.ScrollRight(g_ScrollApp.g_hWndForeground, false, false, g_ScrollApp.g_dwTiltMode);
					}
					if (shXDiff < -GSCROLL_THRESHOLD)
					{
						uiSleep = (GSCROLL_SLEEP_INTERVAL / (UINT)(-shXDiff / GSCROLL_TICK_SIZE));

						g_ScrollApp.ScrollLeft(g_ScrollApp.g_hWndForeground, false, false, g_ScrollApp.g_dwTiltMode);
					}
					if (shYDiff > GSCROLL_THRESHOLD)
					{
						uiSleep = (GSCROLL_SLEEP_INTERVAL / (UINT)(shYDiff / GSCROLL_TICK_SIZE));

						g_ScrollApp.ScrollDown(g_ScrollApp.g_hWndForeground, false, false, g_ScrollApp.g_dwTiltMode);
					}
					if (shYDiff < -GSCROLL_THRESHOLD)
					{
						uiSleep = (GSCROLL_SLEEP_INTERVAL / (UINT)(-shYDiff / GSCROLL_TICK_SIZE));

						g_ScrollApp.ScrollUp(g_ScrollApp.g_hWndForeground, false, false, g_ScrollApp.g_dwTiltMode);
					}

					Sleep(max(uiMinSleep, uiSleep));
				}
				else
				{
					g_ScrollApp.g_bNormalized = true;
					SensorGetDataTilt(hSensor, &normalizedData);

					Sleep(250);

					LedOn(1);
					Sleep(50);
					LedOff(1);
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

	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE hSensor = NULL;
	bool bDemoMode = false;
	DWORD dwStartTime = GetTickCount();
	const DWORD dwEvalTime = 1000 * 60 * 5; // 5 inutes demo

	DWORD dwDisable = 0;
	GetFromRegistry(REG_GSCROLLDISABLE, &dwDisable);
	if (dwDisable) { return -1; }

	if (!CheckIfRegistered(REG_GSCROLLREGCODE, RPN_CVALUE, RPN_VARIANT))
	{
		// enter demo mode
		bDemoMode = true;
		MessageBox(NULL, L"GScroll is running in demo mode and will exit after 5 minutes.", L"GScroll Not Activated", MB_OK | MB_ICONWARNING);
	}

	HANDLE hEvent = CreateEvent(NULL, FALSE, TRUE, EVENT_GSCROLL_MUTEX);

	if (!hEvent)
	{
		return -1;
	}
	if (ERROR_ALREADY_EXISTS == GetLastError())
	{
		// the event already exists
		CloseHandle(hEvent);
		return -2;
	}

	NOTIFICATIONCONDITION nc;
	HREGNOTIFY hNotifyActiveApplication = NULL, hNotifyActiveCallCount = NULL;
    // Receive a notification whenever that bit toggles.
    nc.ctComparisonType = REG_CT_ANYCHANGE;
    nc.TargetValue.dw = 0;

	//RegistryNotifyCallback(SN_ACTIVEAPPLICATION_ROOT, SN_ACTIVEAPPLICATION_PATH, SN_ACTIVEAPPLICATION_VALUE, ActiveApplicationCallback, 0, NULL, &hNotifyActiveApplication);
	RegistryNotifyCallback(SN_ACTIVECALLCOUNT_ROOT, SN_ACTIVECALLCOUNT_PATH, SN_ACTIVECALLCOUNT_VALUE, ActiveCallCountCallback, 0, NULL, &hNotifyActiveCallCount);
	
	HANDLE hEventDown = CreateEvent(NULL, TRUE, FALSE, EVENT_GSCROLLDOWN);
	HANDLE hEventFocus = CreateEvent(NULL, FALSE, TRUE, EVENT_GSCROLLFOCUS);
	HANDLE hEventExit = CreateEvent(NULL, TRUE, FALSE, EVENT_GSCROLLEXIT);
	HANDLE hEventDebug1 = CreateEvent(NULL, TRUE, FALSE, EVENT_GSCROLLDEBUG1);
	HANDLE hEventReturn = CreateEvent(NULL, TRUE, FALSE, EVENT_GSCROLLRETURN);

	SensorInit(SENSOR_TILT_PORT, &hSensor);
	HANDLE hPollThread = CreateThread(NULL, 0, HandleGPoll, hSensor, 0, 0);

	HANDLE hEvents[4];
	hEvents[0] = hEventFocus;
	hEvents[1] = hEventExit;
	hEvents[2] = g_ScrollApp.g_hMsgQueueRead;
	hEvents[3] = hEventDebug1;

	DWORD dwEvent = NULL;
	int resetCount = 0;
	while (WAIT_ABANDONED_0 != (dwEvent = WaitForMultipleObjects(4, hEvents, FALSE, 1*1000)))
	{
		DWORD dwCurrTime = GetTickCount();
		if (bDemoMode && (dwCurrTime < dwStartTime || dwCurrTime > (dwStartTime + dwEvalTime)))
		{
			g_ScrollApp.UnhookWindow();
			break;
		}
		else if (dwEvent == WAIT_TIMEOUT)
		{
			if (g_ScrollApp.HookForegroundWindow())
			{
				// Couldn't hook here. Let's just continue
				//break;
				ResetEvent(hEventDown);
			}
		}
		// We should probably timeout here and do a foreground window check -- just to be safe
		else if (dwEvent == WAIT_OBJECT_0) // Focus
		{
			ResetEvent(hEventFocus);
			// Our settings have changed
			if (g_ScrollApp.HookForegroundWindow())
			{
				// Couldn't hook here. Let's just continue
				ResetEvent(hEventDown);
			}
		}
		else if (dwEvent == WAIT_OBJECT_0+1) // Exit
		{
			ResetEvent(hEventExit);
			g_ScrollApp.UnhookWindow();
			break;
		}
		else if (dwEvent == WAIT_OBJECT_0+2) // mouse down
		{
			ResetEvent(hEventDown);
			HTCTOUCH_COPY queueData = {0};
			DWORD dwBytesRead = 0, dwFlags = 0;
			if (ReadMsgQueue(g_ScrollApp.g_hMsgQueueRead, &queueData, sizeof(HTCTOUCH_COPY), &dwBytesRead, 0, &dwFlags))
			{
				ProcessTouchData(g_ScrollApp.g_hWndForeground, queueData.wParam, queueData.lParam);
			}
		}
		else if (dwEvent == WAIT_OBJECT_0+3) // mouse down
		{
			ResetEvent(hEventDebug1);
			ProcessTouchData(g_ScrollApp.g_hWndForeground, g_ScrollApp.g_wParam, g_ScrollApp.g_lParam);
			PulseEvent(hEventReturn);
		}
		//else if (dwEvent == WAIT_OBJECT_0+3) // mouse down
		//{
		//	DWORD dwTilt = 0;
		//	GetFromRegistry(REG_GSCROLLTILT, &dwTilt);

		//	if (dwTilt)
		//	{
		//		HandleGPoll(hSensor);
		//	}
		//	else
		//	{
		//		ResetEvent(hEventDown);
		//	}
		//}
		else
		{
			g_ScrollApp.UnhookWindow();
			break;
		}
	}

	SetEvent(hEventExit);
	TerminateThread(hPollThread, 0);
	SensorUninit(hSensor);

	RegistryCloseNotification(hNotifyActiveApplication);
	RegistryCloseNotification(hNotifyActiveCallCount);

	CloseHandle(hPollThread);
	CloseHandle(hEventDown);
	CloseHandle(hEventFocus);
	CloseHandle(hEventExit);
	CloseHandle(hEventDebug1);
	CloseHandle(hEventReturn);

    return 0;
}

