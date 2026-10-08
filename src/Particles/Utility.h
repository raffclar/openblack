/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <vector>

#include <glm/vec3.hpp>

#include "Magic/Gestures/GestureShapes.h"

namespace openblack::magic::gestures
{
class GestureSystem;
struct Result;
} // namespace openblack::magic::gestures

// The utility effects the interface keeps while casting: the gesture trail
// (PARTICLE_TYPE_GESTURE_LOCAL 48, SF_GestureChain), the "recognised" sparkles (PARTICLE_TYPE_GESTURE 35, SF_Gesture)
// and the open selection (PARTICLE_TYPE_SPELL_SELECTION 28, SF_SpellSelection). Wiki: docs/bw1-notes/magic.md.

namespace openblack::psys::utility
{
/// The utility effects, made once: the trail (48) and its "active", the recognised sparkles (35), SF_OnFire (37),
/// SF_LightningStrike (61), SF_ManaPathNew (22), SF_BeliefSprite (24), the selection (28) and its "active". (The
/// exploded meshes' SF_ExplodeObject is Particles/Rules/ExplodeObject.cpp's own.) Kept in the particle system's state
struct UtilityEffects
{
	uint32_t trail {0};
	bool trailActive {false};
	uint32_t recognised {0};
	uint32_t onFire {0};
	uint32_t lightningStrike {0};
	uint32_t manaPath {0};
	uint32_t belief {0};
	uint32_t selection {0};
	bool selectionActive {false};
};

/// When a gesture is recognised: the land points of the buffer and the gesture's ideal shape laid on the land over
/// the matched samples' box go into a record for UR_GesturingRecognised (the SF_Gesture effect, stepped once a turn
/// by ProcessTurn), which turns it into the sparkles
void GestureRecognised(const magic::gestures::GestureSystem& system, const magic::gestures::Result& result);

/// The records waiting for UR_GesturingRecognised (it takes the newest one per step)
[[nodiscard]] std::vector<magic::gestures::RecognisedGesture>& PendingRecognised();

/// Every frame: the trail on while the game expects a gesture, following the hand with magnitude handScale x
/// f(camera distance); the selection effect while the selection is open. seconds = the frame's game time.
void Update(float seconds, const glm::vec3& handPosition, float handScale, float cameraDistance);

/// Once a turn, at the end of the game loop (after the exploded meshes' effect, which explode_object::GameLoopEnd
/// keeps): SF_OnFire, SF_ManaPathNew, SF_BeliefSprite, SF_Gesture (the recognised sparkles) and SF_LightningStrike, in
/// that order, each made when missing, stepped with an empty ProcessInfo (power 1, enabled) and the turn's ms, and
/// dropped when it ends
void ProcessTurn();

/// A land is loaded
void Reset();
} // namespace openblack::psys::utility
