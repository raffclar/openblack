/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Abodes.h"

#include <algorithm>

#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Common/GUtilsDistance.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/StreetLanternArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/DrawMesh.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fields.h"
#include "ECS/Footpaths.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/Buildings.h"
#include "ECS/Physics/CollisionSounds.h"
#include "ECS/Physics/PartialBuild.h"
#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/Rocks.h"
#include "ECS/Scaffolds.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/Graveyard.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownEmergency.h"
#include "ECS/Town/TownPlacement.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/TownStores.h"
#include "ECS/Town/Wonders.h"
#include "ECS/Town/Workshops.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerEmergency.h"
#include "Game/GameStats.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/TownBelief.h"
#include "Resources/ResourcesInterface.h"
#include "Worship/Citadel.h"
#include "Worship/TownCentreSpellIcon.h"
#include "Worship/WorshipPercentage.h"
#include "Worship/WorshipSite.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

std::optional<AbodeType> abodes::TypeOf(entt::entity abode)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.TryGet<const Abode>(abode);
	if (component == nullptr)
	{
		// The citadel heart and the worship sites are of the citadel type
		if (registry.AllOf<CitadelPartBuild>(abode))
		{
			return AbodeType::Citadel;
		}
		return std::nullopt;
	}
	// The type comes from the info record the abode was made with. openblack keeps the abode number (AbodeArchetype),
	// so the record is looked up by it and by the mesh, as influence::AbodeInfoOf does; (inferred) every tribe's record
	// of one abode number carries the same ABODE_TYPE bits. The record AbodeArchetype kept comes first
	if (const auto i = static_cast<size_t>(static_cast<int32_t>(component->info));
	    component->info != AbodeInfo::None && i < Locator::infoConstants::value().abode.size())
	{
		return Locator::infoConstants::value().abode.at(i).abodeType;
	}
	const auto* mesh = registry.TryGet<const Mesh>(abode);
	const auto meshId = mesh != nullptr ? mesh->id : 0;
	std::optional<AbodeType> byNumber;
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if (info.abodeNumber != component->type)
		{
			continue;
		}
		if (town_stats::MeshIdHash(info.meshId) == meshId)
		{
			return info.abodeType;
		}
		if (!byNumber.has_value())
		{
			byNumber = info.abodeType;
		}
	}
	return byNumber;
}

bool abodes::InterfaceValidToTap(entt::entity abode)
{
	// always true for an abode
	return Locator::entitiesRegistry::value().AllOf<Abode>(abode);
}

void abodes::InterfaceTap(entt::entity abode, const glm::vec3& handPosition, bool local, glm::vec3 handTransformPos)
{
	// (not ported) remembering the town and counting the knock. For any abode, before the type test, each inhabitant
	// reacts to the tap (VillagerEmergency.h). (openblack) a copy of the list; the original walks it live, and the
	// reaction does not change it
	const std::vector<entt::entity> inhabitants = Locator::entitiesRegistry::value().Get<Abode>(abode).inhabitants;
	for (const auto v : inhabitants)
	{
		villager::SetStateWhenTappedOnAbode(v);
	}
	// only an abode whose ABODE_TYPE has the living-quarters bit knocks; the houses A..F and the windmill have it, the
	// civic buildings (totem, storage pit, creche, workshop, wonder, graveyard, town centre, football pitch, spell
	// dispenser, field) do not.
	const auto type = TypeOf(abode);
	if (!type.has_value() || (static_cast<uint32_t>(*type) & static_cast<uint32_t>(AbodeType::LivingQuarters)) == 0)
	{
		return;
	}
	// the local player's hand knocks on the roof (the Ctap_house animation), before the sound
	if (local && Locator::handSystem::has_value())
	{
		Locator::handSystem::value().StartFixedPosAnimation("Ctap_house", handTransformPos);
	}
	// the knock sound: bank InGame, sample 110 G_KnockRoofMulti plus a counter (0..8 in turn), owned by the abode, 3D,
	// not tracked, at the hand's point; mode and loops stay the defaults (3 and 0), and the .sad gives the sample 5 %
	// pitch spread and min / max 100 / 150.
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), 110 + audio::NextCounter(audio::Counter::KnockRoof)};
	options.owner = audio::Owner::Thing(abode);
	options.is3D = true;
	options.track = false;
	options.position = handPosition;
	audio::PlaySoundEffect(options);
}

// ---- life and damage (docs/bw1-notes/buildings.md) -------------------------------------------------------------

namespace
{
/// The repair base is 1.1 x life - 0.1
constexpr float k_RepairBaseScale = 1.1f;
constexpr float k_RepairBaseOffset = 0.1f;
/// The info.dat effect 3 used for physical destruction: crush 1, alignment modification 1, radius 1
constexpr size_t k_PhysicalDestructionEffect = 3;
/// The abode age from which the player's statistic counts
constexpr uint8_t k_StatisticAge = 200;

/// The player of the hitter, as the effect's player when it has an applier (taken before the caused player). The
/// hitters of a physics impact are mobile statics: a rock has none ((pending) nothing sets it on the hand's path, null
/// here), any other mobile static its owner's player ((not ported) null) else the neutral player. Also a villager (its
/// own player) and a tree (none)
std::optional<PlayerNames> HitterPlayer(entt::entity hitter)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (hitter == entt::null || !registry.Valid(hitter) || registry.AllOf<Tree>(hitter) || Rocks::IsRock(hitter))
	{
		return std::nullopt;
	}
	if (registry.AllOf<Villager>(hitter))
	{
		return villager::GetPlayerOf(hitter);
	}
	return PlayerNames::NEUTRAL;
}
} // namespace

void abodes::StopBeingFunctional(entt::entity building, std::optional<PlayerNames> player)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto logger = spdlog::get("game");
	// with a player and an abode at least 200 old, the player's statistics count it (the multiplayer part is skipped).
	// Nothing else: the villagers stay, no site
	const auto* a = registry.TryGet<const Abode>(building);
	if (player.has_value() && a != nullptr && a->field0xB9 >= k_StatisticAge)
	{
		game_stats::BuildingStoppedFunctional(static_cast<size_t>(*player));
	}
	if (logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Buildings: {} no longer works", static_cast<uint32_t>(building));
	}
	if (const auto* pit = registry.TryGet<const StoragePit>(building); pit != nullptr)
	{
		// a storage pit, after the base: the food pile when it holds food sets up its pot reaction; then the five wood
		// piles when they hold wood
		const StoragePit piles = *pit;
		const auto setup = [&registry](entt::entity pile, ResourceType type) {
			// (openblack, guard) a pile that is gone
			if (pile != entt::null && registry.Valid(pile) && object_resources::PotAmount(pile, type) != 0)
			{
				animal_ai::SetupPotReaction(pile);
			}
		};
		setup(piles.foodPile, ResourceType::Food);
		for (const auto pile : piles.woodPiles)
		{
			setup(pile, ResourceType::Wood);
		}
	}
	else if (TypeOf(building) == AbodeType::TownCentre)
	{
		// a town centre, after the base: its town's worship percentage goes to 0
		if (const auto town = abode_villagers::TownOf(building); town != entt::null)
		{
			worship::percentage::SetWorshipPercentage(town, 0.0f);
		}
	}
}

