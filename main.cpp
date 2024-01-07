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
double cursorSpeed = 1.0;

// Timer stuff
long long lastTime = 0;

DWORD mainThreadID = GetCurrentThreadId();
DWORD trayThreadID = 0;

HHOOK kHook;

int layer = 0;
KeyState  capsState = {VK_CAPS,},
          altState  = {VK_ALT,},
          ctrlState = {VK_LCONTROL,},
          hState    = {VK_H,},
          jState    = {VK_J,},
          kState    = {VK_K,},
          lState    = {VK_L,},
          yState    = {VK_Y,},
          oState    = {VK_O,},
          uState    = {VK_U,},
          iState    = {VK_I,};

// Timer callback function for cursor movement
VOID CALLBACK timerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    long long time = getTime();
    long long delta = (lastTime == 0) ? 0 : time - lastTime;
    lastTime = time;

    POINT p;
    if (!enabled) return;
    if (!GetCursorPos(&p)) return;
    if (layer != 1) return;

    // Cursor movement
    int _h = (int)lState.down - (int)hState.down;
    int _v = (int)jState.down - (int)kState.down;

    double _spd = (double)delta*0.3*cursorSpeed;
    if (capsState.down) _spd *= 2.0;
    if (_h != 0 && _v != 0) _spd *= 0.70710678118;
    if (_h != 0 || _v != 0) {
      SetCursorPos(p.x + (int)(_h*_spd), p.y + (int)(_v*_spd));
    }

    // Wheel movement
    int _wheel = (int)oState.down - (int)yState.down;
    _spd = (double)delta;
    if (capsState.down) _spd *= 1.5;
    if (_wheel != 0) {
      inputMouseWheel((int)(_wheel*_spd));
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
  updateKeyState(capsState, vkCode, keydown);
  updateKeyState(altState, vkCode, keydown);
  updateKeyState(ctrlState, vkCode, keydown);
  updateKeyState(hState, vkCode, keydown);
  updateKeyState(jState, vkCode, keydown);
  updateKeyState(kState, vkCode, keydown);
  updateKeyState(lState, vkCode, keydown);
  updateKeyState(yState, vkCode, keydown);
  updateKeyState(oState, vkCode, keydown);
  updateKeyState(uState, vkCode, keydown);
  updateKeyState(iState, vkCode, keydown);

  // layer switching
  layer = altState.down ? 1 : 0;
  // safe release
  if (!capsState.down && !ctrlState.down && isKeyDown(VK_LCONTROL))
    inputKey(VK_LCONTROL, 0, 1);
  if (!altState.down && isKeyDown(VK_LMENU))
    inputKey(VK_LMENU, 0, 1);
  if (uState.released && isKeyDown(VK_LBUTTON)) {
    inputMouse(true, 1);
  }
  if (iState.released && isKeyDown(VK_RBUTTON)) {
    inputMouse(false, 1);
  }
  
  if (layer == 0) { // ------- LAYER 0
    if (capsState.state == KS_HOLD && !isKeyDown(VK_LCONTROL))
      inputKey(VK_LCONTROL, 0, 0);
    if (capsState.state == KS_TAP) {
      inputKey(VK_ESCAPE, 0, 0);
      inputKey(VK_ESCAPE, 0, 1);
    }
    if (altState.state == KS_TAP) {
      inputKey(VK_LMENU, 0, 0);
      inputKey(VK_LMENU, 0, 1);
    }
  } else if (layer == 1) {  // LAYER 1
    if (altState.state == KS_HOLD && !isKeyDown(VK_LMENU) && !(vkCode == VK_H || vkCode == VK_J || vkCode == VK_K || vkCode == VK_L || vkCode == VK_U || vkCode == VK_I || vkCode == VK_Y || vkCode == VK_O))
      inputKey(VK_LMENU, 0, 0);
    if (uState.pressed) {
      if (isKeyDown(VK_LMENU)) inputKey(VK_LMENU, 0, 1);
      inputMouse(true, 0);
    }
    if (iState.pressed) {
      if (isKeyDown(VK_LMENU)) inputKey(VK_LMENU, 0, 1);
      inputMouse(false, 0);
    }
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
void select_cb(struct tray_menu *item);
void quit_cb(struct tray_menu *item);

struct tray tray = {
    .icon = "MAINICON",
    .menu = (struct tray_menu[]) {
      {"Enable", 0, 1, toggle_cb, NULL},
      {"-", 0, 0, NULL, NULL},
      {"Cursor Speed", 1, 0, NULL, NULL},
      {"x0.6", 0, 0, select_cb, (void*)0},
      {"x0.8", 0, 0, select_cb, (void*)1},
      {"x1.0", 0, 1, select_cb, (void*)2},
      {"x1.2", 0, 0, select_cb, (void*)3},
      {"x1.5", 0, 0, select_cb, (void*)4},
      {"x2.0", 0, 0, select_cb, (void*)5},
      {"-", 0, 0, NULL, NULL},
      {"Quit", 0, 0, quit_cb, NULL},
      {NULL, 0, 0, NULL, NULL}
    },
};

void toggle_cb(struct tray_menu *item) {
	item->checked = !item->checked;
  enabled = item->checked;
	tray_update(&tray);
}

void select_cb(struct tray_menu *item) {
  for (int i = 3; i <= 8; i++) {
    tray.menu[i].checked = false;
  }
  item->checked = true;
  double spds[] = {0.6, 0.8, 1.0, 1.2, 1.5, 2.0};
  intptr_t idx = (intptr_t)item->context;
  cursorSpeed = spds[idx];
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