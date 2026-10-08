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

#include <functional>
#include <utility>
#include <vector>

#include <entt/entity/fwd.hpp>

namespace openblack::ecs::systems
{
/// What the game's animals share: the visual time of day of the turn, the death listeners and each species' own
/// dying (ecs::animal_ai goes through it; each animal's own data is in its components)
class AnimalAISystemInterface
{
public:
	/// Called with an animal as it dies
	using DeathCallback = std::function<void(entt::entity)>;
	/// The listeners with their ids, in the order they were added
	using DeathListeners = std::vector<std::pair<uint32_t, DeathCallback>>;

	virtual ~AnimalAISystemInterface() = default;

	/// The visual time of day (hours) of this turn
	[[nodiscard]] virtual float VisualTime() const = 0;
	virtual void SetVisualTime(float hours) = 0;

	/// Adds a listener and returns its id (from 1, never reused)
	[[nodiscard]] virtual uint32_t AddDeathListener(DeathCallback callback) = 0;
	virtual void RemoveDeathListener(uint32_t id) = 0;
	[[nodiscard]] virtual const DeathListeners& GetDeathListeners() const = 0;
	/// The id of the listener of the single slot (0: none)
	[[nodiscard]] virtual uint32_t SingleSlotId() const = 0;
	virtual void SetSingleSlotId(uint32_t id) = 0;

	/// The species' own dying (empty: the common one)
	virtual void SetSpeciesDying(std::size_t species, DeathCallback dying) = 0;
	/// The species' own dying; null when it has none
	[[nodiscard]] virtual const DeathCallback* SpeciesDying(std::size_t species) const = 0;
};
} // namespace openblack::ecs::systems
