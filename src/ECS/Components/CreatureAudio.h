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

#include <deque>
#include <string>

#include "Audio/Engine/AnimEffectKeys.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/Game/Banks.h"
#include "Creature/CreatureAudio.h"

namespace openblack::ecs::components
{

/// What a creature's animations have played so far, so that each frame sounds the moments they have passed since, and
/// the sounds it made last
struct CreatureAudio
{
	/// Where each layer of the body was at the last frame in which time passed
	creature_audio::Layers last;

	/// A sound the creature made, or was to make
	struct Heard
	{
		/// The game time it was asked for, in milliseconds since the creature was first heard
		float atMs;
		creature_audio::EventKind kind;
		audio::AnimEffectKeys keys;
		/// The bank it was looked up in, none when the species has no voice bank
		audio::BankId bank {audio::k_NoBank};
		/// The channel it started on, none when the bank's filters left it out or nothing started
		audio::Channel channel {audio::k_NoChannel};
		bool played {false};
		/// Why it was silent, if it was
		std::string note;
	};
	/// The last sounds, the newest last
	std::deque<Heard> recent;
	static constexpr size_t k_RecentCount = 24;
	float clockMs {0.0f};
};

} // namespace openblack::ecs::components
