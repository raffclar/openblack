/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <limits>
#include <optional>

#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "Common/CreatureEditActs.h"
#include "Common/PortableMath.h"
#include "Common/PortableRotations.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Registry.h"
#include "Editor/EditorCreatureActs.h"
#include "Editor/EditorMath.h"

using namespace openblack;
using namespace openblack::editor;
using creature_edit_acts::Act;
using player_commands::EditorCreatureAct;

namespace
{

[[nodiscard]] EditorCreatureAct Of(Act act)
{
	return {.creature = static_cast<entt::entity>(1), .act = static_cast<uint8_t>(act)};
}

[[nodiscard]] EditorCreatureAct WithIndex(Act act, uint16_t index)
{
	auto edit = Of(act);
	edit.index = index;
	return edit;
}

[[nodiscard]] EditorCreatureAct WithArg(Act act, uint8_t arg)
{
	auto edit = Of(act);
	edit.args[0] = arg;
	return edit;
}

/// A creature with the parts that the acts need
entt::entity MakeCreature(ecs::Registry& registry)
{
	const auto entity = registry.Create();
	registry.Assign<ecs::components::Creature>(entity);
	registry.Assign<ecs::components::CreatureNeeds>(entity);
	registry.Assign<ecs::components::CreatureMindState>(entity);
	return entity;
}

} // namespace

TEST(EditorCreatureActs, EachActHasANameAndANumber)
{
	for (size_t index = 0; index < creature_edit_acts::k_ActCount; ++index)
	{
		const auto act = static_cast<Act>(index);
		EXPECT_FALSE(creature_edit_acts::NameOf(act).empty());
		EXPECT_EQ(creature_edit_acts::ActFrom(static_cast<uint8_t>(index)), act);
		EXPECT_EQ(creature_edit_acts::ActNamed(creature_edit_acts::NameOf(act)), act);
	}
	EXPECT_FALSE(creature_edit_acts::ActFrom(static_cast<uint8_t>(creature_edit_acts::k_ActCount)).has_value());
	EXPECT_FALSE(creature_edit_acts::ActNamed("fly").has_value());
}

TEST(EditorCreatureActs, EachActWithItsUsualNumbersCanGo)
{
	// Each act with its first index and its switch off is in its limits, except the leashes, which start at 1
	for (size_t index = 0; index < creature_edit_acts::k_ActCount; ++index)
	{
		auto edit = Of(static_cast<Act>(index));
		if (edit.act == static_cast<uint8_t>(Act::KnowsLeash) || edit.act == static_cast<uint8_t>(Act::PutOnLeash))
		{
			edit.index = static_cast<uint16_t>(LeashType::Rope);
		}
		EXPECT_FALSE(ActRefusal(edit).has_value()) << creature_edit_acts::NameOf(static_cast<Act>(index));
	}
}

TEST(EditorCreatureActs, AnActThatIsNotKnownIsRefused)
{
	const EditorCreatureAct edit {.act = static_cast<uint8_t>(creature_edit_acts::k_ActCount)};
	EXPECT_TRUE(ActRefusal(edit).has_value());
}

TEST(EditorCreatureActs, ANumberThatIsNotFiniteIsRefused)
{
	auto edit = Of(Act::GoTo);
	edit.position.x = std::numeric_limits<float>::quiet_NaN();
	EXPECT_TRUE(ActRefusal(edit).has_value());
	edit = Of(Act::Feedback);
	edit.amount = std::numeric_limits<float>::infinity();
	EXPECT_TRUE(ActRefusal(edit).has_value());
}

TEST(EditorCreatureActs, ASwitchIsOnOrOff)
{
	for (const auto act : {Act::PauseMind, Act::TakesNotice, Act::Fainting, Act::GoTo, Act::Leashable, Act::LeashWorks,
	                       Act::LeashDrawn, Act::StartFight, Act::FightsByItself, Act::Heal})
	{
		EXPECT_FALSE(ActRefusal(WithArg(act, 1)).has_value()) << creature_edit_acts::NameOf(act);
		EXPECT_TRUE(ActRefusal(WithArg(act, 2)).has_value()) << creature_edit_acts::NameOf(act);
	}
	auto knows = WithIndex(Act::KnowsLeash, static_cast<uint16_t>(LeashType::Good));
	knows.args[0] = 2;
	EXPECT_TRUE(ActRefusal(knows).has_value());
}

