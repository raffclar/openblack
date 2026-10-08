/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <string>
#include <string_view>
#include <tuple>

#include <entt/entity/entity.hpp>
#include <entt/fwd.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

namespace openblack::ecs::components
{

struct Villager
{
	/// Originally VillagerTasks
	enum class Task : uint8_t
	{
		IDLE,

		_COUNT
	};
	static constexpr std::array<std::string_view, static_cast<uint8_t>(Task::_COUNT)> k_TaskStrs = {
	    "IDLE", //
	};

	/// Originally VillagerSex
	enum class Sex : uint8_t
	{
		MALE,
		FEMALE,

		_COUNT
	};
	static constexpr std::array<std::string_view, static_cast<uint8_t>(Sex::_COUNT)> k_SexStrs = {
	    "MALE",   //
	    "FEMALE", //
	};

	/// Originally VillagerLifeStage
	enum class LifeStage : uint8_t
	{
		Child,
		Adult,

		_COUNT
	};
	static constexpr std::array<std::string_view, static_cast<uint8_t>(LifeStage::_COUNT)> k_LifeStageStrs = {
	    "Child", //
	    "Adult", //
	};

	using Type = std::tuple<Tribe, Villager::LifeStage, Villager::Sex, VillagerNumber>;

	/// The villager's flag bits (docs/bw1-notes/villagers.md, section Flags): 0x1 after a tap on its abode, 0x2 at the
	/// worship site, 0x4 inside its home, 0x8 a child (SetAge), 0x10 on the way to the worship site / a fire, 0x20 in the
	/// hand, 0x80 football / script, 0x200 / 0x400 disciple / disciple follower, 0x2000 going to bed, 0xC000 the tree
	/// type of the wood carried. 0x800 / 0x1000 (into / out-of clip) live in SkeletalAnimation::transitionFlags.
	static constexpr uint16_t k_FlagAfterTapOnAbode = 0x1;
	static constexpr uint16_t k_FlagAtWorshipSite = 0x2;
	static constexpr uint16_t k_FlagAtHome = 0x4;
	static constexpr uint16_t k_FlagChild = 0x8;
	static constexpr uint16_t k_FlagOnWayToWorshipSite = 0x10;
	static constexpr uint16_t k_FlagInHand = 0x20;
	/// out of the world population: set once when it starts dying, read when it is destroyed (openblack counts the
	/// entities instead and skips these)
	static constexpr uint16_t k_FlagCountedOut = 0x40;
	static constexpr uint16_t k_FlagFootball = 0x80;
	static constexpr uint16_t k_FlagDisciple = 0x200;
	static constexpr uint16_t k_FlagDiscipleFollower = 0x400;
	static constexpr uint16_t k_FlagGoingToBed = 0x2000;
	/// bits 14-15: the tree type (CarriedTreeType 0..3) of the wood carried, set when it picks the wood up and read for
	/// the carried wood's object
	static constexpr uint16_t k_FlagTreeTypeMask = 0xC000;
	static constexpr uint16_t k_TreeTypeShift = 14; ///< flags >> 14

	/// The Living status bits a villager keeps here (docs/bw1-notes/villagers.md, section Death): 0x1 dead (set by
	/// SetDying), 0x30 the landType of the last landing (0 on its feet, 1 / 2 on a side, 3 none; SetDying ors in 3),
	/// 0x40 a skeleton (SET_SKELETON), 0x400 special (set as its physics ends, for the special test).
	/// The other bits are elsewhere: 0x2 poisoned (components::Poisoned), 0x80 downed (components::DownedVillager)
	static constexpr uint16_t k_StatusDead = 0x1;
	static constexpr uint16_t k_StatusLandTypeMask = 0x30;
	static constexpr uint16_t k_StatusSkeleton = 0x40;
	static constexpr uint16_t k_StatusSpecial = 0x400;
	static constexpr uint16_t k_LandTypeShift = 4; ///< (status & 0x30) >> 4

	float life; ///< 0..1 (ecs::life has the setters)
	/// The turn it was born (set from an age as turn - age * 1500); its age is ecs::villager::GetAge
	int32_t birthTurn {0};
	uint16_t flags {0};                     ///< the k_Flag bits above
	uint16_t status {0};                    ///< the k_Status bits above
	float food {0.0f};                      ///< the food in its belly (the constructor: 0.5 .. 1.1)
	uint32_t lastCheckTurn {0};             ///< the turn of the last periodic check (the hunger check resets it)
	uint8_t foodSpeedUp {0};                ///< the food speed-up
	uint8_t discipleType {0};               ///< VillagerDisciple
	std::array<int16_t, 2> resourceHeld {}; ///< FOOD and WOOD carried
	int16_t pregnancy {0};                  ///< turns left of a pregnancy (0 none)
	/// The BuildingSite (its entity, components::BuildingSite) the villager builds (VillagerBuild.cpp): written on
	/// the way to the site or to the storage pit for materials, cleared by ExitBuilding and the building state; none at
	/// creation
	entt::entity buildingSite {entt::null};
	entt::entity mother {entt::null};
	entt::entity targetThing {entt::null}; ///< TargetThing, what the jobs work on
	/// The same field while in a building state: the index 0..127 of the site's ring point (written on the way to the
	/// site, by ReenterBuildingState and the building state, read by ArrivesAtBuildingSite). The original's union with
	/// TargetThing (saved as 4 bytes); kept apart: every builder read follows a builder write on the same path
	int32_t buildPosIndex {0};
	/// The same field (u8) too: the death reason VillagerDead writes after SetDying (read back and saved for the dead).
	/// (inferred) a union with the low byte of TargetThing; kept apart, nothing reads TargetThing after death
	DeathReason deathReason {DeathReason::None};
	// The next field (a union: the football / the trade town / the wander area) is left out until a job reads it;
	// the scripts use its first two dwords:
	/// SET_SCRIPT_ULONG's clip, SCRIPT_PLAY_ANIM's
	uint32_t scriptAnim {0};
	/// how many times SCRIPT_PLAY_ANIM plays it (ScriptPlayAnim counts it down)
	uint32_t scriptAnimLoops {0};
	/// The same bytes (a MapCoords) too: the farmer's work point in its field (set on the way to the field, read by
	/// FarmerArrivesAtFarm; VillagerFarmer.cpp). (approximate) kept apart from the script's clip / loops it shares
	/// the bytes with in the original: they only meet when a script plays an animation on a farmer
	map_coords::MapCoords workPos {};
	/// mirrors flags & k_FlagChild (the meshes, the drawing, the sounds read it)
	LifeStage lifeStage;
	Sex sex;
	Tribe tribe;
	VillagerNumber number;
	Task task;
	entt::entity town;
	entt::entity abode;
	/// The flock a script put it in (ECS/Flocks.h, flocks::SetFlock), entt::null for none
	entt::entity flock {entt::null};
	/// Its place in a flock's member list (0 at creation; AddLeader sets it)
	uint8_t flockOrder {0};
	/// The creche walk (0 at creation; ChildAtCreche), the path in the low word and the step in the high one.
	/// (pending) the field's other readers in the original were not searched
	int32_t crecheWalk {0};
};
} // namespace openblack::ecs::components
