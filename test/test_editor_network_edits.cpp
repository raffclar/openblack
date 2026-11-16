/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <limits>
#include <optional>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "Editor/EditorEntities.h"
#include "Editor/EditorMath.h"
#include "Editor/EditorNetworkEdits.h"
#include "Editor/EditorOutline.h"

using namespace openblack;
using namespace openblack::editor;
using player_commands::Action;
using player_commands::EditorEdit;

namespace
{

entt::entity Entity(uint32_t value)
{
	return static_cast<entt::entity>(value);
}

/// The commands that the writes sent, and the turn of the game that the test sets
struct Sent
{
	std::vector<Action> actions;
	uint32_t turn {0};

	CommandWrites Writes(CommandWrites::CopyOf copyOf = {})
	{
		return {[this](Action action) { actions.push_back(std::move(action)); }, [this] { return turn; }, std::move(copyOf)};
	}

	[[nodiscard]] const EditorEdit& Edit(size_t index) const { return std::get<EditorEdit>(actions.at(index)); }
};

} // namespace

TEST(EditorNetworkEdits, TheWritesOfANetworkGameChangeNothingAtOnce)
{
	Sent sent;
	auto writes = sent.Writes();
	EXPECT_FALSE(writes.ChangesAtOnce());
	EXPECT_TRUE(LocalWrites {}.ChangesAtOnce());
	EXPECT_FALSE(writes.Place({.kind = PlaceKind::Tree, .type = 2}, {1.0f, 0.0f, 2.0f}, 0.5f).has_value());
	ASSERT_EQ(sent.actions.size(), 1u);
	EXPECT_EQ(sent.Edit(0).kind, EditorEdit::Kind::Place);
	EXPECT_EQ(sent.Edit(0).placeKind, static_cast<uint8_t>(PlaceKind::Tree));
	EXPECT_EQ(sent.Edit(0).placeType, 2);
	EXPECT_EQ(sent.Edit(0).radians, 0.5f);
}

TEST(EditorNetworkEdits, TheMovesAndTheTurnsOfADragGoOnceForEachTurn)
{
	Sent sent;
	auto writes = sent.Writes();
	writes.MoveTo(Entity(3), {1.0f, 0.0f, 1.0f});
	writes.Turn(Entity(3), 0.25f);
	writes.MoveTo(Entity(3), {2.0f, 0.0f, 2.0f});
	writes.Turn(Entity(3), 0.25f);
	writes.Flush();
	// One turn, then one move to the last position
	ASSERT_EQ(sent.actions.size(), 2u);
	EXPECT_EQ(sent.Edit(0).kind, EditorEdit::Kind::Turn);
	EXPECT_EQ(sent.Edit(0).radians, 0.5f);
	EXPECT_EQ(sent.Edit(1).kind, EditorEdit::Kind::Move);
	EXPECT_EQ(sent.Edit(1).position, glm::vec3(2.0f, 0.0f, 2.0f));

	// In the same turn the next moves wait
	writes.MoveTo(Entity(3), {3.0f, 0.0f, 3.0f});
	writes.Flush();
	EXPECT_EQ(sent.actions.size(), 2u);
	++sent.turn;
	writes.Flush();
	ASSERT_EQ(sent.actions.size(), 3u);
	EXPECT_EQ(sent.Edit(2).position, glm::vec3(3.0f, 0.0f, 3.0f));
}

TEST(EditorNetworkEdits, TheEndOfADragSendsTheLastMoveAtOnce)
{
	Sent sent;
	auto writes = sent.Writes();
	writes.MoveTo(Entity(4), {1.0f, 0.0f, 1.0f});
	writes.Flush();
	writes.MoveTo(Entity(4), {5.0f, 0.0f, 5.0f});
	writes.FlushNow();
	ASSERT_EQ(sent.actions.size(), 2u);
	EXPECT_EQ(sent.Edit(1).position, glm::vec3(5.0f, 0.0f, 5.0f));
}

