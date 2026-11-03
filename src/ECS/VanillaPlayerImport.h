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
#include <optional>
#include <utility>
#include <vector>

#include "CreatureRecord.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/PlayerRelations.h"
#include "ECS/ComputerMind.h"
#include "Enums.h"

namespace openblack::gamesave
{
class VanillaWorld;
} // namespace openblack::gamesave

// The players of a world file of the game, and the computer god of each player. The game deletes the computer gods of
// the land before it reads the file, so after the load each player has only the computer god of the file.
namespace openblack::ecs::vanilla_player_import
{

/// The type of a player that a computer god plays
inline constexpr uint32_t k_ComputerPlayerType = 2;

/// The computer god of a player of the file, with the values that the game reads back
struct ImportedComputerGod
{
	/// The mind plays the player
	bool switchedOn {false};
	/// A script paused the mind
	bool paused {false};
	/// A script holds the hand of the god
	bool scriptHold {false};
	/// The mind made its start on the land
	bool started {false};
	/// The speed of the hand of the god in metres a second
	float handSpeed {90.0f};
	/// The map position of the hand of the god
	gamesave::MapCoords hand {};
	/// How many times the mind wanted each miracle
	std::array<uint32_t, computer_mind::k_MagicTypes> castRequests {};
	/// The suppression and the weight of each node of the tree, from the top of the tree
	std::vector<std::pair<float, float>> nodes;
	/// The value that the mind keeps for each player, by player number
	std::vector<float> attitudes;
	/// How many objects each list has that the import does not take: the miracles that the god cast, the things that
	/// it saw and the actions in its queue
	size_t casts {0};
	size_t seen {0};
	size_t queue {0};
};

/// A player of the file
struct ImportedPlayer
{
	PlayerNames player {PlayerNames::PLAYER_ONE};
	/// The type of the player marks a computer player
	bool computerType {false};
	/// The computer god of the player, none when the player has no computer god
	std::optional<ImportedComputerGod> computer;
};

/// The players of the file, in the order of the file. A record with no player number of the game is not in the list.
[[nodiscard]] std::vector<ImportedPlayer> ReadPlayers(const gamesave::VanillaWorld& world);

/// True when the god of the file has the values of a god that the game made and that never played: switched off, not
/// marked, not started, with no pause, no hold, no miracle request, the first speed and the first tree. openblack makes
/// the mind of such a god only when a land script first asks for it, and the new mind has the same values.
[[nodiscard]] bool IsUnusedGod(const ImportedPlayer& player, const ImportedComputerGod& file);

/// The computer god of a player after the load. The god of the file replaces the god of the land. The tree, the speed,
/// the switch, the pause, the hold of a script, the start and the miracle requests come from the file. The other parts
/// start again as the game starts them after a load. The camera of the god goes to the hand.
[[nodiscard]] components::ComputerPlayer ComputerOf(const ImportedPlayer& player, const ImportedComputerGod& file,
                                                    float landHeight);

/// The mind of the player takes the values that the god of the file keeps for each player
void PutAttitudes(components::ComputerAttitudes& attitudes, const ImportedComputerGod& file);

} // namespace openblack::ecs::vanilla_player_import
