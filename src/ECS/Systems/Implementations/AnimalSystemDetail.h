/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <LNDFile.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Animals/BirdRules.h"
#include "Animals/GrazerRules.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

// The animal system's own helpers, shared by its source files: not for use elsewhere

namespace openblack::ecs::systems::animal_detail
{
using components::Animal;
using components::AnimalState;
using components::Mesh;
using components::Transform;

inline ecs::Registry& EntityRegistry()
{
	return Locator::entitiesRegistry::value();
}

inline const GAnimalInfo& InfoOf(AnimalInfo type)
{
	return Locator::infoConstants::value().animal.at(static_cast<size_t>(type));
}

inline float Ground(glm::vec2 xz)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(xz) : 0.0f;
}

inline float Random(float max)
{
	return Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameFloatRand(max) : 0.0f;
}

inline uint32_t RandomWhole(uint32_t n)
{
	return Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameRand(n) : 0;
}

inline bool IsBird(AnimalInfo type)
{
	return animals::birds::IsBird(type);
}

inline uint16_t SpeedStateOf(const GAnimalInfo& info, size_t index)
{
	const auto& group = info.speedGroup;
	const std::array speeds {group.speedDefault, group.speedFleeing, group.speed2, group.speed3, group.speed4, group.speed5};
	return static_cast<uint16_t>(speeds.at(index));
}

inline uint16_t TurnAngleOf(const Animal& animal)
{
	return static_cast<uint16_t>(InfoOf(animal.type).turnAngle);
}

inline glm::vec2 Metres(glm::ivec2 fixed)
{
	return {map_coords::ToMetres(fixed.x), map_coords::ToMetres(fixed.y)};
}

inline glm::ivec2 Fixed(glm::vec2 metres)
{
	return {map_coords::ToFixed(metres.x), map_coords::ToFixed(metres.y)};
}

inline glm::vec2 Xz(const glm::vec3& point)
{
	return {point.x, point.z};
}

inline bool OnMap(glm::vec2 point)
{
	return map_coords::InBounds(map_coords::FromMetres(point));
}

/// The game's number for an animal's state, the row of its table of the animals' states
inline size_t GameStateOf(AnimalState state)
{
	switch (state)
	{
	case AnimalState::MoveToPos:
		return 1;
	case AnimalState::InScript:
		return 4;
	case AnimalState::FleeingFromObject:
		return 6;
	case AnimalState::SetDying:
		return 13;
	case AnimalState::Dying:
		return 14;
	case AnimalState::Dead:
		return 15;
	case AnimalState::Downed:
		return 17;
	case AnimalState::WaitForClip:
		return 23;
	case AnimalState::MoveInFlock:
		return 27;
	case AnimalState::FleeingAndLookingAtObject:
		return 30;
	case AnimalState::StartWander:
		return 31;
	case AnimalState::Wander:
		return 32;
	case AnimalState::Eat:
		return 33;
	case AnimalState::SeekSleep:
		return 34;
	case AnimalState::Sleeps:
		return 35;
	case AnimalState::StartToEat:
		return 38;
	case AnimalState::FinishEating:
		return 39;
	case AnimalState::Pounce:
		return 40;
	case AnimalState::Chase:
		return 41;
	case AnimalState::SpecialMoveToPos:
		return 44;
	case AnimalState::FollowFlock:
		return 45;
	case AnimalState::InteractDecideWhatToDo:
		return 48;
	case AnimalState::GivesBirth:
		return 50;
	case AnimalState::DecideWhatToDo:
	default:
		return 43;
	}
}

/// A grazer's clip of a state: it stands to decide, to be born, while a script holds it, watching what it fled and
/// once up from a landing; it grazes, lowers and raises its head, sleeps, falls dead, and otherwise moves, walking or
/// running by its speed
inline AnimId GrazerClipFor(const Animal& animal, AnimalState state)
{
	using animals::grazers::ClipSlot;
	auto slot = ClipSlot::Move;
	switch (state)
	{
	case AnimalState::DecideWhatToDo:
	case AnimalState::InScript:
	case AnimalState::FleeingAndLookingAtObject:
	case AnimalState::InteractDecideWhatToDo:
	case AnimalState::GivesBirth:
		slot = ClipSlot::Stand;
		break;
	case AnimalState::Eat:
		slot = ClipSlot::Eat;
		break;
	case AnimalState::StartToEat:
		slot = ClipSlot::StartToEat;
		break;
	case AnimalState::FinishEating:
		slot = ClipSlot::FinishEating;
		break;
	case AnimalState::Sleeps:
		slot = ClipSlot::Sleep;
		break;
	case AnimalState::Dying:
		slot = ClipSlot::Dying;
		break;
	default:
		break;
	}
	const auto& row =
	    Locator::infoConstants::value().speedThreshold.at(static_cast<size_t>(animals::grazers::ThresholdRowOf(animal.type)));
	return animals::grazers::Clip(
	    animal.type, slot, animal.move.speed,
	    {.walk = static_cast<uint16_t>(row.speedMaxWalk), .run = static_cast<uint16_t>(row.speedMaxRun)}, RandomWhole);
}

/// The clip of a state: the miracle's doves always flap their clip and its bats theirs; its wolves stand to decide,
/// leap, settle down to eat and eat, and run otherwise; the land's birds choose a flying clip afresh; the grazers take
/// their own
inline AnimId ClipFor(const Animal& animal, AnimalState state)
{
	const auto type = animal.type;
	if (animals::grazers::IsGrazer(type))
	{
		return GrazerClipFor(animal, state);
	}
	if (animals::birds::IsLandBird(type))
	{
		return animals::birds::FlyingClip(type, RandomWhole);
	}
	switch (type)
	{
	case AnimalInfo::SpellDove:
		return AnimId::SpellDoveFlap;
	case AnimalInfo::SpellBat:
		return AnimId::BatFlap;
	case AnimalInfo::SpellWolf:
		switch (state)
		{
		case AnimalState::DecideWhatToDo:
			return AnimId::AWolfStand;
		case AnimalState::Pounce:
			return AnimId::AWolfPounce;
		case AnimalState::StartToEat:
			return AnimId::AWolfGotoEat;
		case AnimalState::Eat:
			return AnimId::AWolfEat;
		default:
			return AnimId::AWolfRun;
		}
	default:
		return InfoOf(type).defaultAnim;
	}
}

/// A state the animal takes, with the clip of that state: a new clip starts from its beginning
inline void SetTopState(Animal& animal, AnimalState state)
{
	animal.state = state;
	animal.turnsInState = 0;
	const auto clip = ClipFor(animal, state);
	if (animal.animation != clip)
	{
		animal.animation = clip;
		animal.clipPlace = 0;
	}
}

/// A state the animal takes keeping its clip
inline void SetState(Animal& animal, AnimalState state)
{
	animal.state = state;
	animal.turnsInState = 0;
}

inline const L3DAnim* ClipOf(AnimId clip)
{
	const auto& animations = Locator::resources::value().GetAnimations();
	const auto id = resources::HashIdentifier(static_cast<uint32_t>(clip));
	return animations.Contains(id) ? &*animations.Handle(id) : nullptr;
}

/// A clip's play time in milliseconds, 0 for none
inline uint32_t PlayTimeOf(AnimId clip)
{
	const auto* anim = ClipOf(clip);
	return anim != nullptr ? anim->GetPlayTime() : 0;
}

/// How far a clip carries its animal each play, in the model's units
inline float StrideOf(AnimId clip)
{
	const auto* anim = ClipOf(clip);
	return anim != nullptr ? anim->GetStride() : 0.0f;
}

/// An object's radius across the land: its model's widest half, across or along, times its scale
inline float RadiusOf(entt::entity entity)
{
	const auto& registry = EntityRegistry();
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == nullptr || transform == nullptr || !meshes.Contains(mesh->id))
	{
		return 0.0f;
	}
	const auto half = meshes.Handle(mesh->id)->GetBoundingBox().Size() * 0.5f;
	return std::max(half.x, half.z) * transform->scale.x;
}

/// Where the animal is in the world and the way it faces, from its move and height
inline void SyncWorld(Animal& animal)
{
	const auto xz = Metres(animal.move.position);
	animal.position = {xz.x, Ground(xz) + animal.height, xz.y};
	animal.heading = gutils::ConvertGameAngleTo3D(animal.move.angle);
}

/// Whether a point (metres) is somewhere an animal of a collision mask can't stand: off the map or on no land, or in
/// the sea or on dry land as its mask says. (The game also marks the trees and fields in a cell and the map's edge;
/// no animal's mask takes them.)
inline bool CellCollides(glm::vec2 point, uint32_t mask)
{
	if (!OnMap(point) || !Locator::terrainSystem::has_value())
	{
		return true;
	}
	const auto* cell = Locator::terrainSystem::value().FindCell(glm::u16vec2(map_coords::CellOf(point)));
	if (cell == nullptr)
	{
		return true;
	}
	const auto here = cell->properties.hasWater != 0 ? CollideType::Water : CollideType::Land;
	return (static_cast<uint32_t>(here) & mask) != 0;
}

/// The nearest town to a point (metres) within a distance, none for none
inline entt::entity NearestTown(glm::vec2 point, float reach)
{
	entt::entity nearest = entt::null;
	float best = reach;
	EntityRegistry().Each<const components::Town, const Transform>(
	    [&](entt::entity entity, const components::Town&, const Transform& transform) {
		    const float distance = gutils::GetDistanceInMetres(point, glm::vec2(transform.position.x, transform.position.z));
		    if (!(distance > best))
		    {
			    best = distance;
			    nearest = entity;
		    }
	    });
	return nearest;
}

} // namespace openblack::ecs::systems::animal_detail
