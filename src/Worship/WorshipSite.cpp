/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorshipSite.h"

#include <cmath>

#include <algorithm>
#include <exception>
#include <limits>

#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Citadel.h"
#include "Common/GUtilsAngle.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectDelivery.h"
#include "ECS/Registry.h"
#include "ECS/TakeResource.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Villager/VillagerCore.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/MagicTables.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "SpecialPoints.h"
#include "WorshipSpellIcon.h"
#include "WorshipTrace.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
/// The citadel heart's worship-site mesh (Data\Citadel\OutsideMeshes\B_WORSHIP.l3d), loaded with the heart and
/// given to the site
constexpr auto k_SiteMesh = entt::hashed_string("temple/B_WORSHIP_l3d");
/// The angle between two site slots: 2 pi / 7
constexpr float k_SlotAngle = 0.8975979f;
/// The spell icon rings are 15 m apart, up to 30 m (rings 0, 15, 30)
constexpr float k_IconRingStep = 15.0f;
constexpr float k_IconRingMax = 30.0f;
/// the dance ring (see DancePosition); (inferred): 6 m has no source, the .DAN rings are not ported
constexpr float k_DanceRadius = 6.0f;

/// The worship mesh of a citadel: B_WORSHIP has no skin of its own, and every primitive of it wears the first skin
/// of the citadel heart's mesh, so the sites take the temple's texture and follow it as the alignment blends it. One
/// copy per heart mesh, made with the citadel's first site and shared by its other sites. The shared B_WORSHIP when
/// the heart, its mesh or the file is missing
entt::id_type CitadelSiteMesh(entt::entity citadelEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto heart = citadel::HeartOf(citadelEntity);
	const auto* heartMesh = heart != entt::null ? registry.TryGet<const Mesh>(heart) : nullptr;
	if (heartMesh == nullptr || !Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return k_SiteMesh;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(heartMesh->id))
	{
		return k_SiteMesh;
	}
	const auto heartModel = meshes.Handle(heartMesh->id).handle();
	const auto skin = heartModel->GetFirstSkin();
	if (!skin)
	{
		return k_SiteMesh;
	}
	const auto name = fmt::format("temple/B_WORSHIP_l3d/{:#x}", heartMesh->id);
	const auto id = entt::hashed_string(name.c_str()).value();
	if (meshes.Contains(id))
	{
		return id;
	}
	auto& fileSystem = Locator::filesystem::value();
	try
	{
		const auto path = fileSystem.GetPath<filesystem::Path::Citadel>() / "OutsideMeshes" / "B_WORSHIP_l3d.zzz";
		meshes.Load(id, resources::L3DLoader::FromDiskTag {}, fileSystem.FindPath(path));
		auto& mesh = *meshes.Handle(id);
		mesh.SetSkinSource(heartModel);
		mesh.SetSkin(*skin);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Worship site mesh {}: {}", name, e.what());
		return k_SiteMesh;
	}
	return id;
}

WorshipSite& SiteOf(entt::entity site)
{
	return Locator::entitiesRegistry::value().Get<WorshipSite>(site);
}

bool IsSite(entt::entity site)
{
	auto& registry = Locator::entitiesRegistry::value();
	return site != entt::null && registry.Valid(site) && registry.AllOf<WorshipSite>(site);
}

/// The town's tribe
Tribe TribeOfTown(entt::entity town)
{
	const auto* tribe = Locator::entitiesRegistry::value().TryGet<const Tribe>(town);
	return tribe != nullptr ? *tribe : Tribe::NONE;
}

PlayerNames OwnerOfTown(entt::entity town)
{
	const auto* data = Locator::entitiesRegistry::value().TryGet<const Town>(town);
	return data != nullptr ? data->owner : PlayerNames::NEUTRAL;
}

/// The town has a TownSpellIcon of that seed
bool TownHasSpellIcon(entt::entity town, SpellSeedType seed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* magic = registry.TryGet<const TownMagic>(town);
	if (magic == nullptr)
	{
		return false;
	}
	return std::ranges::any_of(magic->spellIcons, [&](entt::entity icon) {
		const auto* component = registry.TryGet<const SpellIcon>(icon);
		return component != nullptr && component->seedType == seed;
	});
}

/// The angle of a site slot around the citadel heart
float SlotAngle(const CitadelWorship& citadel, int slot)
{
	return citadel.heartYAngle + static_cast<float>(slot) * k_SlotAngle;
}