void abodes::DestroyedByEffect(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	// (pending) the fading ghost of the mesh; graphics::frame_anim::GoolooFrame exists
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Buildings: {} destroyed", static_cast<uint32_t>(building));
	}
	// (not ported) the script abode's branch. Else: the site's head scaffold when it has any; the player is the town's
	const auto& onSite = building_sites::ScaffoldsOf(GetBuildingSite(building));
	if (const auto scaffold = onSite.empty() ? entt::null : onSite.front(); scaffold != entt::null)
	{
		std::optional<PlayerNames> player;
		if (const auto* t = registry.TryGet<const Town>(abode_villagers::TownOf(building)); t != nullptr)
		{
			player = t->owner;
		}
		// the scaffold leaves the old site (THIS building ToBeDeleted(0), fresh plans), then force-builds the building
		// again (a new site at its position, no fit check). The other scaffolds go with the old site's ToBeDeleted
		scaffolds::RemoveOldBuildingSite(scaffold);
		scaffolds::ForceBuildBuilding(scaffold, player);
		// still available -> ToBeDeleted(0) (normally already marked by RemoveOldBuildingSite)
		if (ecs::IsAvailable(building))
		{
			ToBeDeleted(building);
		}
		return;
	}
	// still available -> ToBeDeleted(0). A second hit on a building already marked does nothing
	if (ecs::IsAvailable(building))
	{
		ToBeDeleted(building);
	}
}

namespace
{
/// The food pile and the five wood piles
constexpr size_t k_PitPiles = 6;

/// The first other functional abode of the town's list (town_stats::AbodesOf) whose ABODE_TYPE has one of `bits`, as
/// the storage pit and the creche search it on deletion: the bits, then functional, then not this one. Literal: the
/// Civic bit 2 is in both masks, so any functional civic building passes. The order: a new abode is linked at the head
/// of the town's list, so the newest abode comes first, AbodesOf's order (the creation index, descending)
entt::entity FirstOtherFunctional(entt::entity town, entt::entity self, uint32_t bits)
{
	for (const auto abode : town_stats::AbodesOf(town))
	{
		const auto type = abodes::TypeOf(abode);
		const auto have = type.has_value() ? static_cast<uint32_t>(*type) : 0u;
		if ((have & bits) != 0 && abode_queries::IsFunctional(abode) && abode != self)
		{
			return abode;
		}
	}
	return entt::null;
}

/// The storage pit's own part of DeleteDependants (before the abode part): when the town's storage pit is this one,
/// the first other functional abode with ABODE_TYPE bit 2 or 5 becomes it if it is a storage pit (else none)
void StoragePitHandOn(entt::entity pit)
{
	const auto town = abode_villagers::TownOf(pit);
	if (town == entt::null || town_queries::GetStoragePit(town) != pit)
	{
		return;
	}
	const auto found = FirstOtherFunctional(town, pit, 0x24u);
	const bool isPit = found != entt::null && Locator::entitiesRegistry::value().AllOf<StoragePit>(found);
	town_stores::SetStoragePit(town, isPit ? found : entt::null);
}

/// The creche's own part of DeleteDependants (before the abode part): with a town, found = its creche is this one ?
/// the first other functional abode with ABODE_TYPE bit 2 or 6 : none; then the town's creche = found, unconditionally
void CrecheHandOn(entt::entity creche)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = abode_villagers::TownOf(creche);
	if (town == entt::null || !registry.AllOf<Town>(town))
	{
		return;
	}
	const auto found = registry.Get<const Town>(town).creche == creche ? FirstOtherFunctional(town, creche, 0x44u) : entt::null;
	registry.Get<Town>(town).creche = found;
}

/// The town centre's own part of DeleteDependants (the abode part follows)
void TownCentreDeleteDependants(entt::entity centre)
{
	auto& registry = Locator::entitiesRegistry::value();
	// no town -> only the icons
	const auto town = abode_villagers::TownOf(centre);
	if (auto* t = town != entt::null ? registry.TryGet<Town>(town) : nullptr; t != nullptr)
	{
		// when the town's centre is this one: the first in the town's list (the newest first) whose type equals
		// TownCentre (an equality, not FirstOtherFunctional's bit test), functional and not this one; the town's centre =
		// that (none too when it was another centre: literal)
		entt::entity next = entt::null;
		if (t->centre == centre)
		{
			for (const auto other : town_stats::AbodesOf(town))
			{
				if (other != centre && abodes::TypeOf(other) == AbodeType::TownCentre && abode_queries::IsFunctional(other))
				{
					next = other;
					break;
				}
			}
		}
		t->centre = next;
	}
	// the six spell icons
	worship::town_centre::DeleteDependants(centre);
}

/// DeleteDependants of the abode's class: a field (fields::DeleteDependants; no abode part); the storage pit,
/// graveyard, workshop, wonder and creche (their part, then the abode part); the town centre; the abode part removes
/// all its villagers (each leaves the abode for the town's homeless list, 129 HOMELESS_START)
void ClassDeleteDependants(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<components::Field>(building))
	{
		fields::DeleteDependants(building);
		return;
	}
	if (registry.AllOf<StoragePit>(building))
	{
		StoragePitHandOn(building);
	}
	else if (registry.AllOf<components::Graveyard>(building))
	{
		graveyard::DeleteDependants(building);
	}
	else if (workshops::IsWorkshop(building))
	{
		workshops::DeleteDependants(building);
	}
	else if (wonders::IsWonder(building))
	{
		wonders::DeleteDependants(building);
	}
	else if (abodes::TypeOf(building) == AbodeType::Creche)
	{
		CrecheHandOn(building);
	}
	else if (abodes::TypeOf(building) == AbodeType::TownCentre)
	{
		TownCentreDeleteDependants(building);
	}
	abode_villagers::RemoveAllVillagersFromAbode(building);
}

/// The storage pit's part of ToBeDeleted, before the abode part: DeleteDependants, then the food pile and the five
/// wood piles, each one available: unlinked from the pit, its ToBeDeleted(now), the slot cleared. An unavailable one
/// keeps its slot (literal)
void StoragePitToBeDeleted(entt::entity pit, bool now)
{
	ClassDeleteDependants(pit);
	auto& registry = Locator::entitiesRegistry::value();
	for (size_t slot = 0; slot < k_PitPiles; ++slot)
	{
		auto* p = registry.TryGet<StoragePit>(pit);
		if (p == nullptr)
		{
			return;
		}
		auto& entry = slot == 0 ? p->foodPile : p->woodPiles.at(slot - 1);
		const auto pile = entry;
		if (pile == entt::null || !ecs::IsAvailable(pile))
		{
			continue;
		}
		// unlinking: (approximate) the pile's structure is this slot (StoragePitStore::OwnerOf), so the slot is cleared
		// first instead of after the ToBeDeleted
		entry = entt::null;
		// the pile's ToBeDeleted: a food pile closes its speed-up visual (nothing for a wood pile); both remove the pot
		// reaction, then the common deletion
		pot_resource::SetSpeedUp(pile, false);
		animal_ai::RemovePotReaction(pile);
		ecs::ToBeDeleted(pile, now);
	}
}

/// Taking a building out of its town, the part openblack keeps: the abode answers no town and leaves the town's list.
/// openblack's town lists are the abodes with the town's id (town_stats::AbodesOf, town_queries::TownAbodes,
/// TownCentreOf, TownSystem::FindAbodeWithSpace, the influence), so the id goes and a zombie abode is in none of them.
/// (not ported) an unbuilt abode's sites in the town (MultiMapFixedToBeDeleted deletes its own site), the built
/// abode's part, an emptied town's ToBeDeleted (openblack never deletes a town) and two further unlinks. The town area
/// is set again unless the original deletes the town there
void RemoveStructureFromTown(entt::entity town, entt::entity building)
{
	// a workshop -> workshops::RemoveWorkshop (its DeleteDependants has done it already)
	if (workshops::IsWorkshop(building))
	{
		workshops::RemoveWorkshop(town, building);
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* a = registry.TryGet<Abode>(building); a != nullptr)
	{
		a->townId = Abode::k_NoTown;
	}
	// no abode left and no plan -> the town's ToBeDeleted, no SetTownArea ((pending) a further town condition, taken as
	// true); else SetTownArea (this abode is out of town_stats::AbodesOf already)
	if (const auto* t = registry.TryGet<const Town>(town);
	    t != nullptr && (!town_stats::AbodesOf(town).empty() || !t->plannedAbodes.empty()))
	{
		town_placement::SetTownArea(town);
	}
}

/// The fixed object's part of ToBeDeleted (before the generic part of ecs::ToBeDeleted): the reactions it started; its
/// footpath link ((not ported) footpaths); out of a global list (openblack keeps no such list); a map flag ((pending)
/// its reader); the building site ToBeDeleted (building_sites::ToBeDeleted clears the link itself). Nothing of the
/// physics at the mark (neither the DestructionMesh nor the body is touched): the body goes at the next turn or with
/// the immediate deletion, the FragMesh's model when the entity is really destroyed (OnBuildingDamageDestroyed /
/// OnDrawMeshDestroyed)
void MultiMapFixedToBeDeleted(entt::entity building, bool now)
{
	effects::reactions::RemoveAllReactionsInitiatedByObject(building);
	// the site's ToBeDeleted with this one's argument
	if (const auto site = abodes::GetBuildingSite(building); site != entt::null)
	{
		ecs::ToBeDeleted(site, now);
	}
}

/// The abode part of ToBeDeleted
void AbodeToBeDeleted(entt::entity building, bool now)
{
	// the town, before anything
	const auto town = abode_villagers::TownOf(building);
	// DeleteDependants. Literal: StoragePit, Graveyard, Creche, Workshop and Wonder have run it already, so it runs
	// twice for them; the second pass of a graveyard / a creche finds the town's graveyard / creche no longer this one
	// and clears it; a wonder is taken off its player twice
	ClassDeleteDependants(building);
	// with a town ((pending) a game flag taken as clear, as in BuildingSite's ToBeDeleted): MoveAbodeToPlannedAbodes,
	// then out of the town
	if (town != entt::null)
	{
		abodes::MoveAbodeToPlannedAbodes(building);
		RemoveStructureFromTown(town, building);
	}
	// the did-you-know sign and the street lantern
	abodes::DeleteAbodeSurroundingObjects(building);
	// the fixed object's part with the argument
	MultiMapFixedToBeDeleted(building, now);
}

/// The citadel's ToBeDeleted, called by the heart's with 0: on openblack's citadel entity (the heart's)
void CitadelToBeDeleted(entt::entity citadel)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the six worship sites, each ToBeDeleted; a site clears its slot itself (a copy: the sites change the array)
	if (const auto* worship = registry.TryGet<const CitadelWorship>(citadel); worship != nullptr)
	{
		const auto sites = worship->sites;
		for (const auto site : sites)
		{
			if (site != entt::null && registry.Valid(site))
			{
				ecs::ToBeDeleted(site, false);
			}
		}
	}
	// (not ported) the citadel's own lists: openblack's citadel keeps none of them. The player's citadel is cleared:
	// worship::citadel::Of skips an unavailable citadel. (not ported) the virtual influence reset, multiplayer only.
	// The influence is recomputed
	influence::ForceNeedUpdateInfluence();
	// the rest is ecs::ToBeDeleted's own part
}

/// The citadel heart's ToBeDeleted(now)
void CitadelHeartToBeDeleted(entt::entity heart, bool now)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the citadel entrance: (not ported) its torches, their night lights and its 3D object, which openblack's entrance
	// has none of; then its ToBeDeleted(now)
	if (const auto entrance = registry.Get<const CitadelHeart>(heart).entrance;
	    entrance != entt::null && registry.Valid(entrance))
	{
		ecs::ToBeDeleted(entrance, now);
	}
	// (not ported) the camera exclusion: none in openblack. The citadel's heart link: openblack's citadel is the heart
	// itself (worship::citadel::HeartOf). Out of the hearts' list: the Each<CitadelHeart> readers. (pending) they are not
	// all checked for an unavailable heart. (pending) one more linked object is not identified. Then the citadel part
	// (out of the citadel's part list: none in openblack) and the fixed object's part
	MultiMapFixedToBeDeleted(heart, now);
	// its citadel, when it is not being deleted already: ToBeDeleted(0)
	if (registry.AllOf<CitadelWorship>(heart))
	{
		CitadelToBeDeleted(heart);
	}
}
} // namespace

void abodes::ToBeDeleted(entt::entity building)
{
	// the class part (OnToBeDeleted) runs inside ecs::ToBeDeleted, at the mark
	ecs::ToBeDeleted(building);
}

void abodes::OnToBeDeleted(entt::entity entity, bool now)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return;
	}
	if (registry.AllOf<BuildingSite>(entity))
	{
		// a building site's ToBeDeleted; the rest of ecs::ToBeDeleted follows
		building_sites::ToBeDeleted(entity, now);
		return;
	}
	if (registry.AllOf<Scaffold>(entity))
	{
		// a scaffold's part before the mobile object's: out of its site and its workshop, the phantom and both plans
		scaffolds::DeleteDependants(entity);
		return;
	}
	if (registry.AllOf<CitadelHeart>(entity))
	{
		CitadelHeartToBeDeleted(entity, now);
		return;
	}
	if (registry.AllOf<components::WorshipSite>(entity))
	{
		// a worship site, then the citadel part (out of the citadel's part list: none in openblack) and the fixed object's
		// part
		worship::site::ToBeDeleted(entity, now);
		MultiMapFixedToBeDeleted(entity, now);
		return;
	}
	if (!registry.AllOf<Abode>(entity))
	{
		return;
	}
	// the class's ToBeDeleted, its part before the abode part
	if (registry.AllOf<StoragePit>(entity))
	{
		StoragePitToBeDeleted(entity, now);
	}
	else if (workshops::IsWorkshop(entity))
	{
		// a workshop: DeleteDependants, its pile and its needs visuals with `now`
		workshops::ToBeDeleted(entity, now);
	}
	else if (TypeOf(entity) == AbodeType::TownCentre)
	{
		// a town centre: its belief effect, DeleteDependants, the totem ToBeDeleted(now), out of the global list
		// (psys::town_belief's centres skip an unavailable one)
		psys::town_belief::RemoveCentre(entity);
		ClassDeleteDependants(entity);
		std::vector<entt::entity> totems;
		registry.Each<const TotemStatue>([&](entt::entity plinth, const TotemStatue& totem) {
			if (totem.townCentre == entity)
			{
				totems.push_back(plinth);
			}
		});
		for (const auto plinth : totems)
		{
			// (openblack) the icon on top is its own entity; the original's TotemStatue is one object with it
			const auto top = registry.Get<const TotemStatue>(plinth).top;
			ecs::ToBeDeleted(plinth, now);
			if (top != entt::null && ecs::IsAvailable(top))
			{
				ecs::ToBeDeleted(top, now);
			}
		}
	}
	else if (registry.AllOf<components::Graveyard>(entity) || TypeOf(entity) == AbodeType::Creche || wonders::IsWonder(entity))
	{
		// graveyard, creche, wonder: DeleteDependants first
		ClassDeleteDependants(entity);
	}
	// a field and every other abode: the abode part only
	AbodeToBeDeleted(entity, now);
}

std::optional<glm::vec3> abodes::GetEntrancePoint(entt::entity abode, int32_t type)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the mesh; none -> nothing. (openblack) also none without the resources (tests)
	const auto* mesh = registry.Valid(abode) ? registry.TryGet<const Mesh>(abode) : nullptr;
	const auto* transform = registry.Valid(abode) ? registry.TryGet<const Transform>(abode) : nullptr;
	if (mesh == nullptr || transform == nullptr || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return std::nullopt;
	}
	// the first entrance point of the type
	for (const auto& [pointType, p] : meshes.Handle(mesh->id)->GetNewEntrancePoints())
	{
		if (pointType != type)
		{
			continue;
		}
		// the point through the world matrix, one float operation at a time
		const glm::mat4 m = affine::Model(*transform);
		const float x0 = m[1][0] * p.y;
		const float x1 = m[2][0] * p.z;
		const float x2 = x0 + x1;
		const float x3 = m[0][0] * p.x;
		const float x4 = x2 + x3;
		const float y0 = m[2][1] * p.z;
		const float y1 = m[1][1] * p.y;
		const float y2 = y0 + y1;
		const float y3 = m[0][1] * p.x;
		const float y4 = y2 + y3;
		const float z0 = m[2][2] * p.z;
		const float z1 = m[1][2] * p.y;
		const float z2 = z0 + z1;
		const float z3 = m[0][2] * p.x;
		const float z4 = z2 + z3;
		return glm::vec3(x4 + m[3][0], y4 + m[3][1], z4 + m[3][2]);
	}
	return std::nullopt;
}

namespace
{
/// The things of one ObjectType in a cell (map_cells::FindType from the head, then from each one found), and the ones
/// a test keeps
template <typename Pred>
std::vector<entt::entity> FindAllOfType(const map_coords::MapCoords& coords, ObjectType type, Pred&& keep)
{
	std::vector<entt::entity> found;
	const auto cell = map_coords::Cell(coords);
	for (auto e = map_cells::FindType(cell, type); e != entt::null; e = map_cells::FindType(cell, type, e))
	{
		if (keep(e))
		{
			found.push_back(e);
		}
	}
	return found;
}

/// Whether any street lantern is within `radius` metres of `coords`
bool IsALanternWithinDistance(const map_coords::MapCoords& coords, float radius)
{
	auto& registry = Locator::entitiesRegistry::value();
	bool within = false;
	registry.Each<const StreetLantern, const Transform>([&](entt::entity, const StreetLantern&, const Transform& t) {
		within = within || !(gutils::GetDistanceInMetres(coords, map_coords::FromWorld(t.position)) > radius);
	});
	return within;
}
} // namespace

void abodes::CreateAbodeSurroundingObjects(entt::entity abode)
{
	const auto* info = InfoOf(abode);
	// (openblack) only with an island loaded, as AbodeArchetype::Create's ground: the placeholder UnloadedIsland throws
	// on the ground height the sign and the lantern stand on (tests make abodes with no land)
	if (info == nullptr || !Locator::terrainSystem::has_value() || Locator::terrainSystem::value().GetMaterialInfo().empty())
	{
		return;
	}
	// the info's didYouKnow and entrance point 1
	if (static_cast<int>(info->didYouKnow) != 0)
	{
		if (const auto point = GetEntrancePoint(abode, 1); point.has_value())
		{
			// the did-you-know highlight at the point (row 1, 0, 0.0, 1.0)
			const auto coords = map_coords::FromWorld(*point);
			const auto sign = script_highlight::Create(
			    map_coords::ToWorld(coords), static_cast<uint32_t>(script_highlight::Info::DidYouKnow), 0, 0.0f, 1.0f);
			if (sign != entt::null)
			{
				// its script id and category, drawn at height 0
				script_highlight::SetScriptId(sign, static_cast<uint32_t>(info->didYouKnow), info->dykCategory);
				script_highlight::SetYPos(sign, 0.0f);
			}
		}
	}
	// entrance point 0 with no lantern within 40 m -> a street lantern
	if (const auto point = GetEntrancePoint(abode, 0); point.has_value())
	{
		auto coords = map_coords::FromWorld(*point);
		if (!IsALanternWithinDistance(coords, 40.0f))
		{
			// the altitude set to 0 first (it stands on the ground)
			coords.altitude = 0.0f;
			archetypes::StreetLanternArchetype::Create(map_coords::ToWorld(coords), MobileStaticInfo::StreetLantern);
		}
	}
}

void abodes::DeleteAbodeSurroundingObjects(entt::entity abode)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* info = InfoOf(abode);
	// the info's didYouKnow and entrance point 1: the cell's script highlights
	if (info != nullptr && static_cast<int>(info->didYouKnow) != 0)
	{
		if (const auto point = GetEntrancePoint(abode, 1); point.has_value())
		{
			for (const auto e : FindAllOfType(map_coords::FromWorld(*point), ObjectType::ScriptHighlight,
			                                  [](entt::entity e) { return script_highlight::IsHighlight(e); }))
			{
				ecs::ToBeDeleted(e, false);
			}
		}
	}
	// entrance point 0: the cell's mobile statics that are street lanterns
	if (const auto point = GetEntrancePoint(abode, 0); point.has_value())
	{
		for (const auto e : FindAllOfType(map_coords::FromWorld(*point), ObjectType::MobileStatic,
		                                  [&registry](entt::entity e) { return registry.AllOf<StreetLantern>(e); }))
		{
			ecs::ToBeDeleted(e, false);
		}
	}
}

bool abodes::GetShouldNotBeAddedToPlanned(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* a = building != entt::null && registry.Valid(building) ? registry.TryGet<const Abode>(building) : nullptr;
	return a != nullptr && a->shouldNotBeAddedToPlanned;
}

void abodes::SetShouldNotBeAddedToPlanned(entt::entity building, bool value)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* a = building != entt::null && registry.Valid(building) ? registry.TryGet<Abode>(building) : nullptr; a != nullptr)
	{
		a->shouldNotBeAddedToPlanned = value;
	}
}

bool abodes::OnPhysicalDamage(entt::entity building, const PhysicalDamage& hit)
{
	auto& registry = Locator::entitiesRegistry::value();
	// no camera -> no sound and no damage. (openblack) the game always has one: taken as present (headless callers get
	// the damage). The crash sound {1, 0, 0x16, 9, 75} from editor.sad (G_Crash_Abode_01..09)
	const auto& at = registry.Get<const Transform>(building).position;
	physics::CollisionSounds::PlayAnimEffect({1, 0, 0x16, 9, 75}, building, at, false);
	if (!registry.AllOf<Life>(building))
	{
		registry.Assign<Life>(building);
	}
	const float before = life::LifeOf(building);
	// the effect preset's numbers and radius, the applier and the player
	auto values = effects::EffectValues::FromEffectInfo(Locator::infoConstants::value().effect.at(k_PhysicalDestructionEffect));
	values.appliedBy = hit.hitter;
	// the effect's player (ReduceLife's, the alignment's) is the applier's, so the hitter's (a hand-thrown rock moves
	// nobody's alignment); the caused player (the town's aggressor) is the hand's
	const auto hitterPlayer = HitterPlayer(hit.hitter);
	values.player = hitterPlayer;
	values.causedPlayer = hit.player;
	// with a DestructionMesh
	if (hit.remaining.has_value() && HasDestructionMesh(building))
	{
		// (not ported) the guidance sprites shown when the local player's building is below 0.4 remaining
		const float remaining = *hit.remaining;
		// f = life - remaining, not below 0
		float factor = before - remaining;
		if (factor < 0.0f)
		{
			factor = 0.0f;
		}
		// the numbers scaled by f, then divided by the defence multiplier number by number (a 0 multiplier divides by 0,
		// literal)
		values.Scale(factor);
		const auto defence = effects::GetDefenseMultiplier(building);
		for (size_t i = 0; i < values.numbers.size(); ++i)
		{
			values.numbers.at(i) = values.numbers.at(i) / defence.at(i);
		}
	}
	// the heal -> IncreaseLife, the damage -> ReduceLife (the site, StopBeingFunctional, the town's emergency), the
	// alignment
	effects::ApplyEffect(building, values);
	const float now = life::LifeOf(building);
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Buildings: {} hit, life {:.2f} -> {:.2f}", static_cast<uint32_t>(building), before, now);
	}
	// a life that went from non-zero to 0 -> DestroyedByEffect. (openblack) called here: effects::ApplyEffect's own
	// DestroyedByEffect has no abode branch yet (not ported)
	if (before != 0.0f && now == 0.0f)
	{
		DestroyedByEffect(building);
		return false;
	}
	return true;
}

