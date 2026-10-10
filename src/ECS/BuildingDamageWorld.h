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
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "Magic/MagicWorldInterface.h"
#include "Magic/SpellRules.h"
#include "Physics/DamageMesh.h"

namespace openblack
{
class GameRandomInterface;
struct GObjectInfo;
namespace ecs
{
class Registry;
namespace systems
{
class DynamicsSystemInterface;
}
} // namespace ecs
} // namespace openblack

/// What breaking buildings needs of the rest of the game: the entities, the land, the buildings' models and the models
/// their broken shapes are drawn with, the physics their pieces fly in, the snow, the random draws, the sounds, the crush
/// a breaking blow applies and the creatures that copy the player. The game's own world works through the game's
/// systems; tests give a fake.
namespace openblack::ecs::building_world
{

/// A drawn part of a model: its material (its sub mesh above, its primitive below) and its triangles' corners in the
/// model's own space
struct ModelPart
{
	uint32_t material {0};
	std::vector<std::array<physics::damage::Corner, 3>> triangles;
};

/// A corner of a face of a broken shape as it is drawn, in the model's own space
struct DrawnVertex
{
	glm::vec3 position {0.0f};
	glm::vec2 uv {0.0f};
	glm::vec3 normal {0.0f, 1.0f, 0.0f};
};

/// The faces of one part of a broken shape, drawn with the material of a part of the building's own model
struct DrawnPart
{
	uint32_t material {0};
	std::vector<DrawnVertex> vertices;
	/// Triangles over its own vertices
	std::vector<uint16_t> indices;
};

class World
{
public:
	virtual ~World() = default;

	[[nodiscard]] virtual Registry& Entities() = 0;
	[[nodiscard]] virtual const Registry& Entities() const = 0;
	/// The land's height at a point, nothing before there is land
	[[nodiscard]] virtual float LandHeight(glm::vec2 point) const = 0;
	/// The drawn parts of a model's nearest level of detail, none for a model not loaded
	[[nodiscard]] virtual std::vector<ModelPart> ModelParts(entt::id_type mesh) const = 0;
	/// Makes a model of that name drawn with the materials of another, replacing one of the same name; none when it
	/// couldn't be made
	[[nodiscard]] virtual entt::id_type MakeModel(const std::string& name, entt::id_type source,
	                                              std::span<const DrawnPart> parts) = 0;
	/// A model made for a broken shape goes
	virtual void EraseModel(entt::id_type mesh) = 0;
	/// The physics, none when there is none
	[[nodiscard]] virtual systems::DynamicsSystemInterface* Dynamics() = 0;
	/// The game's synchronised random numbers, none when there are none
	[[nodiscard]] virtual GameRandomInterface* Random() = 0;
	/// How deep the snow lies at a point, 0 to 255
	[[nodiscard]] virtual int32_t SnowAt(glm::vec3 point) const = 0;
	/// Dust of a colour as it looks where snow lies so deep
	[[nodiscard]] virtual uint32_t DustTint(uint32_t argb, int32_t snow) const = 0;
	/// A sound of the buildings' bank, as an animation's sound effect is played, on a building where it stands
	virtual void PlaySound(std::span<const int32_t> keys, entt::entity building, glm::vec3 position) = 0;

	[[nodiscard]] virtual float LifeOf(entt::entity object) const = 0;
	/// The info row of an object's kind, none for an object of no kind
	[[nodiscard]] virtual const GObjectInfo* InfoOf(entt::entity object) const = 0;
	/// The player an object belongs to, none for one of no player
	[[nodiscard]] virtual std::optional<PlayerNames> PlayerOf(entt::entity object) const = 0;
	/// The object goes from the world
	virtual void Remove(entt::entity object) = 0;

	/// The game's crush, which a breaking blow applies; none before the game's data is loaded or without magic
	[[nodiscard]] virtual std::optional<magic::EffectValues> Crush() const = 0;
	virtual void ApplyEffect(entt::entity object, const magic::EffectValues& values, const magic::EffectSource& source) = 0;
	/// A player did one of the deeds creatures copy at a point to an object
	virtual void PlayerDid(size_t deed, glm::vec3 point, entt::entity object, PlayerNames player) = 0;
};

} // namespace openblack::ecs::building_world