TEST(EditorNetworkEdits, ARemovalSendsTheWaitingMovesFirst)
{
	Sent sent;
	auto writes = sent.Writes();
	writes.MoveTo(Entity(4), {1.0f, 0.0f, 1.0f});
	writes.Remove(Entity(4));
	ASSERT_EQ(sent.actions.size(), 2u);
	EXPECT_EQ(sent.Edit(0).kind, EditorEdit::Kind::Move);
	EXPECT_EQ(sent.Edit(1).kind, EditorEdit::Kind::Remove);
	EXPECT_EQ(sent.Edit(1).object, Entity(4));
}

TEST(EditorNetworkEdits, ACopySendsThePlacementOfTheCopy)
{
	Sent sent;
	auto writes = sent.Writes([](entt::entity entity, glm::vec2 offset) -> std::optional<EditorEdit> {
		if (entity != Entity(6))
		{
			return std::nullopt;
		}
		return EditorEdit {.kind = EditorEdit::Kind::Place, .placeKind = 3, .position = {offset.x, 0.0f, offset.y}};
	});
	EXPECT_FALSE(writes.Duplicate(Entity(6), {2.0f, 3.0f}).has_value());
	EXPECT_FALSE(writes.Duplicate(Entity(7), {2.0f, 3.0f}).has_value());
	ASSERT_EQ(sent.actions.size(), 1u);
	EXPECT_EQ(sent.Edit(0).position, glm::vec3(2.0f, 0.0f, 3.0f));
}

TEST(EditorNetworkEdits, AChangeOfSizeSendsTheWaitingMovesFirstThenTheScale)
{
	Sent sent;
	auto writes = sent.Writes();
	writes.MoveTo(Entity(4), {1.0f, 0.0f, 1.0f});
	writes.ScaleTo(Entity(4), 2.5f);
	ASSERT_EQ(sent.actions.size(), 2u);
	EXPECT_EQ(sent.Edit(0).kind, EditorEdit::Kind::Move);
	EXPECT_EQ(sent.Edit(1).kind, EditorEdit::Kind::Scale);
	EXPECT_EQ(sent.Edit(1).object, Entity(4));
	EXPECT_EQ(sent.Edit(1).scale, 2.5f);
	// Nothing waits after the change of size
	EXPECT_EQ(writes.Waiting(), 0u);
}

TEST(EditorNetworkEdits, TheRuleTakesOnlyAScaleInTheRangeOfTheEditor)
{
	const auto scale = [](entt::entity entity, float value) {
		return EditorEdit {.kind = EditorEdit::Kind::Scale, .object = entity, .scale = value};
	};
	EXPECT_FALSE(EditRefusal(scale(Entity(1), 1.0f)).has_value());
	EXPECT_FALSE(EditRefusal(scale(Entity(1), k_SmallestEditorScale)).has_value());
	EXPECT_FALSE(EditRefusal(scale(Entity(1), k_LargestEditorScale)).has_value());
	EXPECT_TRUE(EditRefusal(scale(entt::null, 1.0f)).has_value());
	EXPECT_TRUE(EditRefusal(scale(Entity(1), 0.0f)).has_value());
	EXPECT_TRUE(EditRefusal(scale(Entity(1), -1.0f)).has_value());
	EXPECT_TRUE(EditRefusal(scale(Entity(1), std::nextafter(k_LargestEditorScale, 100.0f))).has_value());
	EXPECT_TRUE(EditRefusal(scale(Entity(1), std::numeric_limits<float>::quiet_NaN())).has_value());
	EXPECT_TRUE(EditRefusal(scale(Entity(1), std::numeric_limits<float>::infinity())).has_value());
}

