#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <QPointer>
#include <QWidget>

#include "model/toast.hpp"
#include "ui/theme_catalog.hpp"

class QBoxLayout;
class QGridLayout;
class QLabel;
class QScrollArea;
class QLineEdit;
class QTimer;

namespace sky {

class ColorPicker;
class ColorRow;
class ConfirmPopup;
class SectionToggle;
class SelectBox;
class StickyPreview;
class ThemeController;
class ThemePreview;

class ThemeBuilder final : public QWidget {
    Q_OBJECT

public:
    explicit ThemeBuilder(ThemeController &themes, QWidget *parent = nullptr);

    void edit(const ThemeDefinition &draft, bool isNew, AppTheme mode);
    void requestClose();
    void discard();
    bool isDirty() const;

signals:
    void finished(const QString &savedId);
    void toastRequested(const sky::Toast &toast);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Focus {
        PreviewScene scene = PreviewScene::Main;
        QStringList ids;
        QString label;
    };

    struct RowBinding {
        ColorRow *row;
        std::function<QColor(const Theme &)> read;
        std::function<bool(const ThemeColors &, const ThemeColors &)> changed;
    };

    struct SectionBinding {
        SectionToggle *toggle;
        const ThemeSection *section;
        QGridLayout *grid;
        std::vector<ColorRow *> rows;
    };

    void addQuickRow(const QuickColor &quick, PreviewScene scene);
    void showQuickColors(PreviewScene scene);
    QWidget *addSection(const ThemeSection &section);
    void refresh();
    void schedulePreview();
    void attachScroll();
    void placeSticky();
    void layoutGrids(int columns);
    void updateLayoutMode();
    void watchRow(ColorRow *row, Focus focus);
    void pinFocus(Focus focus);
    void applyFocus();
    void pickColor(const QString &title, const QColor &initial, std::function<void(ThemeColors &, const QColor &)> apply);
    void changeColors(const std::function<void(ThemeColors &)> &change);
    void resetAll();
    void save();
    ColorPicker *picker();
    ConfirmPopup *confirm();

    ThemeController &m_themes;
    ThemeDefinition m_draft;
    ThemeDefinition m_initial;
    Theme m_composed;
    AppTheme m_mode = AppTheme::Light;
    QLabel *m_title;
    QLineEdit *m_name;
    SelectBox *m_modeSelect;
    StickyPreview *m_sticky;
    ThemePreview *m_preview;
    QWidget *m_previewSpace;
    QPointer<QScrollArea> m_scroll;
    bool m_compact = false;
    Focus m_pinned;
    std::optional<Focus> m_hovered;
    int m_sideWidth = 0;
    int m_columns = 0;
    std::vector<RowBinding> m_rows;
    std::vector<RowBinding> m_quickRows;
    QLabel *m_quickTitle = nullptr;
    QLabel *m_quickCaption = nullptr;
    QWidget *m_quickHost = nullptr;
    QBoxLayout *m_quickLayout = nullptr;
    std::optional<PreviewScene> m_quickScene;
    std::vector<SectionBinding> m_sections;
    QTimer *m_previewTimer;
    QPointer<ColorPicker> m_picker;
    QPointer<ConfirmPopup> m_confirm;
};

}  // namespace sky
