#include "crash/crash_dialog.hpp"

#include "crash/crash_context.hpp"

#include <commctrl.h>
#include <shellapi.h>

#include <cstring>

namespace sky {

namespace {

constexpr int kRestart = 100;
constexpr int kOpenFolder = 101;
constexpr int kCopyDetails = 102;

void copyToClipboard(const std::wstring &text)
{
    if (!OpenClipboard(nullptr)) {
        return;
    }
    EmptyClipboard();
    const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        if (void *target = GlobalLock(memory)) {
            std::memcpy(target, text.c_str(), bytes);
            GlobalUnlock(memory);
            if (!SetClipboardData(CF_UNICODETEXT, memory)) {
                GlobalFree(memory);
            }
        } else {
            GlobalFree(memory);
        }
    }
    CloseClipboard();
}

HRESULT CALLBACK onDialog(HWND, UINT notification, WPARAM wParam, LPARAM, LONG_PTR data)
{
    if (notification != TDN_BUTTON_CLICKED) {
        return S_OK;
    }
    const auto *content = reinterpret_cast<const CrashDialogContent *>(data);
    if (wParam == kOpenFolder) {
        ShellExecuteW(nullptr, L"open", content->folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return S_FALSE;
    }
    if (wParam == kCopyDetails) {
        copyToClipboard(content->details);
        return S_FALSE;
    }
    return S_OK;
}

}  // namespace

CrashDialogChoice showCrashDialog(const CrashDialogContent &content)
{
    const std::wstring body = L"Something went wrong and the app had to close. A crash report\n"
                              L"was saved so the problem can be fixed.\n\n"
                              L"Reason:  " + content.reason + L"\nReport:  " + content.reportName;
    const TASKDIALOG_BUTTON buttons[] = {
        {kRestart, L"Restart Sky Auto Piano"},
        {kOpenFolder, L"Open crash folder"},
        {kCopyDetails, L"Copy details"},
        {IDCLOSE, L"Close"},
    };
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    config.pszWindowTitle = L"Sky Auto Piano";
    config.pszMainIcon = TD_ERROR_ICON;
    config.pszMainInstruction = L"Sky Auto Piano stopped working";
    config.pszContent = body.c_str();
    config.cButtons = static_cast<UINT>(std::size(buttons));
    config.pButtons = buttons;
    config.nDefaultButton = kRestart;
    config.pfCallback = onDialog;
    config.lpCallbackData = reinterpret_cast<LONG_PTR>(&content);
    int pressed = IDCLOSE;
    if (FAILED(TaskDialogIndirect(&config, &pressed, nullptr, nullptr))) {
        return CrashDialogChoice::Close;
    }
    return pressed == kRestart ? CrashDialogChoice::Restart : CrashDialogChoice::Close;
}

}  // namespace sky
