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

#include <functional>
#include <optional>
#include <span>

/// What a villager whose town wants to relax goes to: the town's football pitch is weighed first, then its player's
/// creature (which never draws anyone), then its artefacts in turn, the one wanted most strictly winning. Pure rules,
/// tested with made-up values.
namespace openblack::ecs::relaxation_rules
{

/// A villager younger than this doesn't play football
inline constexpr uint32_t k_YoungestFootballer = 15;
/// Turns after a game before the pitch draws people as much as ever
inline constexpr uint32_t k_FootballRestTurns = 1800;

/// What a pitch's football has come to
struct FootballGame
{
	/// Players in each team
	uint32_t firstTeam {0};
	uint32_t secondTeam {0};
	bool playing {false};
	/// The turn of the last game, 0 when none has been played
	uint32_t lastGameTurn {0};
};

/// How much a villager wants to play on the town's pitch: not at all for a child or while a game with full teams (more
/// than four a side) is on; fully when no game has been played; else a random share of 1 once the last game is long
/// enough past (the share drawn from 0 before then, which gives nothing)
[[nodiscard]] float FootballDesire(uint32_t age, const FootballGame& game, uint32_t now,
                                   const std::function<float(float)>& gameFloatRand);

enum class Choice : uint8_t
{
	None,
	Football,
	Artefact,
};

struct Chosen
{
	Choice choice {Choice::None};
	/// The artefact's place among those weighed
	size_t index {0};
};

/// The relaxation chosen: the pitch first when the town has one, then each artefact in turn, one taking over only when
/// wanted strictly more; nothing when the best is not wanted at all
[[nodiscard]] Chosen Choose(std::optional<float> football, std::span<const float> artefacts);

} // namespace openblack::ecs::relaxation_rules
