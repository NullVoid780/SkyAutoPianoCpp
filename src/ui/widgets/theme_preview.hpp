#pragma once

#include <optional>

#include <QPixmap>
#include <QStringList>
#include <QWidget>

#include "ui/theme.hpp"
#include "ui/theme_catalog.hpp"

class QPainter;

namespace sky {

void paintThemePreview(QPainter &painter, const QRectF &rect, const Theme &theme, qreal radius,
                       PreviewScene scene = PreviewScene::Main);
int paintThemeSplitPreview(QPainter &painter, const QRectF &rect, const Theme &light, const Theme &dark, qreal radius,
                           bool withText, std::optional<AppTheme> highlight = std::nullopt,
                           PreviewScene scene = PreviewScene::Main, const QStringList &focus = {},
                           bool badges = true);

QStringList previewSlotIds(PreviewScene scene);

class ThemePreview final : public QWidget {
    Q_OBJECT

public:
    explicit ThemePreview(QWidget *parent = nullptr);

    void setThemes(const Theme &light, const Theme &dark, std::optional<AppTheme> highlight);
    void showFocus(PreviewScene scene, const QStringList &slotIds);
    void setModeBadges(bool visible);
    PreviewScene scene() const { return m_scene; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Theme m_light;
    Theme m_dark;
    std::optional<AppTheme> m_highlight;
    PreviewScene m_scene = PreviewScene::Main;
    QStringList m_focus;
    bool m_badges = true;
    QPixmap m_cache;
};

}  // namespace sky
