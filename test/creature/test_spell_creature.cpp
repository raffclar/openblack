/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature miracles in the spell class tree: what each turn's event does to a creature (pure), a creature taking a
// miracle on, extending it and queueing one of the same kind, the close-down that lets a creature's spell go, the
// class's operations against the plain spell's, and a turn of the spells on a creature with fake services

#define LOCATOR_IMPLEMENTATIONS

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/AudioManagerNoOp.h"
#include "Audio/Game/Banks.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureSpells.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Spell.h"
#include "Magic/Core/Spell.h"
#include "Magic/Spells/SpellCreature.h"
#include "creature/CreatureSystemFakes.h"
#include "creature/CreatureSystemWorld.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::creature_spells;
namespace fakes = openblack::test::creature_fakes;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureAnimation;
using openblack::ecs::components::CreatureMindState;
using SpellComponent = openblack::ecs::components::Spell;
using CreatureSpellsComponent = openblack::ecs::components::CreatureSpells;

namespace
{
constexpr float k_Epsilon = 1e-5f;

BodyValues Body()
{
	return {.size = 1.0f, .strength = 0.5f, .fatness = 0.5f, .alignment = 0.0f};
}

TEST(CreatureSpellApply, SizeStartsEasesHoldsAndIsPutBack)
{
	Slot slot;
	auto body = Body();
	SpellLook look;
	const auto start = Apply({.spell = Spell::Big, .event = Event::Start, .ratio = 0.0f}, slot, body, look, nullptr);
	EXPECT_FLOAT_EQ(slot.before, 1.0f);
	EXPECT_EQ(start.sound, EffectOf(Spell::Big).soundAction);
	EXPECT_FALSE(start.stopLocomotion);

	// half way to big: 1.8 times bigger, within the largest
	std::ignore = Apply({.spell = Spell::Big, .event = Event::Ease, .ratio = 0.5f}, slot, body, look, nullptr);
	const float target = SizeTarget(Spell::Big, 1.0f, creature_locomotion::k_MinSize, creature_locomotion::k_MaxSize);
	EXPECT_NEAR(body.size, Ease(1.0f, target, 0.5f), k_Epsilon);
	const auto hold = Apply({.spell = Spell::Big, .event = Event::Hold, .ratio = 1.0f}, slot, body, look, nullptr);
	EXPECT_EQ(hold.sound, 0);
	EXPECT_FALSE(hold.leashWorks.has_value());

	std::ignore = Apply({.spell = Spell::Big, .event = Event::Finish, .ratio = 0.0f}, slot, body, look, nullptr);
	EXPECT_FLOAT_EQ(body.size, 1.0f);
	EXPECT_FLOAT_EQ(body.strength, 0.5f);
}

TEST(CreatureSpellApply, FreezeStopsTheCreatureAndPausesItsMindUntilItThaws)
{
	Slot slot;
	auto body = Body();
	SpellLook look;
	DesireView mind {.desires = nullptr, .paused = false};
	const auto start = Apply({.spell = Spell::Freeze, .event = Event::Start, .ratio = 0.0f}, slot, body, look, &mind);
	EXPECT_TRUE(start.stopLocomotion);
	EXPECT_EQ(start.pauseMind, std::optional(true));
	EXPECT_TRUE(look.pausedMind);
	EXPECT_TRUE(mind.paused);

	const auto ease = Apply({.spell = Spell::Freeze, .event = Event::Ease, .ratio = 0.25f}, slot, body, look, &mind);
	EXPECT_FLOAT_EQ(look.freeze, 0.25f);
	EXPECT_EQ(ease.playbackScale, std::optional(0.75f));

	const auto finish = Apply({.spell = Spell::Freeze, .event = Event::Finish, .ratio = 0.0f}, slot, body, look, &mind);
	EXPECT_FLOAT_EQ(look.freeze, 0.0f);
	EXPECT_EQ(finish.playbackScale, std::optional(1.0f));
	EXPECT_EQ(finish.pauseMind, std::optional(false));
	EXPECT_FALSE(look.pausedMind);
	// the body values are no freeze's
	EXPECT_FLOAT_EQ(body.size, 1.0f);
}

TEST(CreatureSpellApply, AFreezeLeavesAMindPausedByHandPausedAsItThaws)
{
	Slot slot;
	auto body = Body();
	SpellLook look;
	DesireView mind {.desires = nullptr, .paused = true};
	const auto start = Apply({.spell = Spell::Freeze, .event = Event::Start, .ratio = 0.0f}, slot, body, look, &mind);
	EXPECT_FALSE(start.pauseMind.has_value());
	EXPECT_FALSE(look.pausedMind);
	const auto finish = Apply({.spell = Spell::Freeze, .event = Event::Finish, .ratio = 0.0f}, slot, body, look, &mind);
	EXPECT_FALSE(finish.pauseMind.has_value());
	EXPECT_TRUE(mind.paused);
}

TEST(CreatureSpellApply, InvisibleFizzesOutAndBack)
{
	Slot slot;
	auto body = Body();
	SpellLook look;
	std::ignore = Apply({.spell = Spell::Invisible, .event = Event::Start, .ratio = 0.0f}, slot, body, look, nullptr);
	EXPECT_TRUE(look.invisible);
	std::ignore = Apply({.spell = Spell::Invisible, .event = Event::Ease, .ratio = 1.0f}, slot, body, look, nullptr);
	EXPECT_FLOAT_EQ(look.fizz, k_InvisibleFizz);
	std::ignore = Apply({.spell = Spell::Invisible, .event = Event::Finish, .ratio = 0.0f}, slot, body, look, nullptr);
	EXPECT_FALSE(look.invisible);
	EXPECT_FLOAT_EQ(look.fizz, 0.0f);
}

TEST(CreatureSpellApply, ItchyKeepsTheLeashFromWorkingWhileItHolds)
{
	Slot slot;
	auto body = Body();
	SpellLook look;
	const auto hold = Apply({.spell = Spell::Itchy, .event = Event::Hold, .ratio = 1.0f}, slot, body, look, nullptr);
	EXPECT_EQ(hold.leashWorks, std::optional(false));
	const auto finish = Apply({.spell = Spell::Itchy, .event = Event::Finish, .ratio = 0.0f}, slot, body, look, nullptr);
	EXPECT_EQ(finish.leashWorks, std::optional(true));
}

TEST(CreatureSpellApply, AMoodSpellsDesireIsWantedMostThenLeast)
{
	creature_desires::Desires desires;
	for (auto& desire : desires.desires)
	{
		desire.activated = false;
	}
	auto& compassion = desires[creature_desires::Desire::Compassion];
	compassion.max = 0.9f;
	auto& other = desires[creature_desires::Desire::Hunger];
	other.activated = true;
	other.value = 0.39f;
	DesireView mind {.desires = &desires, .paused = false};
	Slot slot;
	auto body = Body();
	SpellLook look;

	std::ignore = Apply({.spell = Spell::Nice, .event = Event::Start, .ratio = 0.0f}, slot, body, look, &mind);
	EXPECT_TRUE(compassion.activated);
	EXPECT_FLOAT_EQ(compassion.value, 0.9f);
	EXPECT_FLOAT_EQ(slot.before, 0.0f);

	compassion.value = 0.2f;
	std::ignore = Apply({.spell = Spell::Nice, .event = Event::Hold, .ratio = 1.0f}, slot, body, look, &mind);
	EXPECT_FLOAT_EQ(compassion.value, 0.9f);

	std::ignore = Apply({.spell = Spell::Nice, .event = Event::Ease, .ratio = 0.5f}, slot, body, look, &mind);
	EXPECT_NEAR(body.alignment, 0.5f * k_NiceAlignment, k_Epsilon);

	std::ignore = Apply({.spell = Spell::Nice, .event = Event::Finish, .ratio = 0.0f}, slot, body, look, &mind);
	EXPECT_FLOAT_EQ(body.alignment, 0.0f);
	EXPECT_FLOAT_EQ(compassion.value, 0.39f / k_LeastDominantFactor);
}

/// A recording audio service: the creature spells' sounds
class RecordingAudio final: public audio::AudioManagerNoOp
{
public:
	struct Played
	{
		audio::Owner owner;
		audio::AnimKey key;
		audio::BankId bank;
	};
	std::vector<Played> played;

