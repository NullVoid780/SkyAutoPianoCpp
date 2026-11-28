#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <vector>

#include "qt_test_support.hpp"
#include "services/library/library_service.hpp"
#include "services/library/playlist_service.hpp"
#include "services/library/recent_service.hpp"
#include "services/notifications/toast_service.hpp"
#include "services/playback/playback_port.hpp"
#include "services/settings/config_service.hpp"
#include "services/settings/ui_state_service.hpp"
#include "ui/controllers/player_controller.hpp"

using namespace sky;

namespace {

class FakePlaybackPort final : public IPlaybackPort {
public:
    std::vector<PlayRequest> requests;
    int stopCalls = 0;
    bool playing = false;

    void play(const PlayRequest &request) override {
        requests.push_back(request);
        playing = true;
        emit started();
    }

    void stop() override {
        ++stopCalls;
        playing = false;
        emit stopped(true);
    }

    bool isPlaying() const override { return playing; }

    void finishNaturally() {
        playing = false;
        emit stopped(false);
    }

    void advanceTo(int second) { emit progress(second); }
};

void writeFile(const std::filesystem::path &file, const std::string &text) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << text;
}

struct PlayerFixture {
    bool appReady = (ensureCoreApp(), true);
    TempDir dir;
    ConfigService config{dir.path() / "config"};
    UiStateService uiState{dir.path() / "data"};
    RecentService recent{uiState};
    PlaylistService playlists{dir.path() / "data"};
    LibraryService library{dir.path() / "data", playlists};
    ToastService toasts;
    FakePlaybackPort playback;
    PlayerController controller{library, playback, recent, config, toasts};

    PlayerFixture() {
        config.load();
        uiState.load();
        playlists.load();
        library.load();

        OrderedJson panel = OrderedJson::object();
        panel["delayNext"] = 0;
        config.updatePanel(panel);

        const auto a = dir.path() / "in" / "SongA.json";
        const auto b = dir.path() / "in" / "SongB.json";
        writeFile(
            a,
            R"([{"name":"Song A","bpm":60,"songNotes":[{"time":0,"key":"Key0"},{"time":3000,"key":"Key1"}]}])");
        writeFile(
            b,
            R"([{"name":"Song B","bpm":60,"songNotes":[{"time":0,"key":"Key2"},{"time":2000,"key":"Key3"}]}])");

        bool done = false;
        QObject::connect(&library, &LibraryService::importFinished,
                         [&](const ImportSummary &) { done = true; });
        library.importFiles({a, b});
        REQUIRE(waitUntil([&] { return done; }));
        REQUIRE(library.size() == 2);

        controller.setVisibleIndicesProvider([this] {
            std::vector<int> indices;
            for (int i = 0; i < library.size(); ++i) {
                indices.push_back(i);
            }
            return indices;
        });
    }
};

}  // namespace

TEST_CASE("PlayerController selects local sheets, tracks recents, seeks, and toggles playback") {
    PlayerFixture f;
    bool loaded = false;
    f.controller.selectLocal(0, true, [&] { loaded = true; });
    REQUIRE(waitUntil([&] { return loaded; }));
    CHECK(f.controller.target().kind == PlaybackTarget::Kind::Local);
    CHECK(f.controller.target().name == "Song A");
    CHECK(f.controller.target().totalSeconds == 4);
    CHECK(f.recent.contains("Song A"));

    f.controller.seek(2);
    CHECK(f.controller.position() == 2);
    f.controller.seek(99);
    CHECK(f.controller.position() == 4);

    f.controller.togglePlay();
    CHECK(f.controller.isPlaying());
    REQUIRE(f.playback.requests.size() == 1);
    CHECK(f.playback.requests[0].startSec == 4);

    f.playback.advanceTo(1);
    CHECK(f.controller.position() == 1);

    f.controller.togglePlay();
    CHECK_FALSE(f.controller.isPlaying());
    CHECK(f.playback.stopCalls == 1);
}

TEST_CASE("PlayerController cycles loop modes and advances on natural completion") {
    PlayerFixture f;
    bool loaded = false;
    f.controller.selectLocal(0, false, [&] { loaded = true; });
    REQUIRE(waitUntil([&] { return loaded; }));

    CHECK(f.controller.loopMode() == LoopMode::Off);
    f.controller.cycleLoopMode();
    CHECK(f.controller.loopMode() == LoopMode::All);
    f.controller.cycleLoopMode();
    CHECK(f.controller.loopMode() == LoopMode::One);
    f.controller.cycleLoopMode();
    CHECK(f.controller.loopMode() == LoopMode::Off);

    // LoopMode::One replays current song after delay.
    f.controller.cycleLoopMode();
    f.controller.cycleLoopMode();
    REQUIRE(f.controller.loopMode() == LoopMode::One);
    f.controller.togglePlay();
    REQUIRE(f.playback.requests.size() == 1);
    f.playback.finishNaturally();
    CHECK_FALSE(f.controller.isPlaying());
    REQUIRE(waitUntil([&] { return f.playback.requests.size() == 2; }, 2000));
    CHECK(f.controller.target().name == "Song A");

    // Stop cleanly before LoopMode::All test.
    f.controller.togglePlay();
    CHECK_FALSE(f.controller.isPlaying());

    // LoopMode::All advances to Song B.
    f.controller.cycleLoopMode();
    f.controller.cycleLoopMode();
    REQUIRE(f.controller.loopMode() == LoopMode::All);
    f.controller.togglePlay();
    f.playback.finishNaturally();
    REQUIRE(waitUntil(
        [&] { return f.controller.target().name == "Song B" && f.playback.requests.size() == 3; },
        2000));
    f.controller.togglePlay();
}

TEST_CASE("PlayerController supersedes stale async selection and reacts to library deletion") {
    PlayerFixture f;
    bool firstRan = false;
    bool secondRan = false;
    f.controller.selectLocal(0, false, [&] { firstRan = true; });
    f.controller.selectLocal(1, false, [&] { secondRan = true; });
    REQUIRE(waitUntil([&] { return secondRan; }));
    CHECK_FALSE(firstRan);
    CHECK(f.controller.target().name == "Song B");

    // Deleting active sheet clears or selects remaining neighbor cleanly.
    REQUIRE(f.library.deleteSheet(1).ok());
    REQUIRE(waitUntil([&] { return f.controller.target().name == "Song A"; }));
    REQUIRE(f.library.deleteSheet(0).ok());
    CHECK(f.controller.target().kind == PlaybackTarget::Kind::None);
}
