#include <iostream>
#include <string.h>

#include <windows.h>
#include <winerror.h>
#include <winuser.h>

#define TRAY_WINAPI 1
#include "tray.h"
#include "helper.hpp"

#define UNUSED [[maybe_unused]]
#define VK_CAPS 0x14
#define VK_ALT 0xA4
#define VK_H 0x48
#define VK_J 0x4A
#define VK_K 0x4B
#define VK_L 0x4C
#define VK_U 0x55
#define VK_I 0x49
#define VK_Y 0x59
#define VK_O 0x4F

bool enabled = true;

// Timer stuff
long long lastTime = 0;

DWORD mainThreadID = GetCurrentThreadId();
DWORD trayThreadID = 0;

HHOOK kHook;
// Keyboard input states
typedef struct KeyState {
  DWORD vkCode;
  bool isDown, isPressed, isReleased;
} KeyState;
enum KEY_STATE {
  KS_CAPS, KS_ALT,
  KS_H, KS_J, KS_K, KS_L,
  KS_U, KS_I,
  KS_Y, KS_O,
  KS_COUNT,
};
KeyState keyStates[KS_COUNT] = {
  {VK_CAPS, false, false, false},
  {VK_ALT, false, false, false},
  {VK_H, false, false, false},
  {VK_J, false, false, false},
  {VK_K, false, false, false},
  {VK_L, false, false, false},
  {VK_U, false, false, false},
  {VK_I, false, false, false},
  {VK_Y, false, false, false},
  {VK_O, false, false, false},
};

// Used to determine the behavior of the Caps/Alt release event
enum KEY_AFTER {
  KA_NONE,
  KA_SPECIAL,
  KA_NORMAL,
};
int keyAfterCaps = KA_NONE, keyAfterAlt = KA_NONE;

// Timer callback function for cursor movement
VOID CALLBACK timerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    POINT p;
    if (!enabled) return;
    if (!GetCursorPos(&p)) return;
    if (!keyStates[KS_ALT].isDown) return;
    long long time = getTime();
    long long delta = (lastTime == 0) ? 0 : time - lastTime;

    // Cursor movement
    int _h = (int)keyStates[KS_L].isDown - (int)keyStates[KS_H].isDown;
    int _v = (int)keyStates[KS_J].isDown - (int)keyStates[KS_K].isDown;

    double _spd = (double)delta*0.4;
    if(keyStates[KS_CAPS].isDown) _spd *= 2.0;
    if(_h != 0 && _v != 0) _spd *= 0.70710678118;

    SetCursorPos(p.x + (int)(_h*_spd), p.y + (int)(_v*_spd));

    // Wheel movement
    int movement = (int)keyStates[KS_O].isDown - (int)keyStates[KS_Y].isDown;
    _spd = (double)delta;
    if(keyStates[KS_CAPS].isDown) _spd *= 2.0;
    if(movement != 0) {
      inputMouseWheel((int)(movement*_spd));
    }

    lastTime = time;
}