	audio::Channel PlayAnimationEffect(audio::Owner owner, float, const audio::AnimKey& key, audio::AnimAction,
	                                   audio::BankId bank, bool, float, float) override
	{
		played.push_back({owner, key, bank});
		return 1;
	}
};

class SpellCreatureTest: public ::testing::Test
{
protected:
	/// A creature miracle entity, as AllocSpell makes one, of a creature spell class
	static entt::entity Miracle(MagicType type, float duration)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto spell = registry.Create();
		registry.Assign<SpellComponent>(
		    spell, SpellComponent {.magicType = type, .spellClass = magic::SpellClass::Creature, .duration = duration});
		return spell;
	}

	// the world lists the spell classes reach (the flock miracles' species dying needs the animals' shared state),
	// reset after the registry as the game's shutdown does
	const test::ScopedWorldSystems _worldSystems;
	test::creature_world::World _world;
};

TEST_F(SpellCreatureTest, TheClassRunsAsThePlainSpellButForTheCastOnAnObjectAndTheCloseDown)
{
	const auto& creature = magic::OpsOf(magic::SpellClass::Creature);
	const auto& general = magic::OpsOf(magic::SpellClass::General);
	EXPECT_EQ(creature.initWithPos, general.initWithPos);
	EXPECT_EQ(creature.process, general.process);
	EXPECT_EQ(creature.spellEvent, general.spellEvent);
	EXPECT_EQ(creature.costToMaintain, general.costToMaintain);
	EXPECT_EQ(creature.toBeDeleted, general.toBeDeleted);
	EXPECT_EQ(creature.hasEnoughChantsForRecast, general.hasEnoughChantsForRecast);
	EXPECT_EQ(creature.particleType, general.particleType);
	EXPECT_EQ(creature.maxObjectsToCreate, general.maxObjectsToCreate);
	EXPECT_NE(creature.initWithObject, general.initWithObject);
	EXPECT_NE(creature.closeDown, general.closeDown);
}

