#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "qt_test_support.hpp"
#include "services/settings/config_service.hpp"
#include "services/playback/playback_service.hpp"

using namespace sky;

namespace {

struct Harness {
    explicit Harness(const char *mode, bool longPress = false) : config(dir.path())
    {
        config.load();
        OrderedJson panel = OrderedJson::object();
        panel["playbackMode"] = mode;
        panel["longPressMode"] = longPress;
        config.updatePanel(panel);
        auto owned = std::make_unique<RecordingSender>();
        sender = owned.get();
        playback = std::make_unique<PlaybackService>(config, std::move(owned));
        QObject::connect(playback.get(), &PlaybackService::stopped, [this](bool manual) {
            stops.push_back(manual);
        });
        QObject::connect(playback.get(), &PlaybackService::progress, [this](int second) { progress.push_back(second); });
    }

    bool appReady = (ensureCoreApp(), true);
    TempDir dir;
    ConfigService config;
    RecordingSender *sender = nullptr;
    std::unique_ptr<PlaybackService> playback;
    std::vector<bool> stops;
    std::vector<int> progress;
};

PlayRequest shortSong()
{
    PlayRequest request;
    request.keyMap = KeyMap({{0.0, {"y", "u"}}, {100.0, {"i"}}, {1000.0, {}}});
    return request;
}

}  // namespace

TEST_CASE("strict playback presses every key, releases them all and ends naturally")
{
    Harness h("strict");
    h.playback->play(shortSong());
    REQUIRE(waitUntil([&] { return !h.stops.empty(); }, 5000));
    CHECK(h.stops == std::vector<bool>{false});
    REQUIRE_FALSE(h.progress.empty());
    CHECK(h.progress.front() == 0);

    const auto events = h.sender->events();
    REQUIRE(events.size() == 6);
    CHECK(events[0].virtualKey == 'Y');
    CHECK(events[0].down);
    CHECK(events[1].virtualKey == 'U');
    CHECK(events[1].atMs - events[0].atMs >= 2.0);
    CHECK(events[1].atMs - events[0].atMs < 15.0);
    const auto iPress = std::find_if(events.begin(), events.end(), [](const RecordedKey &e) { return e.virtualKey == 'I' && e.down; });
    REQUIRE(iPress != events.end());
    CHECK(iPress->atMs - events[0].atMs >= 95.0);
    CHECK(h.sender->heldCount() == 0);
}

TEST_CASE("lite playback releases through the scheduler and ends naturally")
{
    Harness h("lite");
    h.playback->play(shortSong());
    REQUIRE(waitUntil([&] { return !h.stops.empty(); }, 5000));
    CHECK(h.stops == std::vector<bool>{false});
    REQUIRE(waitUntil([&] { return h.sender->heldCount() == 0; }, 2000));
    const auto events = h.sender->events();
    REQUIRE(events.size() == 6);
    CHECK(events[0].down);
    CHECK(events[1].down);
    CHECK(events[1].atMs - events[0].atMs < 2.0);
}

TEST_CASE("a manual stop releases held keys and reports manual")
{
    Harness h("strict", true);
    PlayRequest request;
    request.keyMap = KeyMap({{0.0, {"y"}}, {5000.0, {"u"}}, {6000.0, {}}});
    h.playback->play(request);
    REQUIRE(waitUntil([&] { return h.sender->heldCount() == 1; }, 2000));
    h.playback->stop();
    CHECK(h.sender->heldCount() == 0);
    REQUIRE(waitUntil([&] { return !h.stops.empty(); }, 1000));
    CHECK(h.stops == std::vector<bool>{true});
    CHECK_FALSE(h.playback->isPlaying());
}

TEST_CASE("a new play request supersedes the previous one without a stop event")
{
    Harness h("lite", true);
    PlayRequest longSong;
    longSong.keyMap = KeyMap({{0.0, {"y"}}, {5000.0, {"u"}}, {6000.0, {}}});
    h.playback->play(longSong);
    REQUIRE(waitUntil([&] { return h.sender->heldCount() == 1; }, 2000));
    h.playback->play(shortSong());
    REQUIRE(waitUntil([&] { return !h.stops.empty(); }, 5000));
    CHECK(h.stops == std::vector<bool>{false});
    REQUIRE(waitUntil([&] { return h.sender->heldCount() == 0; }, 2000));
}

TEST_CASE("an empty request stops immediately as a natural end")
{
    Harness h("strict");
    h.playback->play(PlayRequest{});
    REQUIRE(waitUntil([&] { return !h.stops.empty(); }, 1000));
    CHECK(h.stops == std::vector<bool>{false});
}

TEST_CASE("custom keyboard mappings resolve through the legacy alias table")
{
    Harness h("strict");
    OrderedJson keyboard = OrderedJson::object();
    keyboard["customKeyboard"] = true;
    keyboard["keys"] = OrderedJson::array({"esc", "u", "i", "o", "p", "h", "j", "k", "l", ";", "n", "m", ",", ".", "/"});
    h.config.updateKeyboard(keyboard);
    PlayRequest request;
    request.keyMap = KeyMap({{0.0, {"y"}}, {1000.0, {}}});
    h.playback->play(request);
    REQUIRE(waitUntil([&] { return !h.stops.empty(); }, 5000));
    const auto events = h.sender->events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].virtualKey == 0x1B);
}
