/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "PotResource.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <chrono>
#include <vector>

#include <LNDFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "Audio/Services/Guidance.h"
#include "Common/GUtilsDistance.h"
#include "Debug/DebugEnv.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Objects/MagicPiles.h"
#include "Particles/PSysManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
bool Trace()
{
	static const bool trace = debug_env::SpellTrace() || debug_env::HandTrace();
	return trace;
}

/// The MapCoords of a point (x * 6553.6, truncated); its high words are the 10 m cells
glm::ivec2 MapCoordsOf(const glm::vec3& position)
{
	return {map_coords::ToFixed(position.x), map_coords::ToFixed(position.z)};
}

/// The cell, or none out of the 512 x 512 map
std::optional<glm::ivec2> CellOf(glm::ivec2 coords)
{
	const map_coords::MapCoords at {coords.x, coords.y, 0.0f};
	const uint16_t side = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetCellsPerSide() : 512;
	if (!map_coords::InBounds(at, side))
	{
		return std::nullopt;
	}
	return map_coords::Cell(at);
}

const lnd::LNDCell* LandCellOf(const glm::vec3& position)
{
	if (!Locator::terrainSystem::has_value())
	{
		return nullptr;
	}
	const auto cell = CellOf(MapCoordsOf(position));
	if (!cell || position.x < 0.0f || position.z < 0.0f)
	{
		return nullptr;
	}
	return &Locator::terrainSystem::value().GetCell(glm::u16vec2(*cell));
}

ResourceType ResourceOf(const Pot& pot)
{
	if (pot.type == PotInfo::_COUNT || !Locator::infoConstants::has_value())
	{
		return ResourceType::None;
	}
	return Locator::infoConstants::value().pot.at(static_cast<size_t>(pot.type)).resourceType;
}

/// What one object in the cell lists does with the resource: a store of that type, or a pot of that type, within
/// Get2DRadius x the multiplier of pos (distance in metres <= r), takes it. A storage pit, with an interface, then tells
/// the creature what was done, whatever it took (a pot or a pile tells nothing). Returns what it took.
uint32_t OfferTo(entt::entity object, const glm::vec3& position, ResourceType type, uint32_t left, bool poisoned,
                 const pot_resource::Dropper& dropper)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(object);
	if (transform == nullptr)
	{
		return 0;
	}
	const bool store = registry.AllOf<StoragePit>(object);
	auto* pot = registry.TryGet<Pot>(object);
	const auto owner = pot != nullptr ? StoragePitStore::OwnerOf(object) : entt::null;
	if (!store)
	{
		// a storage pit stores any type; a pot of a structure asks the structure it is part of (and the pile's own
		// type); a loose pot is no store, then it must be a pot of the type
		if (pot == nullptr || ResourceOf(*pot) != type)
		{
			return 0;
		}
	}
	// the fire centre: the object's position
	const float radius = pot_resource::Get2DRadius(object) * pot_resource::RadiusMultiplierForApplyingPotToPos(object);
	// within the radius, inclusive
	const float distance = gutils::GetDistanceInMetres(position, transform->position);
	if (!(distance <= radius))
	{
		return 0;
	}
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Pot trace: {} of {} into {} {} (at {:.2f} m, radius {:.2f})", left,
		                   type == ResourceType::Wood ? "wood" : "food", store || owner != entt::null ? "the store of" : "pile",
		                   static_cast<uint32_t>(store ? object : (owner != entt::null ? owner : object)), distance, radius);
	}
	if (store)
	{
		return pot_resource::StoragePitTakesPutDownResource(object, type, left, dropper, poisoned);
	}
	return pot_resource::PotStructureAddResource(object, type, left, poisoned, dropper);
}

/// A cell's lists in the order AddResourceToPos walks them (the fixed list, then the mobile one, each from its head;
/// ecs::map_cells), the storage pits and the pots / piles among them. The pots (type 21) are at the tail of the fixed
/// list. In the original every new pile is there at once; here only the ones made with an InsertMapObject hook
/// (CreateMagicResourcePile: MagicFood / MagicWood, the storm's piles) are, the others (PotArchetype::Create from the
/// villagers or the hand) wait for the next map_cells::Sync (approximate, their owners add the hook)
std::vector<entt::entity> CellObjects(glm::ivec2 cell)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto objects = map_cells::ObjectsInCell(cell);
	std::erase_if(objects, [&registry](entt::entity entity) { return !registry.AnyOf<StoragePit, Pot>(entity); });
	return objects;
}
} // namespace

float pot_resource::PileFoodProportionRaised(uint32_t amount, uint32_t maxInPot)
{
	return object::PileFoodProportionRaised(amount, maxInPot);
}

float pot_resource::Get2DRadius(entt::entity object)
{
	return object::Get2DRadius(object);
}

float pot_resource::RadiusMultiplierForApplyingPotToPos(entt::entity object)
{
	return Locator::entitiesRegistry::value().AllOf<Pot>(object) ? 2.0f : 1.2f;
}

bool pot_resource::IsWater(const glm::vec3& position)
{
	// out of the 512 x 512 cells or without a land block the answer is true, the open sea. The single source is
	// ecs::sea_cells (no cell or no block = water, else the cell's water bit).
	return sea_cells::IsWater(position);
}

bool pot_resource::IsDryLand(const glm::vec3& position)
{
	const auto* cell = LandCellOf(position);
	return cell != nullptr && cell->altitude >= 4;
}

int pot_resource::PileSoundSample(ResourceType type, uint32_t amount, uint32_t t)
{
	if (amount < 200)
	{
		return type == ResourceType::Food ? 77 + static_cast<int>(t % 6) : 92 + static_cast<int>(t % 6);
	}
	return type == ResourceType::Food ? 75 + static_cast<int>(t & 1) : 86 + static_cast<int>(t % 6);
}

void pot_resource::PlayPileSound(entt::entity pile, const glm::vec3& position, ResourceType type, uint32_t amount)
{
	// the tick count picks the sample (the original reads it once for the food sample and once for the wood one; a
	// single reading here), then the sound with bank InGame, owner the pile, 3D, not tracked, at the MapCoords' point
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), PileSoundSample(type, amount, audio::TickCount())};
	options.owner = audio::Owner::Thing(pile);
	options.is3D = true;
	options.track = false;
	options.position = position;
	audio::PlaySoundEffect(options);
}

void pot_resource::SetSpeedUp(entt::entity pile, bool on)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* pot = registry.TryGet<Pot>(pile);
	if (pot == nullptr)
	{
		return;
	}
	const bool was = pot->speedUp;
	pot->speedUp = on;
	const bool food = pot->type != PotInfo::_COUNT &&
	                  Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type)).potType == PotType::PileFood;
	// other pots only keep the flag; a food pile also shows it
	if (!food || on == was)
	{
		return;
	}
	if (pot->speedUpVisual != entt::null && registry.Valid(pot->speedUpVisual))
	{
		registry.Destroy(pot->speedUpVisual); // the particle container closes down
	}
	pot->speedUpVisual = entt::null;
	if (on)
	{
		// the PILEFOOD_SPEEDUP spot visual (46) for ever, on the pile (the 1.0 is taken as the scale, the -1 as the
		// duration: infinite)
		const auto position = registry.Get<const Transform>(pile).position;
		pot->speedUpVisual = psys::manager::CreateSpotVisual(46, position, -1.0f, pile);
	}
}

uint32_t pot_resource::PotStructureAddResource(entt::entity object, ResourceType type, uint32_t amount, bool poisoned,
                                               const Dropper& dropper)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* pot = registry.TryGet<Pot>(object);
	const auto* transform = registry.TryGet<const Transform>(object);
	if (pot == nullptr || transform == nullptr)
	{
		return 0;
	}
	const auto owner = StoragePitStore::OwnerOf(object);
	if (owner != entt::null)
	{
		// the pot of a pit -> the store. Before anything else, if the pit has a building site and the type is WOOD (1)
		// or ANY (-2), the whole call goes to the building site and its answer is returned: a pit that is being built
		// sends its wood to its building site. (pending) openblack has no building sites and nothing builds abodes
		// (ECS/Components/Town.h), so no pit can ever have one and the redirect is unreachable; port it with the
		// building sites. The interface that put it down and the poison go on to the pit
		return StoragePitStore::AddResource(owner, type, amount, dropper, poisoned);
	}
	// any other pot or pile takes it itself
	return pot_resource::AddToPotDirect(object, type, amount, poisoned);
}

uint32_t pot_resource::StoragePitTakesPutDownResource(entt::entity store, ResourceType type, uint32_t amount,
                                                      const Dropper& dropper, bool poisoned)
{
	// the pit's AddResource with the interface that put it down and the poison: with a town, its desire, alignment,
	// belief and deed
	const uint32_t taken = StoragePitStore::AddResource(store, type, amount, dropper, poisoned);
	// then the put-down's own deed, whatever was taken
	if (dropper.hasInterface)
	{
		[[maybe_unused]] const auto deed = StoragePitStore::DoCreatureMimicAfterAddingResource(store, type, dropper);
	}
	return taken;
}

