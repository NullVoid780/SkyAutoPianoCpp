#include "crash/crash_dialog.hpp"

#include "crash/crash_context.hpp"

#include <commctrl.h>
#include <shellapi.h>

#include <cstring>
#include <string>

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

struct CrashText {
    const char *language;
    const wchar_t *heading;
    const wchar_t *body;
    const wchar_t *reason;
    const wchar_t *report;
    const wchar_t *restart;
    const wchar_t *openFolder;
    const wchar_t *copyDetails;
    const wchar_t *close;
};

constexpr CrashText kTexts[] = {
    {"en", L"Sky Auto Piano stopped working",
     L"Something went wrong and the app had to close. A crash report\nwas saved so the problem can be fixed.",
     L"Reason", L"Report", L"Restart Sky Auto Piano", L"Open crash folder", L"Copy details", L"Close"},
    {"vi", L"Sky Auto Piano đã ngừng hoạt động",
     L"Đã xảy ra lỗi và ứng dụng phải đóng lại. Một báo cáo sự cố đã được lưu để có thể khắc phục lỗi này.",
     L"Lý do", L"Báo cáo", L"Khởi động lại Sky Auto Piano", L"Mở thư mục báo cáo sự cố", L"Sao chép chi tiết",
     L"Đóng"},
    {"zh_CN", L"Sky Auto Piano 已停止运行", L"出现问题，应用程序不得不关闭。已保存一份崩溃报告，以便修复此问题。",
     L"原因", L"报告", L"重新启动 Sky Auto Piano", L"打开崩溃报告文件夹", L"复制详细信息", L"关闭"},
    {"ja", L"Sky Auto Piano は動作を停止しました",
     L"問題が発生したため、アプリを終了しました。問題を修正できるよう、クラッシュレポートを保存しました。", L"原因",
     L"レポート", L"Sky Auto Piano を再起動", L"クラッシュレポートのフォルダーを開く", L"詳細をコピー", L"閉じる"},
    {"es", L"Sky Auto Piano dejó de funcionar",
     L"Algo salió mal y la aplicación tuvo que cerrarse. Se guardó un informe de errores para poder solucionar el "
     L"problema.",
     L"Motivo", L"Informe", L"Reiniciar Sky Auto Piano", L"Abrir la carpeta de errores", L"Copiar detalles",
     L"Cerrar"},
};

const CrashText &crashText(const std::string &language)
{
    for (const CrashText &text : kTexts) {
        if (language == text.language) {
            return text;
        }
    }
    return kTexts[0];
}

}  // namespace

CrashDialogChoice showCrashDialog(const CrashDialogContent &content)
{
    const CrashText &text = crashText(content.language);
    const bool cjk = content.language == "zh_CN" || content.language == "ja";
    const std::wstring separator = cjk ? L"：" : L":  ";
    const std::wstring body = std::wstring(text.body) + L"\n\n" + text.reason + separator + content.reason + L"\n" +
                              text.report + separator + content.reportName;
    const TASKDIALOG_BUTTON buttons[] = {
        {kRestart, text.restart},
        {kOpenFolder, text.openFolder},
        {kCopyDetails, text.copyDetails},
        {IDCLOSE, text.close},
    };
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    config.pszWindowTitle = L"Sky Auto Piano";
    config.pszMainIcon = TD_ERROR_ICON;
    config.pszMainInstruction = text.heading;
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
