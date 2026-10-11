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

	/// Its life, from dead at 0 to full at 1
	float life {1.0f};
	/// The turn it was born on; its age in years counts from it
	uint32_t birthTurn;
	/// How full it is, from empty at 0 to full at 1. A newly made villager can start a little over full.
	float food;
	LifeStage lifeStage;
	Sex sex;
	Tribe tribe;
	VillagerNumber number;
	Task task;
	entt::entity town;
	entt::entity abode;
	/// The turn its needs were last looked at
	uint32_t lastCheckTurn {0};
	/// What it shows in its hands
	CarriedObject carried {CarriedObject::None};
	/// A clip taking it into or out of a state is playing, and its state waits for it to end
	bool transitionPlaying {false};
	/// A clip took it out of its last state, and the one taking it into its new state is still to come
	bool intoClipDue {false};
	/// A knock on its home woke it: its own needs leave it be, and it won't go back to bed, for its next decision
	bool woken {false};
	/// The food and wood it carries, and which log its wood shows as (0 to 3)
	int16_t foodHeld {0};
	int16_t woodHeld {0};
	uint8_t woodGraphic {0};
	/// The building site it works for, if any, and which of the site's places round the building it makes for
	entt::entity buildingSite {entt::null};
	int32_t buildPlace {0};
};
} // namespace openblack::ecs::components
