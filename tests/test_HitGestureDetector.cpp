#include "doctest.h"
#include "HitGestureDetector.h"

using Values = std::array<float, HitGestureDetector::kNumSensors>;

static const Values kSilence = {0, 0, 0, 0, 0, 0};

TEST_CASE("idle stays idle with no sensor activity")
{
    HitGestureDetector detector{HitGestureDetector::Config{}};
    CHECK_FALSE(detector.update(kSilence, 0));
    CHECK_FALSE(detector.isCheckingPeaks());
    CHECK_FALSE(detector.isCheckingTroughs());
    CHECK(detector.completedFullHitsCount() == 0);
}

TEST_CASE("a single sensor above threshold is not enough to start a gesture")
{
    HitGestureDetector detector{HitGestureDetector::Config{}};
    Values oneHigh = {0.5f, 0, 0, 0, 0, 0};
    CHECK_FALSE(detector.update(oneHigh, 0));
    CHECK_FALSE(detector.isCheckingPeaks());
    CHECK_FALSE(detector.isCheckingTroughs());
}

TEST_CASE("two simultaneous sensors above threshold begin checking for a trough")
{
    HitGestureDetector detector{HitGestureDetector::Config{}};
    Values twoHigh = {0.5f, 0.5f, 0, 0, 0, 0};
    CHECK_FALSE(detector.update(twoHigh, 0));
    CHECK(detector.isCheckingTroughs());
    CHECK_FALSE(detector.isCheckingPeaks());
}

TEST_CASE("a single-required-hit gesture completes and returns to idle when "
          "the trough lands inside the window")
{
    // requiredHits=1: one full peak/trough cycle is the whole gesture.
    // The completion fires mid-scan (right after sensor index 1 dips), which
    // is quirk #1 -- sensors 2-5 are never engaged here and that's fine.
    HitGestureDetector::Config config;
    config.requiredHits = 1;
    HitGestureDetector detector{config};

    CHECK_FALSE(detector.update(Values{0.5f, 0.5f, 0, 0, 0, 0}, 0));   // peak batch
    CHECK(detector.isCheckingTroughs());

    bool completed = detector.update(Values{0.02f, 0.02f, 0, 0, 0, 0}, 10); // trough, inside the 250ms window
    CHECK(completed);

    // quirk #2: completing inside the window resets fully back to idle.
    CHECK_FALSE(detector.isCheckingPeaks());
    CHECK_FALSE(detector.isCheckingTroughs());
    CHECK(detector.completedFullHitsCount() == 0);
}

TEST_CASE("a two-hit gesture requires two full peak/trough cycles before firing")
{
    HitGestureDetector::Config config;
    config.requiredHits = 2;
    HitGestureDetector detector{config};

    CHECK_FALSE(detector.update(Values{0.5f, 0.5f, 0, 0, 0, 0}, 0));    // cycle 1: peak
    CHECK_FALSE(detector.update(Values{0.02f, 0.02f, 0, 0, 0, 0}, 10)); // cycle 1: trough -> "more cycles needed"
    CHECK(detector.isCheckingPeaks());
    CHECK(detector.completedFullHitsCount() == 1);

    CHECK_FALSE(detector.update(Values{0.5f, 0.5f, 0, 0, 0, 0}, 20));   // cycle 2: peak (same two sensors)
    CHECK(detector.isCheckingTroughs());

    bool completed = detector.update(Values{0.02f, 0.02f, 0, 0, 0, 0}, 30); // cycle 2: trough -> target reached
    CHECK(completed);
    CHECK_FALSE(detector.isCheckingPeaks());
    CHECK_FALSE(detector.isCheckingTroughs());
    CHECK(detector.completedFullHitsCount() == 0);
}

TEST_CASE("trough window expiring without enough troughs aborts back to idle "
          "and resets progress")
{
    HitGestureDetector::Config config;
    config.requiredHits = 2;
    HitGestureDetector detector{config};

    CHECK_FALSE(detector.update(Values{0.5f, 0.5f, 0, 0, 0, 0}, 0));    // cycle 1: peak
    CHECK_FALSE(detector.update(Values{0.02f, 0.02f, 0, 0, 0, 0}, 10)); // cycle 1: trough -> "more cycles needed"
    CHECK(detector.completedFullHitsCount() == 1);

    CHECK_FALSE(detector.update(Values{0.5f, 0.5f, 0, 0, 0, 0}, 20));   // cycle 2: peak
    CHECK(detector.isCheckingTroughs());

    // let the 250ms trough window expire with sensors staying high (no trough)
    CHECK_FALSE(detector.update(Values{0.5f, 0.5f, 0, 0, 0, 0}, 20 + 251));

    CHECK_FALSE(detector.isCheckingPeaks());
    CHECK_FALSE(detector.isCheckingTroughs());
    CHECK(detector.completedFullHitsCount() == 0); // quirk #3: trough-timeout resets progress
}

TEST_CASE("peak window expiring without enough peaks aborts back to idle but "
          "preserves progress from an already-completed cycle")
{
    HitGestureDetector::Config config;
    config.requiredHits = 2;
    HitGestureDetector detector{config};

    CHECK_FALSE(detector.update(Values{0.5f, 0.5f, 0, 0, 0, 0}, 0));    // cycle 1: peak
    CHECK_FALSE(detector.update(Values{0.02f, 0.02f, 0, 0, 0, 0}, 10)); // cycle 1: trough -> "more cycles needed"
    CHECK(detector.isCheckingPeaks());
    CHECK(detector.completedFullHitsCount() == 1);

    // let the 80ms peak window expire with no second peak batch arriving
    CHECK_FALSE(detector.update(kSilence, 10 + 81));

    CHECK_FALSE(detector.isCheckingPeaks());
    CHECK_FALSE(detector.isCheckingTroughs());
    // quirk #3: unlike the trough-timeout case, this does NOT reset progress
    CHECK(detector.completedFullHitsCount() == 1);
}
