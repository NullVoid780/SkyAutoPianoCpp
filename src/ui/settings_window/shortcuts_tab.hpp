#pragma once

#include <array>

#include <QWidget>

#include "model/config.hpp"

namespace sky {

class ShortcutEdit;

class ShortcutsTab final : public QWidget {
    Q_OBJECT

public:
    explicit ShortcutsTab(QWidget *parent = nullptr);

    void load(const Config &config);
    void collect(Config &config) const;

private:
    std::array<ShortcutEdit *, 5> m_fields{};
};

}  // namespace sky
