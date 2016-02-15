#pragma once
#include "utils.h"
#include <ddraw.h>
#include <aygshell.h>
#pragma comment(lib, "ddraw.lib") 

typedef stdext::hash_map<DWORD, HBITMAP> PROCIMGMAP;
typedef stdext::hash_map<DWORD, HICON> PROCICONMAP;
typedef stdext::hash_map<DWORD, DWORD> PROCTIMEMAP;
typedef stdext::hash_map<DWORD, HWND> PROCHWNDMAP;

#define THUBMNAIL_POLL_INTERNVAL (1000 * 1)
#define THUBMNAIL_REFRESH_INTERNVAL (1000 * 30)
#define THUMBNAIL_CAPTURE_WIDTH 

struct node
{
    WCHAR wzText[20];
    HWND hWnd;
    DWORD dwProcessId;
    node* next;
    node* prev;
    HBITMAP hBmp;
    HICON hIcon;
};
struct AppSwitchProcess
{
    bool bIgnore;
    bool bCanClose;
    bool bCanMinimize;
};

typedef bool (WINAPI * PFN_ClickAction)(void* pAltTab, HWND hwndDlg, node** ppNode);
class CAltTab
{
public:
    CAltTab(void);
    ~CAltTab(void);
    void Initialize();
    void Show(int iDir, void* pTouchHandler);
    bool MinimizeProcess(HWND hwndTop, WCHAR* wzProcess, WCHAR* wzClass);
    bool CloseProcess(HWND hwndTop, WCHAR* wzProcess, WCHAR* wzClass);
    void LoadSettings();

private:
    static DWORD WINAPI PollThumbnails(LPVOID lpParameter);
    static LRESULT CALLBACK AltTabProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
    node* BuildNodeList(int* piNodeLen);
    void EnsureCapture(HWND hwndCurrent, bool bForce);
    void LoadAppSwitchSettings();
    void CleanupAppSwitchSettings();
    void CleanupThumbnails();
    HRESULT Flip(HWND hwnd);
    HRESULT RestoreAll();
    BOOL InitSurfaces();
    void ReleaseAllObjects();
    void UpdateFrameAnimateIn(int iAnimateIterations, int iIteration);
    void UpdateFrameAnimateOut(HDC hdcThumb, int iAnimateIterations, int iIteration, node* pActiveNode);
    void UpdateFrameScroll(HDC hdcThumb, HWND hWnd, int xDiff, int yDiff, int xPos, int yPos, node* pHead, node** pActive, node** pUnderMouse, PFN_ClickAction* pClickAction, int* piDistanceFromCenter, int* piMouseFromCenter, bool bFinalStop);
    inline void DrawUpdatedFrame(HWND hwndDlg);
    inline void ResetAutoClose(HWND hwndDlg);
    inline void AbortAutoClose(HWND hwndDlg);
    void LoadAppSwitchDisplaySettings();

public:
    // these are appswitch variables
    DWORD g_dwAppSwitchAutoClose;
    bool g_bAppSwitchEnableShrinkAnimation;
    bool g_bAppSwitchEnableScrollAnimation;
    bool g_bAppSwitchEnableThumbnails;
    int g_iDir;

private:
    void* g_pTouchHandler;
    HWND g_hwndAltTab;
    WNDCLASS g_wndClassAltTab;
    HINSTANCE g_hInstance;
    HANDLE g_hThumbThread;
    CRITICAL_SECTION g_imgCriticalSection;
    PROCIMGMAP g_imgMap;
    PROCICONMAP g_iconMap;
    PROCTIMEMAP g_timeMap;
    PROCHWNDMAP g_procMap;
    HANDLE hFinishedEvent;
    hash_map<WCHAR*, AppSwitchProcess*, strltpred> g_wzProcessList;

    DWORD g_dwAnimateInterval;
    DWORD g_dwDeceleration;

    int g_width;
    int g_height;
    int g_vertShift;
    int g_bigThumbHeight;
    int g_bigThumbWidth;
    int g_bigThumbTop;
    int g_bigThumbMid;
    int g_bigThumbLeft;
    int g_smallThumbHeight;
    int g_smallThumbWidth;
    int g_labelWidth;
    int g_labelHeight;
    int g_labelSpacing;
    int g_labelLeft;
    int g_labelTop;
    int g_labelFont;

    int g_buttonWidth;
    int g_buttonHeight;
    int g_buttonSpacing;
    int g_buttonLeft;
    int g_buttonTop;
    int g_buttonFont;

    SIPINFO g_sipInfo;

    LPDIRECTDRAW                g_pDD;
    LPDIRECTDRAWSURFACE         g_pDDSPrimary;
    LPDIRECTDRAWSURFACE         g_pDDSBuffer;
    LPDIRECTDRAWSURFACE         g_pDDSBackground;
    LPDIRECTDRAWSURFACE         g_pDDSAnimate;
    BOOL                        g_bActive;
    LPDIRECTDRAWCLIPPER         g_pDDClipper;
};