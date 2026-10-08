/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/vec3.hpp>

#include "Particles/SoundAction.h"

// The sounds of the particle system (the miracles' loops, bangs and thunder): each is a ParticleSound tied to the atom that
// started it, played from the spells.sad anim effect table (PlayAnimationEffect on the 16 channels)
// and kept alive or released once per game turn. Wiki: docs/bw1-notes/particles.md (the particle sounds) and
// docs/bw1-notes/audio.md.

namespace openblack::psys
{
class Effect;
struct Atom;
} // namespace openblack::psys

namespace openblack::audio
{

/// A particle sound: in the global list and in its atom's list
struct ParticleSound
{
	psys::SoundAction action;         ///< copied; the attribute slots are filled in by StartSound
	float delay {0.0f};               ///< seconds left before a Delayed sound starts
	const psys::Atom* atom {nullptr}; ///< null once the atom stopped it or is gone
	bool released {false};            ///< the release has been sent
	glm::vec3 position {0.0f};        ///< the 3D sound position: the atom's last drawn position
	/// The channels' owner (this ParticleSound): audio::Owner::Object(owner), registered with audio::RegisterObject for its
	/// 3D sound position while the sound lives
	uint32_t owner {0};
};

namespace spell_sounds
{
/// Nothing for NO_SOUND; the slots come from the action (and the atom: surface with
/// USESURFACE), the sound goes to the front of the atom's list; played now at the camera distance, or distance / 347 s
/// later with the Delayed flag.
void StartSound(const psys::Effect& effect, psys::Atom& atom, const psys::SoundAction& action);
/// Out of the atom's list, atom cleared; the turn update then releases it
void StopSound(psys::Atom& atom, ParticleSound& sound);
void StopAllSounds(psys::Atom& atom);
/// The newest sound of that action on the atom, or null
[[nodiscard]] ParticleSound* GetSoundOfAction(const psys::Atom& atom, int32_t action);

/// Once per game turn after the effects stepped: a live looping sound is re-issued within 1200 of the camera (the bank
/// does nothing if it is already playing), a Delayed one starts when its delay runs out; once the atom is gone the
/// sound fades by FadeStep per turn, is released once (loop released with SOFTRELEASE, else stopped) and is deleted
/// when it no longer plays.
void ProcessTurn(float turnSeconds);
/// Stops and forgets every sound (a new map)
void Clear();
[[nodiscard]] size_t Count();

// The size class (attribute 0: 1 large, 2 medium, 3 small) the rules put in the action before StartSound
/// From a create rule's SoundRadiusFP (defaults: small 200, medium 500)
[[nodiscard]] int32_t SizeFromRadius(float radius, float small, float medium);
/// From the throw speed fraction (0..1) of a create with initial direction
[[nodiscard]] int32_t SizeFromThrow(float fraction);
/// From a gravity with floor impact: the impact speed against ImpactSpeedMedium / Large
[[nodiscard]] int32_t SizeFromImpactSpeed(float speed, float medium, float large);
} // namespace spell_sounds

} // namespace openblack::audio
