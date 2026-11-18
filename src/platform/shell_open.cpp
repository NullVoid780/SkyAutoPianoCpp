#include "platform/shell_open.hpp"

#include <QDesktopServices>
#include <QUrl>

namespace sky {

bool openExternalUrl(const QUrl &url)
{
    if (!url.isValid() || (url.scheme() != QLatin1String("https") && url.scheme() != QLatin1String("http"))) {
        return false;
    }
    return QDesktopServices::openUrl(url);
}

bool openLocalFolder(const std::filesystem::path &folder)
{
    std::error_code error;
    if (!std::filesystem::is_directory(folder, error)) {
        return false;
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdWString(folder.wstring())));
}

}  // namespace sky
