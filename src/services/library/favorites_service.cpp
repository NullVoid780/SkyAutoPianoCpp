#include "services/library/favorites_service.hpp"

#include <algorithm>

#include "services/settings/ui_state_service.hpp"

namespace sky {

FavoritesService::FavoritesService(UiStateService &state, QObject *parent) : QObject(parent), m_state(state)
{
    connect(&m_state, &UiStateService::favoritesChanged, this, &FavoritesService::changed);
}

bool FavoritesService::isFavorite(const std::string &name) const
{
    const auto &favorites = m_state.favorites();
    return std::find(favorites.begin(), favorites.end(), name) != favorites.end();
}

void FavoritesService::toggle(const std::string &name)
{
    std::vector<std::string> favorites = m_state.favorites();
    const auto it = std::find(favorites.begin(), favorites.end(), name);
    if (it != favorites.end()) {
        favorites.erase(it);
    } else {
        favorites.push_back(name);
    }
    m_state.setFavorites(std::move(favorites));
}

const std::vector<std::string> &FavoritesService::names() const
{
    return m_state.favorites();
}

}  // namespace sky
