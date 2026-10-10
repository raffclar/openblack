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
#include <optional>
#include <string_view>
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
	/// The player's profile file the creature is kept under, until players have profiles of their own
	static constexpr std::string_view k_ProfileFile = "Player.erc";

	/// The creature kept is written to and read from the folder, when there is one
	explicit CreatureCarryOverSystem(std::optional<std::filesystem::path> folder = std::nullopt);

	void KeepPlayersCreature() override;
	void Keep(std::shared_ptr<const creaturemind::MindFileData> file) override;
	[[nodiscard]] std::shared_ptr<const creaturemind::MindFileData> Kept() const override;
	std::optional<entt::entity> LoadPlayersCreature(glm::vec2 place) override;
	void ProcessTurn() override;
	void Reset() override;

private:
	struct Arriving
	{
		entt::entity creature;
		creature_carry_over::Fizz fizz;
	};

	/// The creature kept as written to its files, or read from them when none was kept since the game started
	void Write(const creaturemind::MindFileData& mind, const creaturemind::PhysiqueFileData& physique) const;
	void ReadIfNoneKept();

	std::optional<std::filesystem::path> _folder;
	std::shared_ptr<const creaturemind::MindFileData> _kept;
	std::vector<Arriving> _arriving;
};

} // namespace openblack::ecs::systems
