/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Citadel.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "3D/TempleExteriorMorph.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/Services/GameMusic.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"
#include "ECS/Systems/WorshipStateInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/BuildingSites.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "TownMagic.h"
#include "WorshipSite.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
auto& Registry()
{
	return Locator::entitiesRegistry::value();
}

bool IsCitadel(entt::entity citadel)
{
	return citadel != entt::null && Registry().Valid(citadel) && Registry().AllOf<CitadelWorship, Temple, Transform>(citadel);
}

/// The script music played when the temple is built
constexpr int k_TempleBuiltMusic = 0x3D;
/// The argument of the instant save made when the temple is built
constexpr int k_TempleBuiltSave = 0x14;

struct CitadelState
{
	/// SET_INTERFACE_CITADEL's value; 1 after a script reset
	uint32_t interfaceCitadel {1};
};

/// This module's state (Locator::worshipState)
CitadelState& Citadel()
{
	if (!Locator::worshipState::has_value())
	{
		std::fputs("worship::citadel: no worship state in the locator (Locator::worshipState)\n", stderr);
		std::abort();
	}
	return Locator::worshipState::value().Get<CitadelState>();
}

} // namespace

entt::entity citadel::Of(PlayerNames player)
{
	entt::entity found = entt::null;
	Registry().Each<const Temple, const CitadelWorship>([&](entt::entity entity, const Temple& temple, const CitadelWorship&) {
		// marking a citadel for deletion clears the player's citadel, so a marked citadel is nobody's
		if (found == entt::null && temple.owner == player && ecs::IsAvailable(entity))
		{
			found = entity;
		}
	});
	return found;
}

std::array<entt::entity, 6> citadel::WorshipSitesOf(PlayerNames player)
{
	std::array<entt::entity, 6> sites {entt::null, entt::null, entt::null, entt::null, entt::null, entt::null};
	// the player's citadel; none -> nothing
	const auto citadelEntity = Of(player);
	if (!IsCitadel(citadelEntity))
	{
		return sites;
	}
	// the six slots in order (CitadelWorship::sites is indexed by the site's slot)
	const auto& worship = Registry().Get<const CitadelWorship>(citadelEntity);
	for (size_t i = 0; i < sites.size(); ++i)
	{
		const auto site = worship.sites.at(i);
		sites.at(i) = site != entt::null && Registry().Valid(site) && Registry().AllOf<WorshipSite>(site) ? site : entt::null;
	}
	return sites;
}

float citadel::StrainSoundFraction(entt::entity citadelEntity)
{
	return IsCitadel(citadelEntity) ? Registry().Get<const CitadelWorship>(citadelEntity).strainSoundFraction : 0.0f;
}

float citadel::StrainSoundFractionAtMostOne(entt::entity citadelEntity)
{
	const float fraction = StrainSoundFraction(citadelEntity);
	// below 1 or NaN (an unordered compare) keeps the fraction, else 1.0
	return fraction < 1.0f || std::isnan(fraction) ? fraction : 1.0f;
}

entt::entity citadel::FindNearestWorshipSite(entt::entity citadelEntity, const map_coords::MapCoords& coords, float maxDistance)
{
	if (!IsCitadel(citadelEntity))
	{
		return entt::null;
	}
	const auto& worship = Registry().Get<const CitadelWorship>(citadelEntity);
	entt::entity best = entt::null;
	float bestDistance = maxDistance;
	// the six slots in order
	for (const auto site : worship.sites)
	{
		// a site with dancers
		if (site == entt::null || !Registry().Valid(site) || !Registry().AllOf<WorshipSite>(site) ||
		    site::DancerCount(site) == 0)
		{
			continue;
		}
		// the dance centre special point; (inferred) a mesh without the point leaves it at 0, as the original's
		// zeroed MapCoords
		map_coords::MapCoords centre {};
		if (const auto point = site::GetSpecialPos(site, site::Point::DanceCentre); point)
		{
			centre = map_coords::FromWorld(*point);
		}
		// strictly nearer than the best so far
		const float distance = gutils::GetDistanceInMetres(centre, coords);
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = site;
		}
	}
	return best;
}

bool citadel::HasLivingHeart(entt::entity citadelEntity)
{
	// the heart, built, with life strictly above 0
	const auto heart = HeartOf(citadelEntity);
	return heart != entt::null && ecs::abodes::IsBuilt(heart) && ecs::life::LifeOf(heart) > 0.0f;
}