// ---- construction (docs/bw1-notes/buildings.md) -----------------------------------------------------------------

namespace
{
Abode* AbodeOf(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	return building != entt::null && registry.Valid(building) ? registry.TryGet<Abode>(building) : nullptr;
}

/// A CitadelPart's building state (the citadel heart, a worship site); null for anything else
CitadelPartBuild* CitadelPartOf(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	return building != entt::null && registry.Valid(building) ? registry.TryGet<CitadelPartBuild>(building) : nullptr;
}

/// The percent built of an abode or a CitadelPart (the two keep it in their own components); null for anything else
float* PercentBuiltField(entt::entity building)
{
	if (auto* a = AbodeOf(building); a != nullptr)
	{
		return &a->percentBuilt;
	}
	auto* part = CitadelPartOf(building);
	return part != nullptr ? &part->percentBuilt : nullptr;
}

/// A CitadelPart finished, then the class's part: worship::citadel::HeartBuilt or WorshipSiteBuilt
bool BuiltCitadelPart(entt::entity building)
{
	auto* part = CitadelPartOf(building);
	// 1. the site's ToBeDeleted(0): the builders released, the piles released, the link cleared
	if (part->buildingSite != entt::null)
	{
		ecs::ToBeDeleted(part->buildingSite, false);
		part = CitadelPartOf(building);
	}
	// 2. the "new building" reaction: not for the citadel type. 3. the shadow on the texture (the heart's model turns it
	// on as its percent grows instead: CastsShadowOnTexture) 4. the flags set to built, the percent to 1.0
	part->buildFlags = (part->buildFlags & ~CitadelPartBuild::k_UnderConstruction) | CitadelPartBuild::k_Built;
	part->percentBuilt = 1.0f;
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<CitadelHeart>(building))
	{
		worship::citadel::HeartBuilt(building);
	}
	else if (registry.AllOf<WorshipSite>(building))
	{
		worship::citadel::WorshipSiteBuilt(building);
	}
	abodes::RedrawConstruction(building);
	return true;
}

/// The repaired percent drawn for a built building without a FragMesh is scaled by this
constexpr float k_RepairedDrawFactor = 0.98f;
/// The stop-functional threshold of anything without an abode info record
constexpr float k_NonFunctionalDefault = 0.75f;

/// The tail of MakeFunctional: the town's first "a centre, a storage pit and a house" (a town flag, set once)
void CheckTownHasCentrePitAndHouse(entt::entity building, entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* t = registry.TryGet<Town>(town);
	if (t == nullptr || t->hasCentrePitAndHouse)
	{
		return;
	}
	uint32_t pits = 0;
	uint32_t centres = 0;
	uint32_t houses = 0;
	const auto count = [&](entt::entity abode) {
		const auto type = abodes::TypeOf(abode);
		if (type == AbodeType::StoragePit)
		{
			++pits;
		}
		else if (type == AbodeType::TownCentre) // (inferred) a town centre is its type
		{
			++centres;
		}
		else if (type == AbodeType::LivingQuarters) // == 2 exactly (a windmill 0xA is not a house here)
		{
			++houses;
		}
	};
	// the other abodes that are fully built
	for (const auto abode : town_stats::AbodesOf(town))
	{
		if (abode != building && !(abodes::GetPercentBuilt(abode) < 1.0f))
		{
			count(abode);
		}
	}
	// already complete before this one
	if (centres != 0 && pits != 0 && houses != 0)
	{
		return;
	}
	// this one, with no percent-built test
	count(building);
	if (centres != 0 && pits != 0 && houses != 0)
	{
		// the flag set; the player's statistic is multiplayer only: nothing to call. (not ported) the flag's readers are
		// not searched
		t->hasCentrePitAndHouse = true;
	}
}

/// The DrawMesh's generated model, 0 without one
entt::id_type DrawMeshIdOf(entt::entity building)
{
	const auto* draw = Locator::entitiesRegistry::value().TryGet<const DrawMesh>(building);
	return draw != nullptr ? draw->id : 0;
}

/// entt's on_destroy<DrawMesh>: the generated partly built model out of the mesh cache, whatever takes the DrawMesh
/// away (RedrawConstruction's Remove, Registry::Destroy, ecs::ToBeDeleted, and Registry::Reset: entt::registry::clear
/// publishes it for every element, mixin.hpp pop_all). The component is still there during the signal
void OnDrawMeshDestroyed(entt::registry& registry, entt::entity entity)
{
	// (openblack, guard) a reset after the resources have gone (shutdown)
	if (Locator::resources::has_value())
	{
		physics::PartialBuild::EraseMesh(registry.get<DrawMesh>(entity).id);
	}
}
} // namespace

void abodes::ConnectDrawMeshListener()
{
	// connected once per registry (entt's sink::connect disconnects the same listener first: idempotent)
	Locator::entitiesRegistry::value().OnDestroy<DrawMesh>().connect<&OnDrawMeshDestroyed>();
	physics::Buildings::ConnectDamageListener(); // the FragMesh's model at the real destruction (physics)
}

bool abodes::HasDestructionMesh(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* damage = registry.Valid(building) ? registry.TryGet<const BuildingDamage>(building) : nullptr;
	return damage != nullptr && damage->mesh;
}

bool abodes::IsBuilt(entt::entity building)
{
	if (const auto* a = AbodeOf(building); a != nullptr)
	{
		// not under construction and the percent built >= 1
		return (a->buildFlags & Abode::k_UnderConstruction) == 0 && !(a->percentBuilt < 1.0f);
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* feature = registry.Valid(building) ? registry.TryGet<const Feature>(building) : nullptr)
	{
		return !(feature->percentBuilt < 1.0f); // feature_build keeps no flags
	}
	// the citadel heart and a worship site (the same test): not under construction and the percent built >= 1
	if (const auto* part = CitadelPartOf(building); part != nullptr)
	{
		return (part->buildFlags & CitadelPartBuild::k_UnderConstruction) == 0 && !(part->percentBuilt < 1.0f);
	}
	return true;
}

bool abodes::IsRepaired(entt::entity building)
{
	if (AbodeOf(building) != nullptr || CitadelPartOf(building) != nullptr)
	{
		// the life >= 1
		return !(GetPercentRepaired(building) < 1.0f);
	}
	return true;
}

float abodes::GetPercentBuilt(entt::entity building)
{
	if (const auto* a = AbodeOf(building); a != nullptr)
	{
		return a->percentBuilt;
	}
	if (const auto* part = CitadelPartOf(building); part != nullptr)
	{
		return part->percentBuilt; // the citadel heart's and the worship sites' too
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* feature = registry.Valid(building) ? registry.TryGet<const Feature>(building) : nullptr)
	{
		return feature->percentBuilt;
	}
	return 1.0f; // (openblack) not a fixed object that keeps a percent built
}

float abodes::GetPercentRepaired(entt::entity building)
{
	return life::LifeOf(building);
}

float abodes::GetPercentRepairedForNonFunctional(entt::entity building)
{
	const auto* info = InfoOf(building);
	return info != nullptr ? info->thresholdForStopBeingFunctional : k_NonFunctionalDefault;
}

