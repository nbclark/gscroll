#include "stdafx.h"
#include "sensor.h"

PFN_HTCSensorOpen           pfnHTCSensorOpen;
PFN_HTCSensorClose          pfnHTCSensorClose;
PFN_HTCSensorGetDataOutput  pfnHTCSensorGetDataOutput;
PFN_HTCSensorGetDataOutputTILT  pfnHTCSensorGetDataOutputTilt;
PFN_HTCSensorQueryCapability  pfnHTCSensorQueryCapability;
#define             GESTURE_RAW                 8 

HMODULE SensorRawInit(HMODULE hHTCAPI, bool bEnableNavWheel)
{
    bool bNeedInit = false;
    if (NULL == hHTCAPI)
    {
        bNeedInit = true;
        hHTCAPI = LoadLibrary(HTCAPI_DLL);
    }
    if (hHTCAPI == NULL)
    {
        return FALSE;
    }
    PFN_HTCNavOpen pfnHTCNavOpen = (PFN_HTCNavOpen)GetProcAddress(hHTCAPI, L"HTCNavOpen");
    PFN_HTCNavSetMode pfnHTCNavSetMode = (PFN_HTCNavSetMode)GetProcAddress(hHTCAPI, L"HTCNavSetMode");
    PFN_HTCNavGetMode pfnHTCNavGetMode = (PFN_HTCNavGetMode)GetProcAddress(hHTCAPI, L"HTCNavGetMode");
    
    if (bNeedInit)
    {
        pfnHTCNavOpen(0, GESTURE_API);
    }
    DWORD dwMode = pfnHTCNavGetMode(0);
    pfnHTCNavSetMode(0, dwMode | GESTURE_RAW);

    return hHTCAPI;
}

HMODULE SensorGestureInit(HMODULE hHTCAPI, HWND hWnd, bool bEnableNavWheel, bool bNewForeground)
{
    if (NULL == hHTCAPI)
    {
        hHTCAPI = LoadLibrary(HTCAPI_DLL);
    }

    if (hHTCAPI == NULL)
    {
        return NULL;
    }

    PFN_HTCNavOpen pfnHTCNavOpen = (PFN_HTCNavOpen)GetProcAddress(hHTCAPI, L"HTCNavOpen");
    PFN_HTCNavSetMode pfnHTCNavSetMode = (PFN_HTCNavSetMode)GetProcAddress(hHTCAPI, L"HTCNavSetMode");
    PFN_HTCNavGetMode pfnHTCNavGetMode = (PFN_HTCNavGetMode)GetProcAddress(hHTCAPI, L"HTCNavGetMode");
    PFN_HTCNavGetInfo pfnHTCNavGetInfo = (PFN_HTCNavGetInfo)GetProcAddress(hHTCAPI, L"HTCNavGetInfo");

    if (pfnHTCNavOpen == NULL || pfnHTCNavSetMode == NULL || NULL == pfnHTCNavGetMode)
    {
        SetLastError(ERROR_INVALID_MEMBER);
        return NULL;
    }

    if (hWnd)
    {
        DWORD dwMode = pfnHTCNavGetMode(0);
        DWORD dwOldGlobalMode = pfnHTCNavGetMode(0);
		DWORD dwOldMode = pfnHTCNavGetMode(hWnd);

        dwMode = (dwMode) | HTCNAV_TOUCH;
        if (bEnableNavWheel || true)
        {
            dwMode = dwMode | HTCNAV_WHEEL;
        }
        else
        {
            dwMode = dwMode & ~HTCNAV_WHEEL;
        }

		if (dwMode != dwOldMode)
		{
			pfnHTCNavSetMode(hWnd, dwMode);
		}
		if (dwMode != dwOldGlobalMode)
		{
			pfnHTCNavSetMode(0, dwMode | GESTURE_RAW);
		}

        return hHTCAPI;

        /*
        //pfnHTCNavOpen(0, GESTURE_API);
        if (hasNavSensor || hasOtherSensor)
        {
            //pfnHTCNavOpen(hWnd, GESTURE_API);
            pfnHTCNavSetMode(hWnd, dwMode);
            //pfnHTCNavSetMode(hWnd, GESTURE_TOUCH);

            pfnHTCNavSetMode(0, dwMode);
            return hHTCAPI;
        }
        else
        {
            //pfnHTCNavOpen(hWnd, GESTURE_API);
            //pfnHTCNavSetMode(hWnd, GESTURE_GESTURE);
            pfnHTCNavSetMode(hWnd, dwMode);

            pfnHTCNavSetMode(0, dwMode);
            return hHTCAPI;
        }
        */
    }

    FreeLibrary(hHTCAPI);

    return NULL;
}

