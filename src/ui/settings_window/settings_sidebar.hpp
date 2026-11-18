#pragma once

#include <vector>

#include <QWidget>

namespace sky {

class SidebarItem;

class SettingsSidebar final : public QWidget {
    Q_OBJECT

public:
    static constexpr int kWidth = 250;

    explicit SettingsSidebar(const QString &version, QWidget *parent = nullptr);

    int addItem(const QString &iconId, const QString &label);
    void addSeparator();
    void setCurrent(int index);
    void setBadge(int index, bool visible);
    int current() const { return m_current; }

signals:
    void currentChanged(int index);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QRectF versionCard() const;

    QString m_version;
    std::vector<SidebarItem *> m_items;
    int m_current = -1;
};

}  // namespace sky
