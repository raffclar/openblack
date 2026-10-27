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
#include <span>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Player.h"
#include "Enums.h"

/// The rules of a computer player's start on a land: the creature it gets when it has none, the size of its creature and
/// the miracles that its creature knows.
namespace openblack::computer_player
{

/// The mind file of the creature that a computer player gets when it has none
inline constexpr const char* k_CreatureMindFile = "ComputerControlledCreature";
/// The number of species that the game chooses from for that creature, by the species number from 0
constexpr uint32_t k_SpeciesChoices = 17;
/// The smallest size of the creature, as a share of the size of the local player's creature
constexpr float k_SmallestShareOfSize = 0.9f;
/// The most that a random value adds to that share
constexpr float k_MostAddedShare = 0.3f;
/// The miracles that the creature always knows, also when its player cannot cast them
constexpr std::array<MagicType, 4> k_AlwaysKnown {MagicType::Heal, MagicType::Food, MagicType::Wood, MagicType::Water};

/// A land script switches the mind on or off. Switched on, the mind also marks the player as a computer player.
void SwitchOn(ecs::components::ComputerPlayer& player, bool on);

/// The mind starts at its first turn while it is switched on, one time on each land
[[nodiscard]] bool DueToStart(const ecs::components::ComputerPlayer& player);

/// What the start of the mind looks at
struct StartFacts
{
	/// The player is the local player
	bool local {false};
	/// The player has a temple
	bool temple {false};
	/// The temple of the player has a pen for the creature
	bool pen {false};
	/// The player has a creature
	bool creature {false};
	/// The land is a skirmish
	bool skirmish {false};
	/// The game is a network game
	bool network {false};
	/// The local player has a creature
	bool localCreature {false};
};

/// In a skirmish that is not a network game, a player with a temple and a pen but no creature gets a creature
[[nodiscard]] bool GetsCreature(const StartFacts& facts);
/// The creature of a player that is not the local player gets a size from the local player's creature. This occurs
/// only when a land script named a player for it and the local player has a creature.
[[nodiscard]] bool TakesSize(const ecs::components::ComputerPlayer& player, const StartFacts& facts);
/// The creature of a marked computer player that is not the local player learns its player's miracles
[[nodiscard]] bool LearnsMiracles(const ecs::components::ComputerPlayer& player, const StartFacts& facts);

/// The species of the creature for a random value below k_SpeciesChoices
[[nodiscard]] CreatureType SpeciesFor(uint32_t roll);
/// The size of the creature: the shown size of the local player's creature times 0.9 plus the random share added
[[nodiscard]] float SizeLike(float localShownSize, float addedShare);
/// The miracles that the creature learns, in the order of the magic types. The player must have a temple. Then the
/// creature learns each miracle that the player can cast, and the miracles that it always knows.
[[nodiscard]] std::vector<MagicType> MiraclesToLearn(const ecs::components::Player::Miracles& miracles, bool temple);

/// What the start of a computer mind reads and changes in the game
class World
{
public:
	virtual ~World() = default;

	[[nodiscard]] virtual PlayerNames LocalPlayer() const = 0;
	/// The first creature of the player that is still the player's
	[[nodiscard]] virtual std::optional<entt::entity> Creature(PlayerNames player) const = 0;
	/// The size that the creature shows
	[[nodiscard]] virtual float ShownSize(entt::entity creature) const = 0;
	/// The creature gets the size at once, and shows it
	virtual void SetSize(entt::entity creature, float size) = 0;
	[[nodiscard]] virtual bool HasTemple(PlayerNames player) const = 0;
	/// The place of the creature's pen at the temple of the player, on the land
	[[nodiscard]] virtual std::optional<glm::vec3> Pen(PlayerNames player) const = 0;
	[[nodiscard]] virtual bool Skirmish() const = 0;
	/// The game is a network game
	[[nodiscard]] virtual bool NetworkGame() const { return false; }
	[[nodiscard]] virtual ecs::components::Player::Miracles Miracles(PlayerNames player) const = 0;
	virtual void MakeCreature(PlayerNames player, CreatureType species, const glm::vec3& position) = 0;
	/// The creature learns the miracle as a script teaches it
	virtual void Teach(entt::entity creature, MagicType miracle) = 0;
	virtual uint32_t GameRand(uint32_t range) = 0;
	virtual float GameFloatRand(float most) = 0;
};

/// The start of a computer mind on the land, in the order of the game
void Start(World& world, PlayerNames player, ecs::components::ComputerPlayer& state);

/// The number of species that the start of a network game chooses from for the creature of a slot with no player, by
/// the species number from 0
constexpr uint32_t k_SlotSpeciesChoices = 11;
/// The number of help texts that the start of a network game chooses from for the name of that creature
constexpr uint32_t k_SlotNameChoices = 99;
/// The stage of development that the creature of a slot with no player grows to at once
constexpr uint32_t k_SlotCreatureStage = 12;

/// At the start of a network game, the slot of a god is a slot with no player when the god has a citadel, and no human
/// and no computer mind plays the god
[[nodiscard]] bool SlotHasNoPlayer(bool human, bool computer, bool citadel);

/// A line of a land script switches a computer mind on or off, but not while the map script of a network game runs
[[nodiscard]] bool LandScriptSwitchesMind(bool networkMapScript);

/// What the start of a network game reads and changes in the game when it fills the slots with no player
class SlotWorld
{
public:
	virtual ~SlotWorld() = default;

	[[nodiscard]] virtual bool HasCitadel(PlayerNames player) const = 0;
	/// The player is marked as a computer player
	[[nodiscard]] virtual bool IsComputerPlayer(PlayerNames player) const = 0;
	/// The game makes the player when the land has none yet
	virtual void MakePlayer(PlayerNames player) = 0;
	/// The computer mind of the player switches on, as a land script switches it on
	virtual void SwitchOn(PlayerNames player) = 0;
	/// Makes the creature of the player at the creature home of its citadel, names it with the words of the help text
	/// and grows it to the stage
	virtual void MakeSlotCreature(PlayerNames player, CreatureType species, uint32_t nameText, uint32_t stage) = 0;
	virtual uint32_t GameRand(uint32_t range) = 0;
};

/// At the start of a network game, in the order of the gods: each slot with no player gets a computer god. The game
/// takes a random help text for the name and a random species, switches the mind of the god on, and makes its creature
/// fully grown at the creature home of its citadel.
void FillEmptySlots(SlotWorld& world, std::span<const PlayerNames> gods, std::span<const PlayerNames> humans);

/// The gods of the slots of a network game, in the order of the players. The neutral player is not a god of a slot.
constexpr std::array<PlayerNames, 7> k_SlotGods {PlayerNames::PLAYER_ONE,  PlayerNames::PLAYER_TWO,  PlayerNames::PLAYER_THREE,
                                                 PlayerNames::PLAYER_FOUR, PlayerNames::PLAYER_FIVE, PlayerNames::PLAYER_SIX,
                                                 PlayerNames::PLAYER_SEVEN};

/// At the start of a network game, all the humans of the network session join in the same turn. The game makes the
/// player of each human when the land has none. Then, when the land waits for its humans, each slot with no player
/// gets a computer god.
void HumansJoin(SlotWorld& world, std::span<const PlayerNames> humans, bool landWaits);

} // namespace openblack::computer_player
