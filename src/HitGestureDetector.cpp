#include "HitGestureDetector.h"

bool HitGestureDetector::update(const std::array<float, kNumSensors> & v, uint64_t elapsedMillis)
{
    bool hitCompleted = false;

    // Block 1: idle, waiting for the first peak batch.
    if (!isCheckingHitPeaks && !isCheckingHitTroughs)
    {
        for (int u = 0; u < kNumSensors; u++)
        {
            if (v[u] > config.hitThreshold)
            {
                hitPeakChecker += 1;
                hadHitPeak[u] = true;
            }
        }
        if (hitPeakChecker > 1)
        {
            isCheckingHitTroughs = true;
            timeSinceLastHitTrough = elapsedMillis;
            hitPeakChecker = 0;
        }
        else
        {
            hitPeakChecker = 0;
            for (int z = 0; z < kNumSensors; z++)
            {
                hadHitPeak[z] = false;
            }
        }
    }

    // Block 2: inside the trough-detection window.
    if (elapsedMillis - timeSinceLastHitTrough < config.maxTroughDurationMs && isCheckingHitTroughs)
    {
        hitTroughChecker = 0;
        for (int u = 0; u < kNumSensors; u++)
        {
            hadHitTrough[u] = false;
            if (hadHitPeak[u])
            {
                if (v[u] < config.troughThreshold)
                {
                    hitTroughChecker += 1;
                    hadHitTrough[u] = true;
                }
                if (hitTroughChecker > 1)
                {
                    completedFullHits += 1;
                    if (completedFullHits > config.requiredHits - 1)
                    {
                        timeSinceLastHitPeak = 0;
                        timeSinceLastHitTrough = 0;
                        hitTroughChecker = 0;
                        completedFullHits = 0;
                        for (int t = 0; t < kNumSensors; t++)
                        {
                            hadHitPeak[t] = false;
                            hadHitTrough[t] = false;
                        }
                        isCheckingHitTroughs = false;
                        isCheckingHitPeaks = false;
                        hitCompleted = true;
                        // the original's `goto finished` here is behaviorally a
                        // no-op: every block below is gated on
                        // isCheckingHitTroughs/isCheckingHitPeaks, both just set
                        // false above, so falling through reaches the same result.
                    }
                    else
                    {
                        hitTroughChecker = 0;
                        timeSinceLastHitPeak = elapsedMillis;
                        isCheckingHitPeaks = true;
                        isCheckingHitTroughs = false;
                    }
                }
            }
        }
    }

    // Block 3: trough-detection window has expired.
    if (elapsedMillis - timeSinceLastHitTrough > config.maxTroughDurationMs && isCheckingHitTroughs)
    {
        if (hitTroughChecker > 1)
        {
            completedFullHits += 1;
            if (completedFullHits > config.requiredHits - 1)
            {
                completedFullHits = 0;
                hitCompleted = true;
                // NOTE: unlike block 2's completion path above, this one does
                // not reset timeSinceLastHitPeak/timeSinceLastHitTrough or
                // clear hadHitPeak/hadHitTrough -- preserved as-is (see the
                // class-level comment on this asymmetry).
            }
            hitTroughChecker = 0;
            timeSinceLastHitPeak = elapsedMillis;
            isCheckingHitPeaks = true;
            isCheckingHitTroughs = false;
        }
        else
        {
            isCheckingHitTroughs = false;
            isCheckingHitPeaks = false;
            timeSinceLastHitPeak = 0;
            timeSinceLastHitTrough = 0;
            hitTroughChecker = 0;
            completedFullHits = 0;
            for (int t = 0; t < kNumSensors; t++)
            {
                hadHitPeak[t] = false;
                hadHitTrough[t] = false;
            }
        }
    }

    // Block 4: inside the peak-detection window (checking for the next batch).
    if (elapsedMillis - timeSinceLastHitPeak < config.maxPeakDurationMs && isCheckingHitPeaks)
    {
        hitPeakChecker = 0;
        for (int u = 0; u < kNumSensors; u++)
        {
            hadHitPeak[u] = false;
            if (hadHitTrough[u])
            {
                if (v[u] > config.hitThreshold)
                {
                    hitPeakChecker += 1;
                    hadHitPeak[u] = true;
                    if (hitPeakChecker > 1)
                    {
                        isCheckingHitTroughs = true;
                        hitPeakChecker = 0;
                        timeSinceLastHitTrough = elapsedMillis;
                    }
                }
            }
        }
    }

    // Block 5: peak-detection window has expired.
    if (elapsedMillis - timeSinceLastHitPeak > config.maxPeakDurationMs && isCheckingHitPeaks)
    {
        if (hitPeakChecker > 1)
        {
            isCheckingHitTroughs = true;
            hitPeakChecker = 0;
            timeSinceLastHitTrough = elapsedMillis;
        }
        else
        {
            isCheckingHitPeaks = false;
            isCheckingHitTroughs = false;
            timeSinceLastHitPeak = 0;
            timeSinceLastHitTrough = 0;
            hitPeakChecker = 0;
            for (int t = 0; t < kNumSensors; t++)
            {
                hadHitPeak[t] = false;
                hadHitTrough[t] = false;
            }
        }
    }

    return hitCompleted;
}
