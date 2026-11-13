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
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "Enums.h"
#include "PlayerSlots.h"

/// The network map for 32 gods (deviation 92). The map has one slot for each god. Each slot has a citadel and a starting
/// village. The slots stand in a ring round the middle of a round island, at equal angles, so that each slot has the
/// same land and the same distance to its two neighbours.
///
/// The village of each slot is the village of one god of a land that the game has for network play. The game reads
/// that land's script when it makes the map, and puts a copy of the village in each slot, turned to face the middle as
/// the original faces the middle of its own map. The map then is a land's script, as the game's own network maps are.
/// Everything here is pure, so the tests use their own small scripts.
namespace openblack::gods_ring_map
{
/// The slots: one for each god of a network game
inline constexpr size_t k_Slots = player_slots::k_MaxPlayers;
/// The radius of the ring of citadels, from the middle of the map, in units of the map
inline constexpr float k_RingRadius = 2000.0f;
/// The direction from the middle of the map to the first slot's citadel: south, toward the low z
inline constexpr float k_FirstSlotAngle = -std::numbers::pi_v<float> * 0.5f;
/// The angle between two slots that are next to each other
inline constexpr float k_SlotStep = 2.0f * std::numbers::pi_v<float> / static_cast<float>(k_Slots);

/// The camera at the start of a test on this computer alone: this far beyond the first slot's citadel and this high
/// above the land, and it looks at the point this far in from the citadel
inline constexpr float k_StartCameraBack = 300.0f;
inline constexpr float k_StartCameraHeight = 250.0f;
inline constexpr float k_StartCameraAhead = 150.0f;

/// The map's file name and its name in the list of the lands for network play
inline constexpr std::string_view k_File = "obm_32p_1";
inline constexpr std::string_view k_Name = "The ring of gods - 32 players";

/// The land that gives the village, in the game's folder, and the god whose village it is
inline constexpr std::string_view k_TemplateMap = "Online Maps/mpm_4p_1.map";
inline constexpr PlayerNames k_TemplateGod = PlayerNames::PLAYER_ONE;
/// The god next to the template god. The neutral towns between their citadels go in each gap between two slots.
inline constexpr PlayerNames k_TemplateNeighbour = PlayerNames::PLAYER_TWO;

/// A forest, or a group of trees in no forest, is a town's when its middle is nearer than this to the town's storage
/// pit. The game links a forest to a town by the same distance from the storage pit, from the town info.
inline constexpr float k_TownForestReach = 250.0f;
/// A neutral town nearer than this to the middle of the land is the town in the middle
inline constexpr float k_MiddleTownReach = 200.0f;
/// Each town of the map has its own range of numbers for its flocks and its forests, from the first number on
inline constexpr int32_t k_FirstThingNumber = 1000;
inline constexpr int32_t k_ThingsPerTown = 100;

/// The god of a slot (player_slots::PlayerOfSlot): the first 7 slots have the gods of a land, in order, and the later
/// slots the gods after the neutral player. None after the last slot.
[[nodiscard]] std::optional<PlayerNames> GodOfSlot(size_t slot);

/// A slot of the map: where its citadel stands and which god has it
struct Slot
{
	size_t index {0};
	glm::vec2 citadel {0.0f};
	/// The direction from the middle of the map to the citadel, in radians from the x axis toward the z axis
	float outward {0.0f};
	std::optional<PlayerNames> god;
};

/// The slots of the map round the middle of the map
[[nodiscard]] std::array<Slot, k_Slots> LayOutSlots(glm::vec2 middle);

/// A command line of a land's script, such as CREATE_ABODE(0, "3101.73,2064.61", "CELTIC_ABODE_B", 3926, 1000, 0, 0)
struct Line
{
	struct Argument
	{
		/// The argument as the script gives it, with no quotes
		std::string text;
		bool quoted {false};
	};
	std::string command;
	std::vector<Argument> arguments;
	/// The line as the script gives it
	std::string original;

	/// The command line of a line of a script. Nothing for an empty line, a "rem" line or a line with no command.
	[[nodiscard]] static std::optional<Line> Parse(std::string_view text);
	/// The line as a script gives it, from its command and its arguments
	[[nodiscard]] std::string Text() const;
};

/// A place of a script, "x,z" in units of the map. Nothing for an argument that is not a place.
[[nodiscard]] std::optional<glm::vec2> PlaceOf(const Line::Argument& argument);

/// The things of a town that are not part of its own lines: its flocks with their animals, its forests with their trees,
/// and the trees near it that are in no forest. The lines are in the order of the script.
struct TownThings
{
	std::vector<Line> lines;
	/// The numbers of the flocks and of the forests, in the order of the script. The map gives each one a new number.
	std::vector<std::string> flocks;
	std::vector<std::string> forests;
};

/// A neutral town of the land that gives the village, with all of its things
struct TownTemplate
{
	/// The lines of the town: the town, its spells, caps, fields, buildings and villagers
	std::vector<Line> town;
	TownThings things;
	/// The number of the town, as the script gives it
	std::string number;
	/// Where the town stands
	glm::vec2 centre {0.0f};
};

/// What the map takes from the land that gives the village
struct VillageTemplate
{
	/// The land's settings before its first town: its land number, the balance of its land, the influence multipliers
	/// and the length of its day. The chances of the spells that the fireflies give are in it too. The land's landscape
	/// and the start of its camera are not in it.
	std::vector<Line> settings;
	/// The god's village: the lines from the god's town to the god's citadel
	std::vector<Line> village;
	/// The flocks, forests and trees of the god's village (deviation 256)
	TownThings things;
	/// The land's weather
	std::vector<Line> weather;
	/// The number of the god's town, as the script gives it
	std::string town;
	/// The script's name of the god of the template
	std::string god;
	/// Where the god's citadel stands
	glm::vec2 citadel {0.0f};
	/// The direction from the middle of the land's map to the citadel, as Slot::outward
	float outward {0.0f};
	/// The middle of the land's map. The middle of this map is the same point.
	glm::vec2 middle {0.0f};
	/// The neutral towns between the god's citadel and the neighbour god's citadel, in the order of the script. Each gap
	/// between two slots gets a copy of each (deviation 256).
	std::vector<TownTemplate> gapTowns;
	/// The big forests between the god's citadel and the neighbour god's citadel, in the order of the script. Each gap
	/// gets a copy of each (deviation 256).
	std::vector<Line> gapBigForests;
	/// The mean distance of the citadels of the god and of the neighbour god from the middle of the land's map
	float godsRadius {0.0f};
	/// The neutral town in the middle of the land, which the middle of this map gets (deviation 256)
	std::optional<TownTemplate> middleTown;
};

/// The god's village of a land's script, on a map with this middle. Nothing when the script has no town or no citadel
/// for the god.
[[nodiscard]] std::optional<VillageTemplate> ReadTemplate(std::string_view script, PlayerNames god, glm::vec2 middle);

/// The village of the template, put in the slot: its places and angles turned about the citadel, its town numbered as
/// the slot, and its god's name changed to the slot's god. A slot with no god has a neutral village. The cap of the
/// belief of each god in the town is the template god's cap, so that each god in `gods` has the same cap in each town.
/// The village's flocks, forests and trees follow its town, with the numbers of the town's range.
[[nodiscard]] std::vector<Line> VillageInSlot(const VillageTemplate& village, const Slot& slot,
                                              std::span<const PlayerNames> gods);

/// The number of a town of the map. The slots' villages have the numbers of the slots. The towns of the gaps come next,
/// in the order of the slots, and the town in the middle is the last.
[[nodiscard]] size_t GapTownNumber(const VillageTemplate& village, size_t slot, size_t gapTown);
[[nodiscard]] size_t MiddleTownNumber(const VillageTemplate& village);

/// The neutral towns in the gap between the slot and the next slot, with their things. Each copy of a town turns as one
/// piece. Its town stands on the line from the middle of the map between the two slots, turned a small angle so that its
/// buildings are half-way between the buildings of the two villages. The town keeps its distance in from the citadels.
/// The big forests of the gap follow the towns. They stand in a row across the gap, at equal distances, half-way across
/// the widest space between the towns, each turned with the gap.
[[nodiscard]] std::vector<Line> GapTowns(const VillageTemplate& village, const Slot& slot, std::span<const PlayerNames> gods);

/// The neutral town in the middle of the map, with its things, at its place in the template
[[nodiscard]] std::vector<Line> MiddleTown(const VillageTemplate& village, std::span<const PlayerNames> gods);

/// The script of the whole map: the settings, a village in each slot, the towns of each gap, the town in the middle, and
/// the weather
[[nodiscard]] std::string MapScript(const VillageTemplate& village, std::span<const Slot> slots);

/// The lines of the frame of the map's script that FillFrame replaces with the lines of the land that gives the village
inline constexpr std::string_view k_SettingsMark = "rem openblack: settings";
inline constexpr std::string_view k_VillageMark = "rem openblack: village ";
inline constexpr std::string_view k_GapMark = "rem openblack: gap ";
inline constexpr std::string_view k_MiddleMark = "rem openblack: middle";
inline constexpr std::string_view k_WeatherMark = "rem openblack: weather";

/// The frame of the map's script: the script of MapScript with a mark line in place of the settings, of the village of
/// each slot, of the towns of each gap, of the town in the middle and of the weather. The frame has no line of the land that
/// gives the village, so that the map package can hold it (deviation 249).
[[nodiscard]] std::string ScriptFrame(std::span<const Slot> slots);

/// The script of the whole map from its frame: each mark line becomes the lines that MapScript puts there. Nothing when
/// a mark line names a slot that is not in the slots.
[[nodiscard]] std::optional<std::string> FillFrame(std::string_view frame, const VillageTemplate& village,
                                                   std::span<const Slot> slots);

/// The text of the list of the lands for network play (a localmaps.txt file), with a line for this map added at its
/// end. The line has the next number of the list. A list that has the map already does not change.
[[nodiscard]] std::string AddToLocalMaps(std::string_view localMaps);

/// Whether a land's file name in the list is this map, whatever its letter case
[[nodiscard]] bool IsThisMap(std::string_view file);
} // namespace openblack::gods_ring_map
