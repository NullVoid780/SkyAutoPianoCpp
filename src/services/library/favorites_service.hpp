#pragma once

#include <string>
#include <vector>

#include <QObject>

namespace sky {

class UiStateService;

class FavoritesService final : public QObject {
    Q_OBJECT

public:
    explicit FavoritesService(UiStateService &state, QObject *parent = nullptr);

    bool isFavorite(const std::string &name) const;
    void toggle(const std::string &name);
    const std::vector<std::string> &names() const;

signals:
    void changed();

private:
    UiStateService &m_state;
};

}  // namespace sky
