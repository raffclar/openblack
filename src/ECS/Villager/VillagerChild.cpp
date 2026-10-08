/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerChild.h"

#include <array>
#include <string>

#include <fmt/format.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/CollisionSounds.h"
#include "ECS/Registry.h"
#include "ECS/Systems/VillagerWorldQueriesInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerTrace.h"
#include "Game.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/SpecialPoints.h"

// The children and the creche (VillagerChild.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;
namespace aq = abode_queries;

namespace
{
/// A promenade path is 5 extra metrics (a signed / 5)
constexpr int32_t k_PointsPerPath = 5;
/// k > 9 starts a new path; k > 4 walks it back (9 - k)
constexpr int32_t k_LastStep = 9;
constexpr int32_t k_LastPoint = 4;
/// The jitter around a promenade point: GameFloatRand(1) - 0.5
constexpr float k_JitterRange = 1.0f;
constexpr float k_JitterHalf = 0.5f;
/// ChildAtCreche's IsTouching margin
constexpr float k_TouchMargin = 0.001f;
/// ChildAtCreche's anim effect key
constexpr std::array<int32_t, 5> k_CrecheSoundKey = {0, 0, 0x13, 0, 0x52};

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

/// The villager's abode (entt::null without one, or when it is gone)
entt::entity AbodeOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	// (guard) the original only reads the link: this stands for the unlinking when the abode is deleted
	return v != nullptr && v->abode != entt::null && ecs::IsAvailable(v->abode) ? v->abode : entt::null;
}

void TraceIf(entt::entity villager, const std::string& line)
{
	if (TraceOn(villager))
	{
		Trace(villager, line);
	}
}

std::string Xz(glm::ivec2 pos)
{
	const auto m = tq::ToMetres(pos);
	return fmt::format("({:.1f}, {:.1f})", m.x, m.y);
}

/// The sky type of the visual time > 1.2 (DayNightClock::IsVisualNight), through the villagers' world queries.
/// (guard) no game: day
bool VisualNight()
{
	return Locator::villagerWorldQueries::value().IsVisualNight();
}

/// The walk to the creche's next promenade point: GetNextDstPromemade(creche, the walk index, the villager's
/// position), then SetupMoveToPos(it, 113)
void WalkTheCreche(entt::entity villager, entt::entity creche)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return;
	}
	const auto out = GetNextDstPromemade(creche, v->crecheWalk, tq::PosOf(villager));
	villager::TraceFormatted(villager, "child 113: promenade {:#x} -> {}", static_cast<uint32_t>(v->crecheWalk), Xz(out));
	// (approximate) the walk goal in metres: fixed -> metres -> fixed can move it by one unit
	SetupMoveToPos(villager, tq::ToMetres(out), VillagerStates::ChildAtCreche);
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

PromenadeStep NextPromenadeStep(int32_t index, int32_t n, bool atDoor)
{
	// p = index & 0xFFFF, k = index >> 16 (arithmetic shift)
	int32_t p = index & 0xFFFF;
	int32_t k = index >> 16;
	int32_t j = 0;
	// GameRand(n), kept only in [0, n), else 0
	const auto roll = [n]() {
		const auto r = static_cast<int32_t>(GameRand(static_cast<uint32_t>(n)));
		return r < 0 || r >= n ? 0 : r;
	};
	if (atDoor)
	{
		// p = GameRand(n); k = 0
		p = roll();
		k = 0;
	}
	else
	{
		// p < 0 -> 0; p >= n -> n - 1
		if (p < 0)
		{
			p = 0;
		}
		else if (p >= n)
		{
			p = n - 1;
		}
		// ++k; negative -> 0
		++k;
		if (k < 0)
		{
			k = 0;
		}
		else if (k > k_LastStep)
		{
			// k > 9 -> k = 0, p = GameRand(n)
			k = 0;
			p = roll();
		}
		else if (k > k_LastPoint)
		{
			// k 5..9 -> j = 9 - k
			j = k_LastStep - k;
		}
		else
		{
			j = k;
		}
	}
	// index = (k << 16) | p
	const auto packed = (static_cast<uint32_t>(k) << 16) | static_cast<uint32_t>(p);
	return {p, k, j, static_cast<int32_t>(packed)};
}

// ---- the child ---------------------------------------------------------------------------------------------------

uint32_t IsMotherAlive(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	// no mother -> 0. (guard) an entity that is no longer a villager: 0
	if (v == nullptr || v->mother == entt::null || !Entities().Valid(v->mother) || !Entities().AllOf<Villager>(v->mother))
	{
		return 0;
	}
	const auto mother = v->mother;
	// she is available
	if (!villager::IsAvailable(mother))
	{
		return 0;
	}
	// the same tribe
	if (InfoOf(mother).tribeType != InfoOf(villager).tribeType)
	{
		return 0;
	}
	// a mother: female
	if (InfoOf(mother).sex != SexType::Female)
	{
		return 0;
	}
	// dead -> 0; else 1
	return (Entities().Get<const Villager>(mother).status & Villager::k_StatusDead) != 0 ? 0 : 1;
}

uint32_t ChildGotoCreche(entt::entity villager)
{
	// the town, its creche, IsFunctional; else 0
	const auto town = GetTown(villager);
	if (town == entt::null)
	{
		return 0;
	}
	const auto creche = tq::GetCreche(town);
	if (creche == entt::null || !aq::IsFunctional(creche))
	{
		return 0;
	}
	// SetupMoveToOnFootpath(creche, its door (abode_queries::GetArrivePos), 113); 1
	const auto door = aq::GetArrivePos(creche);
	villager::TraceFormatted(villager, "child: to the creche's door {}", Xz(door));
	SetupMoveToOnFootpath(villager, creche, door, VillagerStates::ChildAtCreche);
	return 1;
}

glm::ivec2 GetNextDstPromemade(entt::entity creche, int32_t& index, glm::ivec2 from)
{
	// n = the mesh's extra metric count / 5; no extra data -> 0
	const auto n = static_cast<int32_t>(worship::ExtraMetricCount(creche)) / k_PointsPerPath;
	// from == the door (the original compares x, z and the altitude).
	// (approximate) x and z only, from the Transform: the altitudes are 0 on the ground
	const auto door = aq::GetArrivePos(creche);
	const auto step = NextPromenadeStep(index, n, from == door);
	glm::ivec2 out = door;
	// n == 0 -> out = the door
	if (n != 0)
	{
		// the extra metric 5p + j (worship::GetSpecialPoint)
		const auto point = worship::GetSpecialPoint(creche, k_PointsPerPath * step.path + step.point);
		// a = GameFloatRand(1) - 0.5, b = GameFloatRand(1) - 0.5, drawn whenever n != 0, the point found or not, so the
		// RNG order does not depend on it
		const float a = GameFloatRand(k_JitterRange) - k_JitterHalf;
		const float b = GameFloatRand(k_JitterRange) - k_JitterHalf;
		if (point)
		{
			// x += b, z += a (float steps)
			const float x = point->position.x + b;
			const float z = point->position.z + a;
			// x, z by ToFixed
			out = glm::ivec2(map_coords::ToFixed(x), map_coords::ToFixed(z));
		}
	}
	index = step.index;
	return out;
}

uint32_t ChildAtCreche(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// CheckChild == 1 -> 1
	if (CheckChild(villager) == 1)
	{
		return 1;
	}
	// no town -> 0
	const auto town = GetTown(villager);
	if (town == entt::null)
	{
		return 0;
	}
	// CheckNeededForTownDesire == 1 -> 1
	if (CheckNeededForTownDesire(villager) == 1)
	{
		return 1;
	}
	// the town's creche; the sky type of the visual time; no creche -> 0
	const auto creche = tq::GetCreche(town);
	const bool night = VisualNight();
	if (creche == entt::null)
	{
		return 0;
	}
	// the sky type <= 1.2 and IsFunctional
	if (!night && aq::IsFunctional(creche))
	{
		// the creche's anim sound effect {0, 0, 0x13, 0, 0x52} from the editor.sad bank, at its distance from the
		// camera. (approximate) the creche's Transform position as its sound position, as abodes::OnPhysicalDamage does
		if (const auto* t = Entities().TryGet<const Transform>(creche))
		{
			physics::CollisionSounds::PlayAnimEffect(k_CrecheSoundKey, creche, t->position, false);
		}
		// the next promenade point, SetupMoveToPos(out, 113); 1
		WalkTheCreche(villager, creche);
		return 1;
	}
	// an abode: its first inhabitant is at home -> GoHome; 0 either way
	if (const auto abode = AbodeOf(villager); abode != entt::null)
	{
		const auto& list = abode_villagers::VillagersOf(abode);
		if (!list.empty() && IsAtHome(list.front()))
		{
			TraceIf(villager, "child 113: night or closed -> GoHome");
			GoHome(villager);
		}
		return 0;
	}
	// no abode, the creche IsTouching(this, 0.001f) -> the next point
	// (SetupMoveToPos(out, 113)); 0
	if (object::IsTouching(creche, villager, k_TouchMargin))
	{
		WalkTheCreche(villager, creche);
	}
	return 0;
}
} // namespace openblack::ecs::villager
