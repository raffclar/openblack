/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptTypes.h"

#include <cstddef>

#include <spdlog/spdlog.h>

#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/InfluenceRing.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Registry.h"
#include "ECS/Rocks.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/VillagerSpeed.h"
#include "ECS/Weather/WeatherThing.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using ScriptType = openblack::script::ObjectType;

namespace
{
/// The original's script error message is empty: it prints nothing. openblack keeps the message as a diagnostic of
/// its own (not original)
void ScriptError(const char* message)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}", message);
}

uint32_t Index(const void* record, const void* first, size_t size)
{
	return static_cast<uint32_t>((static_cast<const char*>(record) - static_cast<const char*>(first)) /
	                             static_cast<std::ptrdiff_t>(size));
}
} // namespace

ScriptType script_type::TypeOf(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (thing == entt::null || !registry.Valid(thing))
	{
		return ScriptType::None;
	}
	// The containers and the buildings first: openblack's entities carry no second "class" component that would win
	if (registry.AllOf<Temple>(thing))
	{
		return ScriptType::Citadel; // 18 (openblack's Temple is the citadel heart)
	}
	if (registry.AllOf<WorshipSite>(thing))
	{
		return ScriptType::WorshipSite;
	}
	if (registry.AllOf<Town>(thing))
	{
		return ScriptType::Town;
	}
	if (registry.AllOf<Creature>(thing))
	{
		return ScriptType::Creature;
	}
	if (registry.AllOf<Villager>(thing))
	{
		// 4, or 5 for a child
		return villager::IsChild(thing) ? ScriptType::VillagerChild : ScriptType::Villager;
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		// 21 for the Dove classes (Bat, Crow, Dove, Pigeon, Seagull, SpellBat, SpellDove, Swallow, Vulture), 6 for the
		// other animals. (inferred) the Dove classes are animal_ai::IsFlyingSpecies plus the Vulture, which it leaves out
		// (as CastRules.cpp names it). The original's animal factory has no case for the Vulture, CitadelDove and
		// CitadelBat, so none is ever made; the class of the last two is not identified (pending: taken as Animal)
		const bool dove = animal_ai::IsFlyingSpecies(animal->type) || animal->type == AnimalInfo::Vulture;
		return dove ? ScriptType::Bird : ScriptType::Animal;
	}
	if (registry.AnyOf<Field, Abode>(thing))
	{
		return ScriptType::Abode; // 2, not overridden by Field (Field : Abode)
	}
	if (registry.AnyOf<AnimatedStatic, Feature>(thing))
	{
		return ScriptType::Feature; // 3; AnimatedStatic : Feature does not override it
	}
	if (registry.AllOf<TotemStatue>(thing))
	{
		return ScriptType::TotemStatue;
	}
	if (registry.AnyOf<DeadTree, FelledTree>(thing))
	{
		return ScriptType::DeadTree; // 13 (FelledTree : DeadTree)
	}
	if (registry.AnyOf<Fragment, MagicTeleport, StreetLantern>(thing))
	{
		// Fragment takes MobileStatic's 8 (over Rock's 33), MagicTeleport has no override, a street lantern gives 8
		return ScriptType::MobileStatic;
	}
	if (const auto* statics = registry.TryGet<const MobileStatic>(thing))
	{
		// CREATE_MOBILE_STATIC: info 6 is a base-only object (no override: 0); info 8 a Bonfire (MobileStatic's 8),
		// mobileType 2 a Rock (33), else a MobileStatic (8)
		if (statics->type == MobileStaticInfo::SingingStoneBase)
		{
			return ScriptType::None;
		}
		return Rocks::IsRock(thing) ? ScriptType::Rock : ScriptType::MobileStatic;
	}
	if (registry.AnyOf<Shark, OneOffSpellSeed>(thing))
	{
		return ScriptType::MobileObject; // Whale and OneOffSpellSeed keep MobileObject's 20
	}
	if (const auto* mobile = registry.TryGet<const MobileObject>(thing))
	{
		// A Poo (25) is made for mobile object info 5; (inferred) every openblack mobile object of that info came
		// through it. Crops and creeds stay MobileObject's 20
		return mobile->type == MobileObjectInfo::LumpOfPoo ? ScriptType::Poo : ScriptType::MobileObject;
	}
	if (registry.AnyOf<Tree, MagicTree>(thing))
	{
		return ScriptType::Tree; // 22 (MagicTree : Tree)
	}
	if (registry.AllOf<Pot>(thing))
	{
		return ScriptType::Store; // 16 (PileFood, PileWood, MagicFood, MagicWood, PotStructure...)
	}
	if (const auto* seed = registry.TryGet<const SpellSeed>(thing))
	{
		// 30 for a seed from a one-shot spell, else 24
		return seed->fromOneShot ? ScriptType::OneShotSpell : ScriptType::SpellSeed;
	}
	if (registry.AllOf<SpellDispenser>(thing))
	{
		return ScriptType::SpellDispenser;
	}
	if (registry.AllOf<Mist>(thing))
	{
		return ScriptType::Mist;
	}
	if (registry.AllOf<InfluenceRing>(thing))
	{
		return ScriptType::InfluenceRing;
	}
	if (registry.AllOf<Flock>(thing))
	{
		return ScriptType::Flock;
	}
	if (registry.AllOf<WeatherThing>(thing))
	{
		return ScriptType::WeatherThing;
	}
	if (registry.AllOf<ScriptHighlight>(thing))
	{
		return ScriptType::Highlight; // 37 (ECS/ScriptHighlight.h)
	}
	// Every other class gives 0. (pending) the ScriptMarker (1) has no component to tell it by
	return ScriptType::None;
}