TEST(EditorCreatureActs, AnIndexStaysInItsList)
{
	namespace animations = creature_layers::animations;
	const std::array<std::pair<Act, size_t>, 11> limits {{
	    {Act::PlayAction, animations::k_ActionCount},
	    {Act::PlayGesture, animations::k_GestureCount},
	    {Act::PullFace, animations::k_FaceCount},
	    {Act::ShowFeeling, creature_face::k_CueCount},
	    {Act::Trainer, creature_edit_acts::k_TrainerCount},
	    {Act::BodyTime, creature_edit_acts::k_TimeScales.size()},
	    {Act::PutThing, k_PutThings.size()},
	    {Act::LookOver, creature_object_actions::k_KeepAnimationCount},
	    {Act::Owner, static_cast<size_t>(PlayerNames::_COUNT)},
	    {Act::Blow, 3},
	    {Act::FightStep, 4},
	}};
	for (const auto& [act, limit] : limits)
	{
		EXPECT_FALSE(ActRefusal(WithIndex(act, static_cast<uint16_t>(limit - 1))).has_value())
		    << creature_edit_acts::NameOf(act);
		EXPECT_TRUE(ActRefusal(WithIndex(act, static_cast<uint16_t>(limit))).has_value()) << creature_edit_acts::NameOf(act);
	}
}

TEST(EditorCreatureActs, OnlyAKnownLeashGoes)
{
	for (const auto act : {Act::KnowsLeash, Act::PutOnLeash})
	{
		for (const auto type : {LeashType::Evil, LeashType::Rope, LeashType::Good})
		{
			EXPECT_FALSE(ActRefusal(WithIndex(act, static_cast<uint16_t>(type))).has_value());
		}
		EXPECT_TRUE(ActRefusal(WithIndex(act, 0)).has_value());
		EXPECT_TRUE(ActRefusal(WithIndex(act, 4)).has_value());
	}
}

TEST(EditorCreatureActs, TheFeedbackAndTheChargeStayInTheirLimits)
{
	auto feedback = Of(Act::Feedback);
	feedback.amount = -1.0f;
	EXPECT_FALSE(ActRefusal(feedback).has_value());
	feedback.amount = 1.5f;
	EXPECT_TRUE(ActRefusal(feedback).has_value());
	auto blow = Of(Act::Blow);
	blow.amount = creature_fight::k_MaxChargeMs;
	EXPECT_FALSE(ActRefusal(blow).has_value());
	blow.amount = -1.0f;
	EXPECT_TRUE(ActRefusal(blow).has_value());
}

TEST(EditorCreatureActs, AMarkHasAKnownSkinKindAndColumn)
{
	auto wound = Of(Act::AddWound);
	wound.args = {10, 20, 3, 7, 7, 0};
	EXPECT_FALSE(ActRefusal(wound).has_value());
	wound.args[2] = 4;
	EXPECT_TRUE(ActRefusal(wound).has_value());
	wound.args[2] = 0;
	wound.args[3] = 8;
	EXPECT_TRUE(ActRefusal(wound).has_value());
	auto blood = Of(Act::AddBlood);
	blood.args = {10, 20, 4, 0, 0, 0};
	EXPECT_TRUE(ActRefusal(blood).has_value());
}

TEST(EditorCreatureActs, EachTrainerRewardsItsKind)
{
	EXPECT_FALSE(TrainerKind(0).has_value());
	EXPECT_EQ(TrainerKind(1), creature_tree::belief_types::k_Villager);
	EXPECT_EQ(TrainerKind(5), creature_tree::belief_types::k_Abode);
	EXPECT_FALSE(TrainerKind(creature_edit_acts::k_TrainerCount).has_value());
	for (size_t trainer = 0; trainer < creature_edit_acts::k_TrainerCount; ++trainer)
	{
		EXPECT_EQ(TrainerOf(TrainerKind(trainer)), trainer);
	}
	EXPECT_FALSE(TrainerOf(1000u).has_value());
}

TEST(EditorCreatureActs, TheNeedsActsSetTheBody)
{
	ecs::Registry registry;
	const auto creature = MakeCreature(registry);
	auto& body = registry.Get<ecs::components::CreatureNeeds>(creature).needs;
	auto edit = Of(Act::Starve);
	edit.creature = creature;
	EXPECT_TRUE(ApplyCreatureAct(registry, edit).done);
	EXPECT_FLOAT_EQ(body.energy, 0.05f);
	edit.act = static_cast<uint8_t>(Act::TireOut);
	EXPECT_TRUE(ApplyCreatureAct(registry, edit).done);
	EXPECT_FLOAT_EQ(body.exhaustion, 0.85f);
	edit.act = static_cast<uint8_t>(Act::Parch);
	EXPECT_TRUE(ApplyCreatureAct(registry, edit).done);
	EXPECT_FLOAT_EQ(body.dehydration, 0.9f);
	edit.act = static_cast<uint8_t>(Act::Bloat);
	EXPECT_TRUE(ApplyCreatureAct(registry, edit).done);
	EXPECT_FLOAT_EQ(body.poo, 0.9f);
	edit.act = static_cast<uint8_t>(Act::FillUp);
	EXPECT_TRUE(ApplyCreatureAct(registry, edit).done);
	EXPECT_FLOAT_EQ(body.energy, 1.0f);
	EXPECT_FLOAT_EQ(body.exhaustion, 0.0f);
	EXPECT_FLOAT_EQ(body.dehydration, 0.0f);
	EXPECT_FLOAT_EQ(body.poo, 0.0f);
	EXPECT_FLOAT_EQ(body.life, 1.0f);
	EXPECT_FLOAT_EQ(body.warmth, 0.0f);
}

