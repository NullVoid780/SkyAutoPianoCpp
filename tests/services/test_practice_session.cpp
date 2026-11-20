#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "services/practice/practice_session.hpp"

using namespace sky;

namespace {

KeyMap song()
{
    return KeyMap({
        KeyMapEntry{0, {}},
        KeyMapEntry{1000, {"y", "u"}},
        KeyMapEntry{2000, {"h"}},
        KeyMapEntry{3000, {"/", "/", "bad"}},
    });
}

}  // namespace

TEST_CASE("a practice session groups song keys into chords and skips unknown or repeated keys")
{
    const PracticeSession session(song());
    REQUIRE(session.chords().size() == 3);
    CHECK(session.notes().size() == 4);
    CHECK(session.chords()[0].notes.size() == 2);
    CHECK(session.notes()[2].key == 5);
    CHECK(session.notes()[3].key == 14);
    CHECK(session.time() == 1000 - PracticeSession::kIntroMs);
}

TEST_CASE("presses are judged by how close they are to the note")
{
    PracticeSession session(song());
    session.advance(PracticeSession::kIntroMs - 40, 1.0);
    CHECK(session.press(0, 1.0).judgement == PracticeJudgement::Perfect);
    session.advance(140, 1.0);
    CHECK(session.press(1, 1.0).judgement == PracticeJudgement::Good);
    CHECK(session.press(1, 1.0).judgement == PracticeJudgement::Wrong);
    CHECK(session.stats().perfect == 1);
    CHECK(session.stats().good == 1);
    CHECK(session.stats().wrong == 1);
    CHECK(session.stats().combo == 2);
}

TEST_CASE("notes that pass the timing window count as misses and break the combo")
{
    PracticeSession session(song());
    session.advance(PracticeSession::kIntroMs, 1.0);
    session.press(0, 1.0);
    const auto missed = session.advance(1200, 1.0);
    CHECK(missed.size() == 2);
    CHECK(session.stats().miss == 2);
    CHECK(session.stats().combo == 0);
    CHECK(session.stats().bestCombo == 1);
}

TEST_CASE("timing windows are measured in real time at any practice speed")
{
    PracticeSession session(song());
    session.reset(0.5);
    session.advance(PracticeSession::kIntroMs + 60, 0.5);
    CHECK(session.time() == 1030);
    CHECK(session.press(0, 0.5).judgement == PracticeJudgement::Perfect);
    session.advance(60, 0.5);
    CHECK(session.time() == 1060);
    CHECK(session.press(1, 0.5).judgement == PracticeJudgement::Good);
}

TEST_CASE("wait mode holds the song on each chord until every key is pressed")
{
    PracticeSession session(song());
    session.setWaitMode(true);
    CHECK(session.advance(5000, 1.0).empty());
    CHECK(session.waiting());
    CHECK(session.time() == 1000);
    CHECK(session.press(5, 1.0).judgement == PracticeJudgement::Wrong);
    CHECK(session.press(0, 1.0).judgement == PracticeJudgement::Perfect);
    CHECK(session.waiting());
    CHECK(session.press(1, 1.0).judgement == PracticeJudgement::Perfect);
    CHECK_FALSE(session.waiting());
    session.advance(5000, 1.0);
    CHECK(session.time() == 2000);
    CHECK(session.stats().miss == 0);
}

TEST_CASE("the session finishes after the last chord and grades the run")
{
    PracticeSession session(song());
    session.advance(PracticeSession::kIntroMs, 1.0);
    session.press(0, 1.0);
    session.press(1, 1.0);
    session.advance(1000, 1.0);
    session.press(5, 1.0);
    session.advance(1000, 1.0);
    session.press(14, 1.0);
    CHECK_FALSE(session.finished());
    session.advance(PracticeSession::kOutroMs + 10, 1.0);
    CHECK(session.finished());
    CHECK_THAT(session.stats().accuracy(), Catch::Matchers::WithinAbs(100.0, 0.001));
    CHECK(session.stats().grade() == "S");
}
