#pragma once
#include "GScroll.h"

LRESULT CALLBACK LLKeyboardHookCallbackFunction( int nCode, WPARAM wParam, LPARAM lParam );
LRESULT CALLBACK CursorProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);