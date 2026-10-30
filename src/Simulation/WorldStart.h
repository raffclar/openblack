/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include <spdlog/common.h>

#include "Common/DeveloperTools.h"
#include "Common/PlayerProfiles.h"
#include "Creature/CreatureTattoo.h"
#include "Visual/UploadPacer.h"

namespace openblack
{

enum class LoggingSubsystem : uint8_t
{
	game,
	input,
	graphics,
	scripting,
	audio,
	pathfinding,
	ai,

	_count
};

constexpr static std::array<std::string_view, static_cast<size_t>(LoggingSubsystem::_count)> k_LoggingSubsystemStrs {
    "game",        //
    "input",       //
    "graphics",    //
    "scripting",   //
    "audio",       //
    "pathfinding", //
    "ai",          //
};

/// The land a new game starts on, unless another is asked for
inline constexpr std::string_view k_DefaultStartLevel = "Land1.txt";

namespace gui
{
class TextDatabase;
} // namespace gui

} // namespace openblack

namespace openblack::simulation
{

/// About how many bytes of files the loading threads read at a time, and how much of what they make may reach the
/// graphics card each frame, so no frame waits long for it: sixteen buffers or textures keep the renderer's share of a
/// loading frame to about 6 ms on Vulkan, where some models' hundreds of small parts made it 17 ms
inline constexpr size_t k_FrameLoadBudget = 256 << 10;
inline constexpr graphics::UploadPacer::Allowance k_FrameUploads {.bytes = 128 << 10, .uploads = 16};
/// How many bytes of files the loading threads read at a time while the loading screen shows. No game frame waits
/// then, so the threads read as much as they can.
inline constexpr size_t k_LoadingScreenLoadBudget = 64 << 20;

/// The level of the log of each part of the program
using LogLevels = std::array<spdlog::level::level_enum, k_LoggingSubsystemStrs.size()>;

/// Starts the log of each part of the program: to the console, or to one file that all the parts share
void StartLogs(const std::string& logFile, const LogLevels& levels);

/// The parts of the start of the world's data that belong to the view of one player: the timer of the start, the
/// hand's animations and the game's menu. The game's client does them. A server has no view.
class WorldDataViewInterface
{
public:
	virtual ~WorldDataViewInterface() = default;

	/// A step of the start is complete
	virtual void StartupStep(std::string_view step) = 0;
	/// Loads the animations of the hand for the hand's mesh
	virtual void LoadHandAnimation() = 0;
	/// Makes the game's menu with the settings of the profile. Gives the texts of the game, or none when the view has
	/// none.
	[[nodiscard]] virtual const gui::TextDatabase* MakeInterface() = 0;
};

/// Registers every resource of the game's data for the loading threads, in the order of the game: the meshes, the
/// animations, the info tables, the lands, the sound banks and the textures. Gives false when a pack does not load.
[[nodiscard]] bool RegisterWorldData(WorldDataViewInterface& view);

/// What moves the body of each species of creature, from the files of the species
void LoadCreatureRigs();

/// Loads what the tattoos and the marks of the creatures are painted with, in place of what loaded before
void LoadCreatureSkinArt();

/// Reads the pictures of the current profile for the designs of the symbols and the tattoos. Gives the pictures when
/// they changed.
[[nodiscard]] std::optional<creature_tattoo::Pictures> ReadTattooPictures();

/// The options that the player keeps with the profile and that the world uses: the hand, the help and the texts
void ApplyPlayerSettings(const player_profiles::Settings& settings);

/// The parts of the start of play that belong to the view of one player
class PlayStartViewInterface
{
public:
	virtual ~PlayStartViewInterface() = default;

	/// The scene and the reflection of the sea take the size of the window
	virtual void SizeSceneToWindow() = 0;
	/// The video asked for, the custom music and the help record of the player
	virtual void StartPlayView() = 0;
	/// The logo effect and the menu over a game that continued, before the first frame
	virtual void BeforeFirstFrame(bool continued) = 0;
};

/// The start of play after the first land loaded: the things go into the map's cells, the clocks start and, if asked,
/// every resource loads before the first frame. A game that continued from a save keeps the time of the save.
void BeginPlay(bool continued, bool loadAllFirst, PlayStartViewInterface& view);
/// Makes the maps of the land that the creatures use: where they can walk, and where they may go in a fight. The game
/// makes them at the end of the loading screen. The game server calls this at the same point, before the first turn,
/// so that each computer makes the maps from the same land.
void PrepareLandMaps();
/// Loads every resource that waits, as the loading screen does, before the next frame. A thing that waits for its
/// picture or its model then starts after the same frames on each run and on each computer of a network game.
void LoadEveryResource();
/// Every computer of a network game, the game server too, calls this just before it loads the land, so that each starts
/// the land from the same state: the next reset of the registry gives the ids of a new registry, the game's random state
/// is as in a new program, and every stream starts from the seed
void PrepareNetworkLand(uint32_t seed);
/// Tools only: the AI soak modes for the land that loads next. Every computer of a network game calls this with the
/// same values just before it loads the land, the game server too. A build without the developer tools refuses the
/// modes: this gives the reason, and sets nothing.
[[nodiscard]] std::optional<std::string_view> ApplySoakTools(const developer_tools::SoakTools& tools);

} // namespace openblack::simulation