LRESULT CALLBACK hookKeyboard(int nCode, WPARAM wParam, LPARAM lParam) {
  bool keydown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
  KBDLLHOOKSTRUCT *key = (KBDLLHOOKSTRUCT *)lParam;
  DWORD vkCode = key->vkCode;
  bool uJustPressed = false, iJustPressed = false, uJustReleased = false, iJustReleased = false;

  // Ignore injected events
  if (!enabled) goto passing;
  if (nCode != HC_ACTION) goto passing;
  if (key->dwExtraInfo == 0x12345678) goto passing;

  // Set keyboard input states
  for(int i=0; i<KS_COUNT; i++) {
    if(keyStates[i].vkCode == vkCode) {
      keyStates[i].isPressed = keydown && !keyStates[i].isDown;
      keyStates[i].isReleased = !keydown && keyStates[i].isDown;
      keyStates[i].isDown = keydown;
    } else {
      keyStates[i].isPressed = false;
      keyStates[i].isReleased = false;
    }
  }

  // Set afterkey states
  if(keydown && keyStates[KS_CAPS].isDown) {
    if(vkCode == VK_CAPITAL) {
      // Do nothing
    } else if(vkCode == VK_ALT) {
      keyAfterCaps = keyAfterCaps==KA_NONE ? KA_SPECIAL : keyAfterCaps;
    } else {
      keyAfterCaps = KA_NORMAL;
    }
  }
  if(keydown && keyStates[KS_ALT].isDown) {
    if(vkCode == VK_LMENU) {
      // Do nothing
    } else if (vkCode == VK_CAPS || vkCode == VK_H || vkCode == VK_J || vkCode == VK_K || vkCode == VK_L ||
               vkCode == VK_U || vkCode == VK_I || vkCode == VK_Y || vkCode == VK_O) {
      keyAfterAlt = keyAfterAlt==KA_NONE ? KA_SPECIAL : keyAfterAlt;
    } else {
      keyAfterAlt = KA_NORMAL;
    }
  }

  // Handle each case
  if (keyStates[KS_CAPS].isPressed || keyStates[KS_ALT].isPressed) {
    return 1;
  }
  if (keyStates[KS_CAPS].isReleased) {
    if (keyAfterCaps == KA_NONE) {
      // Send Esc key
      inputKey(VK_ESCAPE, 0, 0);
      inputKey(VK_ESCAPE, 0, 1);
    } else if (keyAfterCaps == KA_SPECIAL) {
      // Do nothing
    } else if (keyAfterCaps == KA_NORMAL) {
      // Release Ctrl key
      inputKey(VK_CONTROL, 0, 1);
    }
    keyAfterCaps = KA_NONE;
    return 1;
  }
  if (keyStates[KS_ALT].isReleased) {
    if (keyAfterAlt == KA_NONE) {
      // Send Alt key
      inputKey(VK_LMENU, 0, 0);
      inputKey(VK_LMENU, 0, 1);
    } else if (keyAfterAlt == KA_SPECIAL) {
      // Do nothing
    } else if (keyAfterAlt == KA_NORMAL) {
      // Release Alt key
      inputKey(VK_LMENU, 0, 1);
    }
    keyAfterAlt = KA_NONE;
    return 1;
  }
  if (keyStates[KS_ALT].isDown) {
    if(keyStates[KS_U].isPressed) inputMouse(true, 0);
    if(keyStates[KS_U].isReleased) inputMouse(true, 1);
    if(keyStates[KS_I].isPressed) inputMouse(false, 0);
    if(keyStates[KS_I].isReleased) inputMouse(false, 1);
    if(vkCode == VK_H || vkCode == VK_J || vkCode == VK_K || vkCode == VK_L || vkCode == VK_U || vkCode == VK_I || vkCode == VK_Y || vkCode == VK_O)
       return 1;
    // Send Alt+Key
    if (keydown) inputKey(VK_LMENU, 0, 0);
  }
  if (keyStates[KS_CAPS].isDown) {
    // Send Ctrl+Key
    if (keydown) inputKey(VK_CONTROL, 0, 0);
  }
  
passing:
  return CallNextHookEx(kHook, nCode, wParam, lParam);
}

/// Tray stuff ///
void toggle_cb(struct tray_menu *item);
void quit_cb(struct tray_menu *item);

struct tray tray = {
    .icon = "MAINICON",
    .menu = (struct tray_menu[]) {
      {"Enable", 0, 1, toggle_cb, NULL},
      {"Quit", 0, 0, quit_cb, NULL},
      {NULL, 0, 0, NULL, NULL}
    },
};

void toggle_cb(struct tray_menu *item) {
	item->checked = !item->checked;
  enabled = item->checked;
	tray_update(&tray);
}

void quit_cb(UNUSED struct tray_menu *item) {
  PostThreadMessage(mainThreadID, WM_QUIT, 0, 0);
  tray_exit();
}

void tray_thread(UNUSED void *arg) {
  tray_init(&tray);
  while (tray_loop(1) == 0);
  tray_exit();
}

// When the user closes the console window
BOOL WINAPI consoleHandler(DWORD signal) {
  if (signal == CTRL_CLOSE_EVENT) {
    // Send WM_QUIT to tray thread
    PostThreadMessage(trayThreadID, WM_QUIT, 0, 0);
    return TRUE;
  }
  return FALSE;
}

/// Main ///
int main() {
  UNUSED HANDLE mutex = CreateMutex(NULL, TRUE, "key-remap.single");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    MessageBox(NULL, "Program is already running!", "Alert", MB_OK);
    return -1;
  }

  UNUSED HANDLE trayThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)tray_thread, NULL, 0, &trayThreadID);
  if(trayThread == NULL) {
    MessageBox(NULL, "Failed to create tray thread!", "Alert", MB_OK);
    return -1;
  }

  SetConsoleCtrlHandler(consoleHandler, TRUE);
  SetTimer(NULL, 0, 8, timerProc);

  kHook = SetWindowsHookEx(WH_KEYBOARD_LL, hookKeyboard, NULL, 0);
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    if(msg.message == WM_QUIT) break;

    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return 0;
}