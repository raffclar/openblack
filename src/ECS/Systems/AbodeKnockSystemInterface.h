/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::systems
{

/// The hand knocking on a town's buildings: everyone inside comes out, a house knocks back with a sound and the hand's
/// tap, and every house of the town shows for a few seconds how many people live in it
class AbodeKnockSystemInterface
{
public:
	virtual ~AbodeKnockSystemInterface() = default;

	/// The hand taps a building at a point; `ownHand` when it is this computer's player's hand, which then plays its tap.
	/// Whether the building took the tap.
	virtual bool Tap(entt::entity abode, glm::vec3 handPoint, bool ownHand) = 0;
	/// A villager that a player holds over a house of the town starts or holds the town's read-out, as a knock does.
	/// Nobody comes out, and the house does not knock back.
	virtual void ShowReadout(entt::entity town) = 0;
	/// Each frame: the read-out's time runs down
	virtual void Update(std::chrono::milliseconds frame) = 0;

	/// The town whose houses show the read-out, while it shows
	[[nodiscard]] virtual std::optional<entt::entity> GetReadoutTown() const = 0;
	/// How big the read-out's markers are now, from nothing to full
	[[nodiscard]] virtual float GetReadoutScale() const = 0;
	/// Whether this computer's hand has knocked on a house since it last asked, for it to play its tap
	virtual bool TakeHandKnock() = 0;
};

} // namespace openblack::ecs::systems
