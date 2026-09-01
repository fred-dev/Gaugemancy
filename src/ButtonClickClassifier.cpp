#include "ButtonClickClassifier.h"

ButtonClickClassifier::Event ButtonClickClassifier::update(bool buttonPressed, uint64_t elapsedMillis)
{
    if (elapsedMillis <= config.startupGuardMs)
    {
        return Event::None;
    }

    Event fired = Event::None;

    if (clicks == 3 && elapsedMillis - click3Time > config.clickWindowMs && waitingForClick)
    {
        clicks = 0;
        click1Time = 0;
        click2Time = 0;
        click3Time = 0;
        fired = Event::TripleClick;
    }
    else if (clicks == 2 && elapsedMillis - click2Time > config.clickWindowMs && waitingForClick)
    {
        clicks = 0;
        click1Time = 0;
        click2Time = 0;
        fired = Event::DoubleClick;
    }

    if (clicks == 1 && elapsedMillis - click1Time > config.clickWindowMs && waitingForClick)
    {
        clicks = 0;
        click1Time = 0;
        fired = Event::SingleClick;
    }

    if (buttonPressed && clicks == 0 && waitingForClick)
    {
        clicks = 1;
        click1Time = elapsedMillis;
    }
    else if (buttonPressed && clicks == 1 && elapsedMillis - click1Time < config.clickWindowMs && waitingForClick)
    {
        clicks = 2;
        click2Time = elapsedMillis;
    }
    else if (buttonPressed && clicks == 2 && elapsedMillis - click2Time < config.clickWindowMs && waitingForClick)
    {
        clicks = 3;
        click3Time = elapsedMillis;
    }

    if (buttonPressed)
    {
        waitingForClick = false;
    }
    if (!buttonPressed)
    {
        waitingForClick = true;
        if (clicks == 1)
        {
            clickOneReleaseTime = elapsedMillis;
            if (clickOneReleaseTime - click1Time > 6000)
            {
                click1Time = 0;
                clicks = 0;
                clickOneReleaseTime = 0;
                fired = Event::SixSecondHold;
            }
            if (clickOneReleaseTime - click1Time > 3000)
            {
                click1Time = 0;
                clicks = 0;
                clickOneReleaseTime = 0;
                fired = Event::ThreeSecondHold;
            }
        }
    }

    return fired;
}
