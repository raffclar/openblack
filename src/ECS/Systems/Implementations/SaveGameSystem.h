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
#include <span>
#include <vector>

#include <SavedObjects.h>
#include <VanillaSaveFile.h>
#include <WorldFile.h>
#include <entt/entity/fwd.hpp>

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

	bool LoadVanilla(uint32_t slot, const std::filesystem::path& slotFolder, const std::filesystem::path& mindFolder,
	                 const std::vector<uint8_t>& bytes);
	bool WriteVanilla(uint32_t slot, const std::filesystem::path& slotFolder, std::span<const gamesave::SavedCreature> saved,
	                  std::span<const entt::entity> entities);
	void SetVanillaOrigin(uint32_t slot, uint32_t land, const std::filesystem::path& folder,
	                      const std::optional<std::vector<uint8_t>>& bytes);
	void KeepVanillaRecord(ecs::Registry& registry, entt::entity entity, uint32_t number,
	                       const gamesave::CreatureObject& loaded,
	                       const std::optional<gamesave::LeashStatusObject>& loadedLeash);

	std::optional<VanillaOrigin> _vanilla;
};

} // namespace openblack::ecs::systems
