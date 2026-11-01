/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <chrono>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "Commands/PlayerCommands.h"
#include "Creature/CreatureHandDraw.h"
#include "ECS/Components/HandCreatureDraw.h"

using namespace openblack;
using namespace openblack::creature_hand_draw;
using namespace std::chrono_literals;

namespace
{

constexpr auto k_Creature = static_cast<entt::entity>(7);

/// A frame of the draw in which the input holds the hand to the creature, which the world holds and which faces the
/// player
Frame Held(std::chrono::milliseconds time = 16ms)
{
	return {.held = k_Creature, .time = time, .byCommand = false, .worldHolds = true, .finishing = false};
}

/// A frame of the draw in which the hand holds no creature
Frame Free(std::chrono::milliseconds time = 16ms)
{
	return {.held = std::nullopt, .time = time};
}

/// Where a command comes from in the frame model
enum class Source : uint8_t
{
	/// The input of the release sends it, as in a network game
	Input,
	/// The draw of the hand sends it, as in a game alone
	Draw,
};

/// A model of the frames of the game: the input, the turn that is due, the send of the block and then the draw of the
/// hand. The input of one frame lets go of the creature after a short hold. It tells the turn at which the leash tap and
/// the feedback do their work.
class ReleaseFrames
{
public:
	explicit ReleaseFrames(Source source)
	    : _source(source)
	{
	}

	/// Play one frame with a turn. The button holds the hand to the creature until the frame of the release.
	void Frame(bool released)
	{
		const bool wasHeld = _held;
		if (released)
		{
			_held = false;
			if (_source == Source::Input && wasHeld)
			{
				SubmitBoth();
			}
		}
		++_turn;
		for (const auto& command : _queue.TakeSent())
		{
			if (std::holds_alternative<player_commands::LeashTap>(command.action))
			{
				_leashTapTurn = _turn;
			}
			if (std::holds_alternative<player_commands::CreatureFeedback>(command.action))
			{
				_feedbackTurn = _turn;
			}
		}
		_queue.Send();
		const auto sends = Step(_draw, _held ? Held() : Free());
		if (_source == Source::Draw)
		{
			if (sends.click.has_value())
			{
				_queue.Submit(PlayerNames::PLAYER_ONE, player_commands::LeashTap {.creature = *sends.click});
			}
			if (sends.feedback.has_value())
			{
				_queue.Submit(PlayerNames::PLAYER_ONE,
				              player_commands::CreatureFeedback {.creature = *sends.feedback, .amount = 0.0f});
			}
		}
	}

	[[nodiscard]] std::optional<uint32_t> LeashTapTurn() const { return _leashTapTurn; }
	[[nodiscard]] std::optional<uint32_t> FeedbackTurn() const { return _feedbackTurn; }

private:
	void SubmitBoth()
	{
		_queue.Submit(PlayerNames::PLAYER_ONE, player_commands::LeashTap {.creature = k_Creature});
		_queue.Submit(PlayerNames::PLAYER_ONE, player_commands::CreatureFeedback {.creature = k_Creature, .amount = 0.0f});
	}

	Source _source;
	bool _held {true};
	ecs::components::HandCreatureDraw _draw;
	player_commands::CommandQueue _queue;
	uint32_t _turn {0};
	std::optional<uint32_t> _leashTapTurn;
	std::optional<uint32_t> _feedbackTurn;
};

} // namespace

TEST(CreatureHandDraw, AShortHoldSendsTheLeashTapAndThenTheFeedbackFromOneDraw)
{
	ecs::components::HandCreatureDraw draw;
	EXPECT_FALSE(Step(draw, Held()).click.has_value());
	EXPECT_FALSE(Step(draw, Held()).feedback.has_value());
	const auto sends = Step(draw, Free());
	EXPECT_EQ(sends.click, k_Creature);
	EXPECT_EQ(sends.feedback, k_Creature);
	// The next draw sends nothing more
	const auto after = Step(draw, Free());
	EXPECT_FALSE(after.click.has_value());
	EXPECT_FALSE(after.feedback.has_value());
}

