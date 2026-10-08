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

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Audio/AudioManagerInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "Enums.h"
#include "Physics/BodyShapes.h"
#include "Physics/Materials.h"

namespace openblack
{
class GameRandomInterface;
class LandIslandInterface;
struct GObjectInfo;
namespace v120
{
struct InfoConstants;
}
using InfoConstants = v120::InfoConstants;
namespace water_rings
{
struct Ring;
}
namespace ecs
{
class Registry;
}
} // namespace openblack

/// What the physics needs of the rest of the game: the entities, the land, the models bodies are made from, the map's
/// cells, and the sounds, rings, reactions and fires its bodies set off. The game's own world works through the game's
/// systems; tests give a fake with their own entities and land.
namespace openblack::ecs::dynamics
{

/// What the physics asks of a model before it builds a body from it
struct ModelSize
{
	glm::vec3 size {0.0f};
	bool boned {false};
};

/// A model as the physics builds a body from it: its size, whether bones move it, and its parts' triangles, which stay
/// valid while the model is loaded
struct Model
{
	Model() = default;
	// The parts' views point into the indices kept here, so a model may be moved but not copied
	Model(const Model&) = delete;
	Model& operator=(const Model&) = delete;
	Model(Model&&) = default;
	Model& operator=(Model&&) = default;

	glm::vec3 size {0.0f};
	bool boned {false};
	/// Each part's triangle corners as indices, kept here for the parts' views below
	std::vector<std::vector<uint32_t>> indices;
	std::vector<physics::shapes::ModelPart> parts;
	/// Each part's vertices' bones, the same length as its positions; empty for a part not moved by bones
	std::vector<std::span<const uint16_t>> bones;
};

class World
{
public:
	virtual ~World() = default;

	[[nodiscard]] virtual Registry& Entities() = 0;
	[[nodiscard]] virtual const Registry& Entities() const = 0;
	/// The land, none before one is loaded
	[[nodiscard]] virtual const LandIslandInterface* Land() const = 0;
	[[nodiscard]] virtual const InfoConstants* Info() const = 0;
	/// The info row of an object's kind, none for an object of no kind
	[[nodiscard]] virtual const GObjectInfo* InfoOf(entt::entity object) const = 0;
	[[nodiscard]] virtual float LifeOf(entt::entity object) const = 0;
	/// An object's height as the game measures it
	[[nodiscard]] virtual float HeightOf(entt::entity object) const = 0;
	/// The object goes from the world
	virtual void Remove(entt::entity object) = 0;
	/// A loaded model's size and whether bones move it, none for one not loaded
	[[nodiscard]] virtual std::optional<ModelSize> SizeOfModel(entt::id_type mesh) const = 0;
	/// A loaded model with its parts' triangles, none for one not loaded
	[[nodiscard]] virtual std::optional<Model> ModelOf(entt::id_type mesh) const = 0;
	/// The physics' material of a row of its table; the default for a table not loaded
	[[nodiscard]] virtual physics::Material MaterialOf(physics::MaterialRow row) const = 0;

	/// Whether the map's cells are there to look in
	[[nodiscard]] virtual bool HasMap() const = 0;
	/// What stands in a map cell: the things that stay put first, then those that move
	[[nodiscard]] virtual std::vector<entt::entity> FixedThenMobileInCell(glm::ivec2 cell) const = 0;
	/// What stands in a map cell, in the map's own order of its cell's list
	[[nodiscard]] virtual std::vector<entt::entity> AllInCell(glm::ivec2 cell) const = 0;
	/// The object goes back into the map's cells where it is
	virtual void Refile(entt::entity object) = 0;

	[[nodiscard]] virtual std::optional<glm::vec3> CameraOrigin() const = 0;
	[[nodiscard]] virtual GameRandomInterface* Random() = 0;
	/// A sound heard the same everywhere
	virtual void PlaySound(audio::SoundId sound) = 0;
	/// A collision sound from a bank, as an animation's sound effect is played; none when there is no audio
	virtual std::optional<audio::AnimEffectPlay> PlayCollisionSound(std::span<const int32_t> keys, entt::entity owner,
	                                                                glm::vec3 position) = 0;
	virtual void MoveSound(entt::entity emitter, glm::vec3 position) = 0;
	virtual void AddWaterRing(const water_rings::Ring& ring) = 0;
	/// How deep the snow lies at a point, 0 to 255
	[[nodiscard]] virtual int32_t SnowAt(glm::vec3 point) const = 0;

	/// A burning thing starts to move and leaves its fire's group
	virtual void StartedMoving(entt::entity object) = 0;
	[[nodiscard]] virtual bool IsOnFire(entt::entity object) const = 0;
	virtual void CreateReaction(const systems::ReactionSystemInterface::Source& source) = 0;
	virtual void RemoveReactions(entt::entity initiator, Reaction type) = 0;
	/// Every creature's leash tied to the object lets go of it
	virtual void UntieLeashesTiedTo(entt::entity object) = 0;
	[[nodiscard]] virtual bool IsComputerPlayer(PlayerNames player) const = 0;
	/// A dead tree that came to rest covers the ground as it lies
	virtual void FitDeadTreeObstacle(entt::entity deadTree) = 0;
};

} // namespace openblack::ecs::dynamics
