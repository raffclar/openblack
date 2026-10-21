/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <filesystem>
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

namespace openblack::filesystem
{
class FileSystemInterface;
}

namespace openblack::ecs::systems
{

class CreatureWebPageSystem final: public CreatureWebPageSystemInterface
{
public:
	/// The player whose profile folder gets the files, none when no folder gets them
	using PlayerOfFiles = std::function<std::optional<std::u16string>()>;

	explicit CreatureWebPageSystem(PlayerOfFiles player);

	void ProcessTurn() override;
	void CreatureTurn(entt::entity creature) override;
	void WritePage(entt::entity creature) override;
	void FlushWrites() override;
	[[nodiscard]] bool AnyPictureWaiting() const override;
	void CollectPictures(const Screens& screens) override;
	[[nodiscard]] std::span<const std::u16string> GetCaptions() const override;

private:
	/// A picture of the screen asked for, and where its file goes
	struct Waiting
	{
		std::string name;
		std::u16string player;
		uint32_t slot;
	};

	/// What the writer needs to write the page and the upload package of a creature, made on the game's thread
	struct PageJob
	{
		filesystem::FileSystemInterface* files;
		std::filesystem::path templatePath;
		std::filesystem::path gameFolder;
		std::filesystem::path playerFolder;
		std::string pageFile;
		std::vector<creaturehtml::Value> values;
		uint16_t stamp;
	};

	/// Fills the template with the values into the page in the player's profile folder, if the template is there
	static void WritePageFile(const PageJob& job);
	/// The thread that writes the files, made when the first file is written
	save_files::BackgroundWriter& Writer();

	PlayerOfFiles _player;
	/// The captions belong to the slots of the files, not to a creature
	std::array<std::u16string, creature_web_page::k_MostPictures> _captions {};
	std::vector<Waiting> _waiting;
	uint32_t _asked {0};
	/// Last, so that it writes the files that wait before the members go
	std::unique_ptr<save_files::BackgroundWriter> _writer;
};

} // namespace openblack::ecs::systems