TEST(CreatureHandDraw, TheCountStartsAtTheDrawOfThePressAndIncludesTheDrawOfTheRelease)
{
	// The draw of the press counts nothing. The later draws count 432 ms, and the draw of the release adds its 17 ms.
	ecs::components::HandCreatureDraw click;
	(void)Step(click, Held(100ms));
	for (int frame = 0; frame < 27; ++frame)
	{
		(void)Step(click, Held(16ms));
	}
	EXPECT_EQ(Step(click, Free(17ms)).click, k_Creature);

	// One more millisecond gives 450 ms, which is not a click
	ecs::components::HandCreatureDraw hold;
	(void)Step(hold, Held(100ms));
	for (int frame = 0; frame < 27; ++frame)
	{
		(void)Step(hold, Held(16ms));
	}
	EXPECT_FALSE(Step(hold, Free(18ms)).click.has_value());
}

TEST(CreatureHandDraw, TheTimeWithNoHoldDoesNotCountForTheNextHold)
{
	ecs::components::HandCreatureDraw draw;
	for (int frame = 0; frame < 100; ++frame)
	{
		(void)Step(draw, Free(16ms));
	}
	(void)Step(draw, Held());
	EXPECT_EQ(Step(draw, Free()).click, k_Creature);
}

TEST(CreatureHandDraw, TheHandSendsNoFeedbackWhenItNeverTookItsPoseOnTheCreature)
{
	// The world did not do the lock of the press yet
	ecs::components::HandCreatureDraw notHeld;
	auto frame = Held();
	frame.worldHolds = false;
	(void)Step(notHeld, frame);
	const auto sends = Step(notHeld, Free());
	EXPECT_EQ(sends.click, k_Creature);
	EXPECT_FALSE(sends.feedback.has_value());

	// The creature did not finish what it did before the hold
	ecs::components::HandCreatureDraw finishing;
	frame = Held();
	frame.finishing = true;
	(void)Step(finishing, frame);
	EXPECT_FALSE(Step(finishing, Free()).feedback.has_value());
	EXPECT_FALSE(finishing.touching);
}

TEST(CreatureHandDraw, TheHandTakesItsPoseWhenTheCreatureFinishes)
{
	ecs::components::HandCreatureDraw draw;
	auto frame = Held();
	frame.finishing = true;
	(void)Step(draw, frame);
	EXPECT_FALSE(draw.touching);
	(void)Step(draw, Held());
	EXPECT_TRUE(draw.touching);
	EXPECT_EQ(Step(draw, Free()).feedback, k_Creature);
}

TEST(CreatureHandDraw, AHoldOfACommandSendsNothingFromTheDraw)
{
	ecs::components::HandCreatureDraw draw;
	(void)Step(draw, Held());
	auto frame = Held();
	frame.byCommand = true;
	(void)Step(draw, frame);
	const auto sends = Step(draw, Free());
	EXPECT_FALSE(sends.click.has_value());
	EXPECT_FALSE(sends.feedback.has_value());
}

TEST(CreatureHandDraw, InAGameAloneTheCommandsRunOneTurnAfterTheTurnOfTheInput)
{
	// A turn in each frame. The release is the input of frame 3.
	ReleaseFrames alone(Source::Draw);
	ReleaseFrames network(Source::Input);
	for (int frame = 1; frame <= 6; ++frame)
	{
		alone.Frame(frame == 3);
		network.Frame(frame == 3);
	}
	// In a network game the send of frame 3 takes both, and the turn of frame 4 does them
	EXPECT_EQ(network.LeashTapTurn(), 4u);
	EXPECT_EQ(network.FeedbackTurn(), 4u);
	// In a game alone the draw of frame 3 sends both after the send of frame 3. The send of frame 4 takes them, and the
	// turn of frame 5 does them.
	EXPECT_EQ(alone.LeashTapTurn(), 5u);
	EXPECT_EQ(alone.FeedbackTurn(), 5u);
}
