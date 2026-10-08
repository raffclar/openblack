/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BuildingSites.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <bit>
#include <limits>

#include <glm/gtc/constants.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/ObjectMatrix.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/Graveyard.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownPlacement.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/Workshops.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerScript.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Objects/MagicPiles.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

// Plans and building sites of a town (see BuildingSites.h)

namespace openblack::ecs
{
using namespace components;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

BuildingSite* SiteComponent(entt::entity site)
{
	auto& registry = Entities();
	return site != entt::null && registry.Valid(site) ? registry.TryGet<BuildingSite>(site) : nullptr;
}

Town* TownComponent(entt::entity town)
{
	auto& registry = Entities();
	return town != entt::null && registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr;
}

/// The citadel part of a site, null for a standard site
CitadelBuildingSite* CitadelSiteComponent(entt::entity site)
{
	auto& registry = Entities();
	return site != entt::null && registry.Valid(site) ? registry.TryGet<CitadelBuildingSite>(site) : nullptr;
}

/// A citadel heart plan's GCitadelHeartInfo; null for any other plan
const GCitadelHeartInfo* HeartInfoOf(entt::entity town, plans::PlanIndex plan)
{
	const auto* t = TownComponent(town);
	if (t == nullptr || plan >= t->plannedAbodes.size() || !t->plannedAbodes.at(plan).citadelHeart)
	{
		return nullptr;
	}
	return &archetypes::CitadelArchetype::HeartInfo(t->plannedAbodes.at(plan).heartInfo);
}

// ---- constants ------------------------------------------------------------------------------------------------------

/// The repair base 1.1 x life - 0.1 (Create, and abode damage)
constexpr float k_RepairBaseScale = 1.1f;
constexpr float k_RepairBaseOffset = 0.1f;
/// 2 pi / 128: the ring's angle step
constexpr float k_RingStep = std::bit_cast<float>(0x3D490FDBu);
/// The "low edge" bound: a triangle's two lowest vertices' LOCAL y must be below it (strict)
constexpr float k_LowEdgeY = 2.5f;
/// GetNearestEdge's bounds: below -6 pi -> index 0; above 6 pi -> pi (the same result)
constexpr float k_MinusSixPi = std::bit_cast<float>(0xC196CBE4u);
constexpr float k_SixPi = std::bit_cast<float>(0x4196CBE4u);
/// 1 / 2 pi: GetNearestEdge's index = (angle x it x 128 truncated toward zero) & 0x7F
constexpr float k_InvTwoPi = std::bit_cast<float>(0x3E22F983u);
constexpr int32_t k_RingMask = 0x7F;
/// GetRandomBuildPos: GameFloatRand(pi / 2) - pi / 4, +-45 deg
constexpr float k_RandomBuildSpread = std::bit_cast<float>(0x3FC90FDBu);
constexpr float k_RandomBuildHalf = std::bit_cast<float>(0x3F490FDBu);
/// GetNextPosFromIndex: step = 2.0 / (Get2DRadius x 2 pi x 0.0078125)
constexpr float k_NextPosMetres = 2.0f;
constexpr float k_NextPosPerEntry = 0.0078125f;
/// GetClearAreaRadius's factor
constexpr float k_ClearAreaFactor = 1.2f;
/// ShouldIGetWood's "close to the building" distance
constexpr float k_CloseToSite = 50.0f;
/// ShouldIGetWood's GetDistanceModifier maximum
constexpr float k_WoodDistanceMax = 5000.0f;
/// The wood pile's angle: pi / 8 - GameFloatRand(pi / 4), +-22.5 deg; and 4.0 m past the door's distance
/// (GetResourcePosAndYAngle)
constexpr float k_PileHalfSpread = std::bit_cast<float>(0x3EC90FDBu);
constexpr float k_PileSpread = std::bit_cast<float>(0x3F490FDBu);
constexpr float k_PileBeyondDoor = 4.0f;
/// ToBeDeleted: the builders walk out to Get2DRadius + 2
constexpr float k_BuilderClearance = 2.0f;
/// GetBestBuildingSite's start score
constexpr float k_BestSiteStart = 99999.0f;
/// GetBestBuildingSite's weight GetDesireForVillagers x 0.9 + 0.1
constexpr float k_SiteWeightScale = 0.9f;
constexpr float k_SiteWeightOffset = 0.1f;
/// CheckWhenNewBuildingCreated's clearance round the congregation point
constexpr float k_CongregationClearance = 7.5f;
/// GetDesireToBeBuilt's constants; a wonder with 7 scaffolds or more is 1.0
constexpr float k_HouseCap = 0.8f;
constexpr float k_HouseShortageFloor = -10.0f;
constexpr float k_HouseShare = 0.2f;
constexpr float k_HouseBase = 0.6f;
constexpr float k_HouseCountFloor = 10.0f;
constexpr float k_ScaffoldPenalty = 0.3f;
constexpr uint32_t k_WonderScaffolds = 7;
/// RequestBestPlanned's mask 4 (abode type bit 2: the civic ones and the fields), RequestANewAbode's mask 2 (the living
/// quarters)
constexpr uint32_t k_CivicMask = 4;
constexpr uint32_t k_AbodeMask = 2;
/// The football flag is off at a single-player start; openblack has no football pitch (GetDesireToBeBuilt's football
/// case then returns 0)
constexpr bool k_FootballEnabled = false;
/// TownDesire info 14 (ToBuildWonder) for GetDesireToBeBuilt's wonder case
constexpr auto k_WonderDesire = TownDesireInfo::ToBuildWonder;
/// A citadel site's pile: the worship site angle - 1.1424 rad, 22.0 m out
constexpr float k_CitadelPileAngle = std::bit_cast<float>(0x3F923A14u);
constexpr float k_CitadelPileMetres = std::bit_cast<float>(0x41B00000u);
/// The worship site slots: the heart's Y angle + slot x 2 pi / 7
constexpr float k_WorshipSlotAngle = std::bit_cast<float>(0x3F65C8FAu);
/// A worship site's pile at the local point (9, 0, -50)
constexpr glm::vec3 k_WorshipSitePile {9.0f, 0.0f, -50.0f};

/// The town's totem (read by GetDesireToBeBuilt's totem and town centre cases). (pending) its writer is not read
/// (inferred: when a totem becomes functional); openblack keeps none: null
entt::entity TownTotem(entt::entity)
{
	return entt::null;
}
/// The football pitch (football case). (not ported) null
entt::entity TownFootball(entt::entity)
{
	return entt::null;
}
/// The workshops (the spell dispenser case tests the count): the head, null when none (ecs::workshops)
entt::entity TownWorkshops(entt::entity town)
{
	const auto* t = TownComponent(town);
	return t != nullptr && !t->workshops.empty() ? t->workshops.front() : entt::null;
}

/// The plan's position as a MapCoords: x and z of the script's point (the altitude is not read)
map_coords::MapCoords PlanCoords(const PlannedAbode& plan)
{
	return map_coords::FromMetres(glm::vec2(plan.position.x, plan.position.z));
}

/// When the new building stands closer than 7.5 m to the congregation point (less its 2D radius), the point's cache is
/// reset to (0, 0, 0)
void CheckWhenNewBuildingCreated(entt::entity town, entt::entity building)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	const map_coords::MapCoords congregation {t->congregationPos.x, t->congregationPos.y, t->congregationPosY};
	if (gutils::GetDistanceInMetres(object::MapCoordsOf(building), congregation) - object::Get2DRadius(building) <
	    k_CongregationClearance)
	{
		t->congregationPos = {0, 0};
		t->congregationPosY = 0.0f;
	}
}

/// Steps 1..4 of turning a plan into a building, for the town's plan (plans::CreatePlannedNoFixedCheck, which then
/// deletes it from the list) or a scaffold's, in no list (building_sites::AddBuildingSiteNoFixedCheck(town, plan)). A
/// citadel heart plan is converted before, by its caller (CitadelArchetype::CreatePlannedNoFixedCheck). Returns the
/// building or null (the plan survives)
entt::entity CreateBuildingFromPlan(entt::entity town, const PlannedAbode& p)
{
	const auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return entt::null;
	}
	const uint32_t townId = t->id;
	// 1. The abode under construction (percent = life, food and wood 0; not functional as it is not built) with its
	// surrounding objects; a town centre plan makes the town centre directly. (approximate) both are
	// AbodeArchetype::Create under construction (it has no init / surrounding-objects split)
	const auto building = archetypes::AbodeArchetype::Create(townId, p.position, p.info, p.yAngleRadians, p.scale, 0, 0, true);
	// 2. none -> 0 (the plan survives)
	if (building == entt::null)
	{
		return entt::null;
	}
	// 3. The footpath link moves to the building (TODO(footpaths): not ported); with the plan's town,
	// CheckWhenNewBuildingCreated
	CheckWhenNewBuildingCreated(town, building);
	// 4. Not for a town centre plan: a rebuild plan marks the building not repaired
	if (!p.townCentre && p.wasBuilt)
	{
		if (auto* a = Entities().TryGet<Abode>(building); a != nullptr)
		{
			a->buildFlags |= Abode::k_NotRepaired;
		}
	}
	return building;
}

/// P inside the XZ box of A..B, both ends inclusive; y not tested
bool InsideXZ(const glm::vec3& a, const glm::vec3& b, const glm::vec3& p)
{
	return std::min(a.x, b.x) <= p.x && p.x <= std::max(a.x, b.x) && std::min(a.z, b.z) <= p.z && p.z <= std::max(a.z, b.z);
}

/// The ray segment c -> P2 against the edge L -> S in XZ: the line L + t u against the vertical plane through c with
/// normal N = (-D.z k, 0, D.x k); the hit's y is interpolated along the edge. 1 / sqrt exact (a fast inverse square
/// root would not move it)
bool EdgeHit(const glm::vec3& l, const glm::vec3& s, const glm::vec3& u, const glm::vec3& c, const glm::vec3& p2,
             glm::vec3& hit)
{
	const float dx = p2.x - c.x;
	const float dz = p2.z - c.z;
	const float k = 1.0f / std::sqrt(dx * dx + dz * dz);
	const float nx = -dz * k;
	const float nz = dx * k;
	const float den = nz * u.z + nx * u.x;
	if (den == 0.0f)
	{
		return false;
	}
	const float t = ((c.x - l.x) * nx + (c.z - l.z) * nz) / den;
	hit = l + t * u;
	return InsideXZ(l, s, hit) && InsideXZ(c, p2, hit);
}

/// Builds the ring of 128 points round the building where the builders stand, from openblack's L3D
void PosBuilderProcess(std::array<glm::vec3, BuildingSite::k_RingSize>& ring, entt::entity building, bool circle)
{
	auto& registry = Entities();
	const auto* transform = registry.TryGet<const Transform>(building);
	const auto* meshComponent = registry.TryGet<const Mesh>(building);
	const auto meshId = meshComponent != nullptr ? meshComponent->id : entt::id_type {0};
	// (no 3D object -> nothing; the ring stays zero-filled)
	if (transform == nullptr || meshId == 0 || !Locator::resources::has_value() ||
	    !Locator::resources::value().GetMeshes().Contains(meshId))
	{
		return;
	}
	const auto& mesh = *Locator::resources::value().GetMeshes().Handle(meshId);
	// 1. every entry = the translation; the best distances = 0
	ring.fill(transform->position);
	std::array<float, BuildingSite::k_RingSize> best {};
	// 2. c = the matrix x the mesh's bounding-box centre; R = scale x the mesh's half diagonal
	const glm::mat4 toWorld = affine::Model(*transform);
	const auto box = mesh.GetBoundingBox();
	const glm::vec3 centre = glm::vec3(toWorld * glm::vec4(box.Center(), 1.0f));
	const float radius = object::GetScale(building) * object::MeshHalfDiagonal(meshId);
	if (circle)
	{
		// 3. a accumulated, x = sin a x R + c.x, z = cos a x R + c.z (the mirror of the mesh branch, literal), y = the
		// altitude at (x x 65536 x 0.1, z x 65536 x 0.1), both truncated toward zero; no post-pass
		float a = 0.0f;
		for (auto& entry : ring)
		{
			const float x = std::sin(a) * radius + centre.x;
			const float z = std::cos(a) * radius + centre.z;
			// the two products before the truncation, each rounded to float: the original's FPU runs at 24-bit precision
			const map_coords::MapCoords at {map_coords::FtoL(x * 65536.0f * 0.1f), map_coords::FtoL(z * 65536.0f * 0.1f), 0.0f};
			entry = glm::vec3(x, map_coords::ToWorld(at).y, z);
			a = a + k_RingStep;
		}
		return;
	}
	// 4. every triangle of every primitive of every sub-mesh, in file order
	for (const auto& sub : mesh.GetSubMeshes())
	{
		const auto& positions = sub->GetCollisionPositions();
		const auto& indices = sub->GetCollisionIndices();
		for (const auto& [first, count] : sub->GetCollisionRanges())
		{
			for (uint32_t k = first; k + 2 < first + count && k + 2 < indices.size(); k += 3)
			{
				const std::array<glm::vec3, 3> v {positions.at(indices.at(k)), positions.at(indices.at(k + 1)),
				                                  positions.at(indices.at(k + 2))};
				// the lowest (L) and the second lowest (S) by local y
				size_t lo = 0;
				size_t second = 0;
				if (v[0].y <= v[1].y)
				{
					if (v[2].y <= v[0].y)
					{
						lo = 2;
						second = 0;
					}
					else
					{
						lo = 0;
						second = v[2].y <= v[1].y ? 2 : 1;
					}
				}
				else if (v[2].y <= v[1].y)
				{
					lo = 2;
					second = 1;
				}
				else
				{
					lo = 1;
					second = v[0].y <= v[2].y ? 0 : 2;
				}
				if (!(v[lo].y < k_LowEdgeY && v[second].y < k_LowEdgeY))
				{
					continue;
				}
				// to world, u = normalize(S - L) in 3D
				const glm::vec3 l = glm::vec3(toWorld * glm::vec4(v[lo], 1.0f));
				const glm::vec3 s = glm::vec3(toWorld * glm::vec4(v[second], 1.0f));
				const glm::vec3 d = s - l;
				const float length = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
				if (length == 0.0f)
				{
					continue; // (openblack, guard) a degenerate edge: the original's u would be NaN and never hit
				}
				const glm::vec3 u = d / length;
				for (size_t i = 0; i < ring.size(); ++i)
				{
					// a = (float)i x step, P2 = (c.x + R cos a, c.y, c.z + R sin a)
					const float a = static_cast<float>(i) * k_RingStep;
					const glm::vec3 p2(centre.x + radius * std::cos(a), centre.y, centre.z + radius * std::sin(a));
					glm::vec3 hit;
					if (EdgeHit(l, s, u, centre, p2, hit))
					{
						const float hx = hit.x - centre.x;
						const float hz = hit.z - centre.z;
						const float q = hx * hx + hz * hz;
						if (q > best.at(i)) // strict: ties keep the first
						{
							ring.at(i) = hit;
							best.at(i) = q;
						}
					}
				}
			}
		}
	}
	// 5. each entry pushed 1 m outwards along its 3D direction from c (no hit: from the translation)
	for (auto& entry : ring)
	{
		const glm::vec3 d = entry - centre;
		if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f)
		{
			const float l = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
			entry = centre + d * (l + 1.0f) / l;
		}
	}
}

/// A building with a 3D object gets its ring, a circle for a football pitch
void ComputeRing(entt::entity site)
{
	auto* s = SiteComponent(site);
	const auto building = building_sites::GetBuilding(site);
	if (s == nullptr || building == entt::null)
	{
		return;
	}
	PosBuilderProcess(s->ring, building, abodes::TypeOf(building) == AbodeType::FootballPitch);
}

/// The ring entry as a MapCoords, as the villager and GetNearestEdge read it: (x x 6553.6, z x 6553.6, 0), truncated
/// toward zero
map_coords::MapCoords RingCoords(const BuildingSite& s, size_t index)
{
	const auto& point = s.ring.at(index);
	return {map_coords::ToFixed(point.x), map_coords::ToFixed(point.z), 0.0f};
}

/// Deletes a pile: ecs::ToBeDeleted (out of the physics and the map cells, then the entity)
void DeletePile(entt::entity pile)
{
	ecs::ToBeDeleted(pile);
}
} // namespace

// =====================================================================================================================
// plans
// =====================================================================================================================

size_t plans::PlansOf(entt::entity town)
{
	const auto* t = TownComponent(town);
	return t != nullptr ? t->plannedAbodes.size() : 0;
}

plans::PlanIndex plans::AddPlanned(entt::entity town, PlannedAbode plan)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return 0;
	}
	// stamps the creation turn; added at the tail
	plan.creationTurn = game_clock::Turn();
	t->plannedAbodes.push_back(plan);
	return t->plannedAbodes.size() - 1;
}

void plans::RemovePlanned(entt::entity town, PlanIndex plan)
{
	// Out of the list (town_stats::Compute counts the list)
	if (auto* t = TownComponent(town); t != nullptr && plan < t->plannedAbodes.size())
	{
		t->plannedAbodes.erase(t->plannedAbodes.begin() + static_cast<std::ptrdiff_t>(plan));
	}
}

const GAbodeInfo* plans::InfoOf(entt::entity town, PlanIndex plan)
{
	const auto* t = TownComponent(town);
	if (t == nullptr || plan >= t->plannedAbodes.size())
	{
		return nullptr;
	}
	const auto& infos = Locator::infoConstants::value().abode;
	const auto i = static_cast<size_t>(static_cast<int32_t>(t->plannedAbodes.at(plan).info));
	return i < infos.size() ? &infos.at(i) : nullptr;
}

map_coords::MapCoords plans::CoordsOf(entt::entity town, PlanIndex plan)
{
	const auto* t = TownComponent(town);
	return t != nullptr && plan < t->plannedAbodes.size() ? PlanCoords(t->plannedAbodes.at(plan)) : map_coords::MapCoords {};
}

float plans::ScaleOf(entt::entity town, PlanIndex plan)
{
	const auto* t = TownComponent(town);
	return t != nullptr && plan < t->plannedAbodes.size() ? t->plannedAbodes.at(plan).scale : 0.0f;
}

AbodeType plans::GetAbodeType(entt::entity town, PlanIndex plan)
{
	// A citadel heart plan is a Citadel (bit 2: GetBestPlanned's mask 4 takes it)
	if (HeartInfoOf(town, plan) != nullptr)
	{
		return AbodeType::Citadel;
	}
	// Its info's abode type (the generic plan type is never a town's plan here)
	const auto* info = InfoOf(town, plan);
	return info != nullptr ? info->abodeType : AbodeType::General;
}

bool plans::IsCivic(entt::entity town, PlanIndex plan)
{
	const auto* t = TownComponent(town);
	if (t == nullptr || plan >= t->plannedAbodes.size())
	{
		return false;
	}
	if (t->plannedAbodes.at(plan).citadelHeart)
	{
		return false; // a citadel heart plan is never civic
	}
	if (t->plannedAbodes.at(plan).townCentre)
	{
		return true; // a town centre plan is always civic
	}
	return town_stats::IsCivic(GetAbodeType(town, plan));
}

float plans::GetDesireToBeRepaired(entt::entity town, PlanIndex plan)
{
	// built ? the info's desireToBeRepaired : 0.0 (a citadel heart plan's info: its GCitadelHeartInfo)
	const auto* t = TownComponent(town);
	if (const auto* heart = HeartInfoOf(town, plan); heart != nullptr)
	{
		return t->plannedAbodes.at(plan).wasBuilt ? heart->desireToBeRepaired : 0.0f;
	}
	const auto* info = InfoOf(town, plan);
	if (t == nullptr || info == nullptr || !t->plannedAbodes.at(plan).wasBuilt)
	{
		return 0.0f;
	}
	return info->desireToBeRepaired;
}

float plans::GetDesireToBeBuilt(entt::entity town, const GAbodeInfo& info, uint32_t scaffolds)
{
	return GetDesireToBeBuilt(town, info, info.abodeType, &info, scaffolds);
}

float plans::GetDesireToBeBuilt(entt::entity town, const GMultiMapFixedInfo& info, AbodeType abodeType, const GAbodeInfo* abode,
                                uint32_t scaffolds)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return 0.0f;
	}
	// b = the info's desire to be built; the type is its abode type (Citadel for the citadel heart)
	float b = info.desireToBeBuilt;
	const auto type = static_cast<uint32_t>(abodeType);
	// s = the sites whose building has that abode type
	uint32_t s = 0;
	for (const auto site : t->buildingSites)
	{
		const auto building = building_sites::GetBuilding(site);
		if (building == entt::null)
		{
			continue; // (openblack, guard) the original would call through a NULL building
		}
		if (const auto other = abodes::TypeOf(building); other.has_value() && static_cast<uint32_t>(*other) == type)
		{
			++s;
		}
	}
	// b == 0 -> b
	if (b == 0.0f)
	{
		return b;
	}
	const bool neutral = t->owner == PlayerNames::NEUTRAL;
	float r = 0.0f;
	bool divide = true; // the early exits only skip the division by s
	switch (type)
	{
	case 0x2: // LIVING_QUARTERS
	{
		if (abode == nullptr)
		{
			break; // (openblack, guard) only a GAbodeInfo has this type
		}
		// freePlaces = the free adult places - the homeless, signed
		const int32_t freePlaces = t->stats.freeAdultPlaces - static_cast<int32_t>(t->homelessVillagers.size());
		// u = (adults + children) / 10 + 1, unsigned
		const uint32_t u = (t->stats.adults + t->stats.children) / 10 + 1;
		// a signed compare
		if (freePlaces > static_cast<int32_t>(u) && scaffolds == 0)
		{
			b = 0.0f;
		}
		else if (freePlaces < 0)
		{
			const float v = std::min(b - static_cast<float>(freePlaces) / static_cast<float>(u), k_HouseCap);
			const auto q =
			    static_cast<uint32_t>(map_coords::FtoL(-std::max(static_cast<float>(freePlaces), k_HouseShortageFloor)));
			const uint32_t maxVillagers = abode->maxVillagersInAbode;
			// unsigned compare
			const auto qf = static_cast<float>(q);
			const auto mf = static_cast<float>(maxVillagers);
			const float share = q > maxVillagers ? mf / qf * k_HouseShare : qf / mf * k_HouseShare + k_HouseShare;
			b = (share + k_HouseBase) * v;
		}
		// m = the town's count of abodes with this abode number; b -= b / max(m + 1, 10) x m
		const auto number = static_cast<size_t>(static_cast<int32_t>(abode->abodeNumber));
		const float m = number < t->stats.abodesByNumber.size() ? static_cast<float>(t->stats.abodesByNumber.at(number)) : 0.0f;
		b = b - b / std::max(m + 1.0f, k_HouseCountFloor) * m;
		r = b;
		break;
	}
	case 0x14: // TOTEM
		if (neutral || TownTotem(town) != entt::null || s != 0)
		{
			r = 0.0f;
		}
		else if (t->centre == entt::null)
		{
			r = b;
			divide = false;
		}
		break;
	case 0x24: // STORAGE_PIT
		if (town_queries::GetStoragePit(town) == entt::null && s == 0)
		{
			r = b;
			divide = false;
		}
		break;
	case 0x44: // CRECHE
		if (t->creche == entt::null && s == 0)
		{
			r = b;
			divide = false;
		}
		break;
	case 0x84: // WORKSHOP: the count of abodes of number Workshop
	{
		const float v = neutral ? 0.0f : b;
		if (t->stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::Workshop)) > 0 || s != 0)
		{
			r = v * 0.5f;
		}
		else
		{
			r = v;
			divide = false;
		}
		break;
	}
	case 0x100: // WONDER
		if (scaffolds >= k_WonderScaffolds)
		{
			r = 1.0f;
		}
		else
		{
			// the town's wonder desire against its DesireTriggersVillagerAction
			const float d = town_desire::GetDesire(town, k_WonderDesire);
			const auto& desireInfo = Locator::infoConstants::value().townDesire.at(static_cast<size_t>(k_WonderDesire));
			r = d < desireInfo.desireTriggersVillagerAction ? 0.0f : b * d;
		}
		break;
	case 0x204: // GRAVEYARD
		r = graveyard::GetGraveyard(town) != entt::null ? 0.0f : b;
		break;
	case 0x404: // TOWN_CENTRE: the centre, then the totem
		if (t->centre == entt::null && s == 0 && TownTotem(town) == entt::null)
		{
			r = b;
			divide = false;
		}
		break;
	case 0x1004: // FOOTBALL: returns 0 at once (no n-correction) when the football is off
		if constexpr (k_FootballEnabled)
		{
			if (TownFootball(town) == entt::null && s == 0)
			{
				r = b;
				divide = false;
			}
			break;
		}
		else
		{
			return 0.0f;
		}
	case 0x2004: // SPELL_DISPENSER: centre, storage pit, graveyard, creche and workshops
		r = (t->centre != entt::null && town_queries::GetStoragePit(town) != entt::null &&
		     graveyard::GetGraveyard(town) != entt::null && t->creche != entt::null && TownWorkshops(town) != entt::null)
		        ? b
		        : 0.0f;
		break;
	default: // citadel, field, windmill, ...
		r = b;
		break;
	}
	// common: r / s
	if (divide && s != 0)
	{
		r = r / static_cast<float>(s);
	}
	// c = (float)(uint32)(n - ScaffoldsRequired) x 0.3 x r; r -= min(c, r). n below ScaffoldsRequired wraps to a huge
	// value and r becomes 0 (literal)
	if (scaffolds != 0)
	{
		const float c = static_cast<float>(static_cast<uint32_t>(scaffolds - info.scaffoldsRequired)) * k_ScaffoldPenalty * r;
		r = r - std::min(c, r);
	}
	return r;
}

std::optional<plans::PlanIndex> plans::GetBestPlanned(entt::entity town, float& best, uint32_t mask)
{
	best = 0.0f;
	std::optional<PlanIndex> result;
	for (PlanIndex i = 0; i < PlansOf(town); ++i)
	{
		if ((static_cast<uint32_t>(GetAbodeType(town, i)) & mask) == 0)
		{
			continue;
		}
		// GetDesireToBeBuilt(info, 0): a citadel heart plan's info is its GCitadelHeartInfo (type Citadel)
		float d = 0.0f;
		if (const auto* heart = HeartInfoOf(town, i); heart != nullptr)
		{
			d = GetDesireToBeBuilt(town, *heart, AbodeType::Citadel, nullptr, 0);
		}
		else if (const auto* info = InfoOf(town, i); info != nullptr)
		{
			d = GetDesireToBeBuilt(town, *info, 0);
		}
		else
		{
			continue;
		}
		if (d > best) // strict, the first on ties
		{
			best = d;
			result = i;
		}
	}
	return result;
}

std::optional<plans::PlanIndex> plans::GetPlannedAtPos(entt::entity town, const map_coords::MapCoords& pos, float r,
                                                       bool onlyRebuild)
{
	const auto* t = TownComponent(town);
	// best = r; no plans -> none
	float best = r;
	std::optional<PlanIndex> result;
	if (t == nullptr)
	{
		return result;
	}
	for (PlanIndex i = 0; i < t->plannedAbodes.size(); ++i)
	{
		const auto& plan = t->plannedAbodes.at(i);
		if (onlyRebuild && !plan.wasBuilt)
		{
			continue;
		}
		const auto* info = InfoOf(town, i);
		const auto* heart = HeartInfoOf(town, i);
		// the info's mesh 2D radius x scale (the citadel heart's mesh from its info)
		float radius = 0.0f;
		if (heart != nullptr)
		{
			radius = object::MeshRadius2D(resources::HashIdentifier(heart->meshType), plan.scale);
		}
		else if (info != nullptr)
		{
			radius = object::MeshRadius2D(resources::HashIdentifier(info->meshId), plan.scale);
		}
		const float v = gutils::GetDistanceInMetres(pos, PlanCoords(plan)) - (radius + r);
		if (v <= best) // only v > best is skipped: the LAST one on ties
		{
			best = v;
			result = i;
		}
	}
	return result;
}

entt::entity plans::CreatePlanned(entt::entity town, PlanIndex plan, float life)
{
	// A citadel heart plan has its own
	if (HeartInfoOf(town, plan) != nullptr)
	{
		return archetypes::CitadelArchetype::CreatePlanned(town, plan, life);
	}
	const auto* t = TownComponent(town);
	const auto* info = InfoOf(town, plan);
	if (t == nullptr || info == nullptr)
	{
		return entt::null;
	}
	// the fixed check: the info at the plan's position, angle, scale and town
	const auto& p = t->plannedAbodes.at(plan);
	if (!town_placement::IsOkToCreateAtPos(*info, PlanCoords(p), p.yAngleRadians, p.scale, town))
	{
		return entt::null;
	}
	return CreatePlannedNoFixedCheck(town, plan, life);
}

entt::entity plans::CreatePlannedNoFixedCheck(entt::entity town, PlanIndex plan, [[maybe_unused]] float life)
{
	const auto* t = TownComponent(town);
	if (t == nullptr || plan >= t->plannedAbodes.size())
	{
		return entt::null;
	}
	// A citadel heart plan: CitadelArchetype::CreatePlannedNoFixedCheck (the life passed on)
	if (t->plannedAbodes.at(plan).citadelHeart)
	{
		return archetypes::CitadelArchetype::CreatePlannedNoFixedCheck(town, plan, life);
	}
	const PlannedAbode p = t->plannedAbodes.at(plan);
	// 1..4 (CreateBuildingFromPlan)
	const auto building = CreateBuildingFromPlan(town, p);
	if (building == entt::null)
	{
		return entt::null;
	}
	// 5. the plan deleted -> RemovePlanned
	RemovePlanned(town, plan);
	return building;
}

std::optional<plans::PlanIndex> plans::CreateFromBuilding(entt::entity town, entt::entity building)
{
	auto& registry = Entities();
	const auto* a = building != entt::null && registry.Valid(building) ? registry.TryGet<const Abode>(building) : nullptr;
	const auto* info = a != nullptr ? abodes::InfoOf(building) : nullptr;
	const auto* transform = a != nullptr ? registry.TryGet<const Transform>(building) : nullptr;
	// the record's number in info.dat (abodes::InfoOf answers a record of this table)
	const auto& infos = Locator::infoConstants::value().abode;
	std::optional<size_t> index;
	for (size_t i = 0; info != nullptr && i < infos.size(); ++i)
	{
		if (&infos.at(i) == info)
		{
			index = i;
			break;
		}
	}
	// (openblack, guard) the original only fails on allocation
	if (!index.has_value() || transform == nullptr)
	{
		return std::nullopt;
	}
	// a plan copied from the building
	PlannedAbode plan {
	    .info = static_cast<AbodeInfo>(*index),
	    .position = transform->position,
	    .yAngleRadians = map_cells::detail::YAngleOf(transform->rotation),
	    .scale = object::GetScale(building),
	};
	// a plain abode plan, not a town centre plan, whatever the building's class
	plan.townCentre = false;
	// wasBuilt = the building's built bit
	plan.wasBuilt = (a->buildFlags & Abode::k_Built) != 0;
	// The footpath link moves to the plan. TODO(footpaths): not ported
	// Then the plan joins the town's list (AddPlanned writes the creation turn)
	return AddPlanned(town, plan);
}

// =====================================================================================================================
// building sites
// =====================================================================================================================

entt::entity building_sites::Create(entt::entity building)
{
	auto& registry = Entities();
	if (building == entt::null || !registry.Valid(building))
	{
		return entt::null;
	}
	// The site's fields start at the component's defaults
	const auto site = registry.Create();
	auto& s = registry.Assign<BuildingSite>(site);
	auto* a = registry.TryGet<Abode>(building);
	// the repair flag from the building's not-repaired bit; the root, and the building's link back to the site
	s.isRepairSite = a != nullptr && (a->buildFlags & Abode::k_NotRepaired) != 0;
	s.root = building;
	// a workshop's site is a workshop site
	s.kind = workshops::IsWorkshop(building) ? BuildingSite::Kind::Workshop : BuildingSite::Kind::Standard;
	if (a != nullptr)
	{
		a->buildingSite = site;
	}
	// a citadel part (the citadel heart, a worship site) keeps its build flags and site link in
	// components::CitadelPartBuild
	if (auto* part = registry.TryGet<CitadelPartBuild>(building); part != nullptr)
	{
		s.isRepairSite = (part->buildFlags & CitadelPartBuild::k_NotRepaired) != 0;
		part->buildingSite = site;
	}
	// the repair base = the life; no destruction mesh and built -> 1.1 x life - 0.1
	const float life = life::LifeOf(building);
	s.repairBase = life;
	if (!abodes::HasDestructionMesh(building) && abodes::IsBuilt(building))
	{
		s.repairBase = k_RepairBaseScale * life - k_RepairBaseOffset;
	}
	// the ring
	ComputeRing(site);
	// (openblack) IsDrawBuilding holds from now on: the partly built model
	abodes::RedrawConstruction(building);
	// The citadel heart gets a citadel site, then its piles of wood. Any other building (a worship site too) gets a
	// standard site
	if (registry.AllOf<CitadelHeart>(building))
	{
		registry.Assign<CitadelBuildingSite>(site);
		CreatePilesOfWood(site);
	}
	return site;
}

void building_sites::ToBeDeleted(entt::entity site, bool now)
{
	auto& registry = Entities();
	// A citadel site first, for each of the six slots: unlinked from the heart; with wood (and the game flag of step 7
	// clear) the pile reacts (the villagers carry it away), else it is deleted; the slots keep the piles (literal).
	// Then the common steps below, whose pile step finds no pile
	if (auto* citadelSite = CitadelSiteComponent(site); citadelSite != nullptr)
	{
		citadelSite->pilesUnlinked = true;
		const auto piles = citadelSite->piles;
		for (const auto pile : piles)
		{
			if (pile == entt::null || !registry.Valid(pile))
			{
				continue; // (openblack, guard) a pile already gone
			}
			if (object_resources::GetResource(pile, ResourceType::Wood) != 0)
			{
				animal_ai::SetupPotReaction(pile);
			}
			else
			{
				DeletePile(pile);
			}
		}
	}
	auto* s = SiteComponent(site);
	// 1. already being deleted -> return. (approximate) the flag is set first here (the original sets it at the last
	// step): the villagers' exit functions run below and may come back to this site
	if (s == nullptr || s->beingDeleted)
	{
		return;
	}
	s->beingDeleted = true;
	const auto root = s->root;
	// 2. out of the game's site list (openblack keeps no global list)
	// 3. out of every town that lists it
	map_cells::ForEachTown([site](entt::entity town) {
		if (const auto* t = TownComponent(town);
		    t != nullptr && std::find(t->buildingSites.begin(), t->buildingSites.end(), site) != t->buildingSites.end())
		{
			RemoveBuildingSiteFromList(town, site);
		}
		return true;
	});
	// 4. each available builder, in list order: inside Get2DRadius + 2 -> walks out to that circle, then
	// DECIDE_WHAT_TO_DO; else DECIDE_WHAT_TO_DO at once. The villager's own site link is its business
	// (IsBuildingSiteValid fails)
	const auto builders = s->builders; // a copy: the villagers' exit functions may call RemoveBuilder
	for (const auto builder : builders)
	{
		// available: not being deleted, not DYING
		if (!registry.Valid(builder) || !villager::IsAvailable(builder))
		{
			continue;
		}
		const auto rootPos = object::MapCoordsOf(root);
		const auto villagerPos = object::MapCoordsOf(builder);
		const float d = object::Get2DRadius(root) + k_BuilderClearance;
		// (openblack, guard) a root already destroyed has no position: SetTopState
		if (registry.Valid(root) && gutils::GetDistanceInMetres(villagerPos, rootPos) < d)
		{
			const auto p = rootPos + gutils::GetPosFromAngle(gutils::Get3DAngleFromXZ(rootPos, villagerPos), d);
			villager::SetupMoveToWithHug(builder, map_coords::ToMetres(p), VillagerStates::DecideWhatToDo);
		}
		else
		{
			villager::SetTopState(builder, VillagerStates::DecideWhatToDo);
		}
	}
	s = SiteComponent(site);
	if (s == nullptr)
	{
		return;
	}
	// 5. the builder list emptied; the builder counter is NOT reset
	s->builders.clear();
	// 6. every scaffold deleted (each takes itself out of the list: a copy)
	const auto siteScaffolds = s->scaffolds;
	for (const auto scaffold : siteScaffolds)
	{
		if (registry.Valid(scaffold))
		{
			scaffolds::ToBeDeleted(scaffold);
		}
	}
	if ((s = SiteComponent(site)) == nullptr)
	{
		return;
	}
	// 7. the pile, not a workshop's (a workshop site keeps none): unlinked; with wood it reacts (the villagers carry it
	// away), else it is deleted; the site's pile cleared in any case. (pending) the original skips the reaction under a
	// game flag of unknown meaning: taken as clear
	if (const auto pile = s->woodPile; pile != entt::null)
	{
		if (registry.Valid(pile) && object_resources::GetResource(pile, ResourceType::Wood) != 0)
		{
			animal_ai::SetupPotReaction(pile);
		}
		else
		{
			DeletePile(pile);
		}
		if ((s = SiteComponent(site)) != nullptr)
		{
			s->woodPile = entt::null;
		}
	}
	// 8. the root's link to the site and the root cleared
	if (s != nullptr && root != entt::null)
	{
		if (auto* a = registry.Valid(root) ? registry.TryGet<Abode>(root) : nullptr; a != nullptr)
		{
			a->buildingSite = entt::null;
		}
		if (auto* part = registry.Valid(root) ? registry.TryGet<CitadelPartBuild>(root) : nullptr; part != nullptr)
		{
			part->buildingSite = entt::null;
		}
		s->root = entt::null;
		abodes::RedrawConstruction(root); // (openblack) IsDrawBuilding no longer holds
	}
	// 9. the rest of ecs::ToBeDeleted, which called this one (abodes::OnToBeDeleted): at once with `now` or with the
	// deferral off, else UNAVAILABLE and the dead list
	(void)now;
}

bool building_sites::IsAvailable(entt::entity site)
{
	const auto* s = SiteComponent(site);
	return s != nullptr && !s->beingDeleted;
}

void building_sites::Process(entt::entity site)
{
	// A citadel site: each slot whose pile is not available is cleared. (inferred) not reached in the original: the
	// citadel heart's update does not run its site's
	if (auto* citadelSite = CitadelSiteComponent(site); citadelSite != nullptr)
	{
		for (auto& pile : citadelSite->piles)
		{
			if (pile != entt::null && !ecs::IsAvailable(pile))
			{
				pile = entt::null;
			}
		}
		return;
	}
	// A standard site: a pile that is not available is cleared
	if (auto* s = SiteComponent(site); s != nullptr && s->woodPile != entt::null && !ecs::IsAvailable(s->woodPile))
	{
		s->woodPile = entt::null;
	}
}

const std::vector<entt::entity>& building_sites::SitesOf(entt::entity town)
{
	static const std::vector<entt::entity> k_None;
	const auto* t = TownComponent(town);
	return t != nullptr ? t->buildingSites : k_None;
}

bool building_sites::IsBuildingHappening(entt::entity town)
{
	return !SitesOf(town).empty();
}

void building_sites::InsertBuildingSite(entt::entity town, entt::entity site)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	// already in the list -> return (no pulse)
	if (std::find(t->buildingSites.begin(), t->buildingSites.end(), site) != t->buildingSites.end())
	{
		return;
	}
	// the town stats count the list (town_stats::Compute). The site goes at the head
	if (site != entt::null)
	{
		t->buildingSites.insert(t->buildingSites.begin(), site);
	}
	town_queries::Pulse(town);
}

void building_sites::RemoveBuildingSiteFromList(entt::entity town, entt::entity site)
{
	// every entry of the site (the town stats are recomputed)
	if (auto* t = TownComponent(town); t != nullptr)
	{
		auto& sites = t->buildingSites;
		sites.erase(std::remove(sites.begin(), sites.end(), site), sites.end());
	}
}

entt::entity building_sites::AddBuildingSite(entt::entity town, entt::entity building)
{
	// the building's site (Create), then the list insert
	const auto site = Create(building);
	InsertBuildingSite(town, site);
	return site;
}

entt::entity building_sites::AddBuildingSiteFromPlan(entt::entity town, plans::PlanIndex plan)
{
	// the plan's building (CreatePlanned), its site, the list insert
	const auto building = plans::CreatePlanned(town, plan, 0.0f);
	if (building == entt::null)
	{
		return entt::null;
	}
	return AddBuildingSite(town, building);
}

entt::entity building_sites::AddBuildingSiteNoFixedCheck(entt::entity town, plans::PlanIndex plan)
{
	// the same with CreatePlannedNoFixedCheck(0.0)
	const auto building = plans::CreatePlannedNoFixedCheck(town, plan, 0.0f);
	if (building == entt::null)
	{
		return entt::null;
	}
	return AddBuildingSite(town, building);
}

entt::entity building_sites::AddBuildingSiteNoFixedCheck(entt::entity town, const PlannedAbode& plan)
{
	// For a scaffold's plan: CreatePlannedNoFixedCheck's steps 1..4 on the free plan (its step 5, deleting the plan,
	// finds it in no town list), then the site
	const auto building = CreateBuildingFromPlan(town, plan);
	if (building == entt::null)
	{
		return entt::null;
	}
	return AddBuildingSite(town, building);
}

void building_sites::AddScaffold(entt::entity site, entt::entity scaffold)
{
	// at the head of the list (the scaffold's link back: the caller's)
	if (auto* s = SiteComponent(site); s != nullptr)
	{
		s->scaffolds.insert(s->scaffolds.begin(), scaffold);
	}
}

void building_sites::RemoveScaffold(entt::entity site, entt::entity scaffold)
{
	// every entry of the scaffold
	if (auto* s = SiteComponent(site); s != nullptr)
	{
		auto& list = s->scaffolds;
		list.erase(std::remove(list.begin(), list.end(), scaffold), list.end());
	}
}

const std::vector<entt::entity>& building_sites::ScaffoldsOf(entt::entity site)
{
	static const std::vector<entt::entity> k_None;
	const auto* s = SiteComponent(site);
	return s != nullptr ? s->scaffolds : k_None;
}

bool building_sites::RemoveBuildingSite(entt::entity town, entt::entity building)
{
	// the first site whose GetBuilding is the building -> its ToBeDeleted
	const auto site = GetBuildingSiteInList(town, building);
	if (site == entt::null)
	{
		return false;
	}
	ToBeDeleted(site);
	return true;
}

entt::entity building_sites::GetBuildingSiteInList(entt::entity town, entt::entity building)
{
	for (const auto site : SitesOf(town))
	{
		if (GetBuilding(site) == building)
		{
			return site;
		}
	}
	return entt::null;
}

bool building_sites::IsBuildingSiteValid(entt::entity town, entt::entity site)
{
	const auto& sites = SitesOf(town);
	if (std::find(sites.begin(), sites.end(), site) == sites.end())
	{
		return false;
	}
	const auto building = GetBuilding(site);
	return building != entt::null && !(abodes::IsBuilt(building) && abodes::IsRepaired(building));
}

entt::entity building_sites::GetBestBuildingSite(entt::entity town, const map_coords::MapCoords& pos, bool includeFull)
{
	entt::entity best = entt::null;
	float score = k_BestSiteStart;
	const auto sites = SitesOf(town);
	for (const auto site : sites)
	{
		if (GetBuilding(site) == entt::null)
		{
			continue;
		}
		if (!NeedsBuilders(site) && !includeFull)
		{
			continue;
		}
		// GetBuilding read again; none -> return null (abort)
		const auto building = GetBuilding(site);
		if (building == entt::null)
		{
			return entt::null;
		}
		const float w = GetDesireForVillagers(site) * k_SiteWeightScale + k_SiteWeightOffset;
		// the building's nearest edge to pos
		const auto edge = object::GetNearestEdgeToPos(building, pos);
		const float s = gutils::GetDistanceInMetres(pos, edge) * w;
		if (s < score)
		{
			score = s;
			best = site;
		}
	}
	return best;
}

entt::entity building_sites::GetBestRepairBuildingSite(entt::entity town)
{
	entt::entity best = entt::null;
	float bestDesire = 0.0f;
	for (const auto site : SitesOf(town))
	{
		const auto* s = SiteComponent(site);
		if (s == nullptr || !s->isRepairSite)
		{
			continue;
		}
		const float d = GetDesireToBeRepaired(site);
		if (d > bestDesire)
		{
			bestDesire = d;
			best = site;
		}
	}
	return best;
}

building_sites::TownRepairChoice building_sites::ChooseTownRepair(entt::entity town)
{
	TownRepairChoice choice;
	const auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return choice;
	}
	// best = 0
	float best = 0.0f;
	// each built plan: GetDesireToBeRepaired > best (strictly, a NaN never wins)
	for (plans::PlanIndex i = 0; i < t->plannedAbodes.size(); ++i)
	{
		if (!t->plannedAbodes.at(i).wasBuilt)
		{
			continue;
		}
		const float v = plans::GetDesireToBeRepaired(town, i);
		if (v > best)
		{
			best = v;
			choice.plan = i;
		}
	}
	// each abode of the town with no site and not marked not repaired: its GetDesireToBeRepaired > the SAME best
	for (const auto abode : town_stats::AbodesOf(town))
	{
		const auto* a = Entities().TryGet<const Abode>(abode);
		if (a == nullptr || a->buildingSite != entt::null || (a->buildFlags & Abode::k_NotRepaired) != 0)
		{
			continue;
		}
		const float v = abodes::GetDesireToBeRepaired(abode);
		if (v > best)
		{
			best = v;
			choice.abode = abode;
		}
	}
	return choice;
}

void building_sites::ProcessTownRepairs(entt::entity town)
{
	const auto choice = ChooseTownRepair(town);
	// an abode -> marked not repaired, AddBuildingSite (its site copies the mark: a repair site for
	// GetBestRepairBuildingSite) and return, also when a plan was better before
	if (choice.abode != entt::null)
	{
		if (auto* a = Entities().TryGet<Abode>(choice.abode); a != nullptr)
		{
			a->buildFlags |= Abode::k_NotRepaired;
		}
		AddBuildingSite(town, choice.abode);
		return;
	}
	// else a plan -> AddBuildingSiteFromPlan (CreatePlanned WITH the fixed check; a rebuild plan's building is marked
	// too, so its site is a repair site)
	if (choice.plan.has_value())
	{
		AddBuildingSiteFromPlan(town, *choice.plan);
	}
}

bool building_sites::RequestBestPlanned(entt::entity town)
{
	// GetBestPlanned(mask 4) -> AddBuildingSiteNoFixedCheck
	float best = 0.0f;
	const auto plan = plans::GetBestPlanned(town, best, k_CivicMask);
	return plan.has_value() && AddBuildingSiteNoFixedCheck(town, *plan) != entt::null;
}

bool building_sites::RequestANewAbode(entt::entity town, [[maybe_unused]] AbodeType unused)
{
	// GetBestPlanned(mask 2) -> AddBuildingSiteFromPlan (with the fixed check)
	float best = 0.0f;
	const auto plan = plans::GetBestPlanned(town, best, k_AbodeMask);
	return plan.has_value() && AddBuildingSiteFromPlan(town, *plan) != entt::null;
}

void building_sites::AddWoodUsedForBuilding(entt::entity town, uint32_t wood)
{
	if (auto* t = TownComponent(town); t != nullptr)
	{
		t->woodUsedForBuilding = t->woodUsedForBuilding + static_cast<float>(wood);
	}
}

void building_sites::ForceBuildingOfPlannedAtPos(const map_coords::MapCoords& pos, float desire)
{
	// every town of every player, neutral included
	map_cells::ForEachTown([&pos, desire](entt::entity town) {
		if (const auto plan = plans::GetPlannedAtPos(town, pos, 1.0f, false); plan.has_value())
		{
			if (const auto site = AddBuildingSiteNoFixedCheck(town, *plan); site != entt::null)
			{
				SetDesireBoost(site, desire);
			}
		}
		return true;
	});
}

void building_sites::PruneSites(entt::entity town)
{
	// each site, the next read first (a copy here)
	const auto sites = SitesOf(town);
	for (const auto site : sites)
	{
		const auto* s = SiteComponent(site);
		const auto root = s != nullptr ? s->root : entt::null;
		if (root == entt::null || !abode_queries::IsAvailable(root) || (abodes::IsBuilt(root) && abodes::IsRepaired(root)))
		{
			if (IsAvailable(site))
			{
				ToBeDeleted(site);
			}
		}
	}
}

building_sites::DesireInputs building_sites::DesireInputsOf(entt::entity town)
{
	DesireInputs in;
	for (const auto site : SitesOf(town))
	{
		in.siteDesires.push_back(GetDesireForVillagers(site));
		in.siteBuilders.push_back(static_cast<uint32_t>(GetBuilderCount(site)));
		in.sitePlaces.push_back(GetMaxBuilders(site));
	}
	for (plans::PlanIndex i = 0; i < plans::PlansOf(town); ++i)
	{
		in.planRepairDesires.push_back(plans::GetDesireToBeRepaired(town, i));
	}
	return in;
}

// ---- one site
// --------------------------------------------------------------------------------------------------------

entt::entity building_sites::GetRootBuilding(entt::entity site)
{
	const auto* s = SiteComponent(site);
	return s != nullptr ? s->root : entt::null;
}

entt::entity building_sites::GetBuilding(entt::entity site)
{
	// root and available ? root : null
	const auto root = GetRootBuilding(site);
	return root != entt::null && abode_queries::IsAvailable(root) ? root : entt::null;
}

entt::entity building_sites::GetTown(entt::entity site)
{
	// root ? root's town : null. The citadel heart's and a worship site's town is null: TownOf answers null for
	// anything that is not an abode
	const auto root = GetRootBuilding(site);
	return root != entt::null && Entities().Valid(root) ? abode_villagers::TownOf(root) : entt::null;
}

int32_t building_sites::GetBuilderCount(entt::entity site)
{
	const auto* s = SiteComponent(site);
	return s != nullptr ? s->builderCount : 0;
}

int32_t building_sites::GetMaxBuilders(entt::entity site)
{
	// GetBuilding ? its info's MaxVillagerNeededToBuild : 0
	const auto building = GetBuilding(site);
	const auto* info = building != entt::null ? abodes::MultiCellStaticInfoOf(building) : nullptr;
	return info != nullptr ? static_cast<int32_t>(info->maxVillagerNeededToBuild) : 0;
}

int32_t building_sites::GetBuildersNeeded(entt::entity site)
{
	const auto building = GetBuilding(site);
	if (building == entt::null)
	{
		return 0;
	}
	const auto root = GetRootBuilding(site);
	if (abodes::IsBuilt(root))
	{
		if (abodes::IsRepaired(root))
		{
			return 0;
		}
		if (abodes::GetDesireToBeRepaired(building) == 0.0f)
		{
			return 0;
		}
	}
	return GetMaxBuilders(site) - GetBuilderCount(site);
}

bool building_sites::NeedsBuilders(entt::entity site)
{
	return GetBuildersNeeded(site) > 0; // signed
}

bool building_sites::IsBuilder(entt::entity site, entt::entity villager)
{
	const auto* s = SiteComponent(site);
	return s != nullptr && std::find(s->builders.begin(), s->builders.end(), villager) != s->builders.end();
}

float building_sites::GetDesireForVillagers(entt::entity site)
{
	const auto* s = SiteComponent(site);
	if (s == nullptr)
	{
		return 0.0f;
	}
	// clamp(GetBuildersNeeded / GetMaxBuilders + the desire boost, 0, 1); a 0 maximum divides by 0 (literal)
	float r = static_cast<float>(GetBuildersNeeded(site)) / static_cast<float>(GetMaxBuilders(site)) + s->desireBoost;
	if (r < 0.0f)
	{
		r = 0.0f;
	}
	if (r > 1.0f)
	{
		r = 1.0f;
	}
	return r;
}

float building_sites::GetDesireToBeRepaired(entt::entity site)
{
	// GetBuilding ? GetDesireForVillagers x the ROOT's GetDesireToBeRepaired : 0
	if (GetBuilding(site) == entt::null)
	{
		return 0.0f;
	}
	return GetDesireForVillagers(site) * abodes::GetDesireToBeRepaired(GetRootBuilding(site));
}

float building_sites::GetClearAreaRadius(entt::entity site)
{
	// the building's Get2DRadius x 1.2. (openblack, guard) 0 without a building
	const auto building = GetBuilding(site);
	return building != entt::null ? object::Get2DRadius(building) * k_ClearAreaFactor : 0.0f;
}

float building_sites::GetPercentBuilt(entt::entity site)
{
	// GetPercentRepaired x GetPercentBuilt
	const auto building = GetBuilding(site);
	return building != entt::null ? abodes::GetPercentRepaired(building) * abodes::GetPercentBuilt(building) : 0.0f;
}

float building_sites::GetRadius(entt::entity site)
{
	const auto building = GetBuilding(site);
	return building != entt::null ? object::GetRadius(building) : 0.0f;
}

float building_sites::GetWoodValue(entt::entity site)
{
	// (float)(uint32) the info's wood value x GetScale / the player's TribalPower[5]
	const auto building = GetBuilding(site);
	const auto* info = building != entt::null ? abodes::MultiCellStaticInfoOf(building) : nullptr;
	if (info == nullptr)
	{
		return 0.0f;
	}
	// the site's player is always the neutral one
	const float power = magic::players::MagicOf(PlayerNames::NEUTRAL).tribalPower.at(5);
	return static_cast<float>(info->woodValue) * object::GetScale(building) / power;
}

float building_sites::GetWoodNeededToBuild(entt::entity site)
{
	// (root built ? 1 - life : 1 - percent built) x GetWoodValue - (float)(uint) GetResource(WOOD)
	const auto building = GetBuilding(site);
	if (building == entt::null)
	{
		return 0.0f; // (openblack, guard)
	}
	const float part =
	    abodes::IsBuilt(GetRootBuilding(site)) ? 1.0f - life::LifeOf(building) : 1.0f - abodes::GetPercentBuilt(building);
	return part * GetWoodValue(site) - static_cast<float>(GetResource(site, ResourceType::Wood));
}

bool building_sites::IsRepairSite(entt::entity site)
{
	const auto* s = SiteComponent(site);
	return s != nullptr && s->isRepairSite;
}

float building_sites::GetRepairBase(entt::entity site)
{
	const auto* s = SiteComponent(site);
	return s != nullptr ? s->repairBase : 0.0f;
}

void building_sites::SetRepairBase(entt::entity site, float base)
{
	if (auto* s = SiteComponent(site); s != nullptr)
	{
		s->repairBase = base;
	}
}

bool building_sites::ShouldIGetWood(entt::entity site, entt::entity villager,
                                    const std::function<map_coords::MapCoords()>& resourceDropoffPos)
{
	auto& registry = Entities();
	const auto* s = SiteComponent(site);
	if (s == nullptr || !registry.Valid(villager))
	{
		return false;
	}
	// m = (float)(uint) GetResource(WOOD)
	float m = static_cast<float>(GetResource(site, ResourceType::Wood));
	const auto villagerPos = object::MapCoordsOf(villager);
	const auto rootPos = object::MapCoordsOf(s->root);
	// within 50 m -> m == 0
	if (gutils::GetDistanceInMetres(villagerPos, rootPos) < k_CloseToSite)
	{
		return m == 0.0f;
	}
	// + each builder's carried wood (signed)
	for (const auto builder : s->builders)
	{
		if (const auto* v = registry.Valid(builder) ? registry.TryGet<const Villager>(builder) : nullptr; v != nullptr)
		{
			m = m + static_cast<float>(static_cast<int32_t>(v->resourceHeld.at(1)));
		}
	}
	// GetWoodNeededToBuild <= m -> false
	if (GetWoodNeededToBuild(site) <= m)
	{
		return false;
	}
	// r = m / (float)(uint64)((builders + 1) x wood wanted per builder) + (float)(int16) held / (float)(uint64) max
	// wood carried; min 1. Note the list size, not the builder counter
	const auto& info = villager::InfoOf(villager);
	const auto* self = registry.TryGet<const Villager>(villager);
	const auto held = self != nullptr ? static_cast<float>(static_cast<int32_t>(self->resourceHeld.at(1))) : 0.0f;
	const auto wanted = static_cast<uint32_t>((s->builders.size() + 1) * info.amountOfWoodPerBuilderWanted);
	float r = m / static_cast<float>(static_cast<uint64_t>(wanted)) +
	          held / static_cast<float>(static_cast<uint64_t>(info.maxWoodCarried));
	r = r < 1.0f ? r : 1.0f;
	// the wood drop-off point, only now. (openblack, tests only) no callback: the villager's own position
	const auto dropoff = resourceDropoffPos ? resourceDropoffPos() : villagerPos;
	// GetDistanceModifier (5000)
	const float a = gutils::GetDistanceModifier(gutils::GetDistanceInMetres(rootPos, villagerPos), k_WoodDistanceMax) * r;
	const float b =
	    gutils::GetDistanceModifier(gutils::GetDistanceInMetres(dropoff, villagerPos), k_WoodDistanceMax) * (1.0f - r);
	return b > a; // strict
}

uint32_t building_sites::GetResource(entt::entity site, ResourceType type)
{
	// A citadel site: the sum of the six slots' amounts (no availability test: openblack skips a pile already gone)
	if (const auto* citadelSite = CitadelSiteComponent(site); citadelSite != nullptr)
	{
		uint32_t sum = 0;
		for (const auto pile : citadelSite->piles)
		{
			if (pile != entt::null && Entities().Valid(pile))
			{
				sum += object_resources::GetResource(pile, type);
			}
		}
		return sum;
	}
	// pile ? its amount : 0. A site pile answers its own amount through object_resources::GetResource
	const auto pile = GetPileWood(site, nullptr);
	return pile != entt::null ? object_resources::GetResource(pile, type) : 0;
}

uint32_t building_sites::GetWoodForStats(entt::entity site)
{
	// a workshop's site adds nothing to the town stats' wood
	if (const auto* s = SiteComponent(site); s != nullptr && s->kind == BuildingSite::Kind::Workshop)
	{
		return 0;
	}
	return GetResource(site, ResourceType::Wood);
}

uint32_t building_sites::AddResource(entt::entity site, ResourceType type, uint32_t amount,
                                     [[maybe_unused]] const map_coords::MapCoords* pos, bool poisoned)
{
	// A citadel site: pos null -> nothing; else the nearest slot, none -> CreatePilesOfWood and once more; the pile
	// takes it (any type); then the town's wood count (none for the heart's site)
	if (CitadelSiteComponent(site) != nullptr)
	{
		uint32_t taken = 0;
		if (pos != nullptr)
		{
			auto pile = GetPileWood(site, pos);
			if (pile == entt::null)
			{
				CreatePilesOfWood(site);
				pile = GetPileWood(site, pos);
			}
			if (pile != entt::null && Entities().Valid(pile))
			{
				taken = pot_resource::AddToPotDirect(pile, type, amount, poisoned);
			}
		}
		return taken;
	}
	// A workshop site: root ? the workshop takes it : 0
	if (const auto* s = SiteComponent(site); s != nullptr && s->kind == BuildingSite::Kind::Workshop)
	{
		const auto root = GetRootBuilding(site);
		return root != entt::null ? workshops::AddResource(root, type, amount, poisoned) : 0;
	}
	// added = 0; WOOD: no pile -> CreatePileWood; an available pile takes it
	uint32_t added = 0;
	if (type == ResourceType::Wood)
	{
		if (GetPileWood(site, nullptr) == entt::null)
		{
			CreatePileWood(site);
		}
		const auto pile = GetPileWood(site, nullptr);
		if (pile != entt::null && ecs::IsAvailable(pile))
		{
			added = pot_resource::AddToPotDirect(pile, ResourceType::Wood, amount, poisoned);
		}
	}
	// the town's wood at sites: town_stats::Compute sums the site piles (woodAtSites)
	return added;
}

uint32_t building_sites::RemoveResource(entt::entity site, ResourceType type, uint32_t amount,
                                        const map_coords::MapCoords* interfacePos)
{
	// A citadel site: with an interface position, the nearest slot gives wood; without one the slots in order, each
	// available pile gives what is left (any type) while something is left
	if (const auto* citadelSite = CitadelSiteComponent(site); citadelSite != nullptr)
	{
		uint32_t removed = 0;
		if (interfacePos != nullptr)
		{
			if (const auto pile = GetPileWood(site, interfacePos);
			    type == ResourceType::Wood && pile != entt::null && Entities().Valid(pile))
			{
				removed = object_resources::RemoveFromPotDirect(pile, amount);
			}
			return removed;
		}
		const auto piles = citadelSite->piles;
		uint32_t left = amount;
		for (const auto pile : piles)
		{
			if (left == 0)
			{
				break;
			}
			if (pile != entt::null && ecs::IsAvailable(pile))
			{
				const uint32_t n = object_resources::RemoveFromPotDirect(pile, left);
				removed += n;
				left -= n;
			}
		}
		return removed;
	}
	// A workshop site: root ? the workshop gives it : 0. The site's callers pass no status
	if (const auto* s = SiteComponent(site); s != nullptr && s->kind == BuildingSite::Kind::Workshop)
	{
		const auto root = GetRootBuilding(site);
		return root != entt::null ? workshops::RemoveResource(root, type, amount) : 0;
	}
	// WOOD: taken from the pile; the stats are recomputed
	uint32_t removed = 0;
	if (type == ResourceType::Wood)
	{
		if (const auto pile = GetPileWood(site, nullptr); pile != entt::null)
		{
			removed = object_resources::RemoveFromPotDirect(pile, amount);
		}
	}
	return removed;
}

void building_sites::BuildBy(entt::entity site, float amount)
{
	// GetBuilding()->BuildBy(x)
	if (const auto building = GetBuilding(site); building != entt::null)
	{
		abodes::BuildBy(building, amount);
	}
}

map_coords::MapCoords building_sites::GetNearestEdge(entt::entity site, float angle, int32_t& index)
{
	const auto* s = SiteComponent(site);
	if (s == nullptr)
	{
		return {};
	}
	int32_t i = 0;
	// below -6 pi -> index 0
	if (!(angle < k_MinusSixPi))
	{
		if (angle > k_SixPi)
		{
			angle = glm::pi<float>();
		}
		else
		{
			// + 2 pi while below 0; - 2 pi while above 2 pi (strict: 2 pi itself stays)
			while (angle < 0.0f)
			{
				angle += glm::two_pi<float>();
			}
			while (angle > glm::two_pi<float>())
			{
				angle -= glm::two_pi<float>();
			}
		}
		// angle == 0 -> 0; else (angle x 1 / 2 pi x 128 truncated toward zero) & 0x7F (the bound check after the mask is dead).
		// Each step rounded to float, as the original's 24-bit FPU does through the 2 pi loops and both products
		if (angle != 0.0f)
		{
			i = map_coords::FtoL(angle * k_InvTwoPi * static_cast<float>(BuildingSite::k_RingSize)) & k_RingMask;
		}
	}
	index = i;
	return RingCoords(*s, static_cast<size_t>(i));
}

map_coords::MapCoords building_sites::GetRandomBuildPos(entt::entity site, entt::entity villager, int32_t& index)
{
	const auto building = GetBuilding(site);
	if (building == entt::null)
	{
		return {}; // (openblack, guard) the original reads the building unchecked
	}
	// a = the angle from the building to the villager; a + (GameFloatRand(pi / 2) - pi / 4)
	const float a = gutils::Get3DAngleFromXZ(object::MapCoordsOf(building), object::MapCoordsOf(villager));
	// the subtraction then the addition, each rounded to float as by the original's 24-bit FPU
	const float spread = game_random::GameFloatRand(k_RandomBuildSpread) - k_RandomBuildHalf;
	return GetNearestEdge(site, spread + a, index);
}

map_coords::MapCoords building_sites::GetNextPosFromIndex(entt::entity site, int32_t& index)
{
	const auto* s = SiteComponent(site);
	const auto building = GetBuilding(site);
	if (s == nullptr || building == entt::null)
	{
		return {}; // no building -> (0, 0, 0)
	}
	// step = 2.0 / (Get2DRadius x 2 pi x 0.0078125); k = GameFloatRand(step x 0.5) + step, truncated toward zero. The product
	// and the division each rounded to float (24-bit FPU)
	const float step = k_NextPosMetres / (object::Get2DRadius(building) * glm::two_pi<float>() * k_NextPosPerEntry);
	const int32_t k = map_coords::FtoL(game_random::GameFloatRand(step * 0.5f) + step);
	// sgn = GameRand(2) ? +1 : -1
	const int32_t sign = game_random::GameRand(2) != 0 ? 1 : -1;
	// i = *idx + sgn x k, one wrap
	int32_t i = index + sign * k;
	const auto size = static_cast<int32_t>(BuildingSite::k_RingSize);
	if (i >= size)
	{
		i -= size;
	}
	else if (i < 0)
	{
		i += size;
	}
	index = i;
	// (openblack, guard) a k of 128 or more leaves i outside the ring after the one wrap: the original reads past it
	const auto read = static_cast<size_t>(((i % size) + size) % size);
	return RingCoords(*s, read);
}

std::optional<map_coords::MapCoords> building_sites::GetBuildPos(entt::entity site, int32_t index)
{
	const auto* s = SiteComponent(site);
	if (s == nullptr || index < 0 || index >= static_cast<int32_t>(BuildingSite::k_RingSize))
	{
		return std::nullopt;
	}
	return RingCoords(*s, static_cast<size_t>(index));
}

void building_sites::AddBuilder(entt::entity site, entt::entity villager)
{
	// the duplicate search uses nothing it finds (dead loop): the villager at the head, the counter ++
	if (auto* s = SiteComponent(site); s != nullptr)
	{
		s->builders.insert(s->builders.begin(), villager);
		++s->builderCount;
	}
}

void building_sites::RemoveBuilder(entt::entity site, entt::entity villager)
{
	// every entry of the villager, then the counter -- once (also when not found / empty)
	if (auto* s = SiteComponent(site); s != nullptr)
	{
		s->builders.erase(std::remove(s->builders.begin(), s->builders.end(), villager), s->builders.end());
		--s->builderCount;
	}
}

entt::entity building_sites::GetPileWood(entt::entity site, const map_coords::MapCoords* pos)
{
	// A citadel site: pos null -> null; else the slot nearest to pos, the distance as (float)(uint32) strictly below
	// the best (FLT_MAX to start: the first on ties; no availability test)
	if (const auto* citadelSite = CitadelSiteComponent(site); citadelSite != nullptr)
	{
		if (pos == nullptr)
		{
			return entt::null;
		}
		entt::entity nearest = entt::null;
		float best = std::numeric_limits<float>::max();
		for (const auto pile : citadelSite->piles)
		{
			if (pile == entt::null || !Entities().Valid(pile))
			{
				continue; // (openblack, guard) a pile already gone
			}
			const auto whole = static_cast<uint32_t>(gutils::GetDistance(*pos, object::MapCoordsOf(pile)));
			const auto d = static_cast<float>(whole);
			if (d < best)
			{
				best = d;
				nearest = pile;
			}
		}
		return nearest;
	}
	const auto* s = SiteComponent(site);
	// A workshop site: the workshop's pile
	if (s != nullptr && s->kind == BuildingSite::Kind::Workshop)
	{
		return workshops::GetPileWood(s->root);
	}
	return s != nullptr ? s->woodPile : entt::null;
}

entt::entity building_sites::SiteOfPile(entt::entity pile)
{
	if (pile == entt::null)
	{
		return entt::null;
	}
	entt::entity found = entt::null;
	Entities().Each<const BuildingSite>([pile, &found](entt::entity site, const BuildingSite& s) {
		if (s.woodPile == pile)
		{
			found = site;
		}
	});
	// A citadel site's six slots, while the piles are still linked to the heart (ToBeDeleted unlinks them)
	if (found == entt::null)
	{
		Entities().Each<const CitadelBuildingSite>([pile, &found](entt::entity site, const CitadelBuildingSite& s) {
			if (!s.pilesUnlinked && std::find(s.piles.begin(), s.piles.end(), pile) != s.piles.end())
			{
				found = site;
			}
		});
	}
	return found;
}

void building_sites::CreatePileWood(entt::entity site)
{
	auto* s = SiteComponent(site);
	// A workshop site: no pile and available -> a pile at the workshop's special point 4, its wood going to the
	// workshop. (approximate) openblack's piles keep no link to a workshop (WorkshopOfPile finds a workshop's pile
	// through the workshop): the pile is made as the workshop's own (workshops::CreatePileWood, the same point), so it
	// is owned and deleted with the workshop instead of one lost entity per call. The workshop normally made one when
	// created; no openblack caller reaches this (the site's AddResource goes to the workshop)
	if (s != nullptr && s->kind == BuildingSite::Kind::Workshop)
	{
		if (GetPileWood(site, nullptr) != entt::null || !IsAvailable(site))
		{
			return;
		}
		workshops::CreatePileWood(GetBuilding(site));
		return;
	}
	// A standard site with no pile, when available
	if (s == nullptr || CitadelSiteComponent(site) != nullptr || s->woodPile != entt::null || !IsAvailable(site))
	{
		return;
	}
	// where the pile goes (the angle starts at 0)
	float angle = 0.0f;
	const auto pos = GetResourcePosAndYAngle(site, ResourceType::Wood, -1, &angle);
	// A "Magic Wood" pot, made with angle 0 and scale 1: the Y angle is computed (and its random draw consumed) and
	// dropped
	const auto pile = magic::objects::CreateMagicWood(map_coords::ToWorld(pos), std::nullopt, 0, true);
	if ((s = SiteComponent(site)) != nullptr)
	{
		s->woodPile = pile;
	}
}

map_coords::MapCoords building_sites::GetResourcePosAndYAngle(entt::entity site, ResourceType type,
                                                              [[maybe_unused]] int32_t index, float* angle)
{
	auto& registry = Entities();
	const auto root = GetRootBuilding(site);
	const auto rootPos = object::MapCoordsOf(root);
	// A citadel site (any type): the slot's worship site angle - 1.1424, the root's MapCoords + GetPosFromAngle(that,
	// 22.0); the angle out-parameter is not written
	if (CitadelSiteComponent(site) != nullptr)
	{
		// The worship site angle: the citadel heart's Y angle + (unsigned) index x 2 pi / 7, then - 1.1424; float
		// steps, as the original's 24-bit FPU
		float heartAngle = 0.0f;
		const auto* heart = registry.TryGet<const CitadelHeart>(root);
		if (heart != nullptr && registry.Valid(heart->citadel))
		{
			if (const auto* worship = registry.TryGet<const CitadelWorship>(heart->citadel); worship != nullptr)
			{
				heartAngle = worship->heartYAngle;
			}
		}
		const auto slot = static_cast<float>(static_cast<uint32_t>(index));
		const float step = slot * k_WorshipSlotAngle;
		const float sum = heartAngle + step;
		const float a = sum - k_CitadelPileAngle;
		return rootPos + gutils::GetPosFromAngle(a, k_CitadelPileMetres);
	}
	// A workshop site: WOOD with a pile -> its position and, when asked, its Y angle; WOOD without one and a root ->
	// the workshop's special point 4 (the root is taken as a workshop, literal); anything else the root's position, the
	// angle not written
	if (const auto* s = SiteComponent(site); s != nullptr && s->kind == BuildingSite::Kind::Workshop)
	{
		if (type == ResourceType::Wood)
		{
			if (const auto pile = GetPileWood(site, nullptr); pile != entt::null && registry.Valid(pile))
			{
				if (angle != nullptr)
				{
					const auto* transform = registry.TryGet<const Transform>(pile);
					*angle = transform != nullptr ? map_cells::detail::YAngleOf(transform->rotation) : 0.0f;
				}
				return object::MapCoordsOf(pile);
			}
			if (root != entt::null)
			{
				return workshops::GetResourcePosAndYAngle(root, ResourceType::Wood, angle);
			}
		}
		return rootPos;
	}
	// not WOOD -> angle 0, the root's position
	if (type != ResourceType::Wood)
	{
		if (angle != nullptr)
		{
			*angle = 0.0f;
		}
		return rootPos;
	}
	// WOOD at a worship site: the local point (9, 0, -50) through the root's matrix as a MapCoords; the angle (when
	// asked) its Y angle
	if (const auto* worship = registry.TryGet<const WorshipSite>(root); worship != nullptr)
	{
		if (angle != nullptr)
		{
			*angle = worship->yAngle;
		}
		const auto* transform = registry.TryGet<const Transform>(root);
		const glm::vec3 point =
		    transform != nullptr ? glm::vec3(affine::Model(*transform) * glm::vec4(k_WorshipSitePile, 1.0f)) : glm::vec3(0.0f);
		return map_coords::FromWorld(point);
	}
	// WOOD with a pile: its position and Y angle
	if (const auto pile = GetPileWood(site, nullptr); pile != entt::null && registry.Valid(pile))
	{
		if (angle != nullptr)
		{
			const auto* transform = registry.TryGet<const Transform>(pile);
			*angle = transform != nullptr ? map_cells::detail::YAngleOf(transform->rotation) : 0.0f;
		}
		return object::MapCoordsOf(pile);
	}
	// WOOD without a pile: a = the angle from the door to the centre + pi / 8 - GameFloatRand(pi / 4) (after the
	// angle); d = the distance from the centre to the door again + 4; p + GetPosFromAngle(a, d); the Y angle a - pi.
	// The pile is on the far side of the building from the door
	const auto doorAt = abode_queries::GetArrivePos(root); // the door position
	const map_coords::MapCoords door {doorAt.x, doorAt.y, 0.0f};
	float a = gutils::Get3DAngleFromXZ(door, rootPos);
	a = a + (k_PileHalfSpread - game_random::GameFloatRand(k_PileSpread));
	const auto doorAgain = abode_queries::GetArrivePos(root);
	const float d =
	    gutils::GetDistanceInMetres(rootPos, map_coords::MapCoords {doorAgain.x, doorAgain.y, 0.0f}) + k_PileBeyondDoor;
	if (angle != nullptr)
	{
		*angle = a - glm::pi<float>();
	}
	return rootPos + gutils::GetPosFromAngle(a, d);
}

bool building_sites::IsLinkedToThisBuildingSite(entt::entity site, entt::entity pot)
{
	if (pot == entt::null)
	{
		return false;
	}
	// A citadel site: one of its six slots
	if (const auto* citadelSite = CitadelSiteComponent(site); citadelSite != nullptr)
	{
		return std::find(citadelSite->piles.begin(), citadelSite->piles.end(), pot) != citadelSite->piles.end();
	}
	// a standard site: its pile; otherwise false
	const auto* s = SiteComponent(site);
	return s != nullptr && s->woodPile == pot;
}

void building_sites::CreatePilesOfWood(entt::entity site)
{
	if (CitadelSiteComponent(site) == nullptr)
	{
		return;
	}
	for (int32_t i = 0; i < static_cast<int32_t>(CitadelBuildingSite::k_Piles); ++i)
	{
		// every slot's position (the angle starts at 0)
		float angle = 0.0f;
		const auto pos = GetResourcePosAndYAngle(site, ResourceType::Wood, i, &angle);
		const auto* citadelSite = CitadelSiteComponent(site);
		const auto old = citadelSite->piles.at(static_cast<size_t>(i));
		// a slot with an available pile keeps it
		if (old != entt::null && ecs::IsAvailable(old))
		{
			continue;
		}
		// A "Magic Wood" pot, which keeps neither the angle nor the scale (see CreatePileWood)
		const auto pile = magic::objects::CreateMagicWood(map_coords::ToWorld(pos), std::nullopt, 0, true);
		if (auto* again = CitadelSiteComponent(site); again != nullptr)
		{
			again->piles.at(static_cast<size_t>(i)) = pile;
		}
	}
}

void building_sites::SetDesireBoost(entt::entity site, float boost)
{
	if (auto* s = SiteComponent(site); s != nullptr)
	{
		s->desireBoost = boost; // raw float
	}
}
} // namespace openblack::ecs
