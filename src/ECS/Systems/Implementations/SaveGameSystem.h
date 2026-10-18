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
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <SavedObjects.h>
#include <VanillaSaveFile.h>
#include <WorldFile.h>
#include <entt/entity/fwd.hpp>
#include <entt/meta/context.hpp>

#include "ECS/Registry.h"
#include "ECS/Systems/SaveGameSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class SaveGameSystem final: public SaveGameSystemInterface
{
public:
	bool Save(uint32_t slot, std::u16string_view name) override;
	bool Load(uint32_t slot) override;
	bool Delete(uint32_t slot) override;
	[[nodiscard]] std::optional<std::vector<uint8_t>> ReadPictureFile(std::string_view name) const override;
	[[nodiscard]] std::vector<Slot> Slots() const override;
	[[nodiscard]] std::vector<VanillaCheck> CheckVanillaRecords() const override;
	void TakeTemplePicture() override;
	bool InstantSave(uint32_t slot) override;
	bool SaveOnExit() override;
	void EndTurn() override;
	bool QuickSave() override;
	bool QuickLoad() override;
	void NewLand() override;
	[[nodiscard]] bool AnyPictureWaiting() const override;
	void CollectPictures(const Screens& screens) override;

private:
	/// The world file in the game's layout that the creatures of the open land came from, as the file had it
	struct VanillaOrigin
	{
		uint32_t slot {0};
		uint32_t land {0};
		std::filesystem::path folder;
		std::vector<uint8_t> bytes;
		std::vector<gamesave::FoundCreature> creatures;
		std::vector<gamesave::FoundLeash> leashes;
	};

	/// A picture of the screen that the system asked for, and the slots that take it when it is back
	struct WaitingPicture
	{
		std::string name;
		std::vector<uint32_t> slots;
	};

	/// Saves the world into the slot. With putPicture, the slot's picture comes from the temporary picture file now.
	bool SaveSlot(uint32_t slot, std::u16string_view name, bool putPicture);
	/// Asks the renderer for a picture of the screen for the temporary picture file, then for the slots. Without a
	/// renderer, the slots take the temporary picture file as it is.
	void AskForPicture(std::vector<uint32_t> slots);
	/// Writes the temporary picture file from a picture of the screen
	void WriteTemporaryPicture(std::span<const uint8_t> screenPicture);
	/// Copies the pictures of the temporary picture file into the picture files of the slot
	void PutSlotPicture(uint32_t slot);

	/// The reflection of the components, which the system registers at its first use
	[[nodiscard]] const entt::meta_ctx& Reflection();
	/// Loads the whole world of a slot: the saved land, its entities, its clock and its scripts
	bool LoadWholeWorld(uint32_t slot, const gamesave::WorldFile& world, const gamesave::GameChunk& game);

	bool LoadVanilla(uint32_t slot, const std::filesystem::path& slotFolder, const std::filesystem::path& mindFolder,
	                 const std::vector<uint8_t>& bytes);
	bool WriteVanilla(uint32_t slot, const std::filesystem::path& slotFolder, std::span<const gamesave::SavedCreature> saved,
	                  std::span<const entt::entity> entities);
	void SetVanillaOrigin(uint32_t slot, uint32_t land, const std::filesystem::path& folder,
	                      const std::optional<std::vector<uint8_t>>& bytes);
	void KeepVanillaRecord(ecs::Registry& registry, entt::entity entity, uint32_t number,
	                       const gamesave::CreatureObject& loaded,
	                       const std::optional<gamesave::LeashStatusObject>& loadedLeash);

	std::unique_ptr<entt::meta_ctx> _reflection;
	std::optional<VanillaOrigin> _vanilla;
	std::vector<WaitingPicture> _waitingPictures;
	uint32_t _pictureRequests {0};
	/// The turns of the last autosave, the last quick save and the last load. The turn of the last autosave starts again
	/// with each land; the other two stay from land to land.
	uint32_t _lastAutosaveTurn {0};
	uint32_t _lastQuickSaveTurn {0};
	uint32_t _lastLoadTurn {0};
};

} // namespace openblack::ecs::systems