entt::entity abodes::GetBuildingSite(entt::entity building)
{
	if (const auto* part = CitadelPartOf(building); part != nullptr)
	{
		return part->buildingSite;
	}
	const auto* a = AbodeOf(building);
	return a != nullptr ? a->buildingSite : entt::null;
}

bool abodes::IsDrawBuilding(entt::entity building)
{
	return GetBuildingSite(building) != entt::null;
}

float abodes::GetPercentRepairedFromWhenDamaged(entt::entity building)
{
	// not built -> 1
	if (!IsBuilt(building))
	{
		return 1.0f;
	}
	const float repaired = GetPercentRepaired(building);
	// a DestructionMesh and a site
	if (const auto site = GetBuildingSite(building); site != entt::null && HasDestructionMesh(building))
	{
		const float base = building_sites::GetRepairBase(site);
		const float a = 1.0f - base;
		const float b = repaired - base;
		return (a == 0.0f || b == 0.0f) ? 0.0f : b / a;
	}
	return repaired * k_RepairedDrawFactor;
}

float abodes::GetPercentForDrawBuilding(entt::entity building)
{
	// the smaller of the percent built and the repaired percent
	const float built = GetPercentBuilt(building);
	const float repaired = GetPercentRepairedFromWhenDamaged(building);
	return built <= repaired ? built : repaired;
}

const GAbodeInfo* abodes::InfoOf(entt::entity building)
{
	const auto* a = AbodeOf(building);
	if (a == nullptr)
	{
		return nullptr;
	}
	const auto& infos = Locator::infoConstants::value().abode;
	if (const auto i = static_cast<size_t>(static_cast<int32_t>(a->info)); a->info != AbodeInfo::None && i < infos.size())
	{
		return &infos.at(i);
	}
	// (inferred) as TownDesire's GatherInputs: the town's tribe, CELTIC without one
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = abode_villagers::TownOf(building);
	const auto* tribe = town != entt::null ? registry.TryGet<const Tribe>(town) : nullptr;
	return town_stats::AbodeInfoOf(building, tribe != nullptr ? *tribe : Tribe::CELTIC);
}

const GMultiMapFixedInfo* abodes::MultiCellStaticInfoOf(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (building == entt::null || !registry.Valid(building))
	{
		return nullptr;
	}
	// the citadel heart's GCitadelHeartInfo (openblack keeps the one record), a worship site's GWorshipSiteInfo
	if (registry.AllOf<CitadelHeart>(building))
	{
		return &Locator::infoConstants::value().citadelHeart;
	}
	if (const auto* site = registry.TryGet<const WorshipSite>(building); site != nullptr)
	{
		const auto& infos = Locator::infoConstants::value().worshipSite;
		return site->infoIndex < infos.size() ? &infos.at(site->infoIndex) : nullptr;
	}
	return InfoOf(building);
}

bool abodes::CastsShadowOnTexture(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(building))
	{
		return false;
	}
	// only the creation and Built change the bit, never the draw: a NotDrawn built abode keeps it. The built flag is off
	// from creation until Built, the same lifetime as the shadow bit. The citadel heart's model starts with the bit clear
	// and turns it on when its percent reaches 1: an unbuilt temple casts no texture shadow. (pending) a worship site's
	// bit (its creation is not read): on
	if (const auto* heart = registry.TryGet<const CitadelHeart>(building); heart != nullptr)
	{
		return !(heart->drawPercent < 1.0f);
	}
	const auto* a = AbodeOf(building);
	return a == nullptr || (a->buildFlags & Abode::k_Built) != 0;
}

void abodes::BuildBy(entt::entity building, float amount)
{
	// an abode's percent or a CitadelPart's (the same for the citadel heart and the worship sites; a worship site then
	// builds its totem: (not ported) the WorshipTotem keeps no building state)
	float* percent = PercentBuiltField(building);
	if (percent == nullptr)
	{
		return;
	}
	if (IsBuilt(building))
	{
		if (!IsRepaired(building))
		{
			// an abode's IncreaseLife; a CitadelPart's is the plain life one
			if (CitadelPartOf(building) != nullptr)
			{
				life::IncreaseLife(building, amount);
			}
			else
			{
				IncreaseLife(building, amount);
			}
			if (!(life::LifeOf(building) < 1.0f))
			{
				Repaired(building);
			}
		}
	}
	else
	{
		// the percent grows by x; below 0 -> 0; >= 1 -> Built
		*percent = *percent + amount;
		if (*percent < 0.0f)
		{
			*percent = 0.0f;
		}
		if (!(*percent < 1.0f))
		{
			Built(building);
		}
	}
	RedrawConstruction(building);
}

void abodes::SetPercentBuilt(entt::entity building, float percent)
{
	// an abode's percent or a CitadelPart's (the script's BUILT_PERCENTAGE on the citadel heart: Land 1's 0.375)
	float* field = PercentBuiltField(building);
	if (field == nullptr)
	{
		return;
	}
	// the percent = p; p < 0 -> 0; >= 1 -> Built
	*field = percent;
	if (percent < 0.0f)
	{
		*field = 0.0f;
	}
	if (!(*field < 1.0f))
	{
		Built(building);
	}
	RedrawConstruction(building);
}

bool abodes::Built(entt::entity building)
{
	if (CitadelPartOf(building) != nullptr)
	{
		return BuiltCitadelPart(building);
	}
	auto* a = AbodeOf(building);
	if (a == nullptr)
	{
		return false;
	}
	// the fixed object's part
	// 1. the site's ToBeDeleted(0): the builders released, the pile released, the link cleared
	if (a->buildingSite != entt::null)
	{
		ecs::ToBeDeleted(a->buildingSite, false);
		a = AbodeOf(building);
	}
	// 2. a civic building other than the citadel and the football pitch, with a town: the "new building" reaction.
	//    (not ported) no such reaction in openblack yet
	const auto type = TypeOf(building);
	if (type.has_value() && town_stats::IsCivic(*type) && *type != AbodeType::Citadel && *type != AbodeType::FootballPitch &&
	    abode_villagers::TownOf(building) != entt::null)
	{
		if (auto logger = spdlog::get("game"); logger != nullptr)
		{
			SPDLOG_LOGGER_DEBUG(logger, "Buildings: {} built, reaction 15 not ported", static_cast<uint32_t>(building));
		}
	}
	// 3. not a field: the shadow on the texture (the built bit below: CastsShadowOnTexture) and the texture re-bake (not
	// needed, openblack redraws the static shadows each frame) 4. the flags set to built, the percent to 1.0
	a->buildFlags = (a->buildFlags & ~Abode::k_UnderConstruction) | Abode::k_Built;
	a->percentBuilt = 1.0f;
	// the abode part: with a town and a player (openblack's towns always have one: NEUTRAL), the player's statistics
	// count the building by its type (the multiplayer part is skipped); then with a town MakeFunctional
	if (const auto town = abode_villagers::TownOf(building); town != entt::null)
	{
		if (const auto* t = Locator::entitiesRegistry::value().TryGet<const Town>(town); t != nullptr)
		{
			game_stats::BuildingBuilt(static_cast<size_t>(t->owner), type.has_value() ? static_cast<uint32_t>(*type) : 0u);
		}
		MakeFunctional(building);
	}
	// a wonder, after the abode part: added to its player, with or without a town
	if (wonders::IsWonder(building))
	{
		wonders::Built(building);
	}
	RedrawConstruction(building);
	return true;
}

