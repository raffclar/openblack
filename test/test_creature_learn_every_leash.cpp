/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <memory>

#include <MindFile.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/CreatureMindSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{

class CreatureLearnEveryLeash: public ::testing::Test
{
protected:
	void SetUp() override
	{
		for (const auto* name : {"ai", "game"})
		{
			if (spdlog::get(name) == nullptr)
			{
				spdlog::create<spdlog::sinks::null_sink_st>(name);
			}
		}
		_info = std::make_unique<InfoConstants>();
		Locator::infoConstants::emplace(*_info);
		Locator::entitiesRegistry::emplace<Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		creature = registry.Create();
		registry.Assign<Creature>(creature);
		registry.Assign<CreatureMindState>(creature);
		registry.Assign<CreatureLeash>(creature);
		minds.StartMind(creature);
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		Locator::infoConstants::reset();
	}

	/// A mind file of the creature's own mind, at a stage of growing up, that knows no leash
	std::shared_ptr<const creaturemind::MindFileData> MindAtStage(int32_t stage)
	{
		auto file = minds.SaveMind(creature);
		EXPECT_TRUE(file.has_value());
		auto made = std::make_shared<creaturemind::MindFileData>(file.value_or(creaturemind::MindFileData {}));
		made->developmentPhase = stage;
		made->leashWords = std::array<uint32_t, 4> {0, 0, 0, 0};
		return made;
	}

	std::unique_ptr<InfoConstants> _info;
	CreatureMindSystem minds;
	entt::entity creature {entt::null};
};

} // namespace

// A creature that a script makes from a mind file knows every leash: each leash word of its mind is 1
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureLearnEveryLeash, EachLeashWordOfTheMindBecomesOne)
{
	minds.TakeUpMindFile(creature, MindAtStage(5));
	auto& registry = Locator::entitiesRegistry::value();
	ASSERT_TRUE(registry.Get<const CreatureMindState>(creature).learnt.has_value());
	EXPECT_EQ(registry.Get<const CreatureMindState>(creature).learnt->leashWords, (std::array<uint32_t, 4> {0, 0, 0, 0}));
	EXPECT_TRUE(registry.Get<const CreatureLeash>(creature).known.none());

	minds.LearnEveryLeash(creature);
	EXPECT_EQ(registry.Get<const CreatureMindState>(creature).learnt->leashWords, (std::array<uint32_t, 4> {1, 1, 1, 1}));
	EXPECT_TRUE(registry.Get<const CreatureLeash>(creature).known.all());
}

// A later mind file, such as a swap, gives its own stage and leashes. The creature does not grow up or learn the leashes
// again because a script made it from a file before.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureLearnEveryLeash, ALaterMindFileKeepsItsOwnStageAndLeashes)
{
	minds.TakeUpMindFile(creature, MindAtStage(13));
	minds.LearnEveryLeash(creature);

	minds.TakeUpMindFile(creature, MindAtStage(3));
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& mind = registry.Get<const CreatureMindState>(creature);
	EXPECT_EQ(mind.developmentPhase, 3u);
	ASSERT_TRUE(mind.learnt.has_value());
	EXPECT_EQ(mind.learnt->leashWords, (std::array<uint32_t, 4> {0, 0, 0, 0}));
	EXPECT_TRUE(registry.Get<const CreatureLeash>(creature).known.none());
}
