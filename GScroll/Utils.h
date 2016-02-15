#pragma once

#define REG_GScrollKey                    TEXT("ControlPanel\\GScroll")
#define REG_RegstrationCode                TEXT("RegCode")
#define REG_SwipeMode                    TEXT("Swipe")
#define REG_TiltMode                    TEXT("Tilt")
#define REG_Disabled                    TEXT("Disabled")
#define REG_DisableLaunchGestures		TEXT("DisableLaunchGestures")
#define REG_EnableSmoothScroll            TEXT("EnableSmoothScroll")
#define REG_EnableScrollWheel            TEXT("EnableScrollWheel")
#define REG_EnableGameMode                TEXT("EnableGameMode")
#define REG_EnablePrecisionMode            TEXT("EnablePrecisionMode")
#define REG_GameModeKeyRepeat            TEXT("GameModeKeyRepeat")
#define REG_PrecisionModeScrollSize        TEXT("PrecisionModeScrollSize")
#define REG_PrecisionModeNavigateSize    TEXT("PrecisionModeNavigateSize")

#define REG_TapMinInterval				TEXT("TapMinInterval")
#define REG_TapMaxInterval				TEXT("TapMaxInterval")
#define REG_SwipeMinInterval			TEXT("SwipeMinInterval")
#define REG_DblTapMinInterval			TEXT("DblTapMinInterval")
#define REG_DblTapMaxInterval			TEXT("DblTapMaxInterval")
#define REG_PrecisionInterval			TEXT("PrecisionInterval")
#define REG_TiltStep					TEXT("TiltStep")
#define REG_TiltSleep					TEXT("TiltSleep")
#define REG_TiltThreshold				TEXT("TiltThreshold")

#define REG_Launch_Notify                TEXT("LaunchNotify")
#define REG_LaunchKey                    TEXT("Launch")
#define REG_Launch_SequencesKey            TEXT("Sequences")
#define REG_PerProcessKey                TEXT("PerProcess")

#define REG_AppSwitchKey                     TEXT("AppSwitch")
#define REG_AppSwitch_EnableShrinkAnimation  TEXT("EnableShrinkAnimation")
#define REG_AppSwitch_EnableScrollAnimation  TEXT("EnableScrollAnimation")
#define REG_AppSwitch_EnableThumbnails       TEXT("EnableThumbnails")
#define REG_AppSwitch_AutoCloseDelay         TEXT("AutoCloseDelay")
#define REG_AppSwitch_ProcessKey             TEXT("Processes")
#define REG_AppSwitch_Process_Ignore         TEXT("Ignore")
#define REG_AppSwitch_Process_CanClose       TEXT("CanClose")
#define REG_AppSwitch_Process_CanMinimize    TEXT("CanMinimize")

#define EVENT_GSCROLLFOCUS              TEXT("GScrollFocus")
#define EVENT_GSCROLLCURSOR             TEXT("GScrollCursor")
#define EVENT_GSCROLLDOWN               TEXT("GScrollDown")
#define EVENT_GSCROLLUP                 TEXT("GScrollUp")
#define EVENT_GSCROLLEXIT               TEXT("GScrollExit")
#define EVENT_GSCROLLSETTINGS           TEXT("GScrollSettings")
#define EVENT_GSCROLL_MUTEX             TEXT("GScroll")

#define GScroll_TiltMode_Disabled           0
#define GScroll_TiltMode_ScrollOnly         1
#define GScroll_TiltMode_DPadOnly           2
#define GScroll_TiltMode_SmartScroll        3
#define GScroll_TiltMode_LeftScrollRightDir 4
#define GScroll_TiltMode_LeftDirRightScroll 5

void LedOn(int id);
void LedOff(int id);
BOOL LaunchProgram(LPCTSTR lpFile, LPCTSTR lpParameters);
HRESULT GetFromRegistry(HKEY rootKey, TCHAR* szRegistryKey, TCHAR* szRegValueName, WCHAR wzValue[MAX_PATH]);
HRESULT GetFromRegistry(TCHAR* szRegValueName, WCHAR wzValue[MAX_PATH]);
HRESULT GetFromRegistry(HKEY rootKey, TCHAR* szRegistryKey, TCHAR* szRegValueName, DWORD *pdwValue);
HRESULT GetFromRegistry(TCHAR* szRegValueName, DWORD *pdwValue);
HRESULT SetToRegistry(TCHAR* szRegValueName, DWORD dwValue);
HRESULT SetStringToRegistry(TCHAR* szRegValueName, WCHAR* wzValue);
HICON GetApplicationIcon(WCHAR* szFilteredPath, bool bSmall, WCHAR* wzExeFile, WCHAR* wzWndText, WCHAR* wzClassName);
WCHAR* TrimEndSpaces(WCHAR* szEXEPath2);
bool RectContains(RECT* pRect, int xPos, int yPos);
bool GetProcessName(DWORD dwProcessId, WCHAR wzProcessName[MAX_PATH]);
bool FindLnkIcon(WCHAR szFoundPath[MAX_PATH], WCHAR* szPath, WCHAR* wzProcessName);

struct strltpred
{
   enum
   {
      bucket_size = 4,
      min_buckets = 8
   };

   size_t operator()(WCHAR* s) const
   {
      size_t h = 0;
      for (; *s; ++s)
         h = 5 * h + *s;
      return h;
   }

   bool operator()(WCHAR* a, WCHAR* b) const
   {
      return wcscmp(a, b) < 0;
   }
};