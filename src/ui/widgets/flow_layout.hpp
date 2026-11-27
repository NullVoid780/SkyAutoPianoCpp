#pragma once

#include <vector>

#include <QLayout>

namespace sky {

class FlowLayout final : public QLayout {
public:
    FlowLayout(QWidget *parent, int spacing, int lineSpacing);
    ~FlowLayout() override;

    void addWidget(QWidget *widget, int gapBefore);
    void addItem(QLayoutItem *item) override;
    int count() const override { return static_cast<int>(m_entries.size()); }
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;

    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;
    QSize minimumSize() const override;
    QSize sizeHint() const override;
    void setGeometry(const QRect &rect) override;

private:
    struct Entry {
        QLayoutItem *item = nullptr;
        int gapBefore = 0;
    };

    int arrange(const QRect &rect, bool apply) const;

    std::vector<Entry> m_entries;
    int m_spacing;
    int m_lineSpacing;
};

}  // namespace sky