TEST(EditorNetworkEdits, TheScaleOfTheFirstAxisKeepsTheRatioOfTheOtherAxes)
{
	EXPECT_EQ(ScaledTo({1.0f, 1.0f, 1.0f}, 2.0f), glm::vec3(2.0f, 2.0f, 2.0f));
	EXPECT_EQ(ScaledTo({2.0f, 4.0f, 1.0f}, 1.0f), glm::vec3(1.0f, 2.0f, 0.5f));
	// A thing with a scale of zero does not divide by zero
	const auto fromZero = ScaledTo({0.0f, 0.0f, 0.0f}, 1.0f);
	EXPECT_TRUE(std::isfinite(fromZero.x) && std::isfinite(fromZero.y) && std::isfinite(fromZero.z));
}

TEST(EditorNetworkEdits, TheRuleRefusesABadEdit)
{
	EXPECT_FALSE(EditRefusal(EditorEdit {.kind = EditorEdit::Kind::Move, .object = Entity(1)}).has_value());
	EXPECT_TRUE(EditRefusal(EditorEdit {.kind = EditorEdit::Kind::Move, .object = entt::null}).has_value());
	EXPECT_TRUE(EditRefusal(EditorEdit {.kind = EditorEdit::Kind::Remove, .object = entt::null}).has_value());
	const auto nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_TRUE(EditRefusal(EditorEdit {.kind = EditorEdit::Kind::Move, .object = Entity(1), .position = {nan, 0.0f, 0.0f}})
	                .has_value());
	EXPECT_TRUE(
	    EditRefusal(
	        EditorEdit {.kind = EditorEdit::Kind::Turn, .object = Entity(1), .radians = std::numeric_limits<float>::infinity()})
	        .has_value());
	// A placement needs a kind of the palette that is not a creature, and a type that is not below zero
	EXPECT_FALSE(EditRefusal(EditorEdit {.kind = EditorEdit::Kind::Place, .placeKind = static_cast<uint8_t>(PlaceKind::Tree)})
	                 .has_value());
	EXPECT_TRUE(
	    EditRefusal(EditorEdit {.kind = EditorEdit::Kind::Place, .placeKind = static_cast<uint8_t>(PlaceKind::Creature)})
	        .has_value());
	EXPECT_TRUE(EditRefusal(EditorEdit {.kind = EditorEdit::Kind::Place, .placeKind = 200}).has_value());
	// The animals are the last kind of the palette, and every computer makes them
	EXPECT_FALSE(EditRefusal(EditorEdit {.kind = EditorEdit::Kind::Place, .placeKind = static_cast<uint8_t>(PlaceKind::Animal)})
	                 .has_value());
	EXPECT_TRUE(
	    EditRefusal(EditorEdit {.kind = EditorEdit::Kind::Place, .placeKind = static_cast<uint8_t>(k_LastPlaceKind) + 1})
	        .has_value());
	EXPECT_TRUE(
	    EditRefusal(
	        EditorEdit {.kind = EditorEdit::Kind::Place, .placeKind = static_cast<uint8_t>(PlaceKind::Tree), .placeType = -1})
	        .has_value());
}

TEST(EditorNetworkEdits, AnAnimalOfThePaletteIsCopiedByItsKind)
{
	ecs::Registry registry;
	const auto horse = registry.Create();
	registry.Assign<ecs::components::Animal>(horse).type = AnimalInfo::Horse;
	const auto spellWolf = registry.Create();
	registry.Assign<ecs::components::Animal>(spellWolf).type = AnimalInfo::SpellWolf;

	// The outliner lists each animal under Animals
	EXPECT_EQ(KindOf(registry, horse), EntityKind::Animal);
	EXPECT_EQ(KindOf(registry, spellWolf), EntityKind::Animal);
	// A copy makes the same kind again through the palette; the palette does not make a miracle's wolf
	const auto copy = ItemOf(registry, horse);
	ASSERT_TRUE(copy.has_value());
	EXPECT_EQ(copy->kind, PlaceKind::Animal);
	EXPECT_EQ(copy->type, static_cast<int32_t>(AnimalInfo::Horse));
	EXPECT_FALSE(ItemOf(registry, spellWolf).has_value());
}

