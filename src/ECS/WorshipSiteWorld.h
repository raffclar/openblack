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
	/// The door laid out in the worship site's model, about the temple it stands at; none without the model
	[[nodiscard]] virtual std::optional<glm::vec3> SiteDoor() const { return std::nullopt; }
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
	/// A dance of the info table about a point, danced for a site: made as every dance is, stopped at a quarter speed
	/// with the groups of its file; none when it can't be made
	virtual entt::entity MakeDance(DanceInfo dance, glm::vec3 position, entt::entity site) = 0;
	/// The game turn
	[[nodiscard]] virtual uint32_t Turn() const = 0;
	/// A site's food pot, empty, standing at a point and turned to an angle (radians)
	virtual entt::entity MakeFoodPot(glm::vec3 position, float yAngle) = 0;
	/// The site's turn powers up the artefacts put down by it, every so many turns, by how many dance at it
	virtual void PowerUpArtefacts(entt::entity /*site*/, uint32_t /*dancers*/) {}
	/// A site going up becomes a building site of a town asked to build it, wanting builders by a desire
	virtual void OpenBuildingSite(entt::entity /*site*/, entt::entity /*town*/, float /*desire*/) {}
	/// A site's building site goes: no town builds it any more
	virtual void CloseBuildingSite(entt::entity /*site*/) {}
};

} // namespace openblack::ecs::worship_site
