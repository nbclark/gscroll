#pragma once

#include "touchhandler.h"
CTouchHandler* g_ScrollApp;

#define HTCNAV_GET_DATA_IOCTRL      0xA000C05C

typedef struct tagHTCTOUCH_RAWDATA
{
    BYTE action;   
    SHORT xPosLP;
    SHORT yPosLP;
    SHORT wheelDeg;
    SHORT xPosRP;
    SHORT yPosRP;
} HTCTOUCH_RAWDATA;
