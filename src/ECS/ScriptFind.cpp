/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptFind.h"

#include <cmath>

#include <algorithm>
#include <limits>

#include "Common/GUtilsDistance.h"
#include "Common/X87.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/Weather.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using ScriptType = openblack::script::ObjectType;

namespace
{
/// The row of a kind in its table as a subtype; none for a kind with no row
template <typename Info>
std::optional<uint32_t> Row(Info info)
{
	const auto row = static_cast<int32_t>(info);
	if (row < 0)
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(row);
}

/// The birds are animals of their own type to the scripts: the crow, the dove, the swallow, the pigeon, the seagull,
/// the bat, the vulture and the miracles' doves and bats. The temple's doves and bats aren't known to be birds or animals
std::optional<ScriptType> AnimalType(AnimalInfo info)
{
	switch (info)
	{
	case AnimalInfo::Crow:
	case AnimalInfo::Dove:
	case AnimalInfo::Swallow:
	case AnimalInfo::Pigeon:
	case AnimalInfo::Seagull:
	case AnimalInfo::Bat:
	case AnimalInfo::Vulture:
	case AnimalInfo::SpellDove:
	case AnimalInfo::SpellBat:
		return ScriptType::Bird;
	case AnimalInfo::CitadelDove:
	case AnimalInfo::CitadelBat:
	case AnimalInfo::None:
	case AnimalInfo::_COUNT:
		return std::nullopt;
	default:
		return ScriptType::Animal;
	}
}

/// The tribe of a town's totem, which is the tribe of its town centre: the row of the totems' table. None when the
/// tables are not loaded or the centre has no tribe.
std::optional<uint32_t> TotemTribe(const Registry& registry, const VillageTotem& totem)
{
	const auto* centre = registry.Valid(totem.townCentre) ? registry.TryGet<const Abode>(totem.townCentre) : nullptr;
	if (centre == nullptr || centre->info == AbodeInfo::None || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& rows = Locator::infoConstants::value().abode;
	const auto row = static_cast<size_t>(centre->info);
	if (row >= rows.size())
	{
		return std::nullopt;
	}
	return Row(rows.at(row).tribeType);
}

/// A 16.16 value of a search's edge: the position in metres less or plus the reach, back in 16.16, truncated. Each
/// step rounds to a float
int32_t EdgeFixed(int32_t fixed, float offset)
{
	constexpr float k_Ten = 10.0f;
	constexpr float k_FixedOne = 65536.0f;
	constexpr float k_InverseFixedOne = 1.0f / k_FixedOne;
	const float metres = x87::Mul(static_cast<double>(fixed), k_Ten) * k_InverseFixedOne + offset;
	const float value = metres * k_FixedOne / k_Ten;
	return x87::ToInteger(value);
}
} // namespace

bool script_find::Matches(const Kind& kind, ScriptType type, uint32_t subtype)
{
	if (kind.type != type)
	{
		return false;
	}
	if (subtype == k_AnySubtype)
	{
		return true;
	}
	return kind.subtype.has_value() && *kind.subtype == subtype;
}

std::optional<script_find::Kind> script_find::KindOf(const Registry& registry, entt::entity entity)
{
	if (!registry.Valid(entity))
	{
		return std::nullopt;
	}
	if (const auto* villager = registry.TryGet<const Villager>(entity))
	{
		return Kind {.type =
		                 villager->lifeStage == Villager::LifeStage::Child ? ScriptType::VillagerChild : ScriptType::Villager,
		             .subtype = Row(RowOf(*villager, GVillagerInfo::Find))};
	}
	if (const auto* animal = registry.TryGet<const Animal>(entity))
	{
		if (const auto type = AnimalType(animal->type))
		{
			return Kind {.type = *type, .subtype = Row(animal->type)};
		}
		return std::nullopt;
	}
	if (const auto* creature = registry.TryGet<const Creature>(entity))
	{
		return Kind {.type = ScriptType::Creature, .subtype = script::property_rules::ScriptCreatureType(creature->species)};
	}
	if (const auto* vortex = registry.TryGet<const Vortex>(entity))
	{
		return Kind {.type = ScriptType::Vortex, .subtype = Row(vortex->type)};
	}
	if (registry.AllOf<Town>(entity))
	{
		return Kind {.type = ScriptType::Town};
	}
	if (registry.AllOf<Temple>(entity))
	{
		return Kind {.type = ScriptType::Citadel};
	}
	// A town's totem is a totem statue to the scripts. Its subtype is its tribe.
	if (const auto* totem = registry.TryGet<const VillageTotem>(entity))
	{
		return Kind {.type = ScriptType::TotemStatue, .subtype = TotemTribe(registry, *totem)};
	}
	// A worship site's subtype is its tribe, which is the row of the tribes' table
	if (const auto* site = registry.TryGet<const WorshipSite>(entity))
	{
		return Kind {.type = ScriptType::WorshipSite, .subtype = Row(site->tribe)};
	}
	// A scaffold has a type but no subtype: it matches only any subtype
	if (registry.AllOf<Scaffold>(entity))
	{
		return Kind {.type = ScriptType::Scaffold};
	}
	if (const auto* abode = registry.TryGet<const Abode>(entity))
	{
		return Kind {.type = ScriptType::Abode, .subtype = Row(abode->info)};
	}
	if (const auto* dispenser = registry.TryGet<const SpellDispenser>(entity))
	{
		return Kind {.type = ScriptType::SpellDispenser, .subtype = Row(dispenser->building)};
	}
	// A field is a building to the scripts; the row of its building isn't kept
	if (registry.AllOf<Field>(entity))
	{
		return Kind {.type = ScriptType::Abode};
	}
	if (const auto* feature = registry.TryGet<const Feature>(entity))
	{
		return Kind {.type = ScriptType::Feature, .subtype = Row(feature->type)};
	}
	// The animated statics and the flowers are features whose rows are in other tables: their subtype is no feature's
	if (registry.AnyOf<AnimatedStatic, Flowers>(entity))
	{
		return Kind {.type = ScriptType::Feature};
	}
	if (const auto* tree = registry.TryGet<const Tree>(entity))
	{
		return Kind {.type = ScriptType::Tree, .subtype = Row(tree->type)};
	}
	if (registry.AllOf<DeadTree>(entity))
	{
		return Kind {.type = ScriptType::DeadTree};
	}
	if (const auto* pot = registry.TryGet<const Pot>(entity))
	{
		return Kind {.type = ScriptType::Store, .subtype = Row(pot->type)};
	}
	if (const auto* still = registry.TryGet<const MobileStatic>(entity))
	{
		return Kind {.type = registry.AllOf<Rock>(entity) ? ScriptType::Rock : ScriptType::MobileStatic,
		             .subtype = Row(still->type)};
	}
	// A piece that broke off a building is a mobile static of the rock row. A search of the map cells finds the piece
	// when the piece is at rest.
	if (registry.AllOf<BuildingPiece>(entity))
	{
		return Kind {.type = ScriptType::MobileStatic, .subtype = Row(MobileStaticInfo::Rock)};
	}
	// A street lantern is a mobile static. Its subtype is its row of the statics' table, so a search for the lantern
	// row finds the lanterns of a tree puzzle.
	if (const auto* lantern = registry.TryGet<const StreetLantern>(entity))
	{
		return Kind {.type = ScriptType::MobileStatic, .subtype = Row(lantern->info)};
	}
	// A lump of poo is a thing of its own type to the scripts, with its row of the mobile objects' table as its subtype
	if (const auto* mobile = registry.TryGet<const MobileObject>(entity))
	{
		return Kind {.type = mobile->type == MobileObjectInfo::LumpOfPoo ? ScriptType::Poo : ScriptType::MobileObject,
		             .subtype = Row(mobile->type)};
	}
	if (registry.AllOf<OneOffSpellSeed>(entity))
	{
		return Kind {.type = ScriptType::MobileObject};
	}
	// A miracle in the hand, by its seed: a one-shot miracle when it came out of a globe, else a plain seed
	if (const auto* seed = registry.TryGet<const SpellSeed>(entity))
	{
		return Kind {.type = seed->fromGlobe ? ScriptType::OneShotSpell : ScriptType::SpellSeed,
		             .subtype = Row(seed->seedType)};
	}
	if (registry.AllOf<Ball>(entity))
	{
		return Kind {.type = ScriptType::Ball};
	}
	if (const auto* reward = registry.TryGet<const Reward>(entity))
	{
		return Kind {.type = ScriptType::Reward, .subtype = Row(reward->type)};
	}
	if (const auto* highlight = registry.TryGet<const ScriptHighlight>(entity))
	{
		return Kind {.type = ScriptType::Highlight, .subtype = Row(highlight->kind)};
	}
	return std::nullopt;
}

ScriptType script_find::GameTypeOf(const Registry& registry, entt::entity entity)
{
	if (const auto kind = KindOf(registry, entity))
	{
		return kind->type;
	}
	if (!registry.Valid(entity))
	{
		return ScriptType::None;
	}
	if (registry.AllOf<ScriptMarker>(entity))
	{
		return ScriptType::Marker;
	}
	if (registry.AllOf<Flock>(entity))
	{
		return ScriptType::Flock;
	}
	if (registry.AllOf<Dance>(entity))
	{
		return ScriptType::Dance;
	}
	if (registry.AllOf<InfluenceSource>(entity))
	{
		return ScriptType::InfluenceRing;
	}
	if (registry.AllOf<WeatherThing>(entity))
	{
		return ScriptType::WeatherThing;
	}
	if (registry.AllOf<ScriptTimer>(entity))
	{
		return ScriptType::Timer;
	}
	if (registry.AllOf<Mist>(entity))
	{
		return ScriptType::Mist;
	}
	return ScriptType::None;
}

entt::entity script_find::MeasuredFrom(const Registry& registry, entt::entity entity)
{
	// The game measures a worship site from the place of its altar, at the head of the site
	if (const auto* site = registry.TryGet<const WorshipSite>(entity); site != nullptr && registry.Valid(site->altar))
	{
		return site->altar;
	}
	return entity;
}

script_find::CellRange script_find::CellsAround(const map_coords::MapCoords& from, float reach)
{
	return CellRange {
	    .low = {map_coords::SignedCellOf(EdgeFixed(from.x, -reach)), map_coords::SignedCellOf(EdgeFixed(from.z, -reach))},
	    .high = {map_coords::SignedCellOf(EdgeFixed(from.x, reach)), map_coords::SignedCellOf(EdgeFixed(from.z, reach))}};
}

entt::entity script_find::FindNearest(const map_coords::MapCoords& from, float reach, const CellCandidates& inCell)
{
	const auto range = CellsAround(from, reach);
	entt::entity nearest = entt::null;
	float best = std::numeric_limits<float>::max();
	for (int x = range.low.x; x <= range.high.x; ++x)
	{
		for (int z = range.low.y; z <= range.high.y; ++z)
		{
			const glm::ivec2 cell {x, z};
			if (!map_coords::InBounds(cell))
			{
				continue;
			}
			for (const auto& candidate : inCell(cell))
			{
				const float distance = gutils::GetDistanceInMetres(from, candidate.at);
				if (distance < best)
				{
					best = distance;
					nearest = candidate.entity;
				}
			}
		}
	}
	return nearest;
}

uint32_t script_find::TownIdFromScript(float id)
{
	return static_cast<uint32_t>(x87::ToInteger(id));
}

entt::entity script_find::FindNearestTown(const map_coords::MapCoords& from, float reach, const std::vector<Candidate>& towns)
{
	entt::entity nearest = entt::null;
	float best = reach;
	for (const auto& town : towns)
	{
		const float distance = gutils::GetDistanceInMetres(from, town.at);
		if (distance < best)
		{
			best = distance;
			nearest = town.entity;
		}
	}
	return nearest;
}

entt::entity script_find::FindCreature(const map_coords::MapCoords& from, std::optional<float> radius, uint32_t subtype,
                                       std::span<const CreatureCandidate> newestFirst)
{
	const auto found = std::ranges::find_if(newestFirst, [&](const CreatureCandidate& creature) {
		if (radius.has_value() && creature.subtype != subtype)
		{
			return false;
		}
		return gutils::GetDistanceInMetres(from, creature.at) <= radius.value_or(k_CreatureAtReach);
	});
	return found != newestFirst.end() ? found->entity : entt::entity {entt::null};
}
