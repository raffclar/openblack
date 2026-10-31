/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SavedObjects.h>
#include <VanillaSaveFile.h>
#include <VanillaWorld.h>
#include <WorldFile.h>
#include <entt/entity/fwd.hpp>
#include <entt/meta/context.hpp>

#include "ECS/Registry.h"
#include "ECS/SaveFileWriter.h"
#include "ECS/SaveGameFiles.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/VanillaWorldImport.h"
#include "ECS/WorldSave.h"

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
	[[nodiscard]] std::vector<uint8_t> WorldState() override;
	void PrepareWorldState() override;
	[[nodiscard]] std::function<std::vector<uint8_t>()> WorldStateLater() override;
	bool LoadWorldState(std::span<const uint8_t> bytes) override;
	bool Delete(uint32_t slot) override;
	[[nodiscard]] std::optional<std::vector<uint8_t>> ReadPictureFile(std::string_view name) const override;
	[[nodiscard]] temple::save_game_room::SavedGamesStamp GetSavedGamesStamp() const override;
	[[nodiscard]] std::vector<Slot> Slots() const override;
	[[nodiscard]] std::vector<VanillaCheck> CheckVanillaRecords() const override;
	void TakeTemplePicture() override;
	bool InstantSave(uint32_t slot) override;
	bool SaveOnExit(bool quitting) override;
	[[nodiscard]] bool HasExitSave() const override;
	void EndTurn() override;
	bool PressQuickSave() override;
	bool PressQuickLoad() override;
	bool DoQuickSave() override;
	void DoQuickLoad() override;
	[[nodiscard]] bool LoadAsked() const override { return _askedLoad.has_value(); }
	bool LoadAskedSlot() override;
	void NewLand() override;
	[[nodiscard]] bool AnyPictureWaiting() const override;
	void CollectPictures(const Screens& screens) override;
	void PutChallengePicture(uint32_t place, std::span<const uint8_t> picture) override;
	[[nodiscard]] gamesave::ChallengePictureCells ChallengePictureCells(uint32_t place) override;
	void SetShowBox(ShowBox show) override { _showBox = std::move(show); }
	void SetReportProblem(ReportProblem report) override { _reportProblem = std::move(report); }
	void ReportWriteProblems() override;

