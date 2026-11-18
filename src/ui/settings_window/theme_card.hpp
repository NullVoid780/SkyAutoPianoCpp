#pragma once

#include <vector>

#include <QAbstractButton>
#include <QPixmap>

#include "ui/theme.hpp"
#include "ui/theme_catalog.hpp"

namespace sky {

class HoverTracker;
class IconButton;

class ThemeCard final : public QAbstractButton {
    Q_OBJECT

public:
    enum class Action { Edit, Duplicate, Export, Delete };

    explicit ThemeCard(const ThemeDefinition &definition, QWidget *parent = nullptr);

    const QString &themeId() const { return m_id; }
    void setActive(bool active);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    static constexpr int kHeight = 220;
    static constexpr int kMinWidth = 230;

signals:
    void actionRequested(const QString &id, sky::ThemeCard::Action action);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void addAction(const QString &iconId, const QString &tip, Action action);

    QString m_id;
    QString m_name;
    QString m_subtitle;
    Theme m_light;
    Theme m_dark;
    QPixmap m_previewCache;
    bool m_active = false;
    bool m_keyboardFocus = false;
    HoverTracker *m_hover;
    std::vector<IconButton *> m_actions;
};

}  // namespace sky
