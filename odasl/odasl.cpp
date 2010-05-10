// ODASL.DLL import library fake.

#include "odasl.h"

#ifdef __cplusplus
extern "C" {
#endif

ODASL_API(int) wplInit(wplOptions *inf) { return 0; }
ODASL_API(void) wplExit() {}
ODASL_API(void) wplSetPalette(HPALETTE pal) {}
ODASL_API(int) wplGetSystemMetrics(int nIndex) { return 0; }
ODASL_API(BOOL) wplAdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu) { return 0; }
ODASL_API(HBITMAP) wplLoadResourceBitmap(HMODULE hModule, LPCTSTR lpName) { return NULL; }
ODASL_API(int) wplManualDialogSubclass(HWND dlg) { return 0; }
ODASL_API(void) wplEnable() {}
ODASL_API(void) wplDisable() {}

#ifdef __cplusplus
}
#endif
