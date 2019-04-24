#include "unfocusedKeyInfo.h"

#ifdef __WIN32__
#include <stdint.h>
#include <windows.h>
#else
#include <X11/Xlib.h>
#endif//__WIN32__


bool isAnyKeyPressed()
{
#ifdef __WIN32__
    uint8_t keys[256];
    GetKeyboardState(keys);
    for(int n=0; n<256;n++)
        if (keys[n] & 0x80)
            return true;
#else
    Display* display = XOpenDisplay(nullptr);
    char keys[32];
    XQueryKeymap(display, keys);
    XCloseDisplay(display);
    for(int n=0; n<32; n++)
        if (keys[n])
            return true;
#endif
    return false;
}

bool isExitKeyPressed()
{
#ifdef __WIN32__
    uint8_t keys[256];
    GetKeyboardState(keys);
    return keys[51];
#else
    Display* display = XOpenDisplay(nullptr);
    char keys[32];
    XQueryKeymap(display, keys);
    XCloseDisplay(display);
    return keys[51 / 8] & (1 << (51 % 8));
#endif
}
