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
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Creature/CreatureWebPage.h"
#include "ECS/SaveFileWriter.h"
#include "ECS/Systems/CreatureWebPageSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureWebPageSystem final: public CreatureWebPageSystemInterface
{
public:
	/// The player whose profile folder gets the files, none when no folder gets them
	using PlayerOfFiles = std::function<std::optional<std::u16string>()>;

	explicit CreatureWebPageSystem(PlayerOfFiles player);
	/// Writes every file that waits, then stops the thread of the writer
	~CreatureWebPageSystem() override;
	CreatureWebPageSystem(const CreatureWebPageSystem&) = delete;
	CreatureWebPageSystem& operator=(const CreatureWebPageSystem&) = delete;
	CreatureWebPageSystem(CreatureWebPageSystem&&) = delete;
	CreatureWebPageSystem& operator=(CreatureWebPageSystem&&) = delete;

	void ProcessTurn() override;
	void CreatureTurn(entt::entity creature) override;
	void WritePage(entt::entity creature) override;
	[[nodiscard]] bool AnyPictureWaiting() const override;
	void CollectPictures(const Screens& screens) override;
	[[nodiscard]] std::span<const std::u16string> GetCaptions() const override;
	void FinishWrites() const override;

private:
	/// A picture of the screen asked for, and where its file goes
	struct Waiting
	{
		std::string name;
		std::u16string player;
		uint32_t slot;
	};

	/// Fills the template with the values into the page in the player's profile folder, if the template is there
	static void WritePageFile(std::u16string_view player, std::u16string_view name,
	                          std::span<const creaturehtml::Value> values);
	/// The page, then the package of the values and the pictures for the upload tool. The writer's thread does this.
	static void WritePageFiles(std::u16string_view player, std::u16string_view name,
	                           std::span<const creaturehtml::Value> values, uint16_t stamp);
	[[nodiscard]] save_files::BackgroundWriter& Writer() const;

	PlayerOfFiles _player;
	/// The captions belong to the slots of the files, not to a creature
	std::array<std::u16string, creature_web_page::k_MostPictures> _captions {};
	std::vector<Waiting> _waiting;
	uint32_t _asked {0};
	/// The writer starts with the first file to write. It is the last member, so that it stops first.
	mutable std::unique_ptr<save_files::BackgroundWriter> _writer;
};

} // namespace openblack::ecs::systems
