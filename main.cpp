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
enum KEY_STATE {
  KS_UP, KS_DOWN, KS_HOLD, KS_TAP
};
int layer = 0;
int capsState = KS_UP, altState = KS_UP, ctrlState = KS_UP;
bool capsDown = false, altDown = false, ctrlDown = false;
bool hDown = false, jDown = false, kDown = false, lDown = false, yDown = false, oDown = false;

// Timer callback function for cursor movement
VOID CALLBACK timerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    POINT p;
    if (!enabled) return;
    if (!GetCursorPos(&p)) return;
    if (layer != 1) return;
    long long time = getTime();
    long long delta = (lastTime == 0) ? 0 : time - lastTime;

    // Cursor movement
    int _h = (int)lDown - (int)hDown;
    int _v = (int)jDown - (int)kDown;

    double _spd = (double)delta*0.35;
    if (capsDown) _spd *= 2.0;
    if (_h != 0 && _v != 0) _spd *= 0.70710678118;

    SetCursorPos(p.x + (int)(_h*_spd), p.y + (int)(_v*_spd));

    // Wheel movement
    int movement = (int)oDown - (int)yDown;
    _spd = (double)delta;
    if (capsDown) _spd *= 2.0;
    if (movement != 0) {
      inputMouseWheel((int)(movement*_spd));
    }

    lastTime = time;
}

void updateKeyState(int &keyState, bool match, bool keydown) {
  if (keyState == KS_TAP)
    keyState = KS_UP;
  if (match) {
    if (keydown) {
      keyState = (keyState == KS_UP) ? KS_DOWN : keyState;
    } else {
      if (keyState == KS_DOWN)
        keyState = KS_TAP;
      else
        keyState = KS_UP;
    }
  } else {
    if (keyState == KS_DOWN && keydown) {
      keyState = KS_HOLD;
    }
  }
}

void updateDownState(bool &keyState, bool match, bool keydown) {
  if (match) {
    keyState = keydown;
  }
}

LRESULT CALLBACK hookKeyboard(int nCode, WPARAM wParam, LPARAM lParam) {
  bool keydown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
  KBDLLHOOKSTRUCT *key = (KBDLLHOOKSTRUCT *)lParam;
  DWORD vkCode = key->vkCode;

  // ignore injected events
  if (!enabled) goto passing;
  if (nCode != HC_ACTION) goto passing;
  if (key->dwExtraInfo == 0x12345678) goto passing;
  
  // update key states
  updateKeyState(capsState, vkCode == VK_CAPITAL, keydown);
  updateKeyState(altState, vkCode == VK_LMENU, keydown);
  updateKeyState(ctrlState, vkCode == VK_LCONTROL, keydown);
  updateDownState(capsDown, vkCode == VK_CAPITAL, keydown);
  updateDownState(altDown, vkCode == VK_LMENU, keydown);
  updateDownState(ctrlDown, vkCode == VK_LCONTROL, keydown);
  updateDownState(hDown, vkCode == VK_H, keydown);
  updateDownState(jDown, vkCode == VK_J, keydown);
  updateDownState(kDown, vkCode == VK_K, keydown);
  updateDownState(lDown, vkCode == VK_L, keydown);
  updateDownState(yDown, vkCode == VK_Y, keydown);
  updateDownState(oDown, vkCode == VK_O, keydown);

  // layer switching
  layer = altDown ? 1 : 0;
  // safe release
  if (!capsDown && !ctrlDown && isKeyDown(VK_LCONTROL))
    inputKey(VK_LCONTROL, 0, 1);
  if (!altDown && isKeyDown(VK_LMENU))
    inputKey(VK_LMENU, 0, 1);
  
  if (layer == 0) { // ------- LAYER 0
    if (capsState == KS_HOLD && !isKeyDown(VK_LCONTROL))
      inputKey(VK_LCONTROL, 0, 0);
    if (capsState == KS_TAP) {
      inputKey(VK_ESCAPE, 0, 0);
      inputKey(VK_ESCAPE, 0, 1);
    }
    if (altState == KS_TAP) {
      inputKey(VK_LMENU, 0, 0);
      inputKey(VK_LMENU, 0, 1);
    }
  } else if (layer == 1) {  // LAYER 1
    if (altState == KS_HOLD && !isKeyDown(VK_LMENU))
      inputKey(VK_LMENU, 0, 0);
    if (vkCode == VK_U && keydown && !isKeyDown(VK_LBUTTON))
      inputMouse(true, 0);
    if (vkCode == VK_I && keydown && !isKeyDown(VK_RBUTTON))
      inputMouse(false, 0);
    if (vkCode == VK_U && !keydown && isKeyDown(VK_LBUTTON))
      inputMouse(true, 1);
    if (vkCode == VK_I && !keydown && isKeyDown(VK_RBUTTON))
      inputMouse(false, 1);
    if (vkCode == VK_H || vkCode == VK_J || vkCode == VK_K || vkCode == VK_L || vkCode == VK_U || vkCode == VK_I || vkCode == VK_Y || vkCode == VK_O)
      return 1;
  }

  if(vkCode == VK_CAPITAL || vkCode == VK_LMENU)
    return 1;
  
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
    if(msg.message == WM_QUIT) {
      if (isKeyDown(VK_CAPITAL)) inputKey(VK_CAPITAL, 0, 1);
      if (isKeyDown(VK_LMENU)) inputKey(VK_LMENU, 0, 1);
      if (isKeyDown(VK_LCONTROL)) inputKey(VK_LCONTROL, 0, 1);
      break;
    }

    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return 0;
}