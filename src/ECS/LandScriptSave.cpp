/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandScriptSave.h"

#include <cstring>

#include <array>
#include <utility>
#include <vector>

#include "3D/ObjectMatrix.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/Workshops.h"
#include "InfoConstants.h"
#include "LHScriptX/ScriptWriter.h"
#include "Locator.h"
#include "Magic/MagicTables.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// The pass the object was last saved in
struct SavedInPass
{
	uint16_t pass {0};
};

/// What this module keeps between calls (Locator::scriptState)
struct LandScriptSaveState
{
	uint16_t globalSaveCount {0};
	std::vector<std::pair<std::function<bool(entt::entity)>, land_script_save::SaveObjectFn>> writers {};
};

LandScriptSaveState& LandScriptSaveData()
{
	return openblack::Locator::scriptState::value().Get<LandScriptSaveState>();
}

/// The object's position, or its position minus at when at is given
map_coords::MapCoords WrittenPosition(entt::entity object, const map_coords::MapCoords* at)
{
	const auto coords = map_coords::FromWorld(Locator::entitiesRegistry::value().Get<const Transform>(object).position);
	return at != nullptr ? coords - *at : coords;
}

/// The duplicate pass of the mobile static and mobile object writers: every other object of the same class at exactly
/// the same map coordinates (x, z and the altitude) with the same info is marked saved, so a stack of identical objects
/// is written once (a search of that type over the cell). (approximate) the cell's own order is not the original's
/// search order; the result (all of them marked) is the same
template <typename Class>
void MarkSameObjectsSaved(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto coords = map_coords::FromWorld(registry.Get<const Transform>(object).position);
	const auto type = registry.Get<const Class>(object).type;
	map_cells::ForEachInCell(glm::ivec2(map_coords::CellOf(coords.x), map_coords::CellOf(coords.z)), [&](entt::entity other) {
		if (other != object && registry.AllOf<Class, Transform>(other) && registry.Get<const Class>(other).type == type &&
		    map_coords::FromWorld(registry.Get<const Transform>(other).position) == coords)
		{
			static_cast<void>(land_script_save::CheckAndSetSaved(other));
		}
		return true;
	});
}
} // namespace

void land_script_save::BeginSavePass()
{
	// a new pass: saving the whole map and creating the vortex start one
	++LandScriptSaveData().globalSaveCount;
}

bool land_script_save::CheckAndSetSaved(entt::entity object)
{
	auto& state = LandScriptSaveData();
	// already saved in this pass -> false; else marked with it, true
	auto& registry = Locator::entitiesRegistry::value();
	auto& saved =
	    registry.AllOf<SavedInPass>(object) ? registry.Get<SavedInPass>(object) : registry.Assign<SavedInPass>(object);
	if (saved.pass == state.globalSaveCount)
	{
		return false;
	}
	saved.pass = state.globalSaveCount;
	return true;
}

void land_script_save::Register(std::function<bool(entt::entity)> matches, SaveObjectFn fn)
{
	LandScriptSaveData().writers.emplace_back(std::move(matches), std::move(fn));
}

std::optional<std::string> land_script_save::SaveObject(entt::entity object, const map_coords::MapCoords* at)
{
	for (const auto& [matches, fn] : LandScriptSaveData().writers)
	{
		if (matches(object))
		{
			return fn(object, at);
		}
	}
	return std::nullopt; // the base class has no writer
}

std::optional<std::string> land_script_save::SaveMobileStatic(entt::entity object, const map_coords::MapCoords* at)
{
	// already saved in this pass -> nothing
	if (!CheckAndSetSaved(object))
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto pos = WrittenPosition(object, at);
	// (pending) an object it belongs to set -> nothing written; openblack has no counterpart
	const auto& transform = registry.Get<const Transform>(object);
	// the scale and the Z, Y and X angles, each as a double. (approximate) openblack keeps no angles: DecomposeYXZ of the
	// rotation, equal to them to rounding and wrapped (4 rad comes out -2.28), where the original writes the stored
	// angles as they are
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	affine::DecomposeYXZ(transform.rotation, y, x, z);
	// the info's index, the altitude as the vertical offset. A Fragment (this writer too) has the info every fragment
	// gets: row 2, Rock: written as a rock, it comes back as one
	const auto* mobileStatic = registry.TryGet<const MobileStatic>(object);
	const auto type = mobileStatic != nullptr ? mobileStatic->type : MobileStaticInfo::Rock;
	const auto line = lhscriptx::WriteCommand("CREATE_MOBILE_STATIC",
	                                          {pos, static_cast<int32_t>(type), pos.altitude, x, y, z, transform.scale.x});
	// (pending) a town artifact writes its own line
	if (mobileStatic != nullptr)
	{
		// (approximate) a fragment's twins (other fragments: OBJECT_TYPE 28 with the Rock info) are not looked for:
		// openblack's fragments have no MobileStatic component; two fragments at exactly the same spot are not expected
		MarkSameObjectsSaved<MobileStatic>(object);
	}
	return line.value_or(std::string());
}

std::optional<std::string> land_script_save::SaveMobileObject(entt::entity object, const map_coords::MapCoords* at)
{
	if (!CheckAndSetSaved(object))
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto pos = WrittenPosition(object, at);
	// (pending) an object holding it set -> nothing written; openblack has no counterpart
	const auto& transform = registry.Get<const Transform>(object);
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	affine::DecomposeYXZ(transform.rotation, y, x, z); // (approximate) the Y angle from the rotation
	// Y angle x 1000 and scale x 1000, both truncated toward zero, the info's index
	const auto line = lhscriptx::WriteCommand("CREATE_MOBILEOBJECT",
	                                          {pos, static_cast<int32_t>(registry.Get<const MobileObject>(object).type),
	                                           map_coords::FtoL(y * 1000.0f), map_coords::FtoL(transform.scale.x * 1000.0f)});
	// the twin search filters on the info's object type: MOBILE_OBJECT 20 only, so a Ball (type 15, not ported) never
	// marks its twins; every openblack MobileObject is of type 20
	MarkSameObjectsSaved<MobileObject>(object);
	return line.value_or(std::string());
}

std::optional<std::string> land_script_save::SavePot(entt::entity object, const map_coords::MapCoords* at)
{
	if (!CheckAndSetSaved(object))
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto pos = WrittenPosition(object, at);
	// part of a structure -> nothing written. A plain pot never is; a structure's pot is when its structure is
	// available: a storage pit's pile (StoragePitStore::OwnerOf) or a workshop's (workshops::WorkshopOfPile). (pending) a
	// building site's pile is linked through the site (building_sites::SiteOfPile): whether its structure is set too is
	// not read. (pending, minor) the original also clears the structure when its owner is not available
	const auto pit = StoragePitStore::OwnerOf(object);
	const auto workshop = workshops::WorkshopOfPile(object);
	if ((pit != entt::null && ecs::IsAvailable(pit)) || (workshop != entt::null && ecs::IsAvailable(workshop)))
	{
		return std::string();
	}
	const auto& pot = registry.Get<const Pot>(object);
	// the info's index, the resource type, the amount
	return lhscriptx::WriteCommand("CREATE_POT", {pos, static_cast<int32_t>(pot.type),
	                                              static_cast<int32_t>(object_resources::GetResourceType(object)),
	                                              static_cast<int32_t>(pot.amount)})
	    .value_or(std::string());
}

std::optional<std::string> land_script_save::SaveDeadTree(entt::entity object, const map_coords::MapCoords* at)
{
	if (!CheckAndSetSaved(object))
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto pos = WrittenPosition(object, at);
	// the tree's player, else the local player, by name. (pending) openblack's DeadTree has no player and no
	// local-player lookup here: PLAYER_ONE, the local player of a single-player game
	constexpr std::array<const char*, 8> k_PlayerNames = {"PLAYER_ONE",  "PLAYER_TWO", "PLAYER_THREE", "PLAYER_FOUR",
	                                                      "PLAYER_FIVE", "PLAYER_SIX", "PLAYER_SEVEN", "NEUTRAL"};
	const std::string player = k_PlayerNames[0];
	// the Z, Y and X angles and the life, each a double; (approximate) the angles from DecomposeYXZ of the rotation (openblack
	// keeps no angles). Then the tree info's index
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	affine::DecomposeYXZ(registry.Get<const Transform>(object).rotation, y, x, z);
	const auto line = lhscriptx::WriteCommand(
	    "CREATE_DEAD_TREE",
	    {pos, player, static_cast<int32_t>(registry.Get<const DeadTree>(object).type), life::LifeOf(object), x, y, z});
	return line.value_or(std::string());
}

std::optional<std::string> land_script_save::SaveOneOffSpellSeed(entt::entity object, const map_coords::MapCoords* at)
{
	if (!CheckAndSetSaved(object))
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto pos = WrittenPosition(object, at);
	// the seed's magic info at its power-up level (the base one when that level has no magic type), then the debug
	// string of that info's magic effect, as the script's info-from-text lookup reads it
	const auto& seed = registry.Get<const OneOffSpellSeed>(object);
	const auto& tables = Locator::infoConstants::value();
	const auto& seedInfo = magic::GetSpellSeedInfo(tables, seed.seedType);
	const auto& record = magic::MagicInfoForPowerUpLevel(tables, seedInfo, seed.powerUp);
	const auto& text = magic::GetMagicEffectInfo(tables, record.magicType).debugString;
	const std::string name(text.data(), strnlen(text.data(), text.size()));
	return lhscriptx::WriteCommand("CREATE_ONE_SHOT_SPELL_PU", {pos, name}).value_or(std::string());
}

void land_script_save::RegisterPhysicsWriters()
{
	const auto has = [](auto tag) {
		return [](entt::entity e) { return Locator::entitiesRegistry::value().AllOf<decltype(tag)>(e); };
	};
	Register(has(Pot {}), SavePot);
	// a Scaffold (a MobileObject, Scaffolds.cpp) has its own writer, and so has a Bonfire (a MobileStatic of info
	// Bonfire, BonfireArchetype.cpp): not these
	Register(
	    [](entt::entity e) {
		    const auto& registry = Locator::entitiesRegistry::value();
		    return registry.AllOf<MobileObject>(e) && !registry.AllOf<Scaffold>(e);
	    },
	    SaveMobileObject);
	Register(
	    [](entt::entity e) {
		    const auto& registry = Locator::entitiesRegistry::value();
		    return registry.AllOf<MobileStatic>(e) && registry.Get<const MobileStatic>(e).type != MobileStaticInfo::Bonfire;
	    },
	    SaveMobileStatic);
	Register(has(Fragment {}), SaveMobileStatic); // a Fragment is written as a mobile static
	Register(has(DeadTree {}), SaveDeadTree);
	Register(has(OneOffSpellSeed {}), SaveOneOffSpellSeed);
}
