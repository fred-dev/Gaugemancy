#pragma once

#include <memory>
#include "ofxPDSP.h"

// One performance "slot" (up to 6 in multi-grain mode, 1 in single/
// accumulated-pressure mode) bundling the audio-graph objects that used to
// live in ~19 separate parallel std::vectors indexed by slot number
// (cloud[e], ampControl[e], bitCrusherLs[e], ...), each populated by raw
// `new` in populateVectors()/populateEffectVectors() with no matching
// `delete` -- clearEffectVectors() just called .clear() on the vector<T*>s,
// leaking every previous preset's effect objects on every reload. RAII
// (unique_ptr) fixes that as a side effect of the extraction, not something
// separately "fixed."
//
// Field names are kept identical to the vectors they replace (including the
// grammatically-now-wrong plurals like bitCrusherLs for a single object)
// specifically so every call site becomes a mechanical `slots[e].NAME`
// instead of `NAME[e]` -- a pure rename, not a redesign. Renaming these to
// something nicer is a reasonable follow-up once this step is verified.
//
// All effect objects are allocated unconditionally for every slot in the
// original code, regardless of whether that effect is actually enabled for
// that slot (the effectsPatching flags only control GUI-panel wiring and
// pdsp graph patching, not allocation) -- preserved here the same way.
//
// GUI panels, grain ofParameterGroups, effect parameter structs (EFF*Unit),
// and UI layout fields are NOT here yet -- still separate vectors in ofApp,
// to be folded in as a later, separate step.
struct Slot {
    Slot();

    // Replaces all 12 effect objects with fresh instances (was: clearEffectVectors()
    // + populateEffectVectors() discarding and reallocating the whole
    // vector<T*>, minus the leak -- unique_ptr assignment frees the old
    // object before taking ownership of the new one). Call after
    // disconnecting the old ones from the pdsp graph.
    void resetEffects();

    int grainVoices = 0;
    std::unique_ptr<pdsp::SampleBuffer> sampleData;
    std::unique_ptr<pdsp::GrainCloud> cloud;
    std::unique_ptr<pdsp::ParameterAmp> ampControl;
    std::unique_ptr<pdsp::Amp> outputAmpL;
    std::unique_ptr<pdsp::Amp> outputAmpR;

    std::unique_ptr<pdsp::Bitcruncher> bitCrusherLs;
    std::unique_ptr<pdsp::Bitcruncher> bitCrusherRs;
    std::unique_ptr<pdsp::Decimator> decimatorLs;
    std::unique_ptr<pdsp::Decimator> decimatorRs;
    std::unique_ptr<pdsp::Delay> delayLs;
    std::unique_ptr<pdsp::Delay> delayRs;
    std::unique_ptr<pdsp::Amp> delaySends;
    std::unique_ptr<pdsp::MultiLadder4> multiLadderFilterLs;
    std::unique_ptr<pdsp::MultiLadder4> multiLadderFilterRs;
    std::unique_ptr<pdsp::DimensionChorus> choruss;
    std::unique_ptr<pdsp::BasiVerb> reverbs;
    std::unique_ptr<pdsp::Amp> reverbSends;
    std::unique_ptr<pdsp::Compressor> compressors;
};