bool SensorGestureUninit(HMODULE hHTCAPI, HWND hWnd)
{
    if (hHTCAPI== NULL)
    {
        return false;
    }
    PFN_HTCNavClose pfnHTCNavClose = (PFN_HTCNavClose)GetProcAddress(hHTCAPI, L"HTCNavClose");
    PFN_HTCNavSetMode pfnHTCNavSetMode = (PFN_HTCNavSetMode)GetProcAddress(hHTCAPI, L"HTCNavSetMode");
    if (pfnHTCNavClose && pfnHTCNavSetMode)
    {
        pfnHTCNavSetMode(hWnd, 0);
        pfnHTCNavClose(hWnd);
        //pfnHTCNavClose(GESTURE_TOUCH);
        //pfnHTCNavClose(GESTURE_GESTURE);
        //pfnHTCNavClose(GESTURE_API);
    }

    FreeLibrary(hHTCAPI);

    return true;
}

bool SensorInit(DWORD sensorValue, HANDLE* phSensor)
{
    *phSensor = NULL;

    HMODULE hSensorLib = LoadLibrary(SENSOR_DLL);

    if (hSensorLib == NULL)
    {
        return false;
    }

    pfnHTCSensorOpen = (PFN_HTCSensorOpen)
        GetProcAddress(hSensorLib, L"HTCSensorOpen");
    pfnHTCSensorClose = (PFN_HTCSensorClose)
        GetProcAddress(hSensorLib, L"HTCSensorClose");
    pfnHTCSensorGetDataOutput = (PFN_HTCSensorGetDataOutput)
        GetProcAddress(hSensorLib, L"HTCSensorGetDataOutput");
    pfnHTCSensorGetDataOutputTilt = (PFN_HTCSensorGetDataOutputTILT)
        GetProcAddress(hSensorLib, L"HTCSensorGetDataOutput");

    pfnHTCSensorQueryCapability = (PFN_HTCSensorQueryCapability)
        GetProcAddress(hSensorLib, L"HTCSensorQueryCapability");

    if (pfnHTCSensorOpen == NULL ||
        pfnHTCSensorClose == NULL ||
        pfnHTCSensorGetDataOutput == NULL)
    {
        return false;
    }

    *phSensor = pfnHTCSensorOpen(sensorValue);

    return true;
}

bool SensorUninit(HANDLE hSensor)
{
    pfnHTCSensorClose(hSensor);

    return true;
}

void SensorGetData(HANDLE hSensor, SENSORDATA* pSensorData)
{
    pfnHTCSensorGetDataOutput(hSensor, pSensorData);
}

void SensorGetDataTilt(HANDLE hSensor, SENSORDATATILT* pSensorData)
{
    pfnHTCSensorGetDataOutputTilt(hSensor, pSensorData);
}

SHORT SensorGetOrientation(SENSORDATATILT sdTilt, SHORT shCalibration)
{
    SHORT shOrientation = sdTilt.Orientation;

    shOrientation = shOrientation + shCalibration;

    if (shOrientation > 1000)
    {
        shOrientation = 1000 - (shOrientation - 1000);
    }
    if (shOrientation < -1000)
    {
        shOrientation = -1000 + (-1000 - shOrientation);
    }

    return shOrientation;
}

