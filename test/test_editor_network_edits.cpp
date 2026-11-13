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

#include "Editor/EditorNetworkEdits.h"

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
	EXPECT_TRUE(
	    EditRefusal(
	        EditorEdit {.kind = EditorEdit::Kind::Place, .placeKind = static_cast<uint8_t>(PlaceKind::Tree), .placeType = -1})
	        .has_value());
}