/// The site's matrix at a slot: the citadel's origin, turned to the slot's angle, scale 1.
/// (inferred): the snap to the land's height is openblack's
Transform SiteTransform(const glm::vec3& citadelPosition, float yAngle)
{
	glm::vec3 origin = citadelPosition;
	origin.y = GroundAt(origin);
	return Transform {origin, affine::AngleY(yAngle), glm::vec3(1.0f)};
}

/// The heart's B_WORSHIP special point 9 turned to the angle.
/// UNVERIFIED: the original's lookup is not fully read; its radius may be info.radiusFromCitadel (37.5)
std::optional<glm::vec3> HeartRingPoint(const glm::vec3& citadelPosition, float yAngle)
{
	if (!Locator::resources::has_value() || !Locator::resources::value().GetMeshes().Contains(k_SiteMesh))
	{
		return std::nullopt;
	}
	const auto& metrics = Locator::resources::value().GetMeshes().Handle(k_SiteMesh)->GetExtraMetrics();
	if (metrics.size() <= static_cast<size_t>(site::Point::Arrive))
	{
		return std::nullopt;
	}
	const auto transform = SiteTransform(citadelPosition, yAngle);
	return transform.position + transform.rotation * glm::vec3(metrics[static_cast<size_t>(site::Point::Arrive)][3]);
}

/// Of the free slots, the one whose ring point is nearest (in metres, x and z) to `near`; -1 when all are taken
int FindNearestFreeSlot(const CitadelWorship& citadel, const glm::vec3& citadelPosition, const glm::vec3& near)
{
	int best = -1;
	float bestDistance = 1e6f;
	for (int slot = 0; slot < static_cast<int>(CitadelWorship::k_Sites); ++slot)
	{
		if (citadel.sites[static_cast<size_t>(slot)] != entt::null)
		{
			continue;
		}
		const auto point = HeartRingPoint(citadelPosition, SlotAngle(citadel, slot));
		if (!point)
		{
			continue;
		}
		const float distance = glm::distance(glm::vec2(point->x, point->z), glm::vec2(near.x, near.z));
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = slot;
		}
	}
	return best;
}

/// The slot's point pushed `ring` metres outward along the ray from the site's origin through it. With ring > 0 the
/// point's altitude is set to 0 and it moves along the angle from the site's origin: it ends on the ground. With
/// ring 0 the special point stays as it is
std::optional<glm::vec3> IconPositionFromSlot(entt::entity site, int slot, float ring)
{
	const auto point = site::GetSpecialPos(site, slot);
	if (!point)
	{
		return std::nullopt;
	}
	auto position = *point;
	if (ring > 0.0f)
	{
		const auto& origin = Locator::entitiesRegistry::value().Get<const Transform>(site).position;
		auto coords = map_coords::FromMetres(glm::vec2(position.x, position.z)); // altitude 0
		const float angle = gutils::Get3DAngleFromXZ(map_coords::FromMetres(glm::vec2(origin.x, origin.z)), coords);
		coords += gutils::GetPosFromAngle(angle, ring);
		const auto xz = map_coords::ToMetres(coords);
		position = glm::vec3(xz.x, 0.0f, xz.y);
		position.y = GroundAt(position); // the ground + the altitude 0
	}
	return position;
}

/// For the rings 0, 15, 30, the first slot 10..15 whose candidate is not within 1.0 of an icon of the site
/// ((inferred): per axis); slot -1 when there is no room
glm::vec3 FindIconPosition(entt::entity site, int16_t& slot)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& icons = SiteOf(site).icons;
	glm::vec3 candidate(0.0f);
	for (float ring = 0.0f; ring <= k_IconRingMax; ring += k_IconRingStep)
	{
		for (int s = static_cast<int>(site::Point::FirstIcon); s <= static_cast<int>(site::Point::LastIcon); ++s)
		{
			// (inferred): without the mesh's point the candidate is the site's origin (the original has no fallback)
			candidate = IconPositionFromSlot(site, s, ring).value_or(registry.Get<const Transform>(site).position);
			const bool taken = std::ranges::any_of(icons, [&](entt::entity icon) {
				const auto& at = registry.Get<const Transform>(icon).position;
				return std::abs(at.x - candidate.x) <= 1.0f && std::abs(at.z - candidate.z) <= 1.0f;
			});
			if (!taken)
			{
				slot = static_cast<int16_t>(s);
				return candidate;
			}
		}
	}
	slot = -1;
	return candidate;
}

