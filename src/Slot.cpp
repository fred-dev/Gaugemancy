#include "Slot.h"

Slot::Slot()
    : sampleData(std::make_unique<pdsp::SampleBuffer>())
    , cloud(std::make_unique<pdsp::GrainCloud>())
    , ampControl(std::make_unique<pdsp::ParameterAmp>())
    , outputAmpL(std::make_unique<pdsp::Amp>())
    , outputAmpR(std::make_unique<pdsp::Amp>())
    , bitCrusherLs(std::make_unique<pdsp::Bitcruncher>())
    , bitCrusherRs(std::make_unique<pdsp::Bitcruncher>())
    , decimatorLs(std::make_unique<pdsp::Decimator>())
    , decimatorRs(std::make_unique<pdsp::Decimator>())
    , delayLs(std::make_unique<pdsp::Delay>())
    , delayRs(std::make_unique<pdsp::Delay>())
    , delaySends(std::make_unique<pdsp::Amp>())
    , multiLadderFilterLs(std::make_unique<pdsp::MultiLadder4>())
    , multiLadderFilterRs(std::make_unique<pdsp::MultiLadder4>())
    , choruss(std::make_unique<pdsp::DimensionChorus>())
    , reverbs(std::make_unique<pdsp::BasiVerb>())
    , reverbSends(std::make_unique<pdsp::Amp>())
    , compressors(std::make_unique<pdsp::Compressor>())
{
}

void Slot::resetEffects()
{
    bitCrusherLs = std::make_unique<pdsp::Bitcruncher>();
    bitCrusherRs = std::make_unique<pdsp::Bitcruncher>();
    decimatorLs = std::make_unique<pdsp::Decimator>();
    decimatorRs = std::make_unique<pdsp::Decimator>();
    delayLs = std::make_unique<pdsp::Delay>();
    delayRs = std::make_unique<pdsp::Delay>();
    delaySends = std::make_unique<pdsp::Amp>();
    multiLadderFilterLs = std::make_unique<pdsp::MultiLadder4>();
    multiLadderFilterRs = std::make_unique<pdsp::MultiLadder4>();
    choruss = std::make_unique<pdsp::DimensionChorus>();
    reverbs = std::make_unique<pdsp::BasiVerb>();
    reverbSends = std::make_unique<pdsp::Amp>();
    compressors = std::make_unique<pdsp::Compressor>();
}
