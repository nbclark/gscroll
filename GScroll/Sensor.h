#ifndef _GSENSOR
#define _GSENSOR

#define SENSOR_DLL      L"HTCSensorSDK.dll"

#define SENSOR_TILT_PORT 1

typedef struct _SENSORDATA
{
    int Reserved0; // this value is always 3
    byte Luminance; // this value ranges between 0 and 30
} SENSORDATA, *PSENSORDATA;

#define GSCROLL_DATA_EVENT TEXT("GSCROLL_DATA_EVENT")

typedef struct _SENSORDATATILT
{
    SHORT   TiltX;          // From -1000 to 1000 (about), 0 is flat
    SHORT   TiltY;          // From -1000 to 1000 (about), 0 is flat
    SHORT   Orientation;    // From -1000 to 1000 (about)
                            // 0 = Straight up, -1000 = Flat, 1000 = Upside down
    WORD    Unknown1;       // Always zero
    DWORD   AngleY;         // From 0 to 359
    DWORD   AngleX;         // From 0 to 359
    DWORD   Unknown2;       // Bit field?
} SENSORDATATILT, *PSENSORDATATILT;

typedef HANDLE (WINAPI * PFN_HTCSensorOpen)(DWORD);
typedef void (WINAPI * PFN_HTCSensorClose)(HANDLE);
typedef DWORD (WINAPI * PFN_HTCSensorGetDataOutput)(HANDLE, PSENSORDATA);
typedef DWORD (WINAPI * PFN_HTCSensorGetDataOutputTILT)(HANDLE, PSENSORDATATILT);
typedef DWORD (WINAPI * PFN_HTCSensorQueryCapability)(DWORD);
typedef DWORD (WINAPI * PFN_HTCNavRegisterCallback)(void*,void*,void*);

// -- Notifications for gestures around the d-pad -----------------
#define HTCAPI_DLL      L"HTCAPI.dll"

typedef DWORD (WINAPI * PFN_HTCNavGetInfo)(HWND);
typedef DWORD (WINAPI * PFN_HTCNavGetMode)(HWND);
typedef DWORD (WINAPI * PFN_HTCNavOpen)(HWND, DWORD);
typedef DWORD (WINAPI * PFN_HTCNavSetMode)(HWND, DWORD);
typedef DWORD (WINAPI * PFN_HTCNavClose)(HWND);

#define GESTURE_API         1
#define GESTURE_GESTURE     4   // Generates WM_HTC_GESTURE messages
#define GESTURE_TOUCH       5   // Generates WM_HTC_TOUCH messages
                                //  (with the HTCTOUCH structs)
#define HTCNAV_VSLIDE       0x040
#define HTCNAV_HSLIDE       0x080
#define HTCNAV_8WAY         0x100
#define HTCNAV_4WAY         0x200
#define HTCNAV_WHEEL        0x004
#define HTCNAV_TOUCH        0x008

#define WM_HTC_GESTURE      (WM_USER + 200)
#define WM_HTC_TOUCH        (WM_USER + 209)

#define ROTATION_MASK       0x00000F00  // WPARAM
#define ROTATION_CLOCKWISE  0x00000800
#define ROTATION_COUNTER    0x00000900

#define COMPLETION_MASK     0x00F00000 // LPARAM
#define DIRECTION_MASK      0x0000F000 // LPARAM

#define HTCNAV_WHEEL_MASK_CIRCLE    0x11
#define HTCNAV_WHEEL_MASK_DIR       0x55

#define HTCNAV_WHEEL_DEG            75.0
#define HTCNAV_WHEEL_LEFT           0
#define HTCNAV_WHEEL_UP             18
#define HTCNAV_WHEEL_RIGHT          37
#define HTCNAV_WHEEL_DOWN           56

#define GSCROLL_GAMEMODE_PADDING    10
#define GSCROLL_PANEL_WIDTH         67
#define GSCROLL_PANEL_HEIGHT        100

typedef struct tagHTCTOUCH_WPARAM
{
    BYTE Up;    //0=KeyDown,1=KeyUp
    BYTE Where; //Where the click occurs (left pane, wheel, right pane)
                //Pos for the Right Pane click
    BYTE xPosRP;
    BYTE yPosRP;
} HTCTOUCH_WPARAM;

typedef struct tagHTCTOUCH_LPARAM
{
        //Pos for the Left Pane click
    BYTE xPosLP;
    BYTE yPosLP;
        //Wheel click angle
    BYTE WheelAngle;
    BYTE Unknown4;
} HTCTOUCH_LPARAM;

typedef struct tagHTCTOUCH_COPY
{
    UINT            uMsg;
    HTCTOUCH_WPARAM wParam;
    HTCTOUCH_LPARAM lParam;
} HTCTOUCH_COPY;


HMODULE SensorRawInit(HMODULE hHTCAPI, bool bEnableNavWheel);
HMODULE SensorGestureInit(HMODULE hSensorMod, HWND hWnd, bool bEnableNavWheel, bool bNewForeground);
bool SensorGestureUninit(HMODULE hSensorMod, HWND hWnd);
bool SensorInit(DWORD sensorValue, HANDLE* phSensor);
bool SensorUninit(HANDLE hSensor);
void SensorGetData(HANDLE hSensor, SENSORDATA* pSensorData);
void SensorGetDataTilt(HANDLE hSensor, SENSORDATATILT* pSensorData);
SHORT SensorGetOrientation(SENSORDATATILT sdTilt, SHORT shCalibration);
SHORT SensorGetOrientation(HANDLE hSensor, SHORT shCalibration);

#endif