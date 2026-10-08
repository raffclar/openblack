/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Scaffolds.h"

#include <algorithm>
#include <bit>
#include <utility>
#include <vector>

#include "Audio/Audio.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Abodes.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/TakeResource.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/ScaffoldPlans.h"
#include "ECS/Town/TownPlacement.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/Wonders.h"
#include "ECS/Town/Workshops.h"
#include "ECS/Trees.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/PSysManager.h"
#include "Resources/ResourceManager.h"

// The scaffold object: its value, plans, hand, tap and physics behaviour (see Scaffolds.h)

namespace openblack::ecs
{
using namespace components;

namespace
{
/// The script id every town founded by a scaffold gets
constexpr uint32_t k_FoundedTownScriptId = 0xABA52;
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Scaffold* ScaffoldOf(entt::entity s)
{
	auto& registry = Entities();
	return s != entt::null && registry.Valid(s) ? registry.TryGet<Scaffold>(s) : nullptr;
}

/// The single shared scaffold info record
const GScaffoldInfo& Info()
{
	return Locator::infoConstants::value().scaffold;
}

// ---- constants ------------------------------------------------------------------------------------------------------

/// GetValue / SetValue: 0.5, 5.0 and 0.2
constexpr float k_ValueOffset = std::bit_cast<float>(0x3F000000u);
constexpr float k_ValueFactor = std::bit_cast<float>(0x40A00000u);
constexpr float k_ScalePerValue = std::bit_cast<float>(0x3E4CCCCDu);
/// 2 pi: ProcessInHand's phantom rotation wrap
constexpr float k_TwoPi = std::bit_cast<float>(0x40C90FDBu);
/// pi / 4: ChoosePlan's town-centre angles
constexpr float k_QuarterPi = std::bit_cast<float>(0x3F490FDBu);
/// pi / 2: the tap split's quarter turns, one of four picked at random
constexpr float k_HalfPi = std::bit_cast<float>(0x3FC90FDBu);
constexpr uint32_t k_SplitQuarters = 4;
/// GetTownForBuilding: the abode search radius and the town / plan start distance
constexpr float k_TownForBuildingRadius = 50.0f;
/// FindTouchingScaffold: the search radius (15.0) and the touching margin (0.001)
constexpr float k_TouchingSearch = std::bit_cast<float>(0x41700000u);
constexpr float k_TouchingMargin = std::bit_cast<float>(0x3A83126Fu);
/// DeletePlannedBuildingsUnderMe: the plan range
constexpr float k_PlansUnderMeRange = 100.0f;
/// DestroyThingsInWay: 36 spiral cells; the push rise 0.5 and upward velocity 0.2
constexpr int32_t k_DestroyCells = 0x24;
constexpr float k_PushRise = std::bit_cast<float>(0x3F000000u);
constexpr float k_PushVelocityY = std::bit_cast<float>(0x3E4CCCCDu);
/// The split's share of the angular momentum (half)
constexpr float k_SplitShare = std::bit_cast<float>(0x3F000000u);
/// Whether football is enabled: the combine maximum is maxNumberForCombining - (football ? 0 : 1). The original
/// reads it from the build's feature table. (pending) openblack keeps no football state nor feature table: false, as
/// BuildingSites.cpp's k_FootballEnabled
constexpr bool k_FootballEnabled = false;
/// Spot visual 4 (the "no building here" puff, ProcessInHand, EndPhysics) and 0x1B (the combine)
constexpr int k_SpotNoBuilding = 4;
constexpr int k_SpotCombine = 0x1B;
/// G_ScaffoldTap 151, G_ScaffoldCombine 201, G_PlantScaffold 174
constexpr int k_SampleScaffoldTap = 151;
constexpr int k_SampleScaffoldCombine = 201;
constexpr int k_SamplePlantScaffold = 174;
/// GetOverwriteDropToolTip without a plan
constexpr uint32_t k_DropToolTipNoPlan = 0xEF0;
/// The players visited: slots 0..6, not the neutral one (inferred)
constexpr uint8_t k_PlayersWithoutNeutral = static_cast<uint8_t>(PlayerNames::NEUTRAL);

// ---- small helpers --------------------------------------------------------------------------------------------------

/// The mesh of an info; 0 for none
entt::id_type MeshOf(const GAbodeInfo* info)
{
	return info != nullptr ? resources::HashIdentifier(info->meshId) : 0;
}

/// A new plan at pos: no info, angle 0, scale 1.0. (inferred) the argument order (pos, info, town, angle, scale): the
/// original's body is (pending)
ScaffoldPlan NewPlan(const map_coords::MapCoords& pos)
{
	ScaffoldPlan plan;
	plan.position = pos;
	return plan;
}

/// Whether a plan is set and not deleted
bool Available(const std::optional<ScaffoldPlan>& plan)
{
	return plan.has_value() && plan->available;
}

/// Deletes a scaffold's plan: it is in no town list, so only its availability goes
void PlanToBeDeleted(std::optional<ScaffoldPlan>& plan)
{
	if (plan.has_value())
	{
		plan->available = false;
	}
}

map_coords::MapCoords PosOf(entt::entity e)
{
	return object::MapCoordsOf(e);
}

/// Sets the scaffold's scale, the Transform's uniform scale (the map cells follow)
void SetScale(entt::entity s, float scale)
{
	auto& registry = Entities();
	if (auto* t = registry.TryGet<Transform>(s); t != nullptr)
	{
		t->scale = glm::vec3(scale);
		map_cells::OnAnglesOrScaleChanged(s);
	}
}

/// Takes the object out of the map cells, moves it and puts it back
void MoveTo(entt::entity e, const map_coords::MapCoords& pos)
{
	auto& registry = Entities();
	map_cells::RemoveMapObject(e);
	if (auto* t = registry.TryGet<Transform>(e); t != nullptr)
	{
		t->position = map_coords::ToWorld(pos);
	}
	map_cells::InsertMapObject(e);
}

/// The scaffold's owner when it is a workshop (RemoveFromWorkshop, the split)
entt::entity WorkshopOwner(const Scaffold& c)
{
	return c.owner != entt::null && Entities().Valid(c.owner) && workshops::IsWorkshop(c.owner) ? c.owner : entt::null;
}

/// Deletes any object the scaffold deletes: an abode's own clean-up, else ecs::ToBeDeleted. A scaffold among them
/// gets its own clean-up from the dead-list hook inside ecs::ToBeDeleted (abodes::OnToBeDeleted ->
/// DeleteDependants), as every generic deletion does
void DeleteObject(entt::entity o)
{
	if (Entities().AllOf<Abode>(o))
	{
		abodes::ToBeDeleted(o);
	}
	else
	{
		ecs::ToBeDeleted(o);
	}
}

/// A spot visual at pos: the original's float argument is not a duration (0 = the entry's own life, see
/// MagicTeleport.cpp)
void SpotVisual(const map_coords::MapCoords& pos, int type)
{
	psys::manager::CreateSpotVisual(type, map_coords::ToWorld(pos), 0.0f, entt::null);
}

/// A sound effect from the InGame bank, untracked, 3D with the owner at `position`
void PlayInGame3D(int sample, entt::entity owner, glm::vec3 position)
{
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), sample};
	options.owner = audio::Owner::Thing(owner);
	options.is3D = true;
	options.track = false;
	options.position = position;
	audio::PlaySoundEffect(options);
}

/// Sets the current plan; with a building, the phantom's mesh becomes the plan info's
void SetPlannedAbode(Scaffold& c, std::optional<ScaffoldPlan> plan)
{
	c.plan = plan;
	if (Available(c.plan) && c.plan->info != nullptr)
	{
		c.phantomMesh = MeshOf(c.plan->info);
	}
}

/// Swaps the current and the candidate plans
void SwapPlannedAbodes(Scaffold& c)
{
	auto t = c.plan;
	SetPlannedAbode(c, c.candidate);
	c.candidate = t;
}

/// Clears the info of the candidate plan, and of the current one unless candidateOnly. (openblack, guard) a missing
/// plan is skipped (the original writes through it)
void SetPlannedInvalid(Scaffold& c, bool candidateOnly)
{
	if (!candidateOnly && c.plan.has_value())
	{
		c.plan->info = nullptr;
	}
	if (c.candidate.has_value())
	{
		c.candidate->info = nullptr;
	}
}

/// Whether the current or candidate plan offers a building
bool HasPlanOf(const Scaffold& c, bool candidate)
{
	const auto& plan = candidate ? c.candidate : c.plan;
	return Available(plan) && plan->info != nullptr;
}

/// A candidate with a building is swapped in first; then the current plan (nullptr when none)
ScaffoldPlan* SwapInCandidate(Scaffold& c)
{
	if (HasPlanOf(c, true))
	{
		SwapPlannedAbodes(c);
	}
	return c.plan.has_value() ? &*c.plan : nullptr;
}

/// Only while held: the nearest scaffold within 15.0 that touches this one, when both may offer any building and this
/// one has no site or can still be adjusted with its site's building not started
entt::entity FindTouchingScaffold(entt::entity s)
{
	auto* c = ScaffoldOf(s);
	if (c == nullptr || !c->holder.has_value())
	{
		return entt::null;
	}
	const auto o = map_cells::FindNearestInSpiral(
	    PosOf(s), [](entt::entity e) { return scaffolds::IsScaffold(e); }, k_TouchingSearch, s);
	const auto* oc = ScaffoldOf(o);
	if (oc == nullptr || !object::IsTouching(s, o, k_TouchingMargin))
	{
		return entt::null;
	}
	if (c->limit != Scaffold::k_AnyAbode || oc->limit != Scaffold::k_AnyAbode)
	{
		return entt::null;
	}
	// no site; else still adjustable and nothing built yet
	if (c->site == entt::null || (scaffolds::CanStillBeAdjusted(s) && building_sites::GetPercentBuilt(c->site) == 0.0f))
	{
		return o;
	}
	return entt::null;
}

/// The town two combined scaffolds keep: one town null -> the other; equal values -> this town when its holder owns
/// it, else o's; else the town of the larger value (unsigned)
entt::entity GetCombinedTown(entt::entity s, entt::entity o)
{
	const auto* c = ScaffoldOf(s);
	const auto* oc = ScaffoldOf(o);
	const auto mine = c != nullptr ? c->town : entt::null;
	const auto theirs = oc != nullptr ? oc->town : entt::null;
	if (mine == entt::null)
	{
		return theirs;
	}
	if (theirs == entt::null)
	{
		return mine;
	}
	const uint32_t a = scaffolds::GetValue(o);
	const uint32_t b = scaffolds::GetValue(s);
	if (b == a)
	{
		// this scaffold's holder is this town's owner
		const auto* t = Entities().TryGet<const Town>(mine);
		return t != nullptr && c->holder.has_value() && *c->holder == t->owner ? mine : theirs;
	}
	return b > a ? mine : theirs;
}

/// The AbodeInfo index of a GAbodeInfo record (the free plan's info)
AbodeInfo InfoIndexOf(const GAbodeInfo* info)
{
	const auto& infos = Locator::infoConstants::value().abode;
	if (info == nullptr || infos.empty() || info < &infos.front() || info > &infos.back())
	{
		return AbodeInfo::None;
	}
	return static_cast<AbodeInfo>(static_cast<int32_t>(info - &infos.front()));
}
} // namespace

// =====================================================================================================================
// the object
// =====================================================================================================================

entt::entity scaffolds::Create(const map_coords::MapCoords& pos, entt::entity town, entt::entity owner, float yAngle,
                               float scale)
{
	auto& registry = Entities();
	const auto& info = Info();
	// The central MobileObjectArchetype makes the mobile object (the creation index, the Transform: Y angle only,
	// uniform scale, the components::MobileObject the map cells and the physics tell the class by (inferred), the map
	// cells). (approximate) the cells' insert runs before the Scaffold part below is assigned: nothing in it reads that
	// part
	const auto s =
	    archetypes::MobileObjectArchetype::Create(map_coords::ToWorld(pos), MobileObjectInfo::OldScaffold, yAngle, scale);
	// the scaffold info's mesh, not the mobile-object row's
	if (auto* mesh = registry.TryGet<Mesh>(s); mesh != nullptr)
	{
		mesh->id = resources::HashIdentifier(info.meshId);
	}
	// the component's defaults, then the owner
	auto& c = registry.Assign<Scaffold>(s);
	c.owner = owner;
	c.town = town;
	// the phantom starts without a mesh; both plans start empty at pos
	SetPlannedAbode(c, NewPlan(pos));
	c.candidate = NewPlan(pos);
	// (openblack) the class's hand tap and physics handlers, registered once
	RegisterTapHandler();
	RegisterPhysicsHandlers();
	return s;
}

bool scaffolds::IsScaffold(entt::entity e)
{
	return ScaffoldOf(e) != nullptr;
}

uint32_t scaffolds::GetValue(entt::entity s)
{
	// (GetScale() - 0.5) x 5.0 + 0.5, truncated toward zero, each step rounded to float
	const float scale = object::GetScale(s);
	const float a = scale - k_ValueOffset;
	const float b = a * k_ValueFactor;
	const float v = b + k_ValueOffset;
	return static_cast<uint32_t>(map_coords::FtoL(v));
}

void scaffolds::SetValue(entt::entity s, uint32_t n)
{
	// SetScale((float)n x 0.2 + 0.5) (exact for any uint32)
	const auto f = static_cast<float>(n);
	const float a = f * k_ScalePerValue;
	const float scale = a + k_ValueOffset;
	SetScale(s, scale);
}

uint32_t scaffolds::GetDefaultResource(entt::entity s)
{
	// value x info WoodValue
	return GetValue(s) * Info().woodValue;
}

bool scaffolds::CanStillBeAdjusted(entt::entity s)
{
	const auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return false;
	}
	// adjustable unless placed long enough ago (gameTurnsAfterPlacingCanStillPickUp) and free to offer any building
	const uint32_t t = c->placedTurn > 0 ? game_clock::Turn() - c->placedTurn : 0;
	return scaffolds::CanStillBeAdjustedAfter(t, c->limit, Info().gameTurnsAfterPlacingCanStillPickUp);
}

bool scaffolds::CanStillBeAdjustedAfter(uint32_t turnsSincePlaced, int32_t limit, uint32_t afterPlacingTurns)
{
	return !(turnsSincePlaced >= afterPlacingTurns && limit == Scaffold::k_AnyAbode);
}

bool scaffolds::HasPlan(entt::entity s, bool candidate)
{
	const auto* c = ScaffoldOf(s);
	return c != nullptr && HasPlanOf(*c, candidate);
}

entt::entity scaffolds::GetTown(entt::entity s)
{
	const auto* c = ScaffoldOf(s);
	return c != nullptr ? c->town : entt::null;
}

entt::entity scaffolds::GetOwner(entt::entity s)
{
	const auto* c = ScaffoldOf(s);
	return c != nullptr ? c->owner : entt::null;
}

void scaffolds::SetOwner(entt::entity s, entt::entity owner)
{
	if (auto* c = ScaffoldOf(s); c != nullptr)
	{
		c->owner = owner;
	}
}

int32_t scaffolds::GetSlot(entt::entity s)
{
	const auto* c = ScaffoldOf(s);
	return c != nullptr ? static_cast<int32_t>((c->flags & Scaffold::k_SlotMask) >> Scaffold::k_SlotShift) : 0;
}

void scaffolds::SetSlot(entt::entity s, int32_t slot)
{
	if (auto* c = ScaffoldOf(s); c != nullptr)
	{
		const auto bits = static_cast<uint16_t>((static_cast<uint32_t>(slot) & 3u) << Scaffold::k_SlotShift);
		c->flags = static_cast<uint16_t>((c->flags & ~Scaffold::k_SlotMask) | bits);
	}
}

// =====================================================================================================================
// deletion
// =====================================================================================================================

void scaffolds::ToBeDeleted(entt::entity s)
{
	// The class part, then the generic deletion (ecs::ToBeDeleted). The class part is safe to run twice (a dead-list
	// hook in ecs::ToBeDeleted may run it again)
	DeleteDependants(s);
	ecs::ToBeDeleted(s);
}

void scaffolds::DeleteDependants(entt::entity s)
{
	auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return;
	}
	// leave the building site
	if (c->site != entt::null)
	{
		building_sites::RemoveScaffold(c->site, s);
		c->site = entt::null;
	}
	RemoveFromWorkshop(s);
	if ((c = ScaffoldOf(s)) == nullptr)
	{
		return;
	}
	// drop the phantom
	c->phantomMesh = 0;
	// delete both plans
	PlanToBeDeleted(c->plan);
	PlanToBeDeleted(c->candidate);
	// The original then deletes the site when still set and nothing is built: dead, as the site was cleared above.
	// ecs::ToBeDeleted does the generic part
}

void scaffolds::RemoveFromWorkshop(entt::entity s)
{
	auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return;
	}
	// leave the owner workshop (when it still lists this scaffold); the owner is cleared in both cases
	if (const auto w = WorkshopOwner(*c); w != entt::null)
	{
		if (workshops::OwnsScaffold(w, s))
		{
			workshops::RemoveScaffold(w, s);
		}
		if ((c = ScaffoldOf(s)) != nullptr)
		{
			c->owner = entt::null;
		}
	}
}

void scaffolds::RemoveOldBuildingSite(entt::entity s)
{
	auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return;
	}
	// leave the site and delete its building
	if (const auto site = c->site; site != entt::null)
	{
		building_sites::RemoveScaffold(site, s);
		if (const auto building = building_sites::GetBuilding(site); building != entt::null)
		{
			abodes::ToBeDeleted(building);
		}
		if ((c = ScaffoldOf(s)) == nullptr)
		{
			return;
		}
		c->site = entt::null;
	}
	// a missing or deleted plan is replaced by an empty one at pos; the same for the candidate
	const auto pos = PosOf(s);
	if (!Available(c->plan))
	{
		SetPlannedAbode(*c, NewPlan(pos));
	}
	if (!Available(c->candidate))
	{
		c->candidate = NewPlan(pos);
	}
}

// =====================================================================================================================
// the building it offers
// =====================================================================================================================

entt::entity scaffolds::GetTownForBuilding(entt::entity s)
{
	auto& registry = Entities();
	const auto pos = PosOf(s);
	// the nearest abode within 50.0 (the nearest, not cut at the radius): its town (even none)
	if (const auto a = map_cells::FindNearType(pos, ObjectType::Abode, k_TownForBuildingRadius);
	    a != entt::null && registry.AllOf<Abode>(a))
	{
		return abode_villagers::TownOf(a);
	}
	// else the nearest town of any player within 50.0; then every plan of each town: the same test (the town is kept)
	float best = k_TownForBuildingRadius;
	entt::entity result = entt::null;
	for (uint8_t p = 0; p < k_PlayersWithoutNeutral; ++p)
	{
		for (const auto town : map_cells::TownsOf(static_cast<PlayerNames>(p)))
		{
			if (const float d = gutils::GetDistanceInMetres(pos, object::MapCoordsOf(town)); d < best)
			{
				best = d;
				result = town;
			}
			for (plans::PlanIndex i = 0; i < plans::PlansOf(town); ++i)
			{
				if (const float d = gutils::GetDistanceInMetres(pos, plans::CoordsOf(town, i)); d < best)
				{
					best = d;
					result = town;
				}
			}
		}
	}
	return result;
}

bool scaffolds::ChoosePlan(entt::entity s, entt::entity town)
{
	auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return false;
	}
	// a town whose workshops are too near -> no plan
	if (town != entt::null && !workshops::IsScaffoldAwayFromWorkshops(town, s))
	{
		return false;
	}
	// the touching scaffold adds its value; `under` is its site's building, if any
	const auto o = FindTouchingScaffold(s);
	uint32_t n = GetValue(s);
	entt::entity under = entt::null;
	if (o != entt::null)
	{
		n += GetValue(o);
		if (const auto* oc = ScaffoldOf(o); oc != nullptr && oc->site != entt::null)
		{
			under = building_sites::GetBuilding(oc->site);
		}
	}
	if ((c = ScaffoldOf(s)) == nullptr)
	{
		return false;
	}
	const auto pos = PosOf(s);
	if (town != entt::null)
	{
		// the tribe of the scaffold's own town, else of the given one; force = the destroy flag
		const Tribe tribe = c->town != entt::null ? town_queries::TribeOf(c->town) : town_queries::TribeOf(town);
		const bool force = (c->flags & Scaffold::k_Destroy) != 0;
		if (!c->candidate.has_value())
		{
			return false; // (openblack, guard) no candidate: the original writes through it
		}
		return scaffold_plans::ChoosePlanForScaffold(town, *c->candidate, pos, n, tribe, under, c->limit, force);
	}
	// a town-less scaffold (CHL CREATE) never founds a town
	if (c->town == entt::null)
	{
		return false;
	}
	// the tribe's town centre; ScaffoldsRequired == 0 or > n (unsigned) -> none
	const auto* info = town_stats::FindAbodeInfo(town_queries::TribeOf(c->town), AbodeNumber::TownCentre);
	if (info == nullptr || info->scaffoldsRequired == 0 || info->scaffoldsRequired > n)
	{
		return false;
	}
	// 8 angles k x pi / 4, the float accumulated from 0
	const auto mesh = MeshOf(info);
	float a = 0.0f;
	for (uint32_t k = 0; k < 8; ++k)
	{
		// `under` is the building under the touching scaffold
		if (town_placement::IsSuitableForFixedObject(pos, mesh, a, 1.0f, under))
		{
			// the candidate: town centre at pos, angle a, scale 1.0; this plan founds a town
			if (!c->candidate.has_value())
			{
				return false; // (openblack, guard)
			}
			c->candidate->info = info;
			c->candidate->position = pos;
			c->candidate->yAngle = a;
			c->candidate->scale = 1.0f;
			c->flags |= Scaffold::k_FoundsTown;
			return true;
		}
		a = a + k_QuarterPi;
	}
	return false;
}

bool scaffolds::ChoosePlanAndUpdate(entt::entity s, entt::entity town)
{
	// ChoosePlan then UpdatePhantomBuildingPointers
	const bool chosen = ChoosePlan(s, town);
	UpdatePhantomBuildingPointers(s);
	return chosen;
}

void scaffolds::UpdatePhantomBuildingPointers(entt::entity s)
{
	auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return;
	}
	// (approximate semantics, literal steps): a = the current plan offers a building, b = the candidate does
	const bool a = HasPlanOf(*c, false);
	const bool b = HasPlanOf(*c, true);
	const auto fade = [c]() {
		// the fade: an active phase ends and the timer becomes 400 - timer; else a finished timer (< 1) starts a new phase
		// at 400
		if ((c->flags & Scaffold::k_FadePhase) != 0)
		{
			c->flags = static_cast<uint16_t>(c->flags & ~Scaffold::k_FadePhase);
			c->fade = Scaffold::k_FadeStart - c->fade;
		}
		else if (c->fade < 1)
		{
			c->flags |= Scaffold::k_FadePhase;
			c->fade = Scaffold::k_FadeStart;
		}
	};
	if (a && b)
	{
		// the same mesh: Swap; a wonder keeps its scale for the phantom; the candidate cleared
		if (MeshOf(c->plan->info) == MeshOf(c->candidate->info))
		{
			SwapPlannedAbodes(*c);
			if (c->plan.has_value() && c->plan->info != nullptr && c->plan->info->abodeNumber == AbodeNumber::Wonder)
			{
				c->phantomScale = c->plan->scale;
			}
			SetPlannedInvalid(*c, true);
			return;
		}
		// a different mesh: a candidate that is not a town centre clears the founds-town flag; then fade
		if (c->candidate->info->abodeNumber != AbodeNumber::TownCentre)
		{
			c->flags = static_cast<uint16_t>(c->flags & ~Scaffold::k_FoundsTown);
		}
		fade();
		return;
	}
	if (a)
	{
		// only the current plan: clear the founds-town flag; then fade
		c->flags = static_cast<uint16_t>(c->flags & ~Scaffold::k_FoundsTown);
		fade();
		return;
	}
	if (b)
	{
		// only the candidate: restart the timer; Swap; the phantom takes the plan's scale; the candidate cleared; fade phase
		c->fade = Scaffold::k_FadeStart;
		SwapPlannedAbodes(*c);
		if (c->plan.has_value())
		{
			c->phantomScale = c->plan->scale;
		}
		SetPlannedInvalid(*c, true);
		c->flags |= Scaffold::k_FadePhase;
		return;
	}
	// neither: Swap; the candidate cleared
	SwapPlannedAbodes(*c);
	SetPlannedInvalid(*c, true);
}

bool scaffolds::BuildBuilding(entt::entity s, std::optional<PlayerNames> player, bool force)
{
	auto& registry = Entities();
	auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return false;
	}
	// no plan, or one that needs more scaffolds than this value -> choose again
	auto town = GetTownForBuilding(s);
	if (!HasPlanOf(*c, false) || c->plan->info->scaffoldsRequired > GetValue(s))
	{
		ChoosePlanAndUpdate(s, town);
	}
	if ((c = ScaffoldOf(s)) == nullptr || !HasPlanOf(*c, false))
	{
		return false;
	}
	// the plan founds a town and offers a town centre: a NEW town
	if ((c->flags & Scaffold::k_FoundsTown) != 0 && c->plan->info->abodeType == AbodeType::TownCentre)
	{
		const auto oldTown = c->town;
		if (oldTown == entt::null)
		{
			return false;
		}
		// TownArchetype::Create with a new unique key (openblack's Town::id: the highest + 1) and the script id shared by
		// every founded town (Town::scriptId). (pending) one constructor argument (0) is not known
		uint32_t id = 0;
		registry.Each<const Town>([&id](entt::entity, const Town& t) { id = std::max(id, t.id + 1); });
		const auto owner = player.value_or(PlayerNames::NEUTRAL);
		const auto at = map_coords::ToWorld(PosOf(s));
		const auto newTown = archetypes::TownArchetype::Create(static_cast<int>(id), at, owner, town_queries::TribeOf(oldTown));
		registry.Get<Town>(newTown).scriptId = k_FoundedTownScriptId;
		// no influence until the town centre's turn clears it
		if (auto* influence = registry.TryGet<TownInfluence>(newTown); influence != nullptr)
		{
			influence->noInfluence = true;
		}
		// assign the forests. (approximate) the reference point: a new town has no storage pit; its temporary store point
		// is taken as its position
		AssignForestsToTown(id, at);
		// The new town holds every magic type the old town holds. TODO: the town's magic types
		// (IsMagicTypeHeld / AddMagicTypesHeld) are not ported here.
		// With a player, the town's belief in the founder is set ((inferred)). TODO: SetBelief and the town
		// info's starting belief are (pending)
		town = newTown;
		if ((c = ScaffoldOf(s)) == nullptr)
		{
			return false;
		}
	}
	if (town == entt::null)
	{
		return false;
	}
	// the phantom's angle is tried first, then the plan's own angle
	const float keep = c->plan->yAngle;
	c->plan->yAngle = c->phantomAngle;
	if (!TryToBuildPlannedBuilding(s, town, force))
	{
		if ((c = ScaffoldOf(s)) == nullptr || !c->plan.has_value())
		{
			return false;
		}
		c->plan->yAngle = keep;
		if (!TryToBuildPlannedBuilding(s, town, force))
		{
			return false;
		}
	}
	// pulse the town
	town_queries::Pulse(town);
	// with a player, a REACT_TO_SCAFFOLD reaction: the spread is the villagers' reaction (effects::reactions),
	// ImpressTowns their belief
	if (player.has_value())
	{
		const auto reaction = effects::reactions::CreateReaction(s, openblack::Reaction::ReactToScaffold, *player, true);
		ImpressTowns(s, reaction);
	}
	DeletePlannedBuildingsUnderMe(s);
	// G_PlantScaffold: 2D without an owner when the local hand dropped it last; else 3D at the scaffold, owned by it;
	// untracked
	if ((c = ScaffoldOf(s)) != nullptr && c->lastDroppedByLocalHand)
	{
		audio::PlayOptions options;
		options.sample = {audio::Bank(audio::SfxBank::InGame), k_SamplePlantScaffold};
		options.is3D = false;
		options.track = false;
		audio::PlaySoundEffect(options);
	}
	else
	{
		PlayInGame3D(k_SamplePlantScaffold, s, map_coords::ToWorld(PosOf(s)));
	}
	return true;
}

bool scaffolds::TryToBuildPlannedBuilding(entt::entity s, entt::entity town, bool force)
{
	auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return false;
	}
	// the plan (a candidate with a building swapped in first); none -> fail
	auto* plan = SwapInCandidate(*c);
	if (plan == nullptr)
	{
		return false;
	}
	// a town whose workshops are too near -> fail
	if (!workshops::IsScaffoldAwayFromWorkshops(town, s))
	{
		return false;
	}
	if ((c = ScaffoldOf(s)) == nullptr || !c->plan.has_value() || c->plan->info == nullptr)
	{
		return false; // (openblack, guard)
	}
	plan = &*c->plan;
	// force, or the plan's building fits at the scaffold's own pos
	const auto pos = PosOf(s);
	const bool ok =
	    force || town_placement::IsSuitableForFixedObject(pos, MeshOf(plan->info), plan->yAngle, plan->scale, entt::null);
	if (!ok)
	{
		return false;
	}
	// move the plan to pos and add it to the town as a building site without the fixed check
	plan->position = pos;
	PlannedAbode freePlan {
	    .info = InfoIndexOf(plan->info),
	    .position = map_coords::ToWorld(pos),
	    .yAngleRadians = plan->yAngle,
	    .scale = plan->scale,
	};
	// (inferred) a scaffold's plan is never a town centre plan
	freePlan.townCentre = false;
	freePlan.wasBuilt = false;
	const auto* info = plan->info;
	const auto site = building_sites::AddBuildingSiteNoFixedCheck(town, freePlan);
	// the plan converted: deleted (CreatePlannedNoFixedCheck step 5)
	if ((c = ScaffoldOf(s)) == nullptr)
	{
		return false;
	}
	if (site != entt::null)
	{
		PlanToBeDeleted(c->plan);
	}
	else
	{
		return false;
	}
	// a free scaffold's building is not re-planned when destroyed; a scripted one is
	if (c->limit == Scaffold::k_AnyAbode)
	{
		abodes::SetShouldNotBeAddedToPlanned(building_sites::GetRootBuilding(site), true);
	}
	// join the site (the only place components::Scaffold::site is set)
	building_sites::AddScaffold(site, s);
	c->site = site;
	// the phantom takes the scaffold's own scale
	c->phantomScale = object::GetScale(s);
	// a wonder gets the town's wonder power at pos
	if (info->abodeNumber == AbodeNumber::Wonder)
	{
		wonders::SetPower(building_sites::GetRootBuilding(site), scaffold_plans::GetWonderPower(town, pos));
	}
	// no current plan; the candidate deleted and cleared
	SetPlannedAbode(*c, std::nullopt);
	PlanToBeDeleted(c->candidate);
	c->candidate = std::nullopt;
	return true;
}

void scaffolds::ForceBuildBuilding(entt::entity s, std::optional<PlayerNames> player)
{
	// a forced build sets the phantom and the placed turn
	if (BuildBuilding(s, player, true))
	{
		SetPhantomMeshAndPosition(s);
		if (auto* c = ScaffoldOf(s); c != nullptr)
		{
			c->placedTurn = game_clock::Turn();
		}
	}
}

void scaffolds::DeletePlannedBuildingsUnderMe(entt::entity s)
{
	const auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return;
	}
	const auto pos = PosOf(s);
	// the site root's 2D radius. (openblack, guard) without a site 0
	const auto root = c->site != entt::null ? building_sites::GetRootBuilding(c->site) : entt::null;
	const float rootRadius = root != entt::null ? object::Get2DRadius(root) : 0.0f;
	// every player's towns: each plan with d = dist(pos, plan) - root radius < 100 and d - (its mesh's 2D radius x its
	// scale) <= 0 is removed. The temporary object the original makes per town and never frees (a leak) is not ported
	for (uint8_t p = 0; p < k_PlayersWithoutNeutral; ++p)
	{
		for (const auto town : map_cells::TownsOf(static_cast<PlayerNames>(p)))
		{
			plans::PlanIndex i = 0;
			while (i < plans::PlansOf(town))
			{
				const float dist = gutils::GetDistanceInMetres(pos, plans::CoordsOf(town, i));
				const float d = dist - rootRadius;
				if (d < k_PlansUnderMeRange)
				{
					const auto* info = plans::InfoOf(town, i);
					const float scale = plans::ScaleOf(town, i);
					const float r = info != nullptr ? object::MeshRadius2D(MeshOf(info), scale) : 0.0f;
					if (d - r <= 0.0f)
					{
						// the next node was read first: the later plans move down one index
						plans::RemovePlanned(town, i);
						continue;
					}
				}
				++i;
			}
		}
	}
}

void scaffolds::ImpressTowns(entt::entity s, uint32_t reaction)
{
	const auto pos = PosOf(s);
	// every town closer than maxDistanceForImpressingTowns (300). (approximate) map_cells::ForEachTown's order
	map_cells::ForEachTown([&](entt::entity town) {
		if (!(gutils::GetDistanceInMetres(pos, object::MapCoordsOf(town)) < Info().maxDistanceForImpressingTowns))
		{
			return true;
		}
		// k = max(1, (adults + children) x proportionOfTownToImpress (0.1) truncated toward zero); walking the town's abodes
		// and their villagers, the first k update how impressed they are (belief only, no state change).
		// TODO: villager_reactions::ScaffoldImpressCount(town) and villager_reactions::UpdateHowImpressed(
		// villager, reaction, false) are not in HEAD; the walk once they land:
		//   uint32_t left = villager_reactions::ScaffoldImpressCount(town);
		//   for (const auto abode : town_stats::AbodesOf(town))
		//       for (const auto villager : abode_villagers::VillagersOf(abode))
		//           if (left != 0) { villager_reactions::UpdateHowImpressed(villager, reaction, false); --left; }
		(void)reaction;
		return true;
	});
}

void scaffolds::SetPhantomMeshAndPosition(entt::entity s)
{
	auto* c = ScaffoldOf(s);
	if (c == nullptr || c->site == entt::null)
	{
		return;
	}
	// With a site and a building, the phantom's mesh becomes the building's. (approximate) the phantom's matrix (the
	// building's scale / Y angle at its pos) and two draw values are not kept: openblack does not draw the phantom yet
	if (const auto building = building_sites::GetBuilding(c->site); building != entt::null)
	{
		c->phantomMesh = MeshOf(abodes::InfoOf(building));
	}
}

void scaffolds::SetScaffoldProperties(entt::entity s, int32_t type, float size, bool destroy)
{
	auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return;
	}
	// SET_SCAFFOLD_PROPERTIES: the building type limit (raw), the value size truncated toward zero and the destroy flag
	c->limit = type;
	SetValue(s, static_cast<uint32_t>(map_coords::FtoL(size)));
	if ((c = ScaffoldOf(s)) != nullptr)
	{
		c->flags = static_cast<uint16_t>((c->flags & ~Scaffold::k_Destroy) | (destroy ? Scaffold::k_Destroy : 0));
	}
}

// =====================================================================================================================
// the hand
// =====================================================================================================================

bool scaffolds::ValidForPlaceInHand(entt::entity s)
{
	const auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return false;
	}
	// no site -> yes; else still adjustable and nothing built yet
	if (c->site == entt::null)
	{
		return true;
	}
	return CanStillBeAdjusted(s) && building_sites::GetPercentBuilt(c->site) == 0.0f;
}

void scaffolds::OnPickedUp(entt::entity s, PlayerNames holder)
{
	auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return;
	}
	// once the hand took it: the holder is set and the placed turn cleared
	c->holder = holder;
	c->placedTurn = 0;
	// the site's root building is kept out of the plans while the site goes
	const auto root = c->site != entt::null ? building_sites::GetRootBuilding(c->site) : entt::null;
	const bool had = root != entt::null && abodes::GetShouldNotBeAddedToPlanned(root);
	if (root != entt::null)
	{
		abodes::SetShouldNotBeAddedToPlanned(root, true);
	}
	RemoveOldBuildingSite(s);
	// restore the flag when it was clear (the root was just deleted: (openblack) only while it lives)
	if (root != entt::null && !had && Entities().Valid(root))
	{
		abodes::SetShouldNotBeAddedToPlanned(root, false);
	}
	// tell the owner workshop it moved (its slot becomes 1)
	if ((c = ScaffoldOf(s)) != nullptr && c->owner != entt::null && Entities().Valid(c->owner))
	{
		workshops::ScaffoldMoved(c->owner, s);
	}
}

bool scaffolds::ProcessInHand(entt::entity s, const HandStatus& status)
{
	auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return true;
	}
	// 1. a running feedback counter (bits 2..5) steps by one, 15 wraps to 0
	if ((c->flags & Scaffold::k_FeedbackMask) != 0)
	{
		const auto f = static_cast<uint32_t>(c->flags);
		const uint32_t next = (f & 0xFFFCu) + Scaffold::k_FeedbackStep;
		c->flags = static_cast<uint16_t>(((next ^ f) & Scaffold::k_FeedbackMask) ^ f);
	}
	// 2. the phantom turns by phantomBuildingRotationPerGameTurn unless rotation is off; above 2 pi (<= kept) it wraps
	const float rotation = (c->flags & Scaffold::k_NoPhantomRotation) != 0 ? 0.0f : Info().phantomBuildingRotationPerGameTurn;
	const float angle = rotation + c->phantomAngle;
	c->phantomAngle = angle;
	if (angle > k_TwoPi)
	{
		c->phantomAngle = angle - k_TwoPi;
	}
	// 3. the fade timer drops by up to 100 (a negative one becomes 0); finished outside a fade phase: Swap; the
	//    candidate cleared; with a plan, the phantom takes its scale (and its angle unless rotation is off), a new fade
	//    phase at 400
	c->fade = c->fade - std::min(c->fade, Scaffold::k_FadeStep);
	if (c->fade <= 0 && (c->flags & Scaffold::k_FadePhase) == 0)
	{
		SwapPlannedAbodes(*c);
		SetPlannedInvalid(*c, true);
		if (HasPlanOf(*c, false))
		{
			c->phantomScale = c->plan->scale;
			if ((c->flags & Scaffold::k_NoPhantomRotation) == 0)
			{
				c->phantomAngle = c->plan->yAngle;
			}
			c->flags |= Scaffold::k_FadePhase;
			c->fade = Scaffold::k_FadeStart;
		}
	}
	// 4. the hand has been still
	if (status.stillTurns != 0)
	{
		if (HasPlanOf(*c, false) && (c->flags & Scaffold::k_HoverDone) != 0)
		{
			return true;
		}
		const auto town = GetTownForBuilding(s);
		const bool chosen = ChoosePlanAndUpdate(s, town);
		if ((c = ScaffoldOf(s)) == nullptr)
		{
			return true;
		}
		// no plan and no feedback running -> the "no building here" puff and the counter starts at 1
		if (!chosen && (c->flags & (Scaffold::k_FeedbackMask | Scaffold::k_HoverDone)) == 0)
		{
			SpotVisual(PosOf(s), k_SpotNoBuilding);
			c->flags = static_cast<uint16_t>((c->flags & 0xFFC7u) | Scaffold::k_FeedbackStep);
		}
		c->flags |= Scaffold::k_HoverDone;
		return true;
	}
	// 5. the hand moved: clear the hover flag
	c->flags = static_cast<uint16_t>(c->flags & ~Scaffold::k_HoverDone);
	const auto town = GetTownForBuilding(s);
	// no town, a plan that founds one -> done
	if (town == entt::null && HasPlanOf(*c, false) && (c->flags & Scaffold::k_FoundsTown) != 0)
	{
		return true;
	}
	// a plan whose phantom fits here with the phantom's angle and scale, in a town whose workshops are far enough -> done
	if (HasPlanOf(*c, false) && c->phantomMesh != 0 &&
	    town_placement::IsSuitableForFixedObject(PosOf(s), c->phantomMesh, c->phantomAngle, c->phantomScale, entt::null) &&
	    town != entt::null && workshops::IsScaffoldAwayFromWorkshops(town, s))
	{
		return true;
	}
	// a finished fade timer -> UpdatePhantomBuildingPointers
	if ((c = ScaffoldOf(s)) != nullptr && c->fade <= 0)
	{
		UpdatePhantomBuildingPointers(s);
	}
	return true;
}

entt::entity scaffolds::OnLeftHand(entt::entity s, PlayerNames holder)
{
	auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return entt::null;
	}
	// (approximate) who dropped it last: the local hand is PLAYER_ONE's (BuildBuilding's plant sound)
	c->lastDroppedByLocalHand = holder == PlayerNames::PLAYER_ONE;
	// a dropped-gently scaffold with a plan and more value than the plan needs ((pending) one player state test is
	// taken as passing)
	if (!HasPlanOf(*c, false) || (c->flags & Scaffold::k_DroppedGently) == 0)
	{
		return entt::null;
	}
	const auto e = static_cast<int32_t>(GetValue(s) - c->plan->info->scaffoldsRequired);
	if (e <= 0)
	{
		return entt::null;
	}
	// the extra e goes into a new scaffold that the caller puts in the hand
	const auto x = Create(PosOf(s), c->town, entt::null, 0.0f, 1.0f);
	if (x == entt::null)
	{
		return entt::null;
	}
	SetValue(x, static_cast<uint32_t>(e));
	SetValue(s, GetValue(s) - static_cast<uint32_t>(e));
	return x;
}

bool scaffolds::ValidToApplyThisToObject(entt::entity s, entt::entity target)
{
	auto& registry = Entities();
	if (ScaffoldOf(s) == nullptr || target == entt::null || !registry.Valid(target))
	{
		return false;
	}
	// a scaffold that is not flying
	if (const auto* oc = ScaffoldOf(target); oc != nullptr && !physics::PhysicsObjects::IsFlying(target))
	{
		const auto* c = ScaffoldOf(s);
		// the combined value is at most maxNumberForCombining (8) - (football ? 0 : 1) (unsigned); both may offer any
		// building; o has no site or nothing built
		const uint32_t max = static_cast<uint32_t>(Info().maxNumberForCombining) - (k_FootballEnabled ? 0u : 1u);
		if (GetValue(target) + GetValue(s) <= max && c->limit == Scaffold::k_AnyAbode && oc->limit == Scaffold::k_AnyAbode &&
		    (oc->site == entt::null || building_sites::GetPercentBuilt(oc->site) == 0.0f))
		{
			return true;
		}
	}
	// else any storage pit takes wood
	return registry.AllOf<StoragePit>(target);
}

int scaffolds::ApplyThisToObject(entt::entity s, entt::entity target, const pot_resource::Dropper& hand,
                                 const glm::vec3& handPos)
{
	auto& registry = Entities();
	if (ScaffoldOf(s) == nullptr || target == entt::null || !registry.Valid(target))
	{
		return 0;
	}
	// a scaffold: combine. (pending) two object state bits, taken as clear
	if (IsScaffold(target))
	{
		Combine(s, target);
		// the combine puff ((not ported): openblack's spot visuals keep no player)
		SpotVisual(PosOf(target), k_SpotCombine);
		// with a hand: G_ScaffoldCombine 201 + a 0..3 counter, 3D, untracked, at the hand's point. (approximate) the
		// owner: the original's is this scaffold, already deleted by Combine; openblack destroys the entity at once, so the
		// surviving one owns it
		if (hand.hasInterface)
		{
			const int sample = k_SampleScaffoldCombine + audio::NextCounter(audio::Counter::ScaffoldCombine);
			PlayInGame3D(sample, target, handPos);
		}
		// the hand starts an immersion effect on this return value
		return 1;
	}
	// a storage pit takes the scaffold's wood -> 3
	if (registry.AllOf<StoragePit>(target) && take_resource::StoragePit(target, s, hand))
	{
		return 3;
	}
	return 0;
}

uint32_t scaffolds::GetOverwriteDropToolTip(entt::entity s)
{
	// the plan's build tool tip, else a default one
	const auto* c = ScaffoldOf(s);
	if (c != nullptr && HasPlanOf(*c, false))
	{
		return static_cast<uint32_t>(c->plan->info->toolTipsForBuild);
	}
	return k_DropToolTipNoPlan;
}

void scaffolds::Combine(entt::entity s, entt::entity target)
{
	auto* c = ScaffoldOf(s);
	auto* oc = ScaffoldOf(target);
	if (c == nullptr || oc == nullptr)
	{
		return;
	}
	// both keep the combined town
	const auto combined = GetCombinedTown(s, target);
	c->town = combined;
	oc->town = combined;
	// o with a site -> leaves it
	if (oc->site != entt::null)
	{
		RemoveOldBuildingSite(target);
	}
	// o in flight only: its linear velocity and angular momentum, both whole, then it leaves the physics
	const bool flying = physics::PhysicsObjects::IsFlying(target);
	glm::vec3 velocity {0.0f};
	glm::vec3 momentum {0.0f};
	if (flying)
	{
		if (const auto* po = physics::PhysicsObjects::Find(target); po != nullptr)
		{
			velocity = po->body.velocity;
			momentum = po->body.angularMomentum;
			physics::PhysicsObjects::RemoveObject(target);
		}
	}
	// o takes both values
	SetValue(target, GetValue(target) + GetValue(s));
	if ((c = ScaffoldOf(s)) == nullptr || (oc = ScaffoldOf(target)) == nullptr)
	{
		return;
	}
	// o's plan deleted; this plan takes this phantom's angle and scale and moves to o with the phantom; this one has none
	PlanToBeDeleted(oc->plan);
	if (c->plan.has_value())
	{
		c->plan->yAngle = c->phantomAngle;
		c->plan->scale = c->phantomScale;
	}
	SetPlannedAbode(*oc, c->plan);
	oc->phantomAngle = c->phantomAngle;
	oc->phantomScale = c->phantomScale;
	SetPlannedAbode(*c, std::nullopt);
	// o was flying -> back into the physics with v; a body gets L back
	if (flying)
	{
		if (auto* po = physics::PhysicsObjects::AddObject(target, velocity, glm::vec3(0.0f)); po != nullptr)
		{
			po->body.angularMomentum = momentum;
		}
	}
	// this one deleted, then o builds for this one's holder (read after the delete in the original: kept before here)
	const auto holder = c->holder;
	ToBeDeleted(s);
	if (BuildBuilding(target, holder, false))
	{
		SetPhantomMeshAndPosition(target);
	}
}

// =====================================================================================================================
// the tap
// =====================================================================================================================

bool scaffolds::ValidToTap(entt::entity s)
{
	const auto* c = ScaffoldOf(s);
	// value > 1, not flying, no site or still adjustable, free to offer any building
	return c != nullptr && GetValue(s) > 1 && !physics::PhysicsObjects::IsFlying(s) &&
	       (c->site == entt::null || CanStillBeAdjusted(s)) && c->limit == Scaffold::k_AnyAbode;
}

uint32_t scaffolds::Tap(entt::entity s, glm::vec3 handPos)
{
	// a scaffold with a site leaves it; then split
	if (const auto* c = ScaffoldOf(s); c != nullptr && c->site != entt::null)
	{
		RemoveOldBuildingSite(s);
	}
	Split(s);
	// G_ScaffoldTap 151 + a 0..3 counter, InGame bank, owned by this one, 3D, untracked, at the hand's point
	PlayInGame3D(k_SampleScaffoldTap + audio::NextCounter(audio::Counter::ScaffoldTap), s, handPos);
	return 1;
}

void scaffolds::Split(entt::entity s)
{
	auto& registry = Entities();
	auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return;
	}
	const auto pos = PosOf(s);
	// x: a new scaffold of this town at pos
	const auto x = Create(pos, c->town, entt::null, 0.0f, 1.0f);
	if (x == entt::null)
	{
		return;
	}
	// off = GetPosFromAngle(Y angle + GameRand(4) x pi / 2, 2D radius): the product first, then the angle added
	const auto quarter = static_cast<float>(game_random::GameRand(k_SplitQuarters));
	const float turn = quarter * k_HalfPi;
	const auto& transform = registry.Get<const Transform>(s);
	// (inferred) the Transform's Y angle, as the map cells read it (map_cells::detail::YAngleOf)
	const float a = map_cells::detail::YAngleOf(transform.rotation) + turn;
	const auto off = gutils::GetPosFromAngle(a, object::Get2DRadius(s));
	// x moves to pos + off with this one's angles
	map_cells::RemoveMapObject(x);
	if (auto* xt = registry.TryGet<Transform>(x); xt != nullptr)
	{
		xt->position = map_coords::ToWorld(pos + off);
		xt->rotation = registry.Get<const Transform>(s).rotation;
	}
	map_cells::InsertMapObject(x);
	// x is worth 1
	SetValue(x, 1);
	// An owner workshop with space in its store takes x into its first free slot: x gets the slot bits and the
	// workshop as owner. workshops::ScaffoldMoved marks the slot as taken away (1) once x has its bits
	if ((c = ScaffoldOf(s)) == nullptr)
	{
		return;
	}
	if (const auto w = WorkshopOwner(*c); w != entt::null && workshops::GetSpaceInStore(w) != 0)
	{
		const auto slot = workshops::GetFirstFreeSlot(w);
		workshops::AddScaffold(w, x);
		SetSlot(x, slot);
		SetOwner(x, w);
		workshops::ScaffoldMoved(w, x);
	}
	// in flight only: L = half the angular momentum, v = the linear velocity (whole); this one leaves the physics. The
	// same pattern as a rock splitting in two (PhysicsObjects.cpp)
	glm::vec3 velocity {0.0f};
	glm::vec3 momentum {0.0f};
	if (physics::PhysicsObjects::IsFlying(s))
	{
		if (const auto* po = physics::PhysicsObjects::Find(s); po != nullptr)
		{
			momentum = po->body.angularMomentum * k_SplitShare;
			velocity = po->body.velocity;
		}
		physics::PhysicsObjects::RemoveObject(s);
	}
	// x always enters the physics with v; a body gets L
	if (auto* po = physics::PhysicsObjects::AddObject(x, velocity, glm::vec3(0.0f)); po != nullptr)
	{
		po->body.angularMomentum = momentum;
	}
	// the fire spreads to x. TODO(Fire): not ported
	// this one loses 1
	SetValue(s, GetValue(s) - 1);
	// this one moves to pos - off. (not ported) the original also resets its 3D object's matrix and two draw values:
	// openblack's Transform is both the object's scale (the value) and the drawn one, so only the position moves
	MoveTo(s, pos - off);
	// back into the physics with v and L when it can be a physics object
	if (physics::PhysicsObjects::CanBecomeAPhysicsObject(s))
	{
		if (auto* po = physics::PhysicsObjects::AddObject(s, velocity, glm::vec3(0.0f)); po != nullptr)
		{
			po->body.angularMomentum = momentum;
		}
	}
}

void scaffolds::RegisterTapHandler()
{
	// the class's valid-to-tap and tap handlers
	hand_tap::Register<Scaffold>(
	    [](entt::entity s, const pot_resource::Dropper&) { return ValidToTap(s); },
	    [](entt::entity s, const pot_resource::Dropper&, glm::vec3 handPos) -> uint32_t { return Tap(s, handPos); });
}

// =====================================================================================================================
// the physics
// =====================================================================================================================

bool scaffolds::CanBecomePhysicsObject(entt::entity s)
{
	const auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return true;
	}
	// snapped in a workshop slot -> no (GetSlotState gives 0, free, for a workshop entity already gone)
	if (c->owner != entt::null && workshops::GetSlotState(c->owner, GetSlot(s)) == Workshop::k_SlotOccupied)
	{
		return false;
	}
	// a site -> no; else as any mobile object (yes)
	return c->site == entt::null;
}

void scaffolds::OnBeforeInitialisePhysicsFromHand(entt::entity s)
{
	// the destroy flag -> DestroyThingsInWay
	if (const auto* c = ScaffoldOf(s); c != nullptr && (c->flags & Scaffold::k_Destroy) != 0)
	{
		DestroyThingsInWay(s);
	}
}

void scaffolds::OnInitialisePhysicsFromHand(entt::entity s, const physics::PhysicsObject& po, bool dontReplant)
{
	// dropped gently = landed && !dontReplant
	if (auto* c = ScaffoldOf(s); c != nullptr)
	{
		const bool gentle = (po.flags & physics::PhysicsObject::k_Landed) != 0 && !dontReplant;
		const uint16_t bit = gentle ? Scaffold::k_DroppedGently : 0;
		c->flags = static_cast<uint16_t>((c->flags & ~Scaffold::k_DroppedGently) | bit);
	}
}

void scaffolds::OnInitialisePhysics(entt::entity s, [[maybe_unused]] physics::PhysicsObject& po, bool fromHand)
{
	// A body with an owner workshop tells it the scaffold moved. The hand's path enters the physics directly and does
	// not come here
	if (fromHand)
	{
		return;
	}
	if (const auto* c = ScaffoldOf(s); c != nullptr && c->owner != entt::null && Entities().Valid(c->owner))
	{
		workshops::ScaffoldMoved(c->owner, s);
	}
}

entt::entity scaffolds::OnEndPhysics(entt::entity s, physics::PhysicsObject& po)
{
	// With a player, the creature could learn to mimic the drop as BUILD_HOUSE ((not ported)).
	// BackInMap runs first: back in the map, or deleted outside it (the scaffold part through the dead-list hook). The
	// original goes on with the rest either way (only its result is kept); (openblack guard) without the deferred
	// deletion the entity is already destroyed here, so only then the rest is skipped
	const bool inMap = physics::PhysicsObjects::BackInMap(s);
	auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return inMap ? s : entt::null;
	}
	// insert (always true in openblack): snapped = the town's workshops took it into a slot
	const auto town = GetTownForBuilding(s);
	const bool snapped = town != entt::null && workshops::CheckScaffoldSnapToPoint(town, s);
	if ((c = ScaffoldOf(s)) == nullptr)
	{
		return s;
	}
	// From the hand and not snapped: a dropped-gently scaffold that builds sets the phantom; otherwise (also when
	// BuildBuilding fails) the "no building here" puff
	if ((po.flags & physics::PhysicsObject::k_FromHand) != 0 && !snapped)
	{
		bool built = false;
		if ((c->flags & Scaffold::k_DroppedGently) != 0 && BuildBuilding(s, c->holder, (c->flags & Scaffold::k_Destroy) != 0))
		{
			SetPhantomMeshAndPosition(s);
			built = true;
		}
		if (!built)
		{
			SpotVisual(PosOf(s), k_SpotNoBuilding);
		}
	}
	// every insert (both paths of the test above join here): no holder, placed now
	if ((c = ScaffoldOf(s)) != nullptr)
	{
		c->holder = std::nullopt;
		c->placedTurn = game_clock::Turn();
	}
	// always clear the dropped-gently flag; return the result
	if ((c = ScaffoldOf(s)) != nullptr)
	{
		c->flags = static_cast<uint16_t>(c->flags & ~Scaffold::k_DroppedGently);
	}
	return inMap ? s : entt::null;
}

void scaffolds::RegisterPhysicsHandlers()
{
	// each call writes the class's slot again with the same handlers (PhysicsObjects::SetClassHandlers)
	physics::PhysicsObjects::ClassHandlers handlers;
	// CanBecomeAPhysicsObject
	handlers.canBecomePhysicsObject = [](entt::entity s) { return CanBecomePhysicsObject(s); };
	// InitialisePhysicsFromHand: one part before the generic object's, one after it
	handlers.beforeInitialisePhysicsFromHand = [](entt::entity s) { OnBeforeInitialisePhysicsFromHand(s); };
	handlers.initialisedPhysicsFromHand = [](entt::entity s, const physics::PhysicsObject& po, bool dontReplant) {
		OnInitialisePhysicsFromHand(s, po, dontReplant);
	};
	// InitialisePhysics
	handlers.initialisePhysics = [](entt::entity s, physics::PhysicsObject& po, bool fromHand) {
		OnInitialisePhysics(s, po, fromHand);
	};
	// EndPhysics: calls PhysicsObjects::BackInMap itself
	handlers.endPhysics = [](entt::entity s, physics::PhysicsObject& po) { return OnEndPhysics(s, po); };
	handlers.callsBackInMap = true;
	physics::PhysicsObjects::SetClassHandlers(physics::PhysicsClass::Scaffold, std::move(handlers));
}

void scaffolds::DestroyThingsInWay(entt::entity s)
{
	auto& registry = Entities();
	const auto* c = ScaffoldOf(s);
	if (c == nullptr)
	{
		return;
	}
	// own = the 2D radius; R = with a plan, its mesh's 2D radius x its scale, else own. The temporary object the
	// original makes for this is never freed (a leak; not ported)
	const float own = object::Get2DRadius(s);
	float big = own;
	if (HasPlanOf(*c, false))
	{
		big = object::MeshRadius2D(MeshOf(c->plan->info), c->plan->scale);
	}
	// 36 cells of the spiral from pos, no bounds test; each cell's objects (fixed then mobile)
	auto coords = PosOf(s);
	const auto self = coords;
	map_coords::Spiral spiral;
	for (int32_t n = 0; n < k_DestroyCells; ++n)
	{
		for (const auto o : map_cells::ObjectsInCell(map_coords::Cell(coords)))
		{
			if (!registry.Valid(o))
			{
				continue;
			}
			// lim = abode ? R : own; dist(pos, o) - o's 2D radius <= lim
			const bool abode = registry.AllOf<Abode>(o);
			const float lim = abode ? big : own;
			const float dist = gutils::GetDistanceInMetres(self, PosOf(o));
			const float d = dist - object::Get2DRadius(o);
			if (!(d <= lim))
			{
				continue;
			}
			// not a multi-map fixed class and able to be a physics object -> pushed; else deleted. (pending) one object
			// state bit, taken as clear
			if (!map_cells::IsMultiCellStaticClass(o) && physics::PhysicsObjects::CanBecomeAPhysicsObject(o))
			{
				// e = the nearest edge of o, p = pos; dx = (e.x - p.x) x 2, dz = (e.z - p.z) x 2; m = dx > dz ? dx : dz
				// (signed); v = (dx / m x 2, 0.2, dz / m x 2)
				const auto e = map_coords::ToWorld(object::GetNearestPosOfObject(s, o));
				const auto p = map_coords::ToWorld(self);
				const float ex = e.x - p.x;
				const float ez = e.z - p.z;
				const float dx = ex + ex;
				const float dz = ez + ez;
				const float m = dx > dz ? dx : dz;
				const float qx = dx / m;
				const float qz = dz / m;
				const glm::vec3 velocity {qx + qx, k_PushVelocityY, qz + qz};
				// the point (dx + p.x, 0.5 + p.y, dz + p.z) is written INTO the spiral's coords (literal: the rest of the
				// spiral goes on from it), then o moves there
				const auto p2 = map_coords::ToWorld(self);
				const glm::vec3 point {dx + p2.x, k_PushRise + p2.y, dz + p2.z};
				coords = map_coords::FromWorld(point);
				map_cells::MoveMapObject(o, map_coords::ToWorld(coords));
				// o enters the physics with v
				physics::PhysicsObjects::AddObject(o, velocity, glm::vec3(0.0f));
				continue;
			}
			// else: an abode is kept out of the plans; o deleted
			if (abode)
			{
				abodes::SetShouldNotBeAddedToPlanned(o, true);
			}
			DeleteObject(o);
		}
		map_coords::AddCells(coords, spiral.Next());
	}
}

} // namespace openblack::ecs
