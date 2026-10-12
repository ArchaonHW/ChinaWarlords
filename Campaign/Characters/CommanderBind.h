#pragma once

#include "Campaign/Characters/Character.h"
#include "Campaign/Narrative/IntelLedger.h"
#include "Gameplay/Fog/QuantumFog.h" // FogConfig
#include "Gameplay/Result.h"

#include <cstdint>

namespace Potato::Campaign {

// Priors binding (Story 12.4): the commander's personality prior is
// persisted identity that biases two surfaces — the QuantumFog
// briefing shape (FogConfig, consumed by battle assembly in 12.5)
// and the hearsay record (IntelLedger, which RivalDeck counter-decks
// already read via DistortionFor/ProvenWrongFor).

// Fog-shape deltas per prior — applied to a FogConfig BEFORE its
// QuantumFog is constructed at battle assembly.
struct PriorFogBias {
    int probeGainDelta = 0;
    int decayPerMinuteDelta = 0;
    int detectThresholdDelta = 0;
    int initialIntelDelta = 0;
};

// aggressive: scouts press hard but read thin (+probe, −initial)
// defensive: holds what it learns (−decay)
// cunning:    sees through feints (−detect threshold)
PriorFogBias PriorBias(RivalPrior p);

// Apply the prior's deltas to `cfg` with FogConfig-semantic clamps
// (decay >= 1, threshold/initialIntel in [0,100], probeGain >= 0).
void ApplyPrior(Gameplay::FogConfig& cfg, RivalPrior p);

// Hearsay binding: record the commander as a RivalTemperament intel
// claim — subject = character.id, claim carries name + prior wire
// name ("統帥<name>，性向<prior>"), chapter = 0 (prologue intel).
// Rivals' dossiers and counter-deck depth read this subject through
// the existing IntelLedger seams; no RivalBook change needed.
Gameplay::Result<std::uint64_t> RecordCommander(
    const Character& c, IntelLedger& intel);

} // namespace Potato::Campaign
