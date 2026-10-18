#include "dialogs.h"

#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

#define IDC_PASSWORD_EDIT 1001
#define IDC_PASSWORD_OK   1002
#define IDC_PASSWORD_CANCEL 1003

static std::wstring g_password_result;

INT_PTR CALLBACK PasswordDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        SetWindowTextW(hDlg, L"Zamok");
        SetFocus(GetDlgItem(hDlg, IDC_PASSWORD_EDIT));
        return TRUE;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == IDC_PASSWORD_OK) {
            wchar_t buf[256] = {0};
            GetWindowTextW(GetDlgItem(hDlg, IDC_PASSWORD_EDIT), buf, 255);
            g_password_result = buf;
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDC_PASSWORD_CANCEL || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

bool dialog_password(HWND owner, const std::wstring& title, const std::wstring& prompt, std::wstring& out_password) {
    g_password_result.clear();

    // Create a simple dialog dynamically
    HWND hDlg = CreateDialogW(NULL, MAKEINTRESOURCE(0), owner, PasswordDlgProc);
    if (!hDlg) return false;

    SetWindowTextW(hDlg, title.c_str());

    // Static prompt
    HWND hStatic = CreateWindowW(L"STATIC", prompt.c_str(),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 20, 360, 60, hDlg, NULL, NULL, NULL);

    // Edit control
    HWND hEdit = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD | ES_AUTOHSCROLL,
        20, 90, 360, 24, hDlg, (HMENU)IDC_PASSWORD_EDIT, NULL, NULL);

    // OK button
    HWND hOk = CreateWindowW(L"BUTTON", L"OK",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        80, 130, 80, 30, hDlg, (HMENU)IDC_PASSWORD_OK, NULL, NULL);

    // Cancel button
    HWND hCancel = CreateWindowW(L"BUTTON", L"\u041e\u0442\u043c\u0435\u043d\u0430",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        180, 130, 80, 30, hDlg, (HMENU)IDC_PASSWORD_CANCEL, NULL, NULL);

    SetWindowPos(hDlg, HWND_TOPMOST, 0, 0, 420, 200, SWP_NOMOVE | SWP_SHOWWINDOW);
    ShowWindow(hDlg, SW_SHOW);

    MSG msg;
    while (IsWindow(hDlg)) {
        if (!GetMessageW(&msg, NULL, 0, 0)) break;
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!IsWindow(hDlg)) break;
    }

    out_password = g_password_result;
    DestroyWindow(hDlg);
    return !out_password.empty();
}

void dialog_message(HWND owner, const std::wstring& title, const std::wstring& message, bool is_error) {
    UINT type = MB_OK | MB_ICONINFORMATION | MB_TOPMOST;
    if (is_error) type = MB_OK | MB_ICONERROR | MB_TOPMOST;
    MessageBoxW(owner, message.c_str(), title.c_str(), type);
}