TEST(EditorCreatureActs, TheBodyShowsItsFatnessAtOnce)
{
	ecs::Registry registry;
	const auto creature = MakeCreature(registry);
	registry.Get<ecs::components::Creature>(creature).fatness = 0.8f;
	auto& morph = registry.Assign<ecs::components::CreatureMorph>(creature);
	morph.shownFatness = 0.2f;
	auto edit = Of(Act::ShowFatnessNow);
	edit.creature = creature;
	EXPECT_TRUE(ApplyCreatureAct(registry, edit).done);
	EXPECT_FLOAT_EQ(morph.shownFatness, 0.8f);
}

TEST(EditorCreatureActs, AnActOnAThingThatIsNotACreatureDoesNothing)
{
	ecs::Registry registry;
	const auto thing = registry.Create();
	auto edit = Of(Act::Starve);
	edit.creature = thing;
	EXPECT_FALSE(ApplyCreatureAct(registry, edit).done);
	edit.creature = entt::null;
	EXPECT_FALSE(ApplyCreatureAct(registry, edit).done);
}

TEST(EditorCreatureActs, AnActThatIsRefusedDoesNothing)
{
	ecs::Registry registry;
	const auto creature = MakeCreature(registry);
	auto edit = Of(Act::Starve);
	edit.creature = creature;
	edit.position.y = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FALSE(ApplyCreatureAct(registry, edit).done);
	EXPECT_NE(registry.Get<ecs::components::CreatureNeeds>(creature).needs.energy, 0.05f);
}

TEST(EditorCreatureActs, AnActWithNoSystemDoesNothing)
{
	// The tests have no mind, leash, fight or skin system: those acts do nothing, and nothing breaks
	ecs::Registry registry;
	const auto creature = MakeCreature(registry);
	for (const auto act : {Act::PauseMind, Act::SitDown, Act::GoTo, Act::PutDown, Act::Owner, Act::KnockOut, Act::Heal,
	                       Act::BodyTime, Act::LeashDrawn})
	{
		auto edit = Of(act);
		edit.creature = creature;
		EXPECT_FALSE(ApplyCreatureAct(registry, edit).done) << creature_edit_acts::NameOf(act);
	}
	EXPECT_FALSE(registry.Get<ecs::components::CreatureMindState>(creature).paused);
}

TEST(EditorCreatureActs, AnActOnAnOtherThingNeedsThatThing)
{
	ecs::Registry registry;
	const auto creature = MakeCreature(registry);
	for (const auto act : {Act::PickUp, Act::KnockDown, Act::TieTo, Act::StartFight})
	{
		auto edit = Of(act);
		edit.creature = creature;
		edit.other = creature;
		EXPECT_FALSE(ApplyCreatureAct(registry, edit).done) << creature_edit_acts::NameOf(act);
		edit.other = entt::null;
		EXPECT_FALSE(ApplyCreatureAct(registry, edit).done) << creature_edit_acts::NameOf(act);
	}
}

TEST(EditorMath, TheTurnOfAnEditHasTheSameBitsOnEachBuild)
{
	// The edits of a network game turn things on computers with different C libraries. The turn uses the portable sine,
	// cosine and arctangent, so that each computer makes the same bits.
	for (const auto yaw : {0.3f, -1.2f, 2.9f, glm::half_pi<float>()})
	{
		const auto rotation = YawRotation(yaw);
		const auto portable = glm::mat3(portable_math::EulerAngleY(yaw));
		for (int column = 0; column < 3; ++column)
		{
			for (int row = 0; row < 3; ++row)
			{
				EXPECT_EQ(rotation[column][row], portable[column][row]);
			}
		}
		EXPECT_EQ(YawOf(rotation), portable_math::Atan2(rotation[2][0], rotation[2][2]));
	}
}
