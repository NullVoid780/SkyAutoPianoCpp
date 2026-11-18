#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "services/playback/song_clock.hpp"

using namespace sky;

TEST_CASE("the song clock advances by elapsed time scaled by speed")
{
    double now = 0.0;
    SongClock clock(1000.0, [&] { return now; }, [](double) {});
    now = 100.0;
    clock.advance(2.0);
    CHECK(clock.songTime() == 1200.0);
    now = 100.0;
    clock.advance(2.0);
    CHECK(clock.songTime() == 1200.0);
    now = 150.0;
    clock.advance(0.5);
    CHECK(clock.songTime() == 1225.0);
}

TEST_CASE("waitUntil sleeps in slices of at most five milliseconds and yields under one")
{
    double now = 0.0;
    std::vector<double> sleeps;
    SongClock clock(0.0, [&] { return now; }, [&](double ms) {
        sleeps.push_back(ms);
        now += ms > 0 ? ms : 0.25;
    });
    REQUIRE(clock.waitUntil(12.0, [] { return 1.0; }, [] { return true; }));
    REQUIRE(sleeps.size() >= 3);
    CHECK(sleeps[0] == 5.0);
    CHECK(sleeps[1] == 5.0);
    CHECK(sleeps[2] == 2.0);
    CHECK(clock.songTime() >= 12.0);
}

TEST_CASE("waitUntil divides the remaining song time by speed")
{
    double now = 0.0;
    std::vector<double> sleeps;
    SongClock clock(0.0, [&] { return now; }, [&](double ms) {
        sleeps.push_back(ms);
        now += ms > 0 ? ms : 0.25;
    });
    REQUIRE(clock.waitUntil(6.0, [] { return 2.0; }, [] { return true; }));
    CHECK(sleeps.front() == 3.0);
}

TEST_CASE("waitUntil stops when the session is superseded")
{
    double now = 0.0;
    int calls = 0;
    SongClock clock(0.0, [&] { return now; }, [&](double ms) { now += ms; });
    CHECK_FALSE(clock.waitUntil(100.0, [] { return 1.0; }, [&] { return ++calls < 3; }));
}