/// The tribe's altar at the site's special point 8, at the site's
/// angle and scale
entt::entity CreateTotem(entt::entity siteEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& site = SiteOf(siteEntity);
	const auto point = site::GetSpecialPos(siteEntity, site::Point::DanceCentre);
	if (!point)
	{
		return entt::null;
	}
	const auto totem = registry.Create();
	ecs::object_index::Assign(totem);
	registry.Assign<Transform>(totem, *point, affine::AngleY(site.yAngle), glm::vec3(1.0f));
	// the mesh is the site info's meshType
	registry.Assign<Mesh>(totem, resources::HashIdentifier(site::InfoOf(site).meshType), static_cast<int8_t>(0),
	                      static_cast<int8_t>(0));
	registry.Assign<WorshipTotem>(totem, siteEntity);
	return totem;
}

/// The site's food pot: pot info 2 (StoragePitFoodPile) at the resource position (the site's local point
/// (9, 0, -38)), its angle + 1.5, scale 0.7
entt::entity CreateFoodPot(entt::entity siteEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(siteEntity);
	glm::vec3 position = transform.position + transform.rotation * glm::vec3(9.0f, 0.0f, -38.0f);
	position.y = GroundAt(position);
	const float yAngle = SiteOf(siteEntity).yAngle + 1.5f;
	const auto pot = ecs::archetypes::PotArchetype::Create(position, yAngle, PotInfo::StoragePitFoodPile, 0, true);
	if (pot != entt::null)
	{
		if (auto* potTransform = registry.TryGet<Transform>(pot); potTransform != nullptr)
		{
			potTransform->scale = glm::vec3(0.7f);
		}
	}
	return pot;
}
} // namespace

const GWorshipSiteInfo& site::InfoOf(const WorshipSite& site)
{
	return Locator::infoConstants::value().worshipSite.at(site.infoIndex);
}

void site::ToBeDeleted(entt::entity siteEntity, bool now)
{
	if (!IsSite(siteEntity))
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// while there are icons (the head first): delete it; icon::ToBeDeleted takes it out of the list
	while (IsSite(siteEntity) && !SiteOf(siteEntity).icons.empty())
	{
		const auto before = SiteOf(siteEntity).icons.size();
		icon::ToBeDeleted(SiteOf(siteEntity).icons.front());
		// (openblack, guard) an entry that did not leave the list (no longer an icon) is taken off, not looped on
		if (auto& icons = SiteOf(siteEntity).icons; icons.size() == before)
		{
			icons.erase(icons.begin());
		}
	}
	// (openblack, guard) the loop also ends when the site is no longer one
	if (!IsSite(siteEntity))
	{
		return;
	}
	// the dance is deleted: each member of each group still available leaves the dance and takes its state after
	// dancing; the groups and their lists go. openblack keeps the dance inside the site (dancers: one group):
	// (approximate) leaving the dance is site::RemoveDancer and the villager's dancing flag; (pending) the state after
	// dancing is not ported (TODO(dance))
	for (const auto dancer : std::vector<entt::entity>(SiteOf(siteEntity).dancers))
	{
		if (!ecs::IsAvailable(dancer))
		{
			continue;
		}
		site::RemoveDancer(siteEntity, dancer);
		if (auto* link = registry.TryGet<WorshipVillager>(dancer); link != nullptr)
		{
			link->dancing = false;
		}
	}
	// (pending) another object of the site is deleted here; it is not identified.
	// the totem, when available: its site link is cleared, then it is deleted; the site's own totem link is not
	// cleared (literal)
	if (const auto totem = SiteOf(siteEntity).totem; ecs::IsAvailable(totem))
	{
		if (auto* t = registry.TryGet<WorshipTotem>(totem); t != nullptr)
		{
			t->site = entt::null;
		}
		ecs::ToBeDeleted(totem, now);
	}
	// each town of the site: its worship site is cleared
	for (const auto town : SiteOf(siteEntity).towns)
	{
		if (auto* magic = registry.Valid(town) ? registry.TryGet<TownMagic>(town) : nullptr; magic != nullptr)
		{
			magic->worshipSite = entt::null;
		}
	}
	// the citadel's site in this slot is cleared
	if (const auto citadel = SiteOf(siteEntity).citadel; citadel != entt::null && registry.Valid(citadel))
	{
		const auto slot = static_cast<size_t>(SiteOf(siteEntity).slot);
		auto* worship = registry.TryGet<CitadelWorship>(citadel);
		if (worship != nullptr && slot < worship->sites.size())
		{
			worship->sites.at(slot) = entt::null;
		}
	}
	// the town list is emptied (from the head, until none is left). A second list is emptied the same way.
	// (pending) the second list and its neighbour are not identified in openblack
	SiteOf(siteEntity).towns.clear();
	// the food pot (no availability test): its fixed-map link is cleared ((approximate) here the site's pot slot,
	// cleared first), its animal reaction is removed, then it is deleted
	if (const auto pot = SiteOf(siteEntity).foodPot; pot != entt::null)
	{
		SiteOf(siteEntity).foodPot = entt::null;
		ecs::animal_ai::RemovePotReaction(pot);
		ecs::ToBeDeleted(pot, now);
	}
	// a last owned object is destroyed and cleared. (pending) that object is not identified
}

entt::entity site::Create(entt::entity citadelEntity, Tribe tribe, const glm::vec3& near)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (citadelEntity == entt::null || !registry.Valid(citadelEntity) || tribe == Tribe::NONE)
	{
		return entt::null;
	}
	auto& citadel = registry.Get<CitadelWorship>(citadelEntity);
	const auto& citadelTransform = registry.Get<const Transform>(citadelEntity);
	const int slot = FindNearestFreeSlot(citadel, citadelTransform.position, near);
	if (slot < 0)
	{
		return entt::null;
	}
	const float yAngle = SlotAngle(citadel, slot);

	// a citadel part at the citadel's origin, with the info, citadel, slot, tribe, angle and scale 1
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	const auto transform = SiteTransform(citadelTransform.position, yAngle);
	registry.Assign<Transform>(entity, transform);
	registry.Assign<Mesh>(entity, CitadelSiteMesh(citadelEntity), static_cast<int8_t>(0), static_cast<int8_t>(0));
	auto& site = registry.Assign<WorshipSite>(entity);
	site.citadel = citadelEntity;
	site.player = registry.Get<const Temple>(citadelEntity).owner;
	site.infoIndex = static_cast<uint8_t>(tribe); // GTribeInfo.worshipSiteInfo = the tribe's index
	site.tribe = tribe;
	site.slot = static_cast<uint8_t>(slot);
	site.yAngle = yAngle;
	citadel.sites[static_cast<size_t>(slot)] = entity;

	// the player's towns whose tribe is the site's are added, in the list's order (the oldest first)
	for (const auto town : ecs::map_cells::TownsOf(site.player))
	{
		const auto* townTribe = registry.TryGet<const Tribe>(town);
		if (townTribe != nullptr && *townTribe == tribe)
		{
			AddTown(entity, town);
		}
	}
	auto& created = SiteOf(entity);
	created.totem = CreateTotem(entity);
	if (created.totem != entt::null)
	{
		// only with a totem: the dance of GDanceInfo[19 + slot] at point 8, intensity 0.5, then the food pot (adding
		// a resource remakes it when gone)
		SetDanceIntensity(entity, 0.5f);
		SiteOf(entity).foodPot = CreateFoodPot(entity);
	}
	const auto& placed = Locator::entitiesRegistry::value().Get<const Transform>(entity).position;
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Worship: site {} of player {} ({}) in slot {} at ({:.1f}, {:.1f}) angle {:.3f}, {} towns",
	                   static_cast<uint32_t>(entity), static_cast<int>(site.player), static_cast<int>(tribe), slot, placed.x,
	                   placed.z, yAngle, SiteOf(entity).towns.size());
	return entity;
}

std::optional<glm::vec3> site::GetSpecialPos(entt::entity site, Point point)
{
	return GetSpecialPos(site, static_cast<int>(point));
}

std::optional<glm::vec3> site::GetSpecialPos(entt::entity site, int point)
{
	const auto special = GetSpecialPoint(site, point);
	if (!special)
	{
		return std::nullopt;
	}
	return special->position;
}

void site::AddTown(entt::entity siteEntity, entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* magic = registry.TryGet<TownMagic>(town); magic != nullptr)
	{
		magic->worshipSite = siteEntity;
	}
	// the original links a footpath from the town: the villagers walk there with openblack's pathfinding instead
	AddTownSpells(siteEntity, town);
	auto& towns = SiteOf(siteEntity).towns;
	towns.insert(towns.begin(), town);
}

void site::RemoveTown(entt::entity siteEntity, entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& towns = SiteOf(siteEntity).towns;
	towns.erase(std::remove(towns.begin(), towns.end(), town), towns.end());
	if (auto* magic = registry.TryGet<TownMagic>(town); magic != nullptr)
	{
		for (const auto icon : std::vector<entt::entity>(magic->spellIcons))
		{
			if (const auto* component = registry.TryGet<const SpellIcon>(icon); component != nullptr)
			{
				RemoveSpellIconIfUnheld(siteEntity, component->seedType);
			}
		}
		magic->worshipSite = entt::null;
	}
}

void site::AddTownSpells(entt::entity siteEntity, entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* magic = registry.TryGet<const TownMagic>(town);
	if (magic == nullptr)
	{
		return;
	}
	// the town's spell icons from the head
	for (const auto icon : std::vector<entt::entity>(magic->spellIcons))
	{
		if (const auto* component = registry.TryGet<const SpellIcon>(icon); component != nullptr)
		{
			AddSpellIconIfNecessary(siteEntity, component->seedType);
		}
	}
}

void site::AddSpellIconIfNecessary(entt::entity siteEntity, SpellSeedType seed)
{
	if (!IsSite(siteEntity))
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto existing = GetSpellIconFromSeedType(siteEntity, seed); existing != entt::null)
	{
		// stop the removal when it was fading
		auto& icon = registry.Get<WorshipSpellIcon>(existing);
		if (icon.removeTimer != 0)
		{
			registry.Get<Transform>(existing).scale = glm::vec3(icon.savedScale);
			icon.removeTimer = 0;
		}
		return;
	}
	int16_t slot = -1;
	const auto position = FindIconPosition(siteEntity, slot);
	icon::Create(position, seed, siteEntity, slot);
}

void site::RemoveSpellIconIfUnheld(entt::entity siteEntity, SpellSeedType seed)
{
	if (!IsSite(siteEntity))
	{
		return;
	}
	const auto& site = SiteOf(siteEntity);
	if (std::ranges::any_of(site.towns, [&](entt::entity town) { return TownHasSpellIcon(town, seed); }))
	{
		return;
	}
	for (const auto icon : std::vector<entt::entity>(site.icons))
	{
		if (icon::SeedTypeOf(icon) == seed)
		{
			icon::ToBeDeleted(icon);
		}
	}
}

entt::entity site::GetSpellIconFromSeedType(entt::entity siteEntity, SpellSeedType seed)
{
	if (!IsSite(siteEntity))
	{
		return entt::null;
	}
	for (const auto icon : SiteOf(siteEntity).icons)
	{
		if (icon::SeedTypeOf(icon) == seed)
		{
			return icon;
		}
	}
	return entt::null;
}

entt::entity site::GetSpellIconFromMagicType(entt::entity siteEntity, MagicType type)
{
	return GetSpellIconFromSeedType(siteEntity, magic::GetFirstSpellSeedForMagicType(Locator::infoConstants::value(), type));
}

int site::DancerCount(const WorshipSite& site)
{
	return static_cast<int>(site.dancers.size());
}

int site::DancerCount(entt::entity siteEntity)
{
	// no dance -> 0, else the dance's member count
	return IsSite(siteEntity) ? DancerCount(SiteOf(siteEntity)) : 0;
}

float site::CalculateFoodNeededByDancers(entt::entity siteEntity)
{
	if (!IsSite(siteEntity))
	{
		return 0.0f;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// the sum starts at 0 (a float, stored back after every dancer)
	float needed = 0.0f;
	// the dance's groups and each group's members; (approximate) the order of the sum is the dancers' join order
	// here, the original's is group by group (it changes only the float rounding)
	for (const auto dancer : SiteOf(siteEntity).dancers)
	{
		// members that are not villagers add nothing
		const auto* villager =
		    dancer != entt::null && registry.Valid(dancer) ? registry.TryGet<const Villager>(dancer) : nullptr;
		if (villager == nullptr)
		{
			continue;
		}
		// (1 - food) x the info's food for dinner (as an int), added to the sum; each step is rounded to float, so
		// float arithmetic, one operation per statement
		const float hunger = 1.0f - villager->food;
		const auto required = static_cast<int32_t>(ecs::villager::InfoOf(dancer).foodReqiredForDinner);
		const float share = hunger * static_cast<float>(required);
		needed = needed + share;
	}
	return needed;
}

uint32_t site::GetFoodResource(entt::entity siteEntity)
{
	if (!IsSite(siteEntity))
	{
		return 0;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// the food pot; none -> 0
	const auto pot = SiteOf(siteEntity).foodPot;
	const auto* component = pot != entt::null && registry.Valid(pot) ? registry.TryGet<const Pot>(pot) : nullptr;
	if (component == nullptr || component->type == PotInfo::_COUNT)
	{
		return 0;
	}
	// the pot's resource type (from its GPotInfo) must be FOOD, then its amount
	const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(component->type));
	return info.resourceType == ResourceType::Food ? component->amount : 0;
}

float site::CalculateDesireForFood(entt::entity siteEntity)
{
	if (!IsSite(siteEntity))
	{
		return 0.0f;
	}
	// the food the dancers need, stored as a float
	const float needed = CalculateFoodNeededByDancers(siteEntity);
	// the food in the pot (unsigned)
	const auto food = GetFoodResource(siteEntity);
	// both + 0.0001, then food / needed
	constexpr float k_Epsilon = 0.0001f;
	const float foodPlus = static_cast<float>(food) + k_Epsilon;
	const float neededPlus = needed + k_Epsilon;
	float ratio = foodPlus / neededPlus;
	// not below 1 -> 1 (a NaN is kept)
	if (!(ratio < 1.0f) && !std::isnan(ratio))
	{
		ratio = 1.0f;
	}
	// 1 - ratio
	return 1.0f - ratio;
}

float site::Capacity(const WorshipSite& site)
{
	const float tribalPower = magic::players::MagicOf(site.player).tribalPower[2];
	return static_cast<float>(DancerCount(site)) * InfoOf(site).chantsPerVillager * tribalPower;
}

float site::MaxBattery(const WorshipSite& site)
{
	const auto& info = InfoOf(site);
	return static_cast<float>(DancerCount(site)) * info.eachVillagerAddToFillBattery + info.chantsToFillBattery;
}

float site::Available(const WorshipSite& site)
{
	return site.infiniteChants ? 1e6f : site.available - site.used;
}

float site::TotalChantsAvailable(const WorshipSite& site)
{
	return site.infiniteChants ? 1e6f : site.available;
}

float site::AvailableForIcons(const WorshipSite& site, bool seedsOut)
{
	const float available = Available(site);
	if (!seedsOut)
	{
		return available;
	}
	// the original reads the int 500 as a float (~7e-43)
	const float reserved = available - InfoOf(site).chantsToReserveForMaintaining;
	return 0.0f < reserved ? reserved : 0.0f;
}

float site::UseChants(entt::entity siteEntity, float amount)
{
	if (amount < 0.0f)
	{
		return 0.0f;
	}
	auto& site = SiteOf(siteEntity);
	site.requested += amount;
	float used = amount;
	if (Available(site) < amount)
	{
		used = Available(site);
		site.used = site.available;
	}
	else
	{
		site.used += amount;
	}
	magic::players::MagicOf(site.player).chantsUsed += used;
	return used;
}

float site::UseChantsIfNotInfinite(entt::entity siteEntity, float amount)
{
	return SiteOf(siteEntity).infiniteChants ? amount : UseChants(siteEntity, amount);
}

float site::MaintainSpell(entt::entity siteEntity, float amount)
{
	return SiteOf(siteEntity).freeMaintenance ? amount : UseChants(siteEntity, amount);
}

void site::ProcessSpellIcons(entt::entity siteEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	{
		auto& site = SiteOf(siteEntity);
		const float capacity = Capacity(site);
		if (capacity != 0.0f)
		{
			site.strain = (site.requested - capacity) / capacity;
		}
		else
		{
			site.strain = site.requested != 0.0f ? 1.0f : 0.0f;
		}
		if (site.strain <= 0.0f)
		{
			// the spells did not ask for more than the dancers make: the charging icons share what is left
			float count = 0.0f;
			float needed = 0.0f;
			bool seedsOut = false;
			site.iconTookChants = false;
			for (const auto icon : site.icons)
			{
				if (icon::IsCharging(icon, PlayerNames::NEUTRAL, true) && icon::GetChantNeeded(icon) > 0.0f)
				{
					count += 1.0f;
					needed += icon::GetChantNeeded(icon);
				}
				if (!registry.Get<const WorshipSpellIcon>(icon).seeds.empty())
				{
					seedsOut = true;
				}
			}
			if (count != 0.0f)
			{
				const float available = AvailableForIcons(site, seedsOut);
				const float share = (available < needed ? available : needed) / count;
				if (share != 0.0f)
				{
					for (const auto icon : std::vector<entt::entity>(site.icons))
					{
						if (icon::IsCharging(icon, PlayerNames::NEUTRAL, true) && icon::GetChantNeeded(icon) > 0.0f)
						{
							const float taken = icon::AddToChantStore(icon, share);
							UseChantsIfNotInfinite(siteEntity, taken);
							SiteOf(siteEntity).iconTookChants = true;
						}
					}
				}
			}
		}
	}
	for (const auto icon : std::vector<entt::entity>(SiteOf(siteEntity).icons))
	{
		if (registry.Valid(icon))
		{
			icon::Process(icon);
		}
	}

	// the end of the turn
	auto& site = SiteOf(siteEntity);
	const float capacity = Capacity(site);
	const float ratio = capacity != 0.0f ? site.used / capacity : 1.0f;
	float boost = 0.5f - site.battery / MaxBattery(site) * 0.5f;
	boost = boost <= 0.0f ? 0.0f : std::max(boost, 0.2f);
	float intensity = ratio + boost;
	if (!(intensity < 1.0f))
	{
		intensity = 1.0f;
	}
	SetDanceIntensity(siteEntity, intensity);
	const float produced = capacity * intensity;
	const int dancers = DancerCount(site);
	// every 1000 turns the artifacts on the site get a power-up (scaled by artifactPowerupMultiplier and N): openblack
	// has no artifacts
	site.chantDamage = dancers != 0 ? produced / static_cast<float>(dancers) : 0.0f;
	float battery = site.battery - (site.used - produced);
	if (battery <= 0.0f)
	{
		battery = 0.0f;
	}
	site.battery = battery;
	site.used = 0.0f;
	site.requested = 0.0f;
	site.available = battery + capacity;
	if (trace::Enabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Worship trace: site {} icons {} N {} C {:.1f} k {:.3f} strain {:.3f} battery {:.1f} / {:.0f} "
		                   "available {:.1f} damage {:.2f}",
		                   static_cast<uint32_t>(siteEntity), site.icons.size(), dancers, capacity, intensity, site.strain,
		                   site.battery, MaxBattery(site), site.available, site.chantDamage);
	}
}

void site::SetMana(entt::entity siteEntity, float chants)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto icon : SiteOf(siteEntity).icons)
	{
		registry.Get<WorshipSpellIcon>(icon).chantStore = 0.0f;
	}
	SiteOf(siteEntity).battery = chants;
}

void site::UpdateStrainVisual(entt::entity siteEntity, float milliseconds)
{
	auto& site = SiteOf(siteEntity);
	if (site.strain > 0.0f)
	{
		const float strain = std::clamp(site.strain, 0.0f, 1.0f);
		site.strainPhase = std::fmod(site.strainPhase + (5.0f + 5.0f * strain) * milliseconds * 0.001f, glm::two_pi<float>());
		site.strainPulse = (std::cos(site.strainPhase) + 1.0f) * 0.5f;
	}
	// the mana path's alpha: strain > 0 ? 255 x pulse : 255 (the mana path particles belong to the casting side)
}

void site::SetDanceIntensity(entt::entity siteEntity, float intensity)
{
	auto& site = SiteOf(siteEntity);
	if (intensity > 0.0f)
	{
		if (site.danceState == 0)
		{
			site.danceState = 1;
		}
	}
	else if (site.danceState == 1)
	{
		site.danceState = 0;
	}
	site.danceSpeed = intensity;
}

entt::entity site::FindAt(const glm::vec3& position)
{
	// null off the map, then ONE lookup of type 8 (CITADEL): the first object of that type in the cell's fixed list.
	// A WorshipSite is the answer; a WorshipSpellIcon gives its site; anything else (the citadel heart, a town
	// centre's icon) gives null. The site and its icons are in every cell of their collide shape (ecs::map_cells).
	// (approximate) the site's own collide shape is not ported: its cells come from the mesh's collision
	auto& registry = Locator::entitiesRegistry::value();
	const auto coords = map_coords::FromWorld(position);
	if (!map_coords::InBounds(coords))
	{
		return entt::null;
	}
	const auto object = ecs::map_cells::FindType(map_coords::Cell(coords), ObjectType::Citadel);
	if (object == entt::null)
	{
		return entt::null;
	}
	if (registry.AllOf<WorshipSite>(object))
	{
		return object;
	}
	if (const auto* icon = registry.TryGet<const WorshipSpellIcon>(object); icon != nullptr)
	{
		return icon->site;
	}
	return entt::null;
}

void site::AddDancer(entt::entity siteEntity, entt::entity villager)
{
	auto& dancers = SiteOf(siteEntity).dancers;
	if (std::ranges::find(dancers, villager) == dancers.end())
	{
		dancers.push_back(villager);
	}
}

void site::RemoveDancer(entt::entity siteEntity, entt::entity villager)
{
	if (!IsSite(siteEntity))
	{
		return;
	}
	auto& dancers = SiteOf(siteEntity).dancers;
	dancers.erase(std::remove(dancers.begin(), dancers.end(), villager), dancers.end());
}

glm::vec3 site::DancePosition(entt::entity siteEntity, entt::entity villager)
{
	const auto& site = SiteOf(siteEntity);
	const auto centre = GetSpecialPos(siteEntity, Point::DanceCentre)
	                        .value_or(Locator::entitiesRegistry::value().Get<const Transform>(siteEntity).position);
	const auto it = std::ranges::find(site.dancers, villager);
	const auto index = static_cast<float>(it - site.dancers.begin());
	const auto count = static_cast<float>(std::max<size_t>(site.dancers.size(), 1));
	// (inferred): the ring starting at the site's yAngle and the cos/sin order are not from the original
	const float angle = index / count * glm::two_pi<float>() + site.yAngle;
	glm::vec3 position = centre + glm::vec3(std::cos(angle), 0.0f, std::sin(angle)) * k_DanceRadius;
	position.y = GroundAt(position);
	return position;
}

int site::VillagersRequestingToGoHome(const WorshipSite& site)
{
	return static_cast<int>(site.goHomeRequests.size());
}

site::AddResourceRoute site::AddResourceRouteOf(ResourceType type, bool hasBuildingSite)
{
	if (hasBuildingSite && (type == ResourceType::Wood || type == ResourceType::Any))
	{
		return AddResourceRoute::BuildingSite;
	}
	return type == ResourceType::Food ? AddResourceRoute::FoodPot : AddResourceRoute::Nothing;
}

uint32_t site::AddResource(entt::entity siteEntity, ResourceType type, uint32_t amount, bool poisoned)
{
	if (!IsSite(siteEntity))
	{
		return 0;
	}
	const auto buildingSite = ecs::abodes::GetBuildingSite(siteEntity);
	switch (AddResourceRouteOf(type, buildingSite != entt::null))
	{
	case AddResourceRoute::BuildingSite:
		return ecs::building_sites::AddResource(buildingSite, type, amount, nullptr, poisoned);
	case AddResourceRoute::FoodPot:
	{
		// no food pot yet: one is made first
		auto& registry = Locator::entitiesRegistry::value();
		if (const auto pot = SiteOf(siteEntity).foodPot; pot == entt::null || !registry.Valid(pot))
		{
			SiteOf(siteEntity).foodPot = CreateFoodPot(siteEntity);
		}
		return ecs::pot_resource::AddToPotDirect(SiteOf(siteEntity).foodPot, ResourceType::Food, amount, poisoned);
	}
	case AddResourceRoute::Nothing:
		return 0;
	}
	return 0;
}

bool site::DeleteObjectAndTakeResource(entt::entity siteEntity, entt::entity object, const ecs::pot_resource::Dropper& is)
{
	// an object thrown by this player triggers the supply help (the same as the storage pit)
	ecs::take_resource::TriggerSupplyHelpIfThrownByMe(object);
	// what was taken is not used (object_resources::AddResource gives it to the site's AddResource)
	ecs::object_delivery::DoDeleteObjectAndTakeResource(siteEntity, object, is);
	return true;
}
