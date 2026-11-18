#pragma once

#include <string>
#include <vector>

#include <QObject>

namespace sky {

class UiStateService;

inline constexpr std::size_t kRecentPlaysLimit = 10;

class RecentService final : public QObject {
    Q_OBJECT

public:
    explicit RecentService(UiStateService &state, QObject *parent = nullptr);

    void add(const std::string &name);
    bool contains(const std::string &name) const;
    const std::vector<std::string> &names() const;

signals:
    void changed();

private:
    UiStateService &m_state;
};

}  // namespace sky
