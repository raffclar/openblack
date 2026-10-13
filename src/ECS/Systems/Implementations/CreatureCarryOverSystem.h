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
#include <optional>
#include <string>
#include <vector>

#include "Creature/CreatureCarryOver.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"

namespace openblack::creaturemind
{
struct PhysiqueFileData;
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

	void KeepPlayersCreature() override;
	void KeepCreature(entt::entity creature) override;
	void Keep(std::shared_ptr<const creaturemind::MindFileData> file) override;
	[[nodiscard]] std::shared_ptr<const creaturemind::MindFileData> Kept() const override;
	std::optional<entt::entity> LoadPlayersCreature(glm::vec2 place) override;
	void ProcessTurn() override;
	void Reset() override;
	void StartGame() override;

private:
	struct Arriving
	{
		entt::entity creature;
		creature_carry_over::Fizz fizz;
	};

	/// The creature kept as written to its files, or read from them when none was kept since the game started
	void Write(const creaturemind::MindFileData& mind, const creaturemind::PhysiqueFileData& physique) const;
	void ReadIfNoneKept();

	/// The kept creature's two files, when there is a folder and a profile to keep them for
	[[nodiscard]] std::optional<creature_carry_over::KeptFiles> Files() const;

	std::optional<std::filesystem::path> _folder;
	ProfileFile _profileFile;
	/// The profile's file the game being played keeps its creature in, taken when it started; none before the first game
	std::optional<std::string> _gameFile;
	std::shared_ptr<const creaturemind::MindFileData> _kept;
	std::vector<Arriving> _arriving;
};

} // namespace openblack::ecs::systems