TEST(EditorNetworkEdits, TheRuleTakesAnEditOfAVillagerOnlyWithAFieldInItsRange)
{
	using player_commands::EditorVillagerEdit;
	using Field = EditorVillagerEdit::Field;
	const auto refused = [](Field field, uint32_t number, float amount, uint8_t index = 0) {
		return VillagerEditRefusal(
		           EditorVillagerEdit {
		               .villager = Entity(2), .field = field, .index = index, .number = number, .amount = amount})
		    .has_value();
	};
	EXPECT_FALSE(refused(Field::Life, 0, 1.0f));
	EXPECT_TRUE(refused(Field::Life, 0, 1.5f));
	EXPECT_TRUE(refused(Field::Life, 0, -0.5f));
	EXPECT_FALSE(refused(Field::Food, 0, 300.0f));
	EXPECT_TRUE(refused(Field::Food, 0, -1.0f));
	EXPECT_TRUE(refused(Field::Food, 0, std::numeric_limits<float>::quiet_NaN()));
	EXPECT_FALSE(refused(Field::Age, 90, 0.0f));
	using ecs::components::Villager;
	EXPECT_FALSE(refused(Field::Sex, static_cast<uint32_t>(Villager::Sex::_COUNT) - 1, 0.0f));
	EXPECT_TRUE(refused(Field::Sex, static_cast<uint32_t>(Villager::Sex::_COUNT), 0.0f));
	EXPECT_TRUE(refused(Field::LifeStage, static_cast<uint32_t>(Villager::LifeStage::_COUNT), 0.0f));
	EXPECT_TRUE(refused(Field::Tribe, static_cast<uint32_t>(Tribe::_COUNT), 0.0f));
	EXPECT_TRUE(refused(Field::Role, static_cast<uint32_t>(VillagerNumber::_COUNT), 0.0f));
	EXPECT_FALSE(refused(Field::State, 1, 0.0f, 2));
	EXPECT_TRUE(refused(Field::State, 1, 0.0f, 3));
	EXPECT_TRUE(refused(Field::State, static_cast<uint32_t>(VillagerStates::_COUNT), 0.0f));
	EXPECT_TRUE(VillagerEditRefusal(EditorVillagerEdit {.villager = entt::null}).has_value());
}

TEST(EditorNetworkEdits, AnEditOfAVillagerChangesOnlyItsField)
{
	using player_commands::EditorVillagerEdit;
	using Field = EditorVillagerEdit::Field;
	using ecs::components::LivingAction;
	using ecs::components::Villager;
	ecs::Registry registry;
	const auto villager = registry.Create();
	registry.Assign<Villager>(villager, Villager {.life = 1.0f, .food = 10.0f, .sex = Villager::Sex::MALE});
	registry.Assign<LivingAction>(villager);
	EXPECT_TRUE(ApplyVillagerEdit(registry, nullptr, {.villager = villager, .field = Field::Life, .amount = 0.25f}));
	EXPECT_EQ(registry.Get<Villager>(villager).life, 0.25f);
	EXPECT_EQ(registry.Get<Villager>(villager).food, 10.0f);
	EXPECT_TRUE(
	    ApplyVillagerEdit(registry, nullptr,
	                      {.villager = villager, .field = Field::Sex, .number = static_cast<uint32_t>(Villager::Sex::FEMALE)}));
	EXPECT_EQ(registry.Get<Villager>(villager).sex, Villager::Sex::FEMALE);
	// A refused edit changes nothing, and with no actions a state does not change
	EXPECT_FALSE(ApplyVillagerEdit(registry, nullptr, {.villager = villager, .field = Field::Life, .amount = 3.0f}));
	EXPECT_EQ(registry.Get<Villager>(villager).life, 0.25f);
	EXPECT_FALSE(ApplyVillagerEdit(registry, nullptr, {.villager = villager, .field = Field::State, .number = 1}));
	// A thing that is not a villager takes no edit
	const auto other = registry.Create();
	EXPECT_FALSE(ApplyVillagerEdit(registry, nullptr, {.villager = other, .field = Field::Life, .amount = 0.5f}));
}