TEST_F(SpellCreatureTest, ACreatureTakesTheMiracleOnForItsTimeAndTheMiracleNoLongerRunsOut)
{
	const auto creature = test::creature_world::World::MakeCreature();
	const auto miracle = Miracle(MagicType::CreatureSpellFreeze, 2.0f);
	auto& registry = test::creature_world::World::Registry();

	magic::spell_creature::Receive(creature, miracle);

	ASSERT_TRUE(std::as_const(registry).AllOf<CreatureSpellsComponent>(creature));
	const auto& slot = registry.Get<CreatureSpellsComponent>(creature).spells[Spell::Freeze];
	EXPECT_EQ(slot.phase, Phase::Waiting);
	// no player: a tribal power of 1, 2 s of 10 turns each
	EXPECT_EQ(slot.holdTurns, TurnsOf(2.0f * magic::GetTribalPower(miracle), 10.0f));
	EXPECT_EQ(slot.miracle, miracle);
	EXPECT_LT(registry.Get<SpellComponent>(miracle).duration, 0.0f);
}

TEST_F(SpellCreatureTest, AnythingButACreatureOrACreatureMiracleTakesNothing)
{
	auto& registry = test::creature_world::World::Registry();
	const auto rock = registry.Create();
	const auto miracle = Miracle(MagicType::CreatureSpellFreeze, 2.0f);
	magic::spell_creature::Receive(rock, miracle);
	EXPECT_FALSE(std::as_const(registry).AllOf<CreatureSpellsComponent>(rock));
	EXPECT_FLOAT_EQ(registry.Get<SpellComponent>(miracle).duration, 2.0f);

	const auto creature = test::creature_world::World::MakeCreature();
	const auto fireball = Miracle(MagicType::Fireball, 2.0f);
	magic::spell_creature::Receive(creature, fireball);
	EXPECT_FALSE(std::as_const(registry).AllOf<CreatureSpellsComponent>(creature));
}

