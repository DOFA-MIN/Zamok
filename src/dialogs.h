#pragma once

#include <windows.h>
#include <string>

// Shows a password dialog. Returns true if user entered password and clicked OK.
// out_password receives the entered password.
bool dialog_password(HWND owner, const std::wstring& title, const std::wstring& prompt, std::wstring& out_password);

// Shows a simple message box.
void dialog_message(HWND owner, const std::wstring& title, const std::wstring& message, bool is_error);
