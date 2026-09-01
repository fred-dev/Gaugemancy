#include "doctest.h"
#include "ButtonClickClassifier.h"

using Event = ButtonClickClassifier::Event;

TEST_CASE("no events fire during the startup guard window")
{
    ButtonClickClassifier c;
    CHECK(c.update(true, 0) == Event::None);
    CHECK(c.update(false, 100) == Event::None);
    CHECK(c.update(true, 5000) == Event::None); // exactly at the boundary: still guarded
}

TEST_CASE("a quick press/release, left alone past the click window, is a single click")
{
    ButtonClickClassifier c; // clickWindowMs=220, startupGuardMs=5000
    CHECK(c.update(true, 5001) == Event::None);  // press
    CHECK(c.update(false, 5010) == Event::None); // release, too soon to classify yet
    CHECK(c.update(false, 5300) == Event::SingleClick); // polled again, window elapsed
}

TEST_CASE("two quick press/release cycles inside the click window is a double click, "
          "not two single clicks")
{
    ButtonClickClassifier c;
    CHECK(c.update(true, 5001) == Event::None);
    CHECK(c.update(false, 5010) == Event::None);
    CHECK(c.update(true, 5050) == Event::None);  // second press, well inside the 220ms window
    CHECK(c.update(false, 5060) == Event::None);
    CHECK(c.update(false, 5400) == Event::DoubleClick); // polled again, window elapsed
}

TEST_CASE("three quick press/release cycles inside the click window is a triple click")
{
    ButtonClickClassifier c;
    CHECK(c.update(true, 5001) == Event::None);
    CHECK(c.update(false, 5010) == Event::None);
    CHECK(c.update(true, 5050) == Event::None);
    CHECK(c.update(false, 5060) == Event::None);
    CHECK(c.update(true, 5100) == Event::None);
    CHECK(c.update(false, 5110) == Event::None);
    CHECK(c.update(false, 5400) == Event::TripleClick);
}

TEST_CASE("holding for just over 3 seconds fires ThreeSecondHold on release")
{
    ButtonClickClassifier c;
    CHECK(c.update(true, 5001) == Event::None);
    // held down the whole time -- classify checks can't fire while still
    // pressed (waitingForClick stays false until a release is processed)
    CHECK(c.update(true, 7000) == Event::None);
    CHECK(c.update(false, 5001 + 3499) == Event::ThreeSecondHold); // ~3.5s hold
}

TEST_CASE("holding for over 6 seconds fires SixSecondHold on release, not both holds")
{
    ButtonClickClassifier c;
    CHECK(c.update(true, 5001) == Event::None);
    CHECK(c.update(false, 5001 + 6499) == Event::SixSecondHold); // ~6.5s hold
}
