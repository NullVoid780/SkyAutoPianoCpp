#include "services/library/recent_service.hpp"

#include <algorithm>

#include "services/settings/ui_state_service.hpp"

namespace sky {

RecentService::RecentService(UiStateService &state, QObject *parent) : QObject(parent), m_state(state)
{
    connect(&m_state, &UiStateService::recentPlaysChanged, this, &RecentService::changed);
}

void RecentService::add(const std::string &name)
{
    std::vector<std::string> recent;
    recent.push_back(name);
    for (const std::string &existing : m_state.recentPlays()) {
        if (existing != name && recent.size() < kRecentPlaysLimit) {
            recent.push_back(existing);
        }
    }
    m_state.setRecentPlays(std::move(recent));
}

bool RecentService::contains(const std::string &name) const
{
    const auto &recent = m_state.recentPlays();
    return std::find(recent.begin(), recent.end(), name) != recent.end();
}

const std::vector<std::string> &RecentService::names() const
{
    return m_state.recentPlays();
}

}  // namespace sky
