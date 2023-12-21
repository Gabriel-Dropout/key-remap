#ifndef HELPER_HPP
#define HELPER_HPP
#include <chrono>

// Get current time in milliseconds
static long long getTime() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// Send keyboard input
static void inputKey(WORD vkCode, WORD scanCode, bool keyup) {
  bool is_extended_key = scanCode >> 8 == 0xE0;

  INPUT input = {
    .type = INPUT_KEYBOARD,
    .ki   = {
      .wVk          = vkCode,
      .wScan        = scanCode,
      .dwFlags      = (DWORD)(
                        (keyup ? KEYEVENTF_KEYUP : 0) |
                        (is_extended_key ? KEYEVENTF_EXTENDEDKEY : 0)
                      ),
      .time         = 0,
      .dwExtraInfo  = (ULONG_PTR)0x12345678
    }
  };

  SendInput(1, &input, sizeof(input));
}

// Send mouse input
static void inputMouse(bool isLeft, bool keyup) {
  INPUT input = {
    .type = INPUT_MOUSE,
    .mi   = {
      .dx           = 0,
      .dy           = 0,
      .mouseData    = 0,
      .dwFlags      = (DWORD)(
                        isLeft ?
                          (keyup ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_LEFTDOWN)
                        :
                          (keyup ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_RIGHTDOWN)
                      ),
      .time        = 0,
      .dwExtraInfo = (ULONG_PTR)0x12345678
    }
  };

  SendInput(1, &input, sizeof(input));
}

// send mouse wheel input
static void inputMouseWheel(int movement) {
  INPUT input = {
    .type = INPUT_MOUSE,
    .mi   = {
      .dx           = 0,
      .dy           = 0,
      .mouseData    = (DWORD)(movement),
      .dwFlags      = MOUSEEVENTF_WHEEL,
      .time        = 0,
      .dwExtraInfo = (ULONG_PTR)0x12345678
    }
  };

  SendInput(1, &input, sizeof(input));
}

#endif // HELPER_HPP
