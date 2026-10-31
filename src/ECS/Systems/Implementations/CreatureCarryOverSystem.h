/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Creature/CreatureCarryOver.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"

namespace openblack::creaturemind
{
struct PhysiqueFileData;
}

namespace openblack::ecs::save_files
{
class BackgroundWriter;
}

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureCarryOverSystem final: public CreatureCarryOverSystemInterface
{
public:
	/// The file of the current player's profile the creature is kept in, none when there is none
	using ProfileFile = std::function<std::optional<std::string>()>;

	/// The creature kept is written to and read from the profile's file in the folder, when there are both
	explicit CreatureCarryOverSystem(std::optional<std::filesystem::path> folder = std::nullopt, ProfileFile profileFile = {});
	/// Writes every file that waits, then stops the thread of the writer
	~CreatureCarryOverSystem() override;
	CreatureCarryOverSystem(const CreatureCarryOverSystem&) = delete;
	CreatureCarryOverSystem& operator=(const CreatureCarryOverSystem&) = delete;
	CreatureCarryOverSystem(CreatureCarryOverSystem&&) = delete;
	CreatureCarryOverSystem& operator=(CreatureCarryOverSystem&&) = delete;

	void KeepPlayersCreature() override;
	void KeepCreature(entt::entity creature) override;
	void Keep(std::shared_ptr<const creaturemind::MindFileData> file) override;
	[[nodiscard]] std::shared_ptr<const creaturemind::MindFileData> Kept() const override;
	[[nodiscard]] std::shared_ptr<const creaturemind::MindFileData> KeptOrRead() override;
	[[nodiscard]] std::optional<std::filesystem::path> ProfilePhysique() const override;
	[[nodiscard]] std::optional<std::filesystem::path> ProfileMind() const override;
	std::optional<entt::entity> LoadPlayersCreature(glm::vec2 place) override;
	void ProcessTurn() override;
	void CreatureTurn(entt::entity creature) override;
	void SetLandKeepsCreatures(bool keeps) override;
	void EndGame() override;
	void Reset() override;
	void StartGame() override;
	void FinishWrites() const override;
	void SetBroughtCreature(entt::entity creature) override;

private:
	struct Arriving
	{
		entt::entity creature;
		creature_carry_over::Fizz fizz;
	};

	/// The creature kept goes to its files on the thread of the writer. The system reads the files when it kept no
	/// creature since the game started.
	void Write(std::shared_ptr<const creaturemind::MindFileData> mind, creaturemind::PhysiqueFileData physique) const;
	void ReadIfNoneKept();
	/// The player's creatures that have lived another 600 turns are written to the player's files
	void SaveAsTheyLive();
	/// The player's creature is written to its files each time it has lived 600 more turns
	void SaveAsItLives(entt::entity creature);
	/// The sparkle into sight of the creatures from a last land, or of one creature, moves on
	void StepArrivals(std::optional<entt::entity> creature);

	/// The kept creature's two files, when there is a folder and a profile to keep them for
	[[nodiscard]] std::optional<creature_carry_over::KeptFiles> Files() const;
	/// Whether this land is a network game. The land stays a network game until it clears, also after this computer
	/// left the network game: the save at the land clear follows the rules of a network game.
	[[nodiscard]] bool NetworkLand();

	std::optional<std::filesystem::path> _folder;
	ProfileFile _profileFile;
	/// The profile's file the game being played keeps its creature in, taken when it started; none before the first game
	std::optional<std::string> _gameFile;
	std::shared_ptr<const creaturemind::MindFileData> _kept;
	std::vector<Arriving> _arriving;
	bool _landKeepsCreatures {true};
	/// The creature that the god of this computer brought into the network game, and the turns that it lived when this
	/// computer last looked at it. This is not world state: only this computer writes the player's files.
	std::optional<entt::entity> _brought;
	std::optional<uint32_t> _broughtTurnsSeen;
	/// This land was a network game at a time that the system looked
	bool _networkLand {false};
	/// The writer starts with the first file to write. It is the last member, so that it stops first.
	mutable std::unique_ptr<save_files::BackgroundWriter> _writer;
};

} // namespace openblack::ecs::systems