TEST_F(SpellCreatureTest, CastAgainItIsExtendedAndTheFirstMiracleIsClosedDown)
{
	const auto creature = test::creature_world::World::MakeCreature();
	const auto first = Miracle(MagicType::CreatureSpellFreeze, 1.0f);
	const auto second = Miracle(MagicType::CreatureSpellFreeze, 1.0f);
	auto& registry = test::creature_world::World::Registry();
	magic::spell_creature::Receive(creature, first);
	magic::spell_creature::Receive(creature, second);

	const auto& slot = registry.Get<CreatureSpellsComponent>(creature).spells[Spell::Freeze];
	EXPECT_EQ(slot.holdTurns, 20);
	EXPECT_EQ(slot.miracle, second);
	EXPECT_TRUE(registry.Get<SpellComponent>(first).closedDown);
	EXPECT_FALSE(registry.Get<SpellComponent>(second).closedDown);
}

TEST_F(SpellCreatureTest, OneOfTheSameKindWaitsForTheFirstToFinish)
{
	const auto creature = test::creature_world::World::MakeCreature();
	const auto small = Miracle(MagicType::CreatureSpellSmall, 1.0f);
	const auto big = Miracle(MagicType::CreatureSpellBig, 1.0f);
	auto& registry = test::creature_world::World::Registry();
	magic::spell_creature::Receive(creature, small);
	magic::spell_creature::Receive(creature, big);

	const auto& spells = registry.Get<CreatureSpellsComponent>(creature).spells;
	ASSERT_EQ(spells.waiting.size(), 1u);
	EXPECT_EQ(spells.waiting.front().spell, Spell::Big);
	EXPECT_EQ(spells.waiting.front().miracle, big);
	EXPECT_EQ(spells[Spell::Big].phase, Phase::Off);
	EXPECT_FALSE(registry.Get<SpellComponent>(small).closedDown);
}

TEST_F(SpellCreatureTest, ClosingTheMiracleDownLetsTheCreaturesSpellGoOfIt)
{
	const auto creature = test::creature_world::World::MakeCreature();
	const auto small = Miracle(MagicType::CreatureSpellSmall, 1.0f);
	const auto big = Miracle(MagicType::CreatureSpellBig, 1.0f);
	auto& registry = test::creature_world::World::Registry();
	magic::spell_creature::Receive(creature, small);
	magic::spell_creature::Receive(creature, big);

	magic::OpsOf(magic::SpellClass::Creature).closeDown(small);
	magic::OpsOf(magic::SpellClass::Creature).closeDown(big);

	EXPECT_TRUE(registry.Get<SpellComponent>(small).closedDown);
	const auto& spells = registry.Get<CreatureSpellsComponent>(creature).spells;
	EXPECT_EQ(spells[Spell::Small].miracle, entt::entity {entt::null});
	ASSERT_EQ(spells.waiting.size(), 1u);
	EXPECT_EQ(spells.waiting.front().miracle, entt::entity {entt::null});
	// the spell itself runs its course
	EXPECT_NE(spells[Spell::Small].phase, Phase::Off);
}

TEST_F(SpellCreatureTest, ATurnOfAFreezeStopsTheCreaturePausesItsMindAndSoundsFromTheCreatureBank)
{
	const test::RestoreService<Locator::creatureLocomotionSystem> restoreLocomotion;
	auto& locomotion = static_cast<fakes::FakeLocomotion&>(Locator::creatureLocomotionSystem::emplace<fakes::FakeLocomotion>());
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& audio = static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	ASSERT_TRUE(std::as_const(registry).AllOf<CreatureMindState>(creature));
	magic::spell_creature::Receive(creature, Miracle(MagicType::CreatureSpellFreeze, 2.0f));

	magic::spell_creature::ProcessTurn();

	const auto& stopped = locomotion.stopped;
	ASSERT_EQ(stopped.size(), 1u);
	EXPECT_EQ(stopped.front(), creature);
	EXPECT_TRUE(registry.Get<CreatureMindState>(creature).paused);
	EXPECT_TRUE(registry.Get<CreatureSpellsComponent>(creature).pausedMind);
	ASSERT_EQ(audio.played.size(), 1u);
	EXPECT_EQ(audio.played.front().owner.thing, creature);
	EXPECT_EQ(audio.played.front().key[4], EffectOf(Spell::Freeze).soundAction);
	EXPECT_EQ(audio.played.front().bank, audio::Bank(audio::SfxBank::Creature));
}
} // namespace
