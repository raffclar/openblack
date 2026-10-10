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

#include <optional>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "Magic/WorshipBattery.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::worship_site
{

/// What the worship sites need of the rest of the game: the land, the worship site's model, its tribes' altars and
/// how many live in a town
class WorldInterface
{
public:
	virtual ~WorldInterface() = default;

	[[nodiscard]] virtual Registry& Entities() = 0;
	/// Which land of the story this is
	[[nodiscard]] virtual int32_t LandNumber() const = 0;
	/// One of the points laid out in the worship site's model, about the temple it stands at; none without the model
	[[nodiscard]] virtual std::optional<glm::vec3> SitePoint(uint32_t index) const = 0;
	/// The worship site's model as it stands at a temple: wearing the temple's own skin once the temple has one, which
	/// changes with its player's alignment
	[[nodiscard]] virtual entt::id_type SiteMesh(entt::entity temple) = 0;
	/// The model of a tribe's altar
	[[nodiscard]] virtual entt::id_type AltarMesh(Tribe tribe) const = 0;
	[[nodiscard]] virtual float LandHeightAt(glm::vec2 point) const = 0;
	/// How many people live in a town, with a home or without
	[[nodiscard]] virtual uint32_t PopulationOf(entt::entity town) const = 0;
	/// The prayer power rules of a tribe's sites for a player: every site chants with the player's Aztec power
	[[nodiscard]] virtual magic::WorshipBatteryRules ChantRules(Tribe tribe, PlayerNames player) const = 0;
	/// Whether a dance of the info table starts by itself once its dancers have come
	[[nodiscard]] virtual bool DanceStartsAutomatically(DanceInfo dance) const = 0;
	/// How many lengths of the clock a dance of the info table runs before it starts over, from its dance file; none
	/// when the file can't be read
	[[nodiscard]] virtual std::optional<uint32_t> DanceLoops(DanceInfo dance) = 0;
	/// The game turn
	[[nodiscard]] virtual uint32_t Turn() const = 0;
	/// A site's food pot, empty, standing at a point and turned to an angle (radians)
	virtual entt::entity MakeFoodPot(glm::vec3 position, float yAngle) = 0;
};

} // namespace openblack::ecs::worship_site