uint32_t pot_resource::AddToPotDirect(entt::entity object, ResourceType type, uint32_t amount, bool poisoned)
{
	// a pile plays the pile sound with the amount asked (not for the hand's pots, infos 11 and 12); the amount is
	// clipped at maxInPot only when nextPotForResource < 19 (no magic or loose pile), then poisoned if either was, and
	// the size
	auto& registry = Locator::entitiesRegistry::value();
	auto* pot = registry.TryGet<Pot>(object);
	const auto* transform = registry.TryGet<const Transform>(object);
	if (pot == nullptr || transform == nullptr)
	{
		return 0;
	}
	const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type));
	if (info.potType != PotType::Pot && pot->type != PotInfo::HandWood && pot->type != PotInfo::HandFood)
	{
		pot_resource::PlayPileSound(object, transform->position, type, amount);
	}
	uint32_t add = amount;
	if (static_cast<int32_t>(info.nextPotForResource) < 19 && pot->amount + add > info.maxAmountInPot)
	{
		add = info.maxAmountInPot > pot->amount ? info.maxAmountInPot - pot->amount : 0u;
	}
	add = std::min<uint32_t>(add, 65535u - pot->amount); // openblack's guard: Pot::amount is a uint16 here
	pot->amount = static_cast<uint16_t>(pot->amount + add);
	pot->poisoned = poisoned || pot->poisoned;
	archetypes::PotArchetype::SetSize(object, true);
	return add;
}

uint32_t pot_resource::AddResourceToPos(const glm::vec3& position, const Dropper& dropper, ResourceType type, uint32_t amount,
                                        bool poisoned, bool speedUp, entt::entity* newPile)
{
	if (newPile != nullptr)
	{
		*newPile = entt::null;
	}
	if (!Locator::entitiesRegistry::has_value() || !Locator::infoConstants::has_value() || position.x < 0.0f ||
	    position.z < 0.0f)
	{
		return 0;
	}
	auto coords = MapCoordsOf(position);
	if (!CellOf(coords))
	{
		return 0; // out of bounds
	}
	uint32_t left = amount;
	map_coords::Spiral spiral;
	for (int i = 0; i < 9; ++i)
	{
		if (const auto cell = CellOf(coords); cell)
		{
			for (const auto object : CellObjects(*cell))
			{
				if (left == 0)
				{
					break;
				}
				const uint32_t taken = OfferTo(object, position, type, left, poisoned, dropper);
				left -= std::min(taken, left);
			}
		}
		// one cell along the spiral
		const auto& step = spiral.Next();
		coords += glm::ivec2(step.x, step.z) * map_coords::k_FixedPerCell;
	}
	if (left == 0 || IsWater(position))
	{
		return amount - left;
	}
	const auto player = dropper.hasInterface ? std::optional(dropper.player) : std::nullopt;
	const auto pile = magic::objects::CreateMagicResourcePile(position, player, type, left);
	if (pile == entt::null)
	{
		return amount - left;
	}
	if (newPile != nullptr)
	{
		*newPile = pile;
	}
	auto& registry = Locator::entitiesRegistry::value();
	PlayPileSound(pile, registry.Get<const Transform>(pile).position, type, left);
	auto& pot = registry.Get<Pot>(pile);
	pot.poisoned = poisoned || pot.poisoned;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Pot trace: new {} pile {} of {} at ({:.1f}, {:.1f}), 2D radius {:.2f}",
		                   type == ResourceType::Wood ? "MagicWood" : "MagicFood", static_cast<uint32_t>(pile), left,
		                   position.x, position.z, Get2DRadius(pile));
	}
	SetSpeedUp(pile, speedUp || pot.speedUp);
	// the guidance resource drop sound only for the local player's interface (`dropper.isMyInterface`), with the drop
	// point and a rain type: 1 for food (type 0) and 2 for wood (type 1), 0 for anything else. The guidance then picks
	// a sample by the needs of the nearest town within 100 m (Audio/Guidance.*)
	if (dropper.isMyInterface)
	{
		const auto rain = type == ResourceType::Wood   ? audio::guidance::RainType::Wood
		                  : type == ResourceType::Food ? audio::guidance::RainType::Food
		                                               : audio::guidance::RainType::None;
		audio::guidance::PlayResourceDropRemark(position, rain);
	}
	return amount - left; // amount - left on every path, so the new pile's part is not counted
}