uint32_t script_type::SubtypeOf(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* info = Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
	switch (TypeOf(thing)) // a type outside 2..40 -> "Unknown type for search"
	{
	case ScriptType::Abode: // the abode info row (456-byte records)
		// (approximate) openblack's Abode keeps only its AbodeNumber, not the info row: fire::traits::AbodeInfo finds the
		// row by abode number and mesh (AbodeInfoOf, FireObjectTraits.cpp) and falls back to the first row with that
		// number. (pending) a Field gets k_NoSubtype: AbodeInfo reads only Abode, the original gives the field's abode
		// info index. No Land 1 intro CALL asks an ABODE sub-type other than 5000
		if (const auto* abode = fire::traits::AbodeInfo(thing); abode != nullptr && info != nullptr)
		{
			return Index(abode, info->abode.data(), sizeof(*abode));
		}
		return k_NoSubtype;
	case ScriptType::Feature: // the feature info row (292-byte records)
		if (const auto* feature = registry.TryGet<const Feature>(thing))
		{
			return static_cast<uint32_t>(feature->type);
		}
		// (approximate) an AnimatedStatic: the original divides its animated static info address by the feature info
		// array's base and size, a number no script asks for; openblack gives no sub-type
		return k_NoSubtype;
	case ScriptType::Villager: // the villager info row (932-byte records)
	case ScriptType::VillagerChild:
		// VillagerInfoOf: null for a villager without an info row (villager::InfoOf would give row 0, not original)
		if (const auto* villagerInfo = VillagerInfoOf(thing); villagerInfo != nullptr && info != nullptr)
		{
			return Index(villagerInfo, info->villager.data(), sizeof(*villagerInfo));
		}
		return k_NoSubtype;
	case ScriptType::Animal: // the animal info row (716-byte records)
	case ScriptType::Bird:
		return static_cast<uint32_t>(registry.Get<const Animal>(thing).type);
	case ScriptType::MobileStatic: // the mobile static info row (300-byte records)
	case ScriptType::Rock:
		if (const auto* statics = registry.TryGet<const MobileStatic>(thing))
		{
			return static_cast<uint32_t>(statics->type);
		}
		if (const auto* lantern = registry.TryGet<const StreetLantern>(thing))
		{
			// A street lantern is made with mobile static info 59 (country lantern) or 7 (street lantern). (approximate)
			// openblack keeps only whether it is not a 7, so any lantern that is not a 7 reads as 59: exact for Land 1,
			// where only 7 and 59 are made
			const auto row = lantern->country ? MobileStaticInfo::CountryLantern : MobileStaticInfo::StreetLantern;
			return static_cast<uint32_t>(row);
		}
		if (registry.AllOf<MagicTeleport>(thing))
		{
			return static_cast<uint32_t>(MobileStaticInfo::Teleport); // MagicTeleport: row 13
		}
		return static_cast<uint32_t>(MobileStaticInfo::Rock); // Fragment: row 2
	case ScriptType::Creature:                                // a field of the creature info
		// (pending, creature) that field is the first dword of InfoConstants' opaque field0x1e4 and openblack keeps no
		// creature info row; (inferred) it is the CREATURE_TYPE openblack stores as the species
		return static_cast<uint32_t>(registry.Get<const Creature>(thing).species);
	case ScriptType::DeadTree:
		ScriptError("Not implemented");
		return k_NoSubtype;
	case ScriptType::Store: // the pot info row (324-byte records)
		return static_cast<uint32_t>(registry.Get<const Pot>(thing).type);
	case ScriptType::WorshipSite: // the tribe info row (28-byte records)
		// (inferred) that is the site's tribe info (WorshipSite.h): the 28-byte records by tribe
		return static_cast<uint32_t>(registry.Get<const WorshipSite>(thing).tribe);
	case ScriptType::MobileObject: // the mobile object info row (276-byte records)
	case ScriptType::Poo:
	case ScriptType::Whale:
	case ScriptType::Ark:
		if (const auto* mobile = registry.TryGet<const MobileObject>(thing))
		{
			return static_cast<uint32_t>(mobile->type);
		}
		if (registry.AllOf<Shark>(thing))
		{
			return static_cast<uint32_t>(MobileObjectInfo::Whale); // Whale: row 24
		}
		return static_cast<uint32_t>(MobileObjectInfo::OneOffSpellSeed); // row 25
	case ScriptType::Tree:                                               // the tree info row (320-byte records)
		if (const auto* tree = registry.TryGet<const Tree>(thing))
		{
			return static_cast<uint32_t>(tree->type);
		}
		return k_NoSubtype;     // (pending) a MagicTree without a Tree component: openblack keeps no info row for it
	case ScriptType::Highlight: // the script highlight info row (272-byte records)
		return script_highlight::InfoIndexOf(thing);
	case ScriptType::TotemStatue: // the totem statue info row (292-byte records)
		return 0;               // (inferred) the first GTotemStatueInfo, as map_cells::TypeOf: openblack's statue keeps no row
	case ScriptType::SpellSeed: // the spell seed info row (400-byte records)
		// Both constructors store the same info twice: openblack's seedType, the spell seed info row
		return static_cast<uint32_t>(registry.Get<const SpellSeed>(thing).seedType);
	case ScriptType::Reward:         // the reward info row (304-byte records)
	case ScriptType::Vortex:         // a field of the vortex
	case ScriptType::OneShotSpell:   // the one-off seed's own field, else as SPELL_SEED
	case ScriptType::PuzzleGame:     // a PuzzleGame's TypeOf is 0: never reached
	case ScriptType::Field:          // the field info row (340-byte records) (a Field's TypeOf is 2)
	case ScriptType::SpellDispenser: // its info against the abode infos
		// (pending) openblack keeps none of these info records on the entity (or has no such thing yet)
		return k_NoSubtype;
	default: // the types with no sub-type
		ScriptError("Unknown type for search");
		return k_NoSubtype;
	}
}

bool script_type::Matches(entt::entity thing, ScriptType type, uint32_t subtype)
{
	if (TypeOf(thing) != type)
	{
		return false;
	}
	if (subtype == k_AnySubtype)
	{
		return true;
	}
	const uint32_t own = SubtypeOf(thing);
	return own != k_NoSubtype && own == subtype;
}
