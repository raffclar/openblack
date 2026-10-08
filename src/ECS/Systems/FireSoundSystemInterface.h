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

#include <array>
#include <unordered_map>

namespace openblack::ecs::fire
{
struct FireEffect;

namespace sound
{
/// One of the two crackle slots: the fire playing in it and its camera distance
struct Slot
{
	FireEffect* fire {nullptr};
	float distance {0.0f};
};
} // namespace sound
} // namespace openblack::ecs::fire

namespace openblack::ecs::systems
{
/// The game's fire crackle: the two slots, the farthest slot's distance and each fire's sound owner
/// (ecs::fire::sound goes through it)
class FireSoundSystemInterface
{
public:
	using Slots = std::array<fire::sound::Slot, 2>;
	/// The channels' owner of each fire that played: an audio object number
	using Owners = std::unordered_map<const fire::FireEffect*, uint32_t>;

	virtual ~FireSoundSystemInterface() = default;

	[[nodiscard]] virtual Slots& GetSlots() = 0;
	[[nodiscard]] virtual float MaxDistance() const = 0;
	virtual void SetMaxDistance(float distance) = 0;
	[[nodiscard]] virtual Owners& GetOwners() = 0;
};
} // namespace openblack::ecs::systems