void citadel::Initialise(entt::entity temple, float heartYAngle)
{
	auto& worship = Registry().AssignOrReplace<CitadelWorship>(temple);
	worship.heartYAngle = heartYAngle;
}

entt::entity citadel::AddTown(entt::entity citadelEntity, entt::entity town)
{
	const auto site = FindOrCreateWorshipSite(citadelEntity, town);
	if (site == entt::null)
	{
		return entt::null;
	}
	const auto& towns = Registry().Get<const WorshipSite>(site).towns;
	if (std::ranges::find(towns, town) == towns.end())
	{
		site::AddTown(site, town);
	}
	// the heart's life is set to 1.0 (UNVERIFIED: a refresh of the heart; nothing to do here)
	return site;
}

entt::entity citadel::FindOrCreateWorshipSite(entt::entity citadelEntity, entt::entity town)
{
	if (!IsCitadel(citadelEntity) || town == entt::null)
	{
		return entt::null;
	}
	if (Registry().Get<const CitadelWorship>(citadelEntity).cannotCreateSites || !town::IsAllowedToCreateWorshipSite(town))
	{
		return entt::null;
	}
	const auto* tribe = Registry().TryGet<const Tribe>(town);
	return tribe != nullptr ? FindOrCreateWorshipSite(citadelEntity, *tribe) : entt::null;
}

entt::entity citadel::FindOrCreateWorshipSite(entt::entity citadelEntity, Tribe tribe)
{
	if (!IsCitadel(citadelEntity))
	{
		return entt::null;
	}
	if (const auto site = FindTribeWorshipSite(citadelEntity, tribe); site != entt::null)
	{
		return site;
	}
	// a new site request: the nearest town of the tribe to the citadel (any abode type, no distance limit); that
	// town's position, else the citadel's
	auto near = Registry().Get<const Transform>(citadelEntity).position;
	if (const auto town = ecs::map_cells::GetNearestTownToPos(
	        map_coords::FromWorld(near), tribe, ecs::map_cells::k_AnyAbodeType, std::numeric_limits<float>::max());
	    town != entt::null)
	{
		near = Registry().Get<const Transform>(town).position;
	}
	// the site is made with angle 0, scale 1.0, 0 % built and not under construction: it counts as built only once
	// it reaches 100 %, and until then it is drawn as a building in progress, nothing at 0 %
	const auto site = site::Create(citadelEntity, tribe, near);
	if (site != entt::null)
	{
		auto& part = Registry().AssignOrReplace<CitadelPartBuild>(site);
		part.buildFlags = CitadelPartBuild::k_Built;
		part.percentBuilt = 0.0f;
		ecs::abodes::RedrawConstruction(site);
	}
	return site;
}

entt::entity citadel::FindTribeWorshipSite(entt::entity citadelEntity, Tribe tribe)
{
	if (!IsCitadel(citadelEntity))
	{
		return entt::null;
	}
	for (const auto site : Registry().Get<const CitadelWorship>(citadelEntity).sites)
	{
		if (site != entt::null && Registry().Valid(site) && Registry().Get<const WorshipSite>(site).tribe == tribe)
		{
			return site;
		}
	}
	return entt::null;
}

void citadel::ProcessSpellIcons(entt::entity citadelEntity)
{
	if (!IsCitadel(citadelEntity))
	{
		return;
	}
	float strain = 0.0f;
	for (const auto site : std::array(Registry().Get<const CitadelWorship>(citadelEntity).sites))
	{
		if (site == entt::null || !Registry().Valid(site))
		{
			continue;
		}
		site::ProcessSpellIcons(site);
		const float siteStrain = Registry().Get<const WorshipSite>(site).strain;
		if (siteStrain > strain)
		{
			strain = siteStrain;
		}
	}
	// the strain sound fraction (the local player's citadel only)
	auto& worship = Registry().Get<CitadelWorship>(citadelEntity);
	if (!magic::players::IsHuman(Registry().Get<const Temple>(citadelEntity).owner) || worship.strainSoundFraction == strain)
	{
		return;
	}
	// moves 0.001 x the milliseconds per turn toward the strain, read every turn
	const float step = static_cast<float>(game_clock::MsPerTurn()) * 0.001f;
	if (worship.strainSoundFraction > strain)
	{
		worship.strainSoundFraction -= step;
		if (worship.strainSoundFraction <= strain)
		{
			worship.strainSoundFraction = strain;
		}
	}
	else
	{
		worship.strainSoundFraction += step;
		if (!(worship.strainSoundFraction < strain))
		{
			worship.strainSoundFraction = strain;
		}
	}
}

