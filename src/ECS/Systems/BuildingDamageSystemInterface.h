/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <span>

#include <entt/entity/fwd.hpp>

#include "Creature/CreatureStomp.h"

namespace openblack::ecs
{
struct PhysicsEntry;
struct ImpactInfo;
} // namespace openblack::ecs

namespace openblack::ecs::components
{
struct BuildingDamage;
} // namespace openblack::ecs::components

namespace openblack::ecs::building_damage
{
/// The model that a thing is drawn with: the broken model of a broken building (`broken`, none for a thing that is not
/// broken) in place of its own model
[[nodiscard]] entt::id_type DrawnMeshOf(const components::BuildingDamage* broken, entt::id_type own);
} // namespace openblack::ecs::building_damage

namespace openblack::ecs::systems
{
class DynamicsSystemInterface;

/// Buildings broken by rocks: the pieces knocked out of their models fly off and lie about until their time runs out,
/// and what is left of a building is drawn as it stands
class BuildingDamageSystemInterface
{
public:
	virtual ~BuildingDamageSystemInterface() = default;

	/// A building's reaction to its last turn's knock: a rock struck it, which only sounds when it is slow, and breaks
	/// pieces off it when it is fast and heavy enough
	virtual void ReactToImpact(DynamicsSystemInterface& dynamics, PhysicsEntry& entry, const ImpactInfo& impact) = 0;
	/// A blow struck on another body, a temple's heart, passed on to a building: it lands on a point of the building's
	/// model and flies its pieces off with no speed at all
	virtual void ReactToPassedOnImpact(DynamicsSystemInterface& dynamics, entt::entity building, PhysicsEntry& struck,
	                                   const ImpactInfo& impact) = 0;
	/// A creature's blow on a building breaks it about where it stands, as far round as the creature is big
	virtual void Smash(entt::entity building, entt::entity creature, float creatureSize) = 0;
	/// The blows of a creature on a building in one moment, such as the two blows of a kick or a blow of a part of its
	/// body: each breaks the building where it lands. The building takes the harm once, after the last blow, and makes
	/// no sound of a stomp. The harm of a kick counts as smashed by a creature for the winning conditions of a network
	/// game; the harm of other blows counts as smashed by the player of the creature.
	virtual void CreatureBlows(entt::entity /*building*/, entt::entity /*creature*/,
	                           std::span<const creature_stomp::Blow> /*blows*/, bool /*kick*/)
	{
	}
	/// A piece comes to rest: a big one goes back into its building as rubble, the rest lie where they are. The object that
	/// stays, none when it went into its building.
	virtual entt::entity PieceAtRest(DynamicsSystemInterface& dynamics, PhysicsEntry* entry, entt::entity piece,
	                                 bool insert) = 0;
	/// A rock stops being one that breaks buildings (it came to rest or was taken): the buildings it broke forget it
	virtual void ForgetHitter(entt::entity rock) = 0;
	/// Every game turn: pieces whose building has gone forget it, and pieces whose time has run out go
	virtual void ProcessTurn() = 0;
	/// A piece goes at once with its model, for example when a creature breaks it in two
	virtual void RemovePiece(entt::entity piece) = 0;
	/// After a load: the models that the game made for these things while it played are made again. A save keeps the
	/// names of these models but not the models, and the game that loads it may not have them.
	virtual void RemakeModelsAfterLoad() {}
	/// The model an object is drawn with in place of its own: a broken building's broken model; its own otherwise
	[[nodiscard]] virtual entt::id_type DrawnMesh(entt::entity object, entt::id_type own) const = 0;
	/// How much of a broken or unfinished building is drawn standing over what is left of it, none for one drawn whole
	/// or not at all
	[[nodiscard]] virtual std::optional<float> PartialShare(entt::entity building) const = 0;
	/// Whether the object is drawn whole as any other: not a building drawn only as far up as it is built or repaired
	[[nodiscard]] virtual bool DrawsWhole(entt::entity object) const = 0;
	/// A building repaired whole is drawn whole again: what was broken off it is mended
	virtual void Repaired(entt::entity building) = 0;
	/// A new land: every building whole again
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
