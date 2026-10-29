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

#include <algorithm>
#include <array>
#include <string_view>

#include <entt/core/type_info.hpp>
#include <entt/fwd.hpp>
#include <glm/vec2.hpp>

namespace openblack::ecs::components
{

enum class MoveStateClockwise
{
	Undefined,
	CounterClockwise,
	Clockwise,
};

enum class MoveState
{
	Linear,
	Orbit,
	ExitCircle,
	StepThrough,
	FinalStep,
	Arrived,
};

template <MoveState S>
struct MoveStateTagComponent
{
	static constexpr MoveState k_Value = S;
	MoveStateClockwise clockwise;
	glm::vec2 stepGoal;
};

using MoveStateLinearTag = MoveStateTagComponent<MoveState::Linear>;
using MoveStateOrbitTag = MoveStateTagComponent<MoveState::Orbit>;
using MoveStateExitCircleTag = MoveStateTagComponent<MoveState::ExitCircle>;
using MoveStateStepThroughTag = MoveStateTagComponent<MoveState::StepThrough>;
using MoveStateFinalStepTag = MoveStateTagComponent<MoveState::FinalStep>;
using MoveStateArrivedTag = MoveStateTagComponent<MoveState::Arrived>;

namespace detail
{
/// The name of a move state's tag with the number of the state, as "...MoveStateTagComponent<1>". Compilers write the
/// argument of the template in different forms (a number or the name of the value), and the save and the synced
/// checksum keep the name, so all builds use this one form.
template <MoveState S>
inline constexpr auto k_MoveStateTagName = [] {
	constexpr std::string_view k_Prefix = "openblack::ecs::components::MoveStateTagComponent<";
	static_assert(static_cast<int>(S) >= 0 && static_cast<int>(S) < 10, "The name keeps one digit");
	std::array<char, k_Prefix.size() + 2> name {};
	std::ranges::copy(k_Prefix, name.begin());
	name[k_Prefix.size()] = static_cast<char>('0' + static_cast<int>(S));
	name[k_Prefix.size() + 1] = '>';
	return name;
}();
} // namespace detail

/// The circle a walker hugs or is heading for, and how many turns it goes before it looks again (0xff for no end in
/// reach)
struct WallHugObjectReference
{
	uint8_t stepsAway;
	/// The thing whose circle it is; none for a stretch of water or the land's edge
	entt::entity entity;
	glm::vec2 centre;
	float radius;
	/// How far from its goal the walker was when it started round the circle, in 128ths of a metre less one
	uint32_t entryDistance {0};
};

/// A walker that came up against water, the land's edge or a fence while going round: once a turn the game takes one
/// such walker and tries out its walk both ways round, turning it round if the other way is better. A walker is tried
/// once until it next leaves a circle, stops hugging or sets off on a new walk.
struct WallHugLookahead
{
	/// Whether its trial has been made (else it waits for one)
	bool tried {false};
	/// How the trial went: the turns to spare each way (-1 for never getting there), and whether it was turned round
	int32_t spareAsItGoes {0};
	int32_t spareTurnedRound {0};
	bool turnedRound {false};
	/// Whether turning round would have put it where a closed ring of overlapping circles shuts it in, so it was not
	/// turned round (openblack's own rule)
	bool landingShutIn {false};
};

/// A walker that walks round what is in its way as an animal does. It takes its walk in its own turn, not with the
/// other walkers. When it aims at its goal, it turns by at most its kind's turn angle. When it steps straight to its
/// goal, it aims again each turn.
struct TurningWalker
{
	/// The most it turns towards its goal in one aim, in game angles (2048 to the circle)
	uint16_t turnAngle {0};
	/// Its step in a turn, in whole map units
	uint16_t speed {0};
};

struct WallHug
{
	/// The place the walker walks to, in whole map units (a 65536th of ten metres), as the game holds it. Metres in a
	/// float cannot hold every such place.
	glm::ivec2 goalPlace {0, 0};
	/// The step the walker makes each turn, in whole map units (a 65536th of ten metres); none until the walk is set up
	glm::ivec2 step {0, 0};
	/// A third part of the step, a height. Each aim of the step sets it to nothing. A turn round swaps it with the
	/// step's z part, both backwards, so a walker that turns round twice before its next aim gets its z part back.
	float stepAltitude {0.0f};
	float yAngle; // FIXME(bwrsandman): member is a little redundant with transform or atan on step could keep or not
	/// In metres a second; the walk goes a tenth of it each turn
	float speed;
	/// The way the walker faces, in game angles (2048 to the circle)
	uint16_t gameAngle {0};
	/// Turns until a walker stepping straight to its goal aims at it again
	int8_t turnsUntilStepRebuild {0};
	/// Where the walk holds the walker, in whole map units, and where it last put it in metres: when something else
	/// moves the walker the walk takes up its new place
	glm::ivec2 position {0, 0};
	glm::vec2 placedAt {-1.0f, -1.0f};
};

} // namespace openblack::ecs::components

/// The same name of each move state's tag on all compilers (see k_MoveStateTagName)
template <openblack::ecs::components::MoveState S>
struct entt::type_name<openblack::ecs::components::MoveStateTagComponent<S>>
{
	[[nodiscard]] static constexpr std::string_view value() noexcept
	{
		const auto& name = openblack::ecs::components::detail::k_MoveStateTagName<S>;
		return {name.data(), name.size()};
	}
	[[nodiscard]] constexpr operator std::string_view() const noexcept { return value(); }
};
