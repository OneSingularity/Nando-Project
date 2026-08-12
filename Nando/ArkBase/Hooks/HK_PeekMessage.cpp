#include "pch.h"
#include <atomic>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#include "Hooks.h"
#include "Interface/Menu.h"

namespace Hooks {
using PeekMessageW_t = BOOL(WINAPI *)(LPMSG, HWND, UINT, UINT, UINT);
static PeekMessageW_t Orig_PeekMessageW = nullptr;
static HWND g_GameHWND = nullptr;

// Track mouse input and key states using standard arrays
bool g_KeyState[256] = {false};
bool g_KeyPendingDown[256] = {false};
POINT g_MousePos = {0, 0};

static void RecordInput(LPMSG lpMsg) {
  if (!lpMsg)
    return;
  UINT msg = lpMsg->message;
  WPARAM wParam = lpMsg->wParam;

  if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
    if (wParam < 256) {
      g_KeyState[wParam] = true;
      g_KeyPendingDown[wParam] = true;
    }
  } else if (msg == WM_KEYUP || msg == WM_SYSKEYUP) {
    if (wParam < 256) {
      g_KeyState[wParam] = false;
    }
  } else if (msg == WM_LBUTTONDOWN) {
    g_KeyState[VK_LBUTTON] = true;
    g_KeyPendingDown[VK_LBUTTON] = true;
  } else if (msg == WM_LBUTTONUP) {
    g_KeyState[VK_LBUTTON] = false;
  } else if (msg == WM_RBUTTONDOWN) {
    g_KeyState[VK_RBUTTON] = true;
    g_KeyPendingDown[VK_RBUTTON] = true;
  } else if (msg == WM_RBUTTONUP) {
    g_KeyState[VK_RBUTTON] = false;
  } else if (msg == WM_MBUTTONDOWN) {
    g_KeyState[VK_MBUTTON] = true;
    g_KeyPendingDown[VK_MBUTTON] = true;
  } else if (msg == WM_MBUTTONUP) {
    g_KeyState[VK_MBUTTON] = false;
  } else if (msg == WM_XBUTTONDOWN) {
    WORD xButton = GET_XBUTTON_WPARAM(wParam);
    int vk = (xButton == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
    g_KeyState[vk] = true;
    g_KeyPendingDown[vk] = true;
  } else if (msg == WM_XBUTTONUP) {
    WORD xButton = GET_XBUTTON_WPARAM(wParam);
    int vk = (xButton == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
    g_KeyState[vk] = false;
  }

  // Get mouse coordinates relative to game client window
  if (g_GameHWND) {
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(g_GameHWND, &pt);
    g_MousePos = pt;
  }
}

static BOOL WINAPI Hooked_PeekMessageW(LPMSG lpMsg, HWND hWnd,
                                       UINT wMsgFilterMin, UINT wMsgFilterMax,
                                       UINT wRemoveMsg) {
  BOOL res =
      Orig_PeekMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
  if (bShuttingDown)
    return res;

  if (res && lpMsg) {
    if (!g_GameHWND && lpMsg->hwnd) {
      g_GameHWND = lpMsg->hwnd;
    }
    RecordInput(lpMsg);

    // Block inputs if the menu is open to prevent clicking through in game
    if (Menu::g_Open) {
      UINT msg = lpMsg->message;
      if ((msg >= WM_KEYFIRST && msg <= WM_KEYLAST) ||
          (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST)) {
        // Suppress key inputs so they don't reach game, except for the Toggle
        // key
        if (lpMsg->message == WM_KEYDOWN && lpMsg->wParam == VK_F8) {
          // Pass through toggle key
        } else {
          lpMsg->message = WM_NULL; // Block dispatching
        }
      }
    }
  }
  return res;
}

static bool HookIAT(const char *importModuleName, const char *functionName,
                    void *newFunc, void **oldFunc) {
  HMODULE mainMod = GetModuleHandleA(nullptr);
  if (!mainMod)
    return false;

  ULONG size = 0;
  PIMAGE_IMPORT_DESCRIPTOR importDesc =
      reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(ImageDirectoryEntryToData(
          mainMod, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size));
  if (!importDesc)
    return false;

  BYTE *base = reinterpret_cast<BYTE *>(mainMod);
  for (; importDesc->Name; importDesc++) {
    const char *name = reinterpret_cast<const char *>(base + importDesc->Name);
    if (_stricmp(name, importModuleName) == 0) {
      auto originalFirstThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(
          base + importDesc->OriginalFirstThunk);
      auto firstThunk =
          reinterpret_cast<PIMAGE_THUNK_DATA>(base + importDesc->FirstThunk);

      for (; firstThunk->u1.Function; originalFirstThunk++, firstThunk++) {
        if (originalFirstThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)
          continue;

        auto importByName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(
            base + originalFirstThunk->u1.AddressOfData);
        if (strcmp(reinterpret_cast<const char *>(importByName->Name),
                   functionName) == 0) {
          DWORD oldProt;
          if (VirtualProtect(&firstThunk->u1.Function, sizeof(uintptr_t),
                             PAGE_READWRITE, &oldProt)) {
            if (oldFunc && !*oldFunc) {
              *oldFunc = reinterpret_cast<void *>(firstThunk->u1.Function);
            }
            firstThunk->u1.Function = reinterpret_cast<uintptr_t>(newFunc);
            VirtualProtect(&firstThunk->u1.Function, sizeof(uintptr_t), oldProt,
                           &oldProt);
            return true;
          }
        }
      }
    }
  }
  return false;
}

bool HookPeekMessage() {
  return HookIAT("user32.dll", "PeekMessageW",
                 reinterpret_cast<void *>(&Hooked_PeekMessageW),
                 reinterpret_cast<void **>(&Orig_PeekMessageW));
}

void UnhookPeekMessage() {
  if (Orig_PeekMessageW) {
    void *dummy = nullptr;
    HookIAT("user32.dll", "PeekMessageW",
            reinterpret_cast<void *>(Orig_PeekMessageW), &dummy);
    Orig_PeekMessageW = nullptr;
  }
}
} // namespace Hooks
