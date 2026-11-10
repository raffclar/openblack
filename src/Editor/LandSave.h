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
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/DayNightClock.h"
#include "3D/LandData.h"

/// What the editor saves of a land: the land's cells into the data of a landscape file, and the lines of a land script
/// for what it placed and drew, as plain rules
namespace openblack::editor::land_save
{

/// Copies the cells of the blocks of the land as it is now into the data of its landscape file. Each block goes to the
/// block of the data at the same place on the map. Gives the count of the blocks that it copied.
size_t CopyCells(LandData& land, std::span<const lnd::LNDBlock> blocks);

/// A tree as the land script makes it again
struct TreeLine
{
	/// The number of its forest, -1 for none
	int32_t forest {-1};
	glm::ivec2 place {0, 0};
	int32_t type {0};
	bool nonScenic {false};
	/// Its turn about the up axis, in radians, as the script gives it
	float yAngle {0.0f};
	float scale {1.0f};
	float maxScale {1.0f};
};

/// The line that makes a tree again. The game writes its turn, its size now and its largest size.
[[nodiscard]] std::string TreeText(const TreeLine& tree);

/// A feature as the land script makes it again
struct FeatureLine
{
	glm::ivec2 place {0, 0};
	/// The name of its row of the features' table
	std::string name;
	/// Its turn about the up axis, in radians, as the script gives it
	float yAngle {0.0f};
	float scale {1.0f};
};

/// The line that makes a feature again. The game writes its turn and its size each times 1000, rounded to a float and
/// cut to a whole number, and a last number that is 0 for the features that a land script makes.
[[nodiscard]] std::string FeatureText(const FeatureLine& feature);

/// A mobile static as the land script makes it again
struct MobileStaticLine
{
	glm::ivec2 place {0, 0};
	/// Its row of the mobile statics' table
	int32_t row {0};
	/// Its height above the land
	float altitude {0.0f};
	/// Its angles about x, y and z, in radians, as the script gives them
	glm::vec3 angles {0.0f};
	float scale {1.0f};
};

/// The line that makes a mobile static again: its place, its row, its height above the land, its three angles and its
/// size, each number with a fraction
[[nodiscard]] std::string MobileStaticText(const MobileStaticLine& thing);

/// A mobile object as the land script makes it again
struct MobileObjectLine
{
	glm::ivec2 place {0, 0};
	/// Its row of the mobile objects' table
	int32_t row {0};
	/// Its turn about the up axis, in radians, as the script gives it
	float yAngle {0.0f};
	float scale {1.0f};
};

/// The line that makes a mobile object again. The game writes its turn and its size each times 1000, rounded to a float
/// and cut to a whole number.
[[nodiscard]] std::string MobileObjectText(const MobileObjectLine& thing);

/// A villager as the land script makes it again
struct VillagerLine
{
	glm::ivec2 place {0, 0};
	/// The number of its town, -1 for none
	int32_t town {-1};
	/// A villager of no tribe is special: the line gives its row of the villagers' table
	bool special {false};
	int32_t row {0};
	/// Its tribe and its kind, such as NORSE_FORESTER
	std::string name;
	/// The place of the door of its home, none when it has no home
	std::optional<glm::ivec2> door;
	int32_t age {0};
};

/// The line that makes a villager again. The game writes one of three lines:
/// - A special villager: its town, its place, its row and its age.
/// - A villager with a home: the place of the door of its home, its place, its name and its age.
/// - Another villager: its town, its place, its name and its age. The command takes a place as its third argument,
///   thus the game writes the name with no quotes.
[[nodiscard]] std::string VillagerText(const VillagerLine& villager);

/// A miracle dispenser as the land script makes it again
struct DispenserLine
{
	/// The number of its town, -1 for none
	int32_t town {-1};
	glm::ivec2 place {0, 0};
	/// The name of its building: the tribe of its town and the name of its row, such as JAPANESE_ABODE_SPELL_DISPENSER
	std::string building;
	/// The name of its miracle
	std::string miracle;
	/// Its turn about the up axis, in radians, as the script gives it
	float yAngle {0.0f};
	float scale {1.0f};
	/// The turns from the time that a player takes its bubble to the next bubble
	uint32_t period {0};
};

/// The line that makes a miracle dispenser again. The game writes the turns between the bubbles as a number with a
/// fraction.
[[nodiscard]] std::string DispenserText(const DispenserLine& dispenser);

/// The line that makes a one-shot miracle again: its place and the name of the miracle that its seed casts at its
/// power-up. The game writes the command that finds the seed from the miracle.
[[nodiscard]] std::string OneShotMiracleText(glm::ivec2 place, std::string_view miracle);

/// The line that makes a creature again: its place, its row of the creatures' table and the number of its player
[[nodiscard]] std::string CreatureText(glm::ivec2 place, int32_t row, int32_t player);

/// A field as the land script makes it again
struct FieldLine
{
	/// The number of its town, -1 for none
	int32_t town {-1};
	glm::ivec2 place {0, 0};
	/// Its row of the fields' table
	int32_t row {0};
	/// Its turn about the up axis, in radians, as the script gives it
	float yAngle {0.0f};
};

/// The line that makes a field again. A field of a town gets the line with the town's number, its row and its turn. A
/// field with no town gets the line with only its place and its row.
[[nodiscard]] std::string FieldText(const FieldLine& field);

/// A building as the land script makes it again
struct AbodeLine
{
	/// The number of its town
	int32_t town {-1};
	glm::ivec2 place {0, 0};
	/// The tribe of its town and the name of its row, such as NORSE_ABODE_A
	std::string name;
	/// Its turn about the up axis, in radians, as the script gives it
	float yAngle {0.0f};
	float scale {1.0f};
	uint32_t food {0};
	uint32_t wood {0};
	/// The building is not complete: the line makes a plan of it
	bool planned {false};
};

/// The line that makes a building again. All the kinds of building have this line, except a town centre and a miracle
/// dispenser. The game writes its turn and its size each times 1000, rounded to a float and cut to a whole number.
[[nodiscard]] std::string AbodeText(const AbodeLine& abode);

/// A town centre as the land script makes it again
struct TownCentreLine
{
	/// The number of its town, -1 for none
	int32_t town {-1};
	glm::ivec2 place {0, 0};
	/// The tribe of its town and the name of its row, such as NORSE_ABODE_TOWN_CENTRE
	std::string name;
	/// Its turn about the up axis, in radians, as the script gives it
	float yAngle {0.0f};
	float scale {1.0f};
	/// The last share of the town's people that its totem sends to worship
	float worshipShare {0.0f};
};

/// The line that makes a town centre again. The game writes its turn, its size and the worship share of its totem each
/// times 1000, rounded to a float and cut to a whole number.
[[nodiscard]] std::string TownCentreText(const TownCentreLine& centre);

/// The belief of a town in one player
struct TownBelief
{
	/// The name of the player, such as PLAYER_ONE
	std::string player;
	float belief {0.0f};
	float cap {10.0f};
};

/// The boost that a script gave to one desire of a town
struct TownDesireBoost
{
	/// The name of the desire, such as Abodes
	std::string desire;
	float boost {0.0f};
};

/// A town as the land script makes it again
struct TownLine
{
	int32_t id {0};
	glm::ivec2 place {0, 0};
	/// The name of its player, such as PLAYER_ONE
	std::string player;
	int32_t tribe {0};
	/// The name of its tribe, such as NORSE
	std::string tribeName;
	/// The names of the miracles that the town holds, in the order of the magic types
	std::vector<std::string> miracles;
	/// Its belief in each player, in the order of the players, with the neutral player last
	std::vector<TownBelief> beliefs;
	/// Its desires in their order
	std::vector<TownDesireBoost> desires;
	bool uninhabitable {false};
	float balanceBeliefScale {1.0f};
	/// The place where its people gather, none when it has no place yet
	std::optional<glm::ivec2> congregation;
};

/// The lines that make a town again, in this order:
/// 1. The town: its number, its place, its player, the number of its tribe and the name of its tribe.
/// 2. A line for each miracle that it holds.
/// 3. For each player, a line for a belief that is not 0, then a line for a cap that is not 10.
/// 4. A line for each desire boost that is not 0.
/// 5. A line when nobody can live in it, then a line for a balance belief scale that is not 1.
/// 6. A line for the place where its people gather, when it has a place that is not at 0, 0.
[[nodiscard]] std::string TownText(const TownLine& town);

/// What the land script keeps of the whole land, as the game writes it after the version line
struct GameLine
{
	int32_t landNumber {0};
	/// The path of the landscape file that the land loads. With no path, the file gets no landscape line.
	std::string landscape;
	/// The start place of the camera in map units. A land with no start place has 0, 0, and the camera then starts at
	/// the temple of the player.
	glm::ivec2 startCamera {0, 0};
	/// The eight balances of the land
	std::array<float, 8> landBalance {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	/// The name of the fly-by file. With no name, the file gets no fly-by line.
	std::string flyByFile;
	float townInfluenceMultiplier {1.0f};
	float playerInfluenceMultiplier {1.0f};
	/// The cycle of day and night
	DayCycle dayCycle {};
};

/// The lines of the whole land, in this order:
/// 1. The number of the land.
/// 2. The landscape file that the land loads, when the land has one.
/// 3. The start place of the camera.
/// 4. A line for each balance of the land that is not 1, with the number of the balance.
/// 5. The fly-by file, when the land has one.
/// 6. The influence multiplier of the towns, then the influence multiplier of the players.
/// 7. The cycle of day and night.
[[nodiscard]] std::string GameText(const GameLine& game);

/// The comment line that the game writes before the towns of a player
[[nodiscard]] std::string PlayerHeading(std::string_view player);

/// The comment line that the game writes before each town
[[nodiscard]] std::string TownHeading(int32_t id);

/// The lines that make a stream again: the stream with its number, then each of its points in order
[[nodiscard]] std::string StreamText(int32_t id, std::span<const glm::ivec2> places);

/// The lines that make a footpath again: a new footpath, which becomes the script's footpath 0, then its nodes from the
/// last to the first. Each node line puts its node first on the path, so the script makes the nodes in the same order.
[[nodiscard]] std::string FootpathText(std::span<const glm::ivec2> places);

/// The streams and the footpaths that a saved land script makes. Each stream is the list of the texts of its places, as
/// the lines give them. Each footpath is the list of the texts of its nodes in the order of the path: each node line puts
/// its node first.
struct SavedPaths
{
	std::vector<std::vector<std::string>> streams;
	std::vector<std::vector<std::string>> footpaths;
};

/// Reads the streams and the footpaths that a land script makes, in the order of the script. The read stops where a
/// load of the script stops.
[[nodiscard]] SavedPaths ReadSavedPaths(const std::string& script);

/// The text of the list of the things that the editor made, in the order that it made them: the number of each entity on
/// a line of its own. A world load gives each entity the same number, so the list finds the things again.
[[nodiscard]] std::string MadeText(std::span<const entt::entity> made);

/// Reads the list of the things that the editor made. A line that is not a number is passed over.
[[nodiscard]] std::vector<entt::entity> ReadMade(std::string_view text);

/// The name of a saved file: the land's name with the extension of the file
[[nodiscard]] std::string FileName(std::string_view landName, std::string_view extension);

/// True when a write to the path must wait for the user to say yes: the path holds a file already, or it lies in the
/// game's own folder (an empty game folder holds nothing). A path that is new and outside the game's folder needs no
/// question.
[[nodiscard]] bool NeedsConfirmation(const std::filesystem::path& path, const std::filesystem::path& gameFolder);

} // namespace openblack::editor::land_save
