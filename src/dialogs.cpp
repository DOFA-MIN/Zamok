#include "dialogs.h"
#include <windowsx.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

// ------------------------------------------------------------------
// Password dialog — pure Win32, no resource dependency
// ------------------------------------------------------------------

#define IDC_PWD_PROMPT   1000
#define IDC_PWD_EDIT     1001
#define IDC_PWD_OK       1002
#define IDC_PWD_CANCEL   1003

struct PwdDlgState {
    std::wstring title;
    std::wstring prompt;
    std::wstring result;
    bool         confirmed = false;
    HFONT        hFont     = NULL;
    HWND         hEdit     = NULL;
};

static void CenterWindow(HWND hwnd, HWND owner) {
    RECT rc, rcOwner;
    GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    int x, y;
    if (owner && IsWindow(owner) && GetWindowRect(owner, &rcOwner)) {
        x = rcOwner.left + ((rcOwner.right - rcOwner.left) - w) / 2;
        y = rcOwner.top  + ((rcOwner.bottom - rcOwner.top) - h) / 2;
    } else {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        x = (sw - w) / 2;
        y = (sh - h) / 2;
    }
    // Clamp to visible area
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

static LRESULT CALLBACK PwdWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PwdDlgState* st = reinterpret_cast<PwdDlgState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        st = reinterpret_cast<PwdDlgState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));

        // Font
        NONCLIENTMETRICSW ncm = { sizeof(ncm) };
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        st->hFont = CreateFontIndirectW(&ncm.lfMessageFont);

        // Prompt static
        HWND hPrompt = CreateWindowExW(0, L"STATIC", st->prompt.c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            16, 16, 368, 48, hwnd, (HMENU)IDC_PWD_PROMPT, GetModuleHandleW(NULL), NULL);
        if (st->hFont) SendMessageW(hPrompt, WM_SETFONT, (WPARAM)st->hFont, TRUE);

        // Edit
        st->hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_PASSWORD | ES_AUTOHSCROLL,
            16, 72, 368, 24, hwnd, (HMENU)IDC_PWD_EDIT, GetModuleHandleW(NULL), NULL);
        if (st->hFont) SendMessageW(st->hEdit, WM_SETFONT, (WPARAM)st->hFont, TRUE);
        SendMessageW(st->hEdit, EM_SETLIMITTEXT, 256, 0);

        // OK
        HWND hOk = CreateWindowExW(0, L"BUTTON", L"OK",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON | BS_TEXT,
            110, 110, 80, 26, hwnd, (HMENU)IDC_PWD_OK, GetModuleHandleW(NULL), NULL);
        if (st->hFont) SendMessageW(hOk, WM_SETFONT, (WPARAM)st->hFont, TRUE);

        // Cancel
        HWND hCancel = CreateWindowExW(0, L"BUTTON", L"Отмена",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | BS_TEXT,
            208, 110, 80, 26, hwnd, (HMENU)IDC_PWD_CANCEL, GetModuleHandleW(NULL), NULL);
        if (st->hFont) SendMessageW(hCancel, WM_SETFONT, (WPARAM)st->hFont, TRUE);

        SetWindowTextW(hwnd, st->title.c_str());
        CenterWindow(hwnd, GetParent(hwnd));
        SetFocus(st->hEdit);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        if (id == IDC_PWD_OK && code == BN_CLICKED) {
            wchar_t buf[512] = {0};
            GetWindowTextW(st->hEdit, buf, 511);
            st->result = buf;
            // Securely clear edit buffer
            SecureZeroMemory(buf, sizeof(buf));
            // Validate: empty password not allowed (caller decides)
            if (st->result.empty()) {
                MessageBeep(MB_ICONWARNING);
                SetFocus(st->hEdit);
                return 0;
            }
            st->confirmed = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (id == IDC_PWD_CANCEL && code == BN_CLICKED) {
            st->confirmed = false;
            DestroyWindow(hwnd);
            return 0;
        }
        if (id == IDC_PWD_EDIT && code == EN_UPDATE) {
            // Enable/disable OK based on content? optional
            break;
        }
        break;
    }
    case WM_CLOSE:
        if (st) st->confirmed = false;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (st && st->hFont) { DeleteObject(st->hFont); st->hFont = NULL; }
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && st) {
            st->confirmed = false;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool dialog_password(HWND owner, const std::wstring& title, const std::wstring& prompt, std::wstring& out_password) {
    out_password.clear();

    const wchar_t CLASS_NAME[] = L"ZamokPwdDlgClass";
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc   = PwdWndProc;
        wc.hInstance     = GetModuleHandleW(NULL);
        wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = CLASS_NAME;
        wc.hIcon         = LoadIconW(NULL, IDI_SHIELD);
        RegisterClassW(&wc);
        registered = true;
    }

    PwdDlgState state;
    state.title  = title;
    state.prompt = prompt;

    InitCommonControls();

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        CLASS_NAME, title.c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 400, 170,
        owner, NULL, GetModuleHandleW(NULL), &state);

    if (!hwnd) return false;

    // Modal behavior: disable owner
    bool ownerDisabled = false;
    if (owner && IsWindow(owner) && IsWindowEnabled(owner)) {
        EnableWindow(owner, FALSE);
        ownerDisabled = true;
    }

    // Message loop — modal
    MSG msg;
    while (IsWindow(hwnd) && GetMessageW(&msg, NULL, 0, 0)) {
        // Handle Enter as OK, Escape as Cancel
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_RETURN && GetFocus() == state.hEdit) {
                wchar_t buf[512] = {0};
                GetWindowTextW(state.hEdit, buf, 511);
                state.result = buf;
                SecureZeroMemory(buf, sizeof(buf));
                if (!state.result.empty()) {
                    state.confirmed = true;
                    DestroyWindow(hwnd);
                    continue;
                }
            }
            if (msg.wParam == VK_ESCAPE) {
                state.confirmed = false;
                DestroyWindow(hwnd);
                continue;
            }
        }
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!IsWindow(hwnd)) break;
    }

    if (ownerDisabled) EnableWindow(owner, TRUE);
    if (owner) SetForegroundWindow(owner);

    if (state.confirmed && !state.result.empty()) {
        out_password = state.result;
        // Clear intermediate copy
        SecureZeroMemory(&state.result[0], state.result.size() * sizeof(wchar_t));
        return true;
    }
    // Clear any residual
    if (!state.result.empty())
        SecureZeroMemory(&state.result[0], state.result.size() * sizeof(wchar_t));
    return false;
}

void dialog_message(HWND owner, const std::wstring& title, const std::wstring& message, bool is_error) {
    UINT type = MB_OK | MB_TOPMOST | MB_SETFOREGROUND;
    type |= is_error ? MB_ICONERROR : MB_ICONINFORMATION;
    if (owner) type |= MB_APPLMODAL;
    MessageBoxW(owner, message.c_str(), title.c_str(), type);
}