SHORT SensorGetOrientation(HANDLE hSensor, SHORT shCalibration)
{
    SENSORDATATILT sdTilt;
    SensorGetDataTilt(hSensor, &sdTilt);

    return SensorGetOrientation(sdTilt, shCalibration);
}
/*
#include "stdafx.h"
#include "sensor.h"


PFN_HTCSensorOpen           pfnHTCSensorOpen;
PFN_HTCSensorClose          pfnHTCSensorClose;
PFN_HTCSensorGetDataOutput  pfnHTCSensorGetDataOutput;
PFN_HTCSensorGetDataOutputTILT  pfnHTCSensorGetDataOutputTilt;
PFN_HTCSensorQueryCapability  pfnHTCSensorQueryCapability;
#define             GESTURE_RAW                 0x08
#define				GESTURE_BUTTON				0x20

HMODULE SensorRawInit(HMODULE hHTCAPI, bool bEnableNavWheel, void* pfn)
{
    bool bNeedInit = false;
    if (NULL == hHTCAPI)
    {
        bNeedInit = true;
        hHTCAPI = LoadLibrary(HTCAPI_DLL);
    }
    if (hHTCAPI == NULL)
    {
        return FALSE;
    }
    PFN_HTCNavOpen pfnHTCNavOpen = (PFN_HTCNavOpen)GetProcAddress(hHTCAPI, L"HTCNavOpen");
    PFN_HTCNavSetMode pfnHTCNavSetMode = (PFN_HTCNavSetMode)GetProcAddress(hHTCAPI, L"HTCNavSetMode");
    PFN_HTCNavGetMode pfnHTCNavGetMode = (PFN_HTCNavGetMode)GetProcAddress(hHTCAPI, L"HTCNavGetMode");
    PFN_HTCNavRegisterCallback pfnHTCNavRegisterCallback = (PFN_HTCNavRegisterCallback)GetProcAddress(hHTCAPI, L"HTCNavRegisterCallback");
    
    //if (bNeedInit)
    {
        pfnHTCNavOpen(0, 1);
    }
    DWORD dwMode = pfnHTCNavGetMode(0);
    pfnHTCNavSetMode(GetForegroundWindow(), dwMode);
	//pfnHTCNavRegisterCallback(pfn, pfn, pfn);
    pfnHTCNavSetMode(0, (dwMode | GESTURE_RAW) & ~GESTURE_BUTTON);

    return hHTCAPI;
}

HMODULE SensorGestureInit(HMODULE hHTCAPI, HWND hWnd, bool bEnableNavWheel, bool bNewForeground, void* pfn)
{
    if (NULL == hHTCAPI)
    {
        hHTCAPI = LoadLibrary(HTCAPI_DLL);
    }

    if (hHTCAPI == NULL)
    {
        return NULL;
    }

    PFN_HTCNavOpen pfnHTCNavOpen = (PFN_HTCNavOpen)GetProcAddress(hHTCAPI, L"HTCNavOpen");
    PFN_HTCNavSetMode pfnHTCNavSetMode = (PFN_HTCNavSetMode)GetProcAddress(hHTCAPI, L"HTCNavSetMode");
    PFN_HTCNavGetMode pfnHTCNavGetMode = (PFN_HTCNavGetMode)GetProcAddress(hHTCAPI, L"HTCNavGetMode");
    PFN_HTCNavGetInfo pfnHTCNavGetInfo = (PFN_HTCNavGetInfo)GetProcAddress(hHTCAPI, L"HTCNavGetInfo");
    PFN_HTCNavRegisterCallback pfnHTCNavRegisterCallback = (PFN_HTCNavRegisterCallback)GetProcAddress(hHTCAPI, L"HTCNavRegisterCallback");

    if (pfnHTCNavOpen == NULL || pfnHTCNavSetMode == NULL || NULL == pfnHTCNavGetMode)
    {
        SetLastError(ERROR_INVALID_MEMBER);
        return NULL;
    }

    if (hWnd)
    {
		if (bNewForeground)
		{
        DWORD dwMode = pfnHTCNavGetMode(0);

        dwMode = (dwMode) | HTCNAV_TOUCH;
		bEnableNavWheel=TRUE;
        if (bEnableNavWheel)
        {
            dwMode = dwMode | HTCNAV_WHEEL;
        }
        else
        {
            dwMode = dwMode & ~HTCNAV_WHEEL;
        }

			pfnHTCNavOpen(hWnd, GESTURE_API);
			pfnHTCNavSetMode(hWnd, dwMode);
		}
        return hHTCAPI;
    }

    return hHTCAPI;
}

bool SensorGestureUninit(HMODULE hHTCAPI, HWND hWnd)
{
    if (hHTCAPI== NULL)
    {
        return false;
    }
    PFN_HTCNavClose pfnHTCNavClose = (PFN_HTCNavClose)GetProcAddress(hHTCAPI, L"HTCNavClose");
    PFN_HTCNavSetMode pfnHTCNavSetMode = (PFN_HTCNavSetMode)GetProcAddress(hHTCAPI, L"HTCNavSetMode");
    if (pfnHTCNavClose && pfnHTCNavSetMode)
    {
        pfnHTCNavSetMode(hWnd, 0);
        pfnHTCNavClose(hWnd);
        //pfnHTCNavClose(GESTURE_TOUCH);
        //pfnHTCNavClose(GESTURE_GESTURE);
        //pfnHTCNavClose(GESTURE_API);
    }

    FreeLibrary(hHTCAPI);

    return true;
}

bool SensorInit(DWORD sensorValue, HANDLE* phSensor)
{
    *phSensor = NULL;

    HMODULE hSensorLib = LoadLibrary(SENSOR_DLL);

    if (hSensorLib == NULL)
    {
        return false;
    }

    pfnHTCSensorOpen = (PFN_HTCSensorOpen)
        GetProcAddress(hSensorLib, L"HTCSensorOpen");
    pfnHTCSensorClose = (PFN_HTCSensorClose)
        GetProcAddress(hSensorLib, L"HTCSensorClose");
    pfnHTCSensorGetDataOutput = (PFN_HTCSensorGetDataOutput)
        GetProcAddress(hSensorLib, L"HTCSensorGetDataOutput");
    pfnHTCSensorGetDataOutputTilt = (PFN_HTCSensorGetDataOutputTILT)
        GetProcAddress(hSensorLib, L"HTCSensorGetDataOutput");

    pfnHTCSensorQueryCapability = (PFN_HTCSensorQueryCapability)
        GetProcAddress(hSensorLib, L"HTCSensorQueryCapability");

    if (pfnHTCSensorOpen == NULL ||
        pfnHTCSensorClose == NULL ||
        pfnHTCSensorGetDataOutput == NULL)
    {
        return false;
    }

    *phSensor = pfnHTCSensorOpen(sensorValue);

    return true;
}

bool SensorUninit(HANDLE hSensor)
{
    pfnHTCSensorClose(hSensor);

    return true;
}

void SensorGetData(HANDLE hSensor, SENSORDATA* pSensorData)
{
    pfnHTCSensorGetDataOutput(hSensor, pSensorData);
}

void SensorGetDataTilt(HANDLE hSensor, SENSORDATATILT* pSensorData)
{
    pfnHTCSensorGetDataOutputTilt(hSensor, pSensorData);
}

SHORT SensorGetOrientation(SENSORDATATILT sdTilt, SHORT shCalibration)
{
    SHORT shOrientation = sdTilt.Orientation;

    shOrientation = shOrientation + shCalibration;

    if (shOrientation > 1000)
    {
        shOrientation = 1000 - (shOrientation - 1000);
    }
    if (shOrientation < -1000)
    {
        shOrientation = -1000 + (-1000 - shOrientation);
    }

    return shOrientation;
}

SHORT SensorGetOrientation(HANDLE hSensor, SHORT shCalibration)
{
    SENSORDATATILT sdTilt;
    SensorGetDataTilt(hSensor, &sdTilt);

    return SensorGetOrientation(sdTilt, shCalibration);
}
*/