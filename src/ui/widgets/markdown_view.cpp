#include "ui/widgets/markdown_view.hpp"

#include <QDesktopServices>
#include <QEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <vector>

#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kListIndent = 18;
constexpr int kHeadingGap = 14;
constexpr int kHeadingAfter = 6;

struct Span {
    int start = 0;
    int length = 0;
    QTextCharFormat format;
};

}  // namespace

MarkdownView::MarkdownView(Style style, QWidget *parent) : QTextBrowser(parent), m_style(std::move(style))
{
    setObjectName(QStringLiteral("MarkdownView"));
    setProperty("markdown", true);
    setFrameShape(QFrame::NoFrame);
    setOpenLinks(false);
    setReadOnly(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    viewport()->setAutoFillBackground(false);
    connect(this, &QTextBrowser::anchorClicked, this, [](const QUrl &url) {
        if (url.scheme() == QLatin1String("https") || url.scheme() == QLatin1String("http")) {
            QDesktopServices::openUrl(url);
        }
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { restyle(); });
    restyle();
}

void MarkdownView::setMarkdownText(const QString &markdown)
{
    m_markdown = markdown;
    restyle();
}

void MarkdownView::changeEvent(QEvent *event)
{
    QTextBrowser::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        restyle();
    }
}

void MarkdownView::restyle()
{
    QFont font = this->font();
    font.setPointSizeF(m_style.fontPixels * 72.0 / logicalDpiY());
    font.setWeight(QFont::Normal);
    document()->setDefaultFont(font);
    document()->setIndentWidth(kListIndent);
    setMarkdown(m_markdown);

    const QColor heading = m_style.heading ? m_style.heading() : m_style.text();
    QTextCursor all(document());
    all.select(QTextCursor::Document);
    QTextCharFormat base;
    base.setForeground(m_style.text());
    all.mergeCharFormat(base);

    std::vector<Span> spans;
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        const int level = block.blockFormat().headingLevel();
        if (level > 0) {
            QTextBlockFormat spacing;
            spacing.setTopMargin(block == document()->begin() ? 0 : kHeadingGap);
            spacing.setBottomMargin(kHeadingAfter);
            QTextCursor(block).mergeBlockFormat(spacing);
        }
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            QTextCharFormat format;
            if (level > 0) {
                format.setFontWeight(QFont::DemiBold);
                format.setForeground(heading);
            }
            if (fragment.charFormat().isAnchor()) {
                format.setForeground(m_style.link());
            }
            if (fragment.charFormat().fontFixedPitch()) {
                format.setBackground(m_style.codeFill());
            }
            if (!format.properties().isEmpty()) {
                spans.push_back(Span{fragment.position(), fragment.length(), format});
            }
        }
    }
    for (const Span &span : spans) {
        QTextCursor cursor(document());
        cursor.setPosition(span.start);
        cursor.setPosition(span.start + span.length, QTextCursor::KeepAnchor);
        cursor.mergeCharFormat(span.format);
    }
}

}  // namespace sky