entt::entity citadel::CreateBuiltWorshipSite(entt::entity citadelEntity, Tribe tribe)
{
	// the tribe's site (found or made); none -> null
	const auto site = FindOrCreateWorshipSite(citadelEntity, tribe);
	if (site == entt::null)
	{
		return entt::null;
	}
	const auto player = Registry().Get<const Temple>(citadelEntity).owner;
	for (const auto town : ecs::map_cells::TownsOf(player))
	{
		const auto* townTribe = Registry().TryGet<const Tribe>(town);
		if (townTribe == nullptr || *townTribe != tribe)
		{
			continue;
		}
		// the town joins the site if it is not in its list
		const auto& towns = Registry().Get<const WorshipSite>(site).towns;
		if (std::ranges::find(towns, town) == towns.end())
		{
			site::AddTown(site, town);
		}
		// the town's building site of it -> fully built and the building site removed; none: the site stays as it
		// is
		if (ecs::building_sites::GetBuildingSiteInList(town, site) != entt::null)
		{
			ecs::abodes::BuildBy(site, 1.0f);
			ecs::building_sites::RemoveBuildingSite(town, site);
		}
		return site;
	}
	// no town of that tribe -> fully built and null
	ecs::abodes::BuildBy(site, 1.0f);
	return entt::null;
}

void citadel::OpenWorshipSites(entt::entity citadelEntity, float boost)
{
	if (!IsCitadel(citadelEntity))
	{
		return;
	}
	// each town of the citadel's player
	for (const auto town : ecs::map_cells::TownsOf(Registry().Get<const Temple>(citadelEntity).owner))
	{
		// the town's site (found or made); none -> the next town
		const auto site = FindOrCreateWorshipSite(citadelEntity, town);
		if (site == entt::null)
		{
			continue;
		}
		// the town joins the site if it is not in its list
		const auto& towns = Registry().Get<const WorshipSite>(site).towns;
		if (std::ranges::find(towns, town) == towns.end())
		{
			site::AddTown(site, town);
		}
		// built and repaired -> the next town
		if (ecs::abodes::IsBuilt(site) && ecs::abodes::IsRepaired(site))
		{
			continue;
		}
		// the town's building site of it, else a new one; none -> the next
		auto buildingSite = ecs::building_sites::GetBuildingSiteInList(town, site);
		if (buildingSite == entt::null)
		{
			buildingSite = ecs::building_sites::AddBuildingSite(town, site);
		}
		if (buildingSite == entt::null)
		{
			continue;
		}
		ecs::building_sites::SetDesireBoost(buildingSite, boost);
	}
}

entt::entity citadel::GetSpellIcon(entt::entity citadelEntity, MagicType type)
{
	if (!IsCitadel(citadelEntity))
	{
		return entt::null;
	}
	for (const auto site : Registry().Get<const CitadelWorship>(citadelEntity).sites)
	{
		if (site != entt::null && Registry().Valid(site))
		{
			if (const auto icon = site::GetSpellIconFromMagicType(site, type); icon != entt::null)
			{
				return icon;
			}
		}
	}
	return entt::null;
}

void citadel::PostLoadCleanup()
{
	std::vector<std::pair<entt::entity, PlayerNames>> citadels;
	Registry().Each<const Temple, const CitadelWorship>(
	    [&](entt::entity entity, const Temple& temple, const CitadelWorship&) { citadels.emplace_back(entity, temple.owner); });
	for (const auto& [citadelEntity, player] : citadels)
	{
		for (const auto town : ecs::map_cells::TownsOf(player))
		{
			const auto* magic = Registry().TryGet<const TownMagic>(town);
			if (magic != nullptr && magic->worshipSite == entt::null)
			{
				AddTown(citadelEntity, town);
			}
		}
	}
}