void abodes::MakeFunctional(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* a = AbodeOf(building);
	// 1. no town -> return
	const auto town = abode_villagers::TownOf(building);
	if (a == nullptr || town == entt::null)
	{
		return;
	}
	// 2. the not-repaired flag = !IsRepaired()
	a->buildFlags = IsRepaired(building) ? (a->buildFlags & ~Abode::k_NotRepaired) : (a->buildFlags | Abode::k_NotRepaired);
	// 3. counted in the town's statistics once (town_stats::Compute counts the abodes with the flag from the next town
	//    turn)
	a->addedToTownStats = true;
	// 4. room left for adults -> the town's villagers check for a new abode, which does nothing in this version
	// 5. repaired and built -> the town's (repair) building site removed, if any
	if (IsRepaired(building) && IsBuilt(building))
	{
		building_sites::RemoveBuildingSite(town, building);
	}
	// 6. a storage pit that is not this one and a turn after the first: the footpath to it
	//    (footpaths::MakeAbodeFootpath, on both)
	if (const auto pit = town_queries::GetStoragePit(town); pit != entt::null && pit != building && game_clock::Turn() > 0)
	{
		static_cast<void>(footpaths::MakeAbodeFootpath(building, pit));
	}
	// 7. the centre, pit and house check
	CheckTownHasCentrePitAndHouse(building, town);
	// the class's own part, after the abode part: storage pit, creche, graveyard, workshop and town centre
	auto& t = registry.Get<Town>(town);
	const auto type = TypeOf(building);
	if (type == AbodeType::StoragePit)
	{
		// the town's storage pit
		town_stores::SetStoragePit(town, building);
	}
	else if (type == AbodeType::Creche)
	{
		// the town's creche when still none
		if (t.creche == entt::null)
		{
			t.creche = building;
		}
	}
	else if (type == AbodeType::Graveyard)
	{
		// the town's graveyard when none, then one dead counted
		graveyard::MakeFunctional(building);
	}
	else if (type == AbodeType::Workshop)
	{
		// the pulse and the town's workshop list
		workshops::MakeFunctional(building);
	}
	else if (type == AbodeType::TownCentre)
	{
		// the totem if necessary, the town's centre when none, the spell icons
		// (AbodeArchetype::MakeTownCentreFunctional, in that order)
		if (const auto* info = InfoOf(building); info != nullptr)
		{
			const auto& transform = registry.Get<const Transform>(building);
			const float yAngle = map_cells::detail::YAngleOf(transform.rotation);
			archetypes::AbodeArchetype::MakeTownCentreFunctional(building, *info, yAngle, transform.scale.x);
		}
	}
}

bool abodes::Repaired(entt::entity building)
{
	if (auto* part = CitadelPartOf(building); part != nullptr)
	{
		// the fixed object's part (the citadel heart's and the worship sites'): the site's ToBeDeleted(0), the damage
		// removed ((pending) not read), the not-repaired flag cleared
		if (part->buildingSite != entt::null)
		{
			ecs::ToBeDeleted(part->buildingSite, false);
			part = CitadelPartOf(building);
		}
		part->buildFlags &= ~CitadelPartBuild::k_NotRepaired;
		RedrawConstruction(building);
		return true;
	}
	auto* a = AbodeOf(building);
	if (a == nullptr)
	{
		return false;
	}
	// the fixed object's part: the site's ToBeDeleted(0)
	if (a->buildingSite != entt::null)
	{
		ecs::ToBeDeleted(a->buildingSite, false);
		a = AbodeOf(building);
	}
	// RemoveDamage: the DestructionMesh goes and the whole model is drawn again
	// (physics::Buildings::RemoveDamage; it redraws the construction itself when it had one)
	physics::Buildings::RemoveDamage(building);
	a = AbodeOf(building);
	// the not-repaired flag cleared
	a->buildFlags &= ~Abode::k_NotRepaired;
	// the abode part: with a town MakeFunctional
	if (abode_villagers::TownOf(building) != entt::null)
	{
		MakeFunctional(building);
	}
	RedrawConstruction(building);
	return true;
}

float abodes::IncreaseLife(entt::entity building, float amount)
{
	// wasAbove = (threshold < life); the life raised (cap 1); crossing the threshold upwards ->
	// RestartBeingFunctional
	const float threshold = GetPercentRepairedForNonFunctional(building);
	const bool wasAbove = threshold < life::LifeOf(building);
	const float now = life::IncreaseLife(building, amount);
	if (!wasAbove && threshold < now)
	{
		RestartBeingFunctional(building);
	}
	return now;
}

void abodes::RestartBeingFunctional(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Buildings: {} works again", static_cast<uint32_t>(building));
	}
	// nothing for an abode. A storage pit: the food pile, then the five wood piles, each when available -> its pot
	// reaction removed
	if (const auto* pit = registry.TryGet<const StoragePit>(building); pit != nullptr)
	{
		const StoragePit piles = *pit;
		const auto remove = [&registry](entt::entity pile) {
			// (inferred) IsAvailable: openblack deletes a pile at once (ecs::ToBeDeleted), a valid one is available
			if (pile != entt::null && registry.Valid(pile))
			{
				animal_ai::RemovePotReaction(pile);
			}
		};
		remove(piles.foodPile);
		for (const auto pile : piles.woodPiles)
		{
			remove(pile);
		}
	}
}

bool abodes::CausesTownEmergencyIfDamaged(entt::entity building)
{
	// a storage pit and a town centre; no other abode
	const auto type = TypeOf(building);
	return type == AbodeType::StoragePit || type == AbodeType::TownCentre;
}

float abodes::ReduceLife(entt::entity building, float amount, std::optional<PlayerNames> player)
{
	if (AbodeOf(building) == nullptr)
	{
		return life::ReduceLife(building, amount);
	}
	// a field (it carries an Abode too): its life does not change
	if (Locator::entitiesRegistry::value().AllOf<Field>(building))
	{
		return life::LifeOf(building);
	}
	// old = the life; wasFunctional = (threshold < old)
	const float threshold = GetPercentRepairedForNonFunctional(building);
	const float old = life::LifeOf(building);
	const bool wasFunctional = threshold < old;
	// the fixed object's part
	float l = 0.0f;
	if (IsBuilt(building))
	{
		l = life::ReduceLife(building, amount);
	}
	else
	{
		// p = the percent built - amount, not below 0; SetPercentBuilt; p == 0 -> the life to 0 too
		float p = GetPercentBuilt(building) - amount;
		p = p > 0.0f ? p : 0.0f;
		SetPercentBuilt(building, p);
		if (p == 0.0f)
		{
			life::ReduceLife(building, life::LifeOf(building));
		}
		l = life::LifeOf(building);
	}
	// the rest only while the life is below 1
	if (!(l < 1.0f))
	{
		return l;
	}
	// every inhabitant reacts as to a tap on the abode (VillagerEmergency.h). (openblack) a copy of the list; the call
	// does not change it
	const std::vector<entt::entity> tapped = AbodeOf(building)->inhabitants;
	for (const auto v : tapped)
	{
		villager::SetStateWhenTappedOnAbode(v);
	}
	// the threshold crossed downwards. Also an unbuilt one whose percent reached 0: its life went 1 -> 0 above
	if (wasFunctional && !(threshold < l))
	{
		StopBeingFunctional(building, player);
		// the town's emergency (no check for a missing town in the original, literal; (openblack, guard) nothing without
		// a town)
		if (CausesTownEmergencyIfDamaged(building))
		{
			if (const auto town = abode_villagers::TownOf(building); town != entt::null)
			{
				town_emergency::SetInStateOfEmergency(town);
			}
		}
	}
	// (otherwise a multiplayer statistic only.) No site and a town -> a building site for this one
	const auto town = abode_villagers::TownOf(building);
	if (GetBuildingSite(building) == entt::null && town != entt::null)
	{
		building_sites::AddBuildingSite(town, building);
	}
	// a site and built -> the site's repair base = 1.1 x l - 0.1
	if (const auto site = GetBuildingSite(building); site != entt::null && IsBuilt(building))
	{
		const float scaled = k_RepairBaseScale * l;
		building_sites::SetRepairBase(site, scaled - k_RepairBaseOffset);
	}
	// (at l == 0 the original calls a function with no effect)
	RedrawConstruction(building);
	return l;
}

