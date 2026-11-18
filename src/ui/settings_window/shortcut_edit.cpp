#include "ui/settings_window/shortcut_edit.hpp"

#include <QContextMenuEvent>
#include <QKeyEvent>

#include "ui/settings_window/key_capture.hpp"

namespace sky {

namespace {

constexpr int kMinWidth = 120;
constexpr int kMinHeight = 36;

}  // namespace

ShortcutEdit::ShortcutEdit(QWidget *parent) : QLineEdit(parent)
{
    setObjectName(QStringLiteral("ShortcutField"));
    setReadOnly(true);
    setAlignment(Qt::AlignCenter);
    setMinimumSize(kMinWidth, kMinHeight);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
    setContextMenuPolicy(Qt::NoContextMenu);
}

void ShortcutEdit::setCaptureEnabled(bool enabled)
{
    m_captureEnabled = enabled;
    setFocusPolicy(enabled ? Qt::StrongFocus : Qt::NoFocus);
    setCursor(enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void ShortcutEdit::keyPressEvent(QKeyEvent *event)
{
    if (!m_captureEnabled) {
        return;
    }
    event->accept();
    if (event->isAutoRepeat()) {
        return;
    }
    const quint32 virtualKey = event->nativeVirtualKey();
    const QString token = shortcutTokenForVirtualKey(virtualKey);
    if (token.isEmpty()) {
        return;
    }
    if (m_held.empty()) {
        m_previous = text();
        m_tokens.clear();
    }
    m_held.insert(virtualKey);
    if (!m_tokens.contains(token)) {
        m_tokens.append(token);
    }
    setText(m_tokens.join(QLatin1Char('+')));
}

void ShortcutEdit::keyReleaseEvent(QKeyEvent *event)
{
    if (!m_captureEnabled) {
        return;
    }
    event->accept();
    if (event->isAutoRepeat() || m_held.empty()) {
        return;
    }
    m_held.erase(event->nativeVirtualKey());
    if (m_held.empty()) {
        finish();
    } else {
        setText(formatShortcut(m_tokens));
    }
}

void ShortcutEdit::focusOutEvent(QFocusEvent *event)
{
    if (!m_held.empty()) {
        m_held.clear();
        finish();
    }
    QLineEdit::focusOutEvent(event);
}

void ShortcutEdit::contextMenuEvent(QContextMenuEvent *event)
{
    event->accept();
}

void ShortcutEdit::finish()
{
    if (!hasMainKey(m_tokens)) {
        setText(m_previous);
        return;
    }
    const QString formatted = formatShortcut(m_tokens);
    setText(formatted);
    if (formatted != m_previous) {
        emit edited();
    }
}

}  // namespace sky