entt::entity citadel::HeartOf(entt::entity citadelEntity)
{
	return IsCitadel(citadelEntity) && Registry().AllOf<CitadelHeart>(citadelEntity) ? citadelEntity : entt::null;
}

void citadel::Process(entt::entity citadelEntity)
{
	// the heart and its model; none -> return
	const auto heart = HeartOf(citadelEntity);
	if (heart == entt::null)
	{
		return;
	}
	// the drawn percentage follows the percentage built; reaching 1 the heart leaves the map cells and enters them
	// again
	const float old = Registry().Get<const CitadelHeart>(heart).drawPercent;
	const float percent = ecs::abodes::GetPercentBuilt(heart);
	if (old < 1.0f && !(percent < 1.0f))
	{
		ecs::map_cells::RemoveMapObject(heart);
		SetHeartDrawPercent(heart, percent);
		ecs::map_cells::InsertMapObject(heart);
	}
	else
	{
		SetHeartDrawPercent(heart, percent);
	}
	// the outside moves toward its targets, built or not; once it has moved far enough the heart leaves the map cells
	// while its mesh is blended again, and enters them again. (not ported) the effect the game may then play.
	// (openblack) tests without the service have no outside
	if (Locator::templeExteriorSystem::has_value())
	{
		// the targets: the player's alignment, and twice the player's share of the influence, over every player's
		// slot, the neutral one too. (approximate) the powers as the last turn left them: openblack works them out
		// after every citadel, the original each player's after its own citadel
		const auto player = Registry().Get<const Temple>(heart).owner;
		const float alignmentTarget = TempleExteriorMorph::AlignmentTarget(ecs::effects::alignment::Get(player));
		float allPowers = 0.0f;
		for (uint8_t p = 0; p < static_cast<uint8_t>(PlayerNames::_COUNT); ++p)
		{
			allPowers = influence::InfluencePower(static_cast<PlayerNames>(p)) + allPowers;
		}
		const float sizeTarget =
		    TempleExteriorMorph::SizeTarget(influence::LandNumber() == 1, influence::InfluencePower(player), allPowers);
		auto& exterior = Locator::templeExteriorSystem::value();
		if (exterior.Step(heart, alignmentTarget, sizeTarget))
		{
			ecs::map_cells::RemoveMapObject(heart);
			exterior.Blend(heart);
			ecs::map_cells::InsertMapObject(heart);
		}
	}
	// (not ported) the heart's alignment flock
}

void citadel::SetHeartDrawPercent(entt::entity heart, float percent)
{
	auto* h = heart != entt::null && Registry().Valid(heart) ? Registry().TryGet<CitadelHeart>(heart) : nullptr;
	if (h == nullptr)
	{
		return;
	}
	// clamped to 0..1
	if (percent < 0.0f)
	{
		percent = 0.0f;
	}
	if (percent > 1.0f)
	{
		percent = 1.0f;
	}
	// (pending) first reaching 1 sets a flag on the model, and 1 sets a per-player "temple complete" flag. Of its
	// readers only the entrance's collide is ported, and it reads this percent instead (EntranceCollides)
	h->drawPercent = percent;
	ecs::abodes::RedrawConstruction(heart);
}