float abodes::GetDesireToBeRepaired(entt::entity building)
{
	if (CitadelPartOf(building) != nullptr)
	{
		// the fixed object's desire (the citadel heart's and the worship sites'): repaired -> 0; else
		// v = ((1 - GetPercentRepaired) x 0.5 + 0.5) x the info's desireToBeRepaired, capped at 1
		const auto* base = MultiCellStaticInfoOf(building);
		if (base == nullptr || IsRepaired(building))
		{
			return 0.0f;
		}
		const float v = ((1.0f - GetPercentRepaired(building)) * 0.5f + 0.5f) * base->desireToBeRepaired;
		return v < 1.0f ? v : 1.0f;
	}
	const auto* a = AbodeOf(building);
	const auto* info = InfoOf(building);
	if (a == nullptr || info == nullptr)
	{
		return 0.0f;
	}
	// the abode's desire, through TownDesire's port
	town_desire::RepairInput input;
	input.life = GetPercentRepaired(building);
	constexpr auto k_LivingQuarters = static_cast<uint32_t>(AbodeType::LivingQuarters);
	input.livingQuarters = (static_cast<uint32_t>(info->abodeType) & k_LivingQuarters) != 0;
	input.inhabitants = static_cast<uint32_t>(a->inhabitants.size());
	input.desireToBeRepaired = info->desireToBeRepaired;
	return town_desire::AbodeDesireToBeRepaired(input, Locator::infoConstants::value().town);
}

bool abodes::MoveAbodeToPlannedAbodes(entt::entity building)
{
	// no town -> false
	const auto town = abode_villagers::TownOf(building);
	if (AbodeOf(building) == nullptr || town == entt::null)
	{
		return false;
	}
	// unless flagged, a rebuild plan made -> true. The flag's only writers are the scaffolds (building a planned
	// building, being put in the hand, destroying things in the way)
	if (!GetShouldNotBeAddedToPlanned(building) && plans::CreateFromBuilding(town, building).has_value())
	{
		return true;
	}
	// else the building site removed from the town; false
	building_sites::RemoveBuildingSite(town, building);
	return false;
}

void abodes::RedrawConstruction(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	// when the class draws the partly built model and at which percent:
	// - the citadel heart: its model draws partly built while its percent is below 1, with the inner walls 1.0 m in;
	// - a worship site: partly built while not built;
	// - an abode: partly built while it has a site; with a FragMesh the physics draws it (RedrawBuilding): no
	//   construction draw of ours either
	bool partly = false;
	float percent = 0.0f;
	const char* tag = "abode-built";
	if (const auto* heart = registry.Valid(building) ? registry.TryGet<const CitadelHeart>(building) : nullptr)
	{
		partly = heart->drawPercent < 1.0f;
		percent = heart->drawPercent;
		tag = "temple-built";
	}
	else if (registry.Valid(building) && registry.AllOf<WorshipSite, CitadelPartBuild>(building))
	{
		partly = !IsBuilt(building);
		percent = GetPercentForDrawBuilding(building);
		tag = "worship-built";
	}
	else if (AbodeOf(building) != nullptr)
	{
		partly = IsDrawBuilding(building) && !HasDestructionMesh(building);
		percent = GetPercentForDrawBuilding(building);
	}
	else
	{
		return;
	}
	if (AbodeOf(building) != nullptr && HasDestructionMesh(building))
	{
		// with a DestructionMesh: the physics' FragMesh plus the intact model partly built over it (RedrawBuilding), not
		// the construction draw: its DrawMesh is the physics', left as it is
		registry.Remove<AbodeConstructionDraw, NotDrawn>(building);
		physics::Buildings::Redraw(building);
		registry.SetDirty();
		return;
	}
	const auto* mesh = registry.TryGet<const Mesh>(building);
	if (!partly || mesh == nullptr)
	{
		// the normal draw: the whole model (the Mesh) again (the generated one is erased by OnDrawMeshDestroyed)
		registry.Remove<AbodeConstructionDraw, DrawMesh, NotDrawn>(building);
		registry.SetDirty();
		return;
	}
	// the percent to draw; rebuilt only when it changed (openblack)
	if (const auto* state = registry.TryGet<const AbodeConstructionDraw>(building);
	    state != nullptr && state->percent == percent)
	{
		return;
	}
	registry.AssignOrReplace<AbodeConstructionDraw>(building, percent);
	const auto old = DrawMeshIdOf(building);
	// a non-zero percent -> the partly built model; at 0 nothing of the building is drawn (the temple at 0 draws only
	// the scaffold sunk by its whole height; PartialBuild draws nothing at 0). (openblack) without resources (tests)
	// nothing. TODO(physics): the heart passes PartialBuildOptions
	// {.innerOffset = PartialBuild::k_TempleInnerOffset, .skipZero = false} (the 1.0 m walls)
	const entt::id_type built = percent != 0.0f && Locator::resources::has_value()
	                                ? physics::PartialBuild::BuildMesh(building, mesh->id, percent, tag)
	                                : entt::id_type {0};
	if (built == 0)
	{
		registry.Remove<DrawMesh>(building); // the old model erased by OnDrawMeshDestroyed
		registry.AssignOrReplace<NotDrawn>(building);
	}
	else
	{
		const auto& meshes = Locator::resources::value().GetMeshes();
		const auto submesh = meshes.Contains(built) && meshes.Handle(built)->GetNumSubMeshes() > 1 ? static_cast<int8_t>(-1)
		                                                                                           : static_cast<int8_t>(0);
		// connected once per registry (entt's sink::connect disconnects the same listener first: idempotent)
		registry.OnDestroy<DrawMesh>().connect<&OnDrawMeshDestroyed>();
		// a replace publishes no destruction: the old model is erased here
		registry.AssignOrReplace<DrawMesh>(building, built, submesh, mesh->bbSubmeshId);
		registry.Remove<NotDrawn>(building);
		if (old != 0 && old != built)
		{
			physics::PartialBuild::EraseMesh(old);
		}
	}
	registry.SetDirty();
}

std::optional<float> abodes::GetBuiltPercentage(entt::entity entity)
{
	if (AbodeOf(entity) == nullptr && CitadelPartOf(entity) == nullptr)
	{
		return std::nullopt;
	}
	return GetPercentBuilt(entity);
}

bool abodes::SetBuiltPercentage(entt::entity entity, float value)
{
	if (AbodeOf(entity) == nullptr && CitadelPartOf(entity) == nullptr)
	{
		return false;
	}
	// SetPercentBuilt. (pending) the town's building list part is not read
	SetPercentBuilt(entity, value);
	return true;
}

void abodes::RegisterTapHandler()
{
	// always valid / knocking on the roof
	hand_tap::Register<Abode>(
	    [](entt::entity abode, const pot_resource::Dropper&) { return InterfaceValidToTap(abode); },
	    [](entt::entity abode, const pot_resource::Dropper& is, glm::vec3 handPos) -> uint32_t {
		    // the hand's own position for the knock (the status's point for the sound)
		    std::optional<glm::vec3> hand;
		    if (Locator::handSystem::has_value())
		    {
			    hand = Locator::handSystem::value()
			               .GetPlayerHandPositions()[static_cast<size_t>(systems::HandSystemInterface::Side::Left)];
		    }
		    InterfaceTap(abode, handPos, is.isMyInterface, hand.value_or(handPos));
		    return 1;
	    });
}