private:
	/// Shows the box of a save or a load for a slot, when the game gave the function
	void ShowSaveBox(uint32_t slot, bool loading) const;
	ShowBox _showBox;
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

	/// Takes the copies of the world for a save into the slot and gives the writer all the folder and file work. With
	/// putPicture, the slot's picture comes from the temporary picture file now. Gives how the write of the world files
	/// ends, which the writer sets on its thread, or none when the game cannot save.
	std::shared_ptr<const std::atomic<ecs::save_files::SaveWrite>> SaveSlot(uint32_t slot, std::u16string_view name,
	                                                                        bool putPicture);
	/// Asks the renderer for a picture of the screen for the temporary picture file, then for the slots. Without a
	/// renderer, the slots take the temporary picture file as it is.
	void AskForPicture(std::vector<uint32_t> slots);
	/// Writes the temporary picture file from a picture of the screen
	void WriteTemporaryPicture(std::span<const uint8_t> screenPicture);
	/// Copies the pictures of the temporary picture file into the picture files of the slot
	void PutSlotPicture(uint32_t slot);
	/// Reads the challenge picture files of the game that the player plays now into the store, the first time and when
	/// the folder of that game changes
	void ReadChallengePictures();
	/// A load copies the challenge picture files of the slot into the folder of the game that the player plays now,
	/// and into the store. Gives the names of the files that it copied.
	std::vector<std::string> CopyChallengePicturesFrom(const std::filesystem::path& slotFolder);
	/// After a load, each challenge record shows its picture from the files that the load copied
	void ShowChallengePictures(const std::vector<std::string>& copied);

	/// The reflection of the components, which the system registers at its first use
	[[nodiscard]] const entt::meta_ctx& Reflection();
	/// Loads the whole world of a slot: the saved land, its entities, its clock and its scripts
	bool LoadWholeWorld(uint32_t slot, const gamesave::WorldFile& world, const gamesave::GameChunk& game,
	                    ecs::world_save::LoadKind kind = ecs::world_save::LoadKind::SavedGame);

	bool LoadVanilla(uint32_t slot, const std::filesystem::path& slotFolder, const std::filesystem::path& mindFolder,
	                 const std::vector<uint8_t>& bytes);
	/// The entity that each object number of a world file of the game became
	using VanillaMade = std::unordered_map<uint32_t, entt::entity>;
	/// Reads the whole world file of the game. Puts its turn, its towns, buildings, villagers, forests, trees, fields, fish
	/// farms, still things and script things into the world, then the running scripts, then takes the seeds of the
	/// random draws. The creatures are in the world already. Returns whether the file reads as a whole world.
	bool ImportVanillaWorld(uint32_t slot, std::span<const uint8_t> bytes, const VanillaMade& creatures);
	/// Each villager that keeps the work of a state of the file: a field or a fish farm, and a spot in a field
	using VanillaWorks = std::vector<std::pair<entt::entity, ecs::vanilla_world_import::ImportedWork>>;
	/// Puts the turn, the towns, the buildings of the towns and the villagers of the world file into the world
	bool ImportVanillaTowns(const gamesave::VanillaWorld& world, VanillaMade& made, VanillaWorks& works);
	/// Puts the forests, the big forests and the trees of the world file into the world, in the place of those of the
	/// land. Each town of the file takes its list of forests.
	void ImportVanillaForests(const gamesave::VanillaWorld& world, VanillaMade& made);
	/// Puts the fields and the fish farms of the world file into the world, in the place of those of the land. Each
	/// villager that keeps its work takes its field or fish farm, and each field and farm takes its workers of the file.
	void ImportVanillaFarms(const gamesave::VanillaWorld& world, VanillaMade& made, const VanillaWorks& works);
	/// Puts the still things and the loose things of the world file into the world: the rocks and the other static
	/// things, the lanterns, the dead trees, the features, the animated things and the loose things. A thing of the land
	/// that the file has at the same place with the same row stays, the other things of the land go, and the file makes
	/// the rest.
	void ImportVanillaStatics(const gamesave::VanillaWorld& world, VanillaMade& made);
	/// Puts the clock of day and night and the count of things made of the world file into the world
	void ImportVanillaGameValues(const gamesave::VanillaWorld& world);
	/// The markers, the timers, the highlights and the influences of the scripts replace those of the land. A heart of
	/// a temple is the temple of the land.
	void ImportVanillaScriptThings(const gamesave::VanillaWorld& world, VanillaMade& made);
	/// The animals and the flocks of the file replace those of the land. The flock of a temple is the flock of the temple
	/// of the land.
	void ImportVanillaAnimals(const gamesave::VanillaWorld& world, VanillaMade& made);
	/// The running scripts of the file replace the scripts of the land, with the table of the objects that they hold
	void ImportVanillaScripts(const gamesave::VanillaWorld& world, const VanillaMade& made);
	/// The world file in the game's layout of a save of a game that came from such a file
	[[nodiscard]] std::vector<ecs::save_files::SaveFile> VanillaFiles(uint32_t slot, const std::filesystem::path& slotFolder,
	                                                                  std::span<const gamesave::SavedCreature> saved,
	                                                                  std::span<const entt::entity> entities);
	/// A load that the game refuses, or that stops after the land cleared: the land that is open starts again as a new
	/// game, from its own script
	void StartLandAgain() const;
	void SetVanillaOrigin(uint32_t slot, uint32_t land, const std::filesystem::path& folder,
	                      const std::optional<std::vector<uint8_t>>& bytes);
	void KeepVanillaRecord(ecs::Registry& registry, entt::entity entity, uint32_t number,
	                       const gamesave::CreatureObject& loaded,
	                       const std::optional<gamesave::LeashStatusObject>& loadedLeash);

	/// Waits until the writer has written every file that it was given, before the game reads saved games
	void FlushWrites() const;
	[[nodiscard]] ecs::save_files::BackgroundWriter& Writer() const;

	ReportProblem _reportProblem;
	/// The saves whose world files the writer did not write completely, and that the player is not told of yet. The
	/// writer's thread counts them.
	std::shared_ptr<std::atomic<uint32_t>> _writeProblems {std::make_shared<std::atomic<uint32_t>>(0)};
	std::unique_ptr<entt::meta_ctx> _reflection;
	/// Writes the files of each save on a thread of its own
	mutable std::unique_ptr<ecs::save_files::BackgroundWriter> _writer;
	std::optional<VanillaOrigin> _vanilla;
	std::vector<WaitingPicture> _waitingPictures;
	/// The challenge picture files of the game that the player plays now, as the writer writes them, and their folder
	gamesave::ChallengePictureStore _challengePictures;
	std::optional<std::filesystem::path> _challengeFolder;
	bool _challengePicturesRead {false};
	uint32_t _pictureRequests {0};
	/// The count of the saves, the deletes and the slot pictures that the game started to write, for the save game room
	uint64_t _savedGamesChanges {0};
	/// The turns of the last autosave, the last quick save and the last load. The turn of the last autosave starts again
	/// with each land; the other two stay from land to land.
	uint32_t _lastAutosaveTurn {0};
	uint32_t _lastQuickSaveTurn {0};
	uint32_t _lastLoadTurn {0};
	/// The turn of the last autosave that the game made, which a save keeps. After a load the next autosave waits from
	/// the turn of the load, but a save still keeps the turn of the last real autosave.
	uint32_t _autosaveTurnToSave {0};
	/// The slot that a command of a turn asked to load. The game loads it after the draw of the frame.
	std::optional<uint32_t> _askedLoad;
};

} // namespace openblack::ecs::systems
