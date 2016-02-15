#pragma once

#include "utils.h"
#include "sensor.h"
#include "AltTab.h"

static long GSCROLL_TAP_MININTERVAL = 90;
static long GSCROLL_TAP_MAXINTERVAL = 600;
static long GSCROLL_SWIPE_MININTERVAL = 15;
static long GSCROLL_DOUBLETAP_MAXINTERVAL = 1100;
static long GSCROLL_DOUBLETAP_MININTERVAL = 400;
static long GSCROLL_PRECISON_INTERVAL = 200;
static long GSCROLL_TILT_STEP = 200;
static long GSCROLL_TILT_SLEEP_INTERVAL = 400;
static long GSCROLL_TILT_THRESHOLD = 200;

#define WM_INITSENSORS WM_USER + 0xC10
#define WM_UNINITSENSORS WM_USER + 0xC11
#define WM_INITSENSORS2 WM_USER + 0xC12
#define WM_INITSENSORS3 WM_USER + 0xC13
#define SN_ACTIVEAPPLICATION_ROOT HKEY_CURRENT_USER
#define SN_ACTIVEAPPLICATION_PATH TEXT("System\\State\\Shell")
#define SN_ACTIVEAPPLICATION_VALUE TEXT("Active Application") 
#define SN_ACTIVECALLCOUNT_ROOT HKEY_LOCAL_MACHINE
#define SN_ACTIVECALLCOUNT_PATH TEXT("System\\State\\Phone")
#define SN_ACTIVECALLCOUNT_VALUE TEXT("Active Call Count") 


class CTouchHandler
{
public:
    CTouchHandler(void);
    ~CTouchHandler(void);

    void Initialize();
    void ProcessMouseData(HWND hwnd, HANDLE hNavDevice, HMODULE hSensorMod, HTCTOUCH_WPARAM* touchW, HTCTOUCH_LPARAM* touchL, bool bTimer);
    void ProcessTouchData(HTCTOUCH_WPARAM touchW, HTCTOUCH_LPARAM touchL, byte wheelNav, byte wheelDeg, bool bTouchOnly);
    bool GetProcessName(DWORD dwProcessId, WCHAR wzProcessName[MAX_PATH]);
    void SetScrollSettings(HWND hWndForeground, HWND* phWndScrollable, bool* pbVScroll, bool* pbHScroll);
    void ScrollWithMouse(HWND hWnd, DWORD dwScrollDir, DWORD dwScrollPage, bool bPage);
    void HandleKeyPress(HWND hWnd, bool bPage, bool bCanScroll, bool bCanScrollAny, DWORD dwScrollDir, DWORD dwScrollPage, DWORD dwScrollLine, DWORD dwKey, DWORD dwMode, short iMultiplier);
    void LaunchApplication(WCHAR* wzRegion);
    void ScrollUp(HWND hWnd, bool bPage, bool bForceKeyDown, DWORD dwMode, short iMultiplier);
    void ScrollDown(HWND hWnd, bool bPage, bool bForceKeyDown, DWORD dwMode, short iMultiplier);
    void ScrollLeft(HWND hWnd, bool bPage, bool bForceKeyDown, DWORD dwMode, short iMultiplier);
    void ScrollRight(HWND hWnd, bool bPage, bool bForceKeyDown, DWORD dwMode, short iMultiplier);
    DWORD GetDPadKey(short shDir);
    bool AltTab(HWND hWnd, int iDir);
    static LRESULT CALLBACK TouchHandlerProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
    void Show();
    void Hide();
    void RegisterTap(short iTap);
    void ClearTaps();
    void Scroll(HWND hwnd, bool bPage, bool bForceKeyDown, int iIterationsX, int iIterationsY, short dwMode);
    void LoadSettings(bool bDefault);
    bool LoadGeneralSettings(bool bDefault);
    void LoadLaunchSettings();
    void CleanupLaunchSettings();
    int GetOrientation();
    void ResetTouchData(bool bSettingsChanged);

public:
    HINSTANCE g_hInstance;
    bool g_bIgnoreCurrentWindow;
    DWORD g_dwProcessIdActive;
    HWND g_hWndActive;
    bool g_bNormalized;
    bool g_bInActiveCall;

    DWORD g_dwSwipeMode, g_dwTiltMode, g_dwGameModeRepeat, g_dwPrecisionModeScrollSize, g_dwPrecisionModeNavigateSize;
    bool g_bDisabled, g_bScrollWheel, g_bPrecisionMode, g_bGameMode, g_bSmoothScroll, g_disableLaunchGestures;
    
    DWORD g_def_dwSwipeMode, g_def_dwGameModeRepeat, g_def_dwPrecisionModeScrollSize, g_def_dwPrecisionModeNavigateSize;
    bool g_def_bDisabled, g_def_bScrollWheel, g_def_bPrecisionMode, g_def_bGameMode, g_def_bSmoothScroll;

    WCHAR g_wzProcessName[MAX_PATH];
    WCHAR g_wzReservedProcs[MAX_PATH], g_wzCriticalProcs[MAX_PATH], g_wzIgnoreProcs[MAX_PATH], g_wzNavReadyProcs[MAX_PATH];
    int g_iInitRet, g_iMouseX, g_iMouseY, g_iMouseAccel;
    bool g_bInAltTab, g_bMouseMode, g_bUseCursorKeys;
    CAltTab g_AltTab;
    HWND g_hwndTouchHandler;
    WNDCLASS g_wndClassTouchHandler;
    list<short> g_typeList;
    hash_map<WCHAR*, WCHAR*, strltpred> g_wzLaunchList;
    HANDLE hFinishedEvent;


protected:

    // these are the processtouch variables
    bool g_bNoUpHandle;
    bool g_bWasDown;
    bool g_bMadeDecision;
    bool g_bKeyWasPressed;
    bool g_bLaunchNotify;
    DWORD g_dwFirstDownTick;
    DWORD g_dwDownTick;
    DWORD g_dwKeyRepeat;
    short g_dwDownRegion;
    int g_previousPositionX;
    int g_previousPositionY;
};