void citadel::HeartBuilt(entt::entity heart)
{
	auto& registry = Registry();
	const auto* h = registry.TryGet<const CitadelHeart>(heart);
	if (h == nullptr)
	{
		return;
	}
	// 2. the player (the citadel's, else the town's); life 1.0
	const auto player = registry.Get<const Temple>(heart).owner;
	ecs::life::SetLife(heart, 1.0f);
	// 3. each town of the player: the heart's building site is removed
	for (const auto town : ecs::map_cells::TownsOf(player))
	{
		if (ecs::building_sites::GetBuildingSiteInList(town, heart) != entt::null)
		{
			ecs::building_sites::RemoveBuildingSite(town, heart);
		}
	}
	// 4. the heart's citadel; none -> done
	const auto citadelEntity = registry.Get<const CitadelHeart>(heart).citadel;
	if (citadelEntity == entt::null)
	{
		return;
	}
	OpenWorshipSites(citadelEntity, 0.0f);
	// the local player only.
	// (approximate) IsHuman stands for it, as ProcessSpellIcons above (openblack's local player is the human one)
	if (!magic::players::IsHuman(player))
	{
		return;
	}
	// not in a script's wide screen
	if (const auto* help = help::Get(); help != nullptr && help->IsScriptWideScreen())
	{
		return;
	}
	// no script music playing and not land 1 -> the temple built music
	{
		const auto lock = audio::game_music::Lock();
		if (auto* music = audio::game_music::Get();
		    music != nullptr && music->GetScriptType() == 0 && influence::LandNumber() != 1)
		{
			music->StartScriptMusic(k_TempleBuiltMusic);
		}
	}
	// not a playground game (openblack has none) nor multiplayer -> an instant save. (not ported) openblack has no
	// save games
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Citadel: the temple is built; InstantSaveGame({:#x}) not ported",
	                   k_TempleBuiltSave);
}

void citadel::WorshipSiteBuilt(entt::entity siteEntity)
{
	// each town of the site's player (the citadel's): the site's building site is removed
	const auto* site = Registry().TryGet<const WorshipSite>(siteEntity);
	if (site == nullptr)
	{
		return;
	}
	for (const auto town : ecs::map_cells::TownsOf(site->player))
	{
		if (ecs::building_sites::GetBuildingSiteInList(town, siteEntity) != entt::null)
		{
			ecs::building_sites::RemoveBuildingSite(town, siteEntity);
		}
	}
}

void citadel::SetInterfaceCitadel(uint32_t value)
{
	Citadel().interfaceCitadel = value;
}

uint32_t citadel::InterfaceCitadel()
{
	return Citadel().interfaceCitadel;
}

void citadel::ResetInterfaceCitadel()
{
	Citadel().interfaceCitadel = 1;
}

bool citadel::EntranceValidToTap([[maybe_unused]] entt::entity entrance)
{
	// always valid in a multiplayer game (openblack: never), else SET_INTERFACE_CITADEL's value != 0
	return Citadel().interfaceCitadel != 0;
}

bool citadel::EntranceCollides(entt::entity entrance)
{
	const auto* door = Registry().Valid(entrance) ? Registry().TryGet<const CitadelEntrance>(entrance) : nullptr;
	if (door == nullptr || !ecs::IsAvailable(entrance) || door->heart == entt::null || !Registry().Valid(door->heart))
	{
		return false;
	}
	const auto* heart = Registry().TryGet<const CitadelHeart>(door->heart);
	return heart != nullptr && !(heart->drawPercent < 1.0f);
}

bool citadel::IsEntranceValidToTap(entt::entity object)
{
	return object != entt::null && Registry().Valid(object) && Registry().AllOf<CitadelEntrance>(object) &&
	       EntranceValidToTap(object);
}

citadel::EntranceToolTip citadel::EntranceToolTipFor(entt::entity object, PlayerNames player)
{
	const auto* door =
	    object != entt::null && Registry().Valid(object) ? Registry().TryGet<const CitadelEntrance>(object) : nullptr;
	const auto heart = door != nullptr ? door->heart : entt::null;
	// an entrance whose heart is built: the heart's player's hand is told to enter, any other hand gets nothing
	if (heart == entt::null || !Registry().Valid(heart) || !Registry().AllOf<Temple>(heart) || !ecs::abodes::IsBuilt(heart))
	{
		return EntranceToolTip::NotEntrance;
	}
	return Registry().Get<const Temple>(heart).owner == player ? EntranceToolTip::Enter : EntranceToolTip::Nothing;
}

uint32_t citadel::EntranceTap(entt::entity entrance, bool myInterface)
{
	// the entrance's heart of the local player, tapped from the local interface -> go inside the citadel; returns 1
	const auto* door = Registry().TryGet<const CitadelEntrance>(entrance);
	const auto heart = door != nullptr ? door->heart : entt::null;
	if (heart == entt::null || !Registry().Valid(heart) || !Registry().AllOf<Temple>(heart))
	{
		return 1;
	}
	// (approximate) IsHuman stands for "the local player", as in HeartBuilt
	if (magic::players::IsHuman(Registry().Get<const Temple>(heart).owner) && myInterface)
	{
		// (approximate) going inside (the inside flag, the camera) is the temple interior's Activate, as
		// ENTER_EXIT_CITADEL(1) does
		if (Locator::temple::has_value() && !Locator::temple::value().Active())
		{
			Locator::temple::value().Activate();
		}
	}
	return 1;
}
