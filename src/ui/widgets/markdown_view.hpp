#pragma once

#include <functional>

#include <QColor>
#include <QTextBrowser>

namespace sky {

class MarkdownView final : public QTextBrowser {
    Q_OBJECT

public:
    struct Style {
        int fontPixels = 14;
        std::function<QColor()> text;
        std::function<QColor()> link;
        std::function<QColor()> codeFill;
        std::function<QColor()> heading;
    };

    MarkdownView(Style style, QWidget *parent = nullptr);

    void setMarkdownText(const QString &markdown);

protected:
    void changeEvent(QEvent *event) override;

private:
    void restyle();

    Style m_style;
    QString m_markdown;
};

}  // namespace sky
