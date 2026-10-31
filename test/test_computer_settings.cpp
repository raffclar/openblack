/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>

#include <ComputerSettings.h>
#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS

#include "Common/PlayerProfiles.h"
#include "ECS/Systems/Implementations/ComputerSettingsSystem.h"
#include "Visual/DetailLevel.h"

using namespace openblack;
using namespace openblack::computer_settings;

namespace
{

/// A folder of its own for one test, removed after it
class TempFolder
{
public:
	explicit TempFolder(std::string_view name)
	    : _path(std::filesystem::temp_directory_path() / "openblack_test_computer_settings" / std::string(name))
	{
		std::error_code error;
		std::filesystem::remove_all(_path, error);
		std::filesystem::create_directories(_path, error);
	}
	~TempFolder()
	{
		std::error_code error;
		std::filesystem::remove_all(_path, error);
	}
	TempFolder(const TempFolder&) = delete;
	TempFolder& operator=(const TempFolder&) = delete;
	TempFolder(TempFolder&&) = delete;
	TempFolder& operator=(TempFolder&&) = delete;

	[[nodiscard]] const std::filesystem::path& Path() const { return _path; }

private:
	std::filesystem::path _path;
};

std::string ReadText(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void WriteText(const std::filesystem::path& path, std::string_view text)
{
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	stream << text;
}

} // namespace

TEST(ComputerSettings, EveryValueOfAFileIsRead)
{
	const auto [settings, problems] = Parse(R"({
		"detail_level": 6, "max_detail_level": 7, "effects_volume": 0.25, "music_volume": 0,
		"width": 800, "height": 600, "window_mode": "fullscreen", "vsync": true,
		"custom_detail": {"model_detail": 0.3, "clouds": false, "water_tiling": 0.5, "rain_splashes": 3,
		                  "fog": false, "low_res_textures": true, "light_boost": 0.5}
	})");
	EXPECT_TRUE(problems.empty());
	EXPECT_EQ(settings.detailLevel, 6);
	EXPECT_EQ(settings.maxDetailLevel, 7);
	EXPECT_EQ(settings.effectsVolume, 0.25f);
	EXPECT_EQ(settings.musicVolume, 0.0f);
	EXPECT_EQ(settings.width, 800);
	EXPECT_EQ(settings.height, 600);
	EXPECT_EQ(settings.windowMode, "fullscreen");
	EXPECT_EQ(settings.vsync, true);
	EXPECT_EQ(settings.customDetail.modelDetail, 0.3f);
	EXPECT_EQ(settings.customDetail.clouds, false);
	EXPECT_EQ(settings.customDetail.waterTiling, 0.5f);
	EXPECT_EQ(settings.customDetail.rainSplashes, 3);
	EXPECT_EQ(settings.customDetail.fog, false);
	EXPECT_EQ(settings.customDetail.lowResTextures, true);
	EXPECT_EQ(settings.customDetail.lightBoost, 0.5f);
	EXPECT_FALSE(settings.customDetail.smallBump.has_value());
}

TEST(ComputerSettings, ALightBoostThatWouldDivideByZeroIsRefused)
{
	EXPECT_EQ(Parse(R"({"custom_detail": {"light_boost": 1.53}})").settings.customDetail.lightBoost, 1.53f);
	for (const auto* text : {R"({"custom_detail": {"light_boost": 1.54}})", R"({"custom_detail": {"light_boost": "high"}})"})
	{
		const auto [settings, problems] = Parse(text);
		EXPECT_EQ(problems.size(), 1u) << text;
		EXPECT_FALSE(settings.customDetail.lightBoost.has_value()) << text;
	}
}

TEST(ComputerSettings, AnEmptyFileGivesTheDefaults)
{
	const auto [settings, problems] = Parse("{}");
	EXPECT_TRUE(problems.empty());
	EXPECT_EQ(settings, Settings {});
	EXPECT_EQ(Resolve({}, {}, settings), StartValues {});
	EXPECT_EQ(StartValues {}.detailLevel, graphics::detail_level::k_Default);
	EXPECT_EQ(StartValues {}.maxDetailLevel, graphics::detail_level::k_MenuLevels);
}

TEST(ComputerSettings, ABadFileGivesTheDefaultsAndSaysSo)
{
	for (const auto* text : {"", "not json", "[1, 2]", "{\"detail_level\": 4", "42"})
	{
		const auto [settings, problems] = Parse(text);
		EXPECT_EQ(settings, Settings {}) << text;
		EXPECT_EQ(problems.size(), 1u) << text;
	}
}

TEST(ComputerSettings, BadValuesAreRefusedAndTheOthersKept)
{
	const auto [settings, problems] = Parse(R"({
		"detail_level": 7, "max_detail_level": 0, "effects_volume": 1.5, "music_volume": "loud",
		"width": 0, "height": 70000, "window_mode": "maximised", "vsync": 1,
		"custom_detail": {"model_detail": 0, "water_tiling": -0.1, "rain_splashes": 256, "fog": "yes", "clouds": true}
	})");
	EXPECT_EQ(problems.size(), 12u);
	EXPECT_FALSE(settings.detailLevel.has_value());
	EXPECT_FALSE(settings.maxDetailLevel.has_value());
	EXPECT_FALSE(settings.effectsVolume.has_value());
	EXPECT_FALSE(settings.musicVolume.has_value());
	EXPECT_FALSE(settings.width.has_value());
	EXPECT_FALSE(settings.height.has_value());
	EXPECT_FALSE(settings.windowMode.has_value());
	EXPECT_FALSE(settings.vsync.has_value());
	EXPECT_FALSE(settings.customDetail.modelDetail.has_value());
	EXPECT_FALSE(settings.customDetail.waterTiling.has_value());
	EXPECT_FALSE(settings.customDetail.rainSplashes.has_value());
	EXPECT_FALSE(settings.customDetail.fog.has_value());
	EXPECT_EQ(settings.customDetail.clouds, true);

	const auto notAnObject = Parse(R"({"detail_level": 2, "custom_detail": 5})");
	EXPECT_EQ(notAnObject.problems.size(), 1u);
	EXPECT_EQ(notAnObject.settings.detailLevel, 2);
}

TEST(ComputerSettings, TheCommandLineWinsOverTheFile)
{
	const Settings file {
	    .detailLevel = 1, .maxDetailLevel = 7, .width = 800, .height = 600, .windowMode = "fullscreen", .vsync = true};
	const StartValues commandLine {.width = 1920, .height = 1080, .windowMode = "borderless", .vsync = false, .detailLevel = 3};

	// Nothing given: the file's values
	const auto fromFile = Resolve(commandLine, {}, file);
	EXPECT_EQ(
	    fromFile,
	    (StartValues {
	        .width = 800, .height = 600, .windowMode = "fullscreen", .vsync = true, .detailLevel = 1, .maxDetailLevel = 7}));

	// All given: the command line's values; the selector's count has no switch, so the file's holds
	const auto given =
	    Resolve(commandLine, {.width = true, .height = true, .windowMode = true, .vsync = true, .detailLevel = true}, file);
	EXPECT_EQ(
	    given,
	    (StartValues {
	        .width = 1920, .height = 1080, .windowMode = "borderless", .vsync = false, .detailLevel = 3, .maxDetailLevel = 7}));

	// Only the detail given
	const auto detailOnly = Resolve(commandLine, {.detailLevel = true}, file);
	EXPECT_EQ(detailOnly.detailLevel, 3);
	EXPECT_EQ(detailOnly.width, 800);

	// A file without values: the command line's defaults
	EXPECT_EQ(Resolve(commandLine, {}, {}), (StartValues {commandLine.width, commandLine.height, commandLine.windowMode,
	                                                      commandLine.vsync, commandLine.detailLevel, 5}));
}

TEST(ComputerSettings, AWriteKeepsTheOtherMembersOfTheFile)
{
	const auto text = Update(R"({"width": 800, "my_note": "keep me", "detail_level": 2})",
	                         {.detailLevel = 3, .effectsVolume = 0.3f, .musicVolume = 1.0f});
	const auto [settings, problems] = Parse(text);
	EXPECT_TRUE(problems.empty());
	EXPECT_EQ(settings.detailLevel, 3);
	EXPECT_EQ(settings.width, 800);
	EXPECT_EQ(settings.effectsVolume, 0.3f);
	EXPECT_EQ(settings.musicVolume, 1.0f);
	EXPECT_NE(text.find("\"my_note\": \"keep me\""), std::string::npos);
	// A float is written as its shortest decimal
	EXPECT_NE(text.find("\"effects_volume\": 0.3"), std::string::npos);
	EXPECT_EQ(text.find("0.30000"), std::string::npos);

	// Text that is not an object is replaced
	const auto replaced = Parse(Update("garbage", {.detailLevel = 0}));
	EXPECT_TRUE(replaced.problems.empty());
	EXPECT_EQ(replaced.settings, (Settings {.detailLevel = 0}));
}

TEST(ComputerSettings, OnlyTheCustomLevelTakesTheCustomValues)
{
	const CustomDetail custom {
	    .modelDetail = 0.2f, .clouds = false, .rainSplashes = 1, .lowResTextures = true, .lightBoost = 0.5f};
	for (uint8_t level = 0; level < graphics::detail_level::k_Count; ++level)
	{
		const auto settings = graphics::detail_level::WithCustom(level, custom);
		const auto table = graphics::detail_level::For(level);
		if (level != graphics::detail_level::k_Custom)
		{
			EXPECT_EQ(settings.modelDetail, table.modelDetail);
			EXPECT_EQ(settings.clouds, table.clouds);
			EXPECT_EQ(settings.rainSplashes, table.rainSplashes);
			EXPECT_EQ(settings.lowResTextures, table.lowResTextures);
			EXPECT_EQ(settings.lightBoost, 0.0f);
			continue;
		}
		EXPECT_EQ(settings.modelDetail, 0.2f);
		EXPECT_FALSE(settings.clouds);
		EXPECT_EQ(settings.rainSplashes, 1);
		EXPECT_TRUE(settings.lowResTextures);
		EXPECT_EQ(settings.lightBoost, 0.5f);
		// What the file does not give is the level's table
		EXPECT_EQ(settings.fog, table.fog);
		EXPECT_EQ(settings.waterTiling, table.waterTiling);
	}
}

TEST(ComputerSettings, TheSelectorShowsTheLevelModuloItsCount)
{
	EXPECT_EQ(graphics::detail_level::MenuLevel(4), 4);
	EXPECT_EQ(graphics::detail_level::MenuLevel(5), 0);
	EXPECT_EQ(graphics::detail_level::MenuLevel(6), 1);
	EXPECT_EQ(graphics::detail_level::MenuLevel(6, 7), 6);
	EXPECT_EQ(graphics::detail_level::MenuLevel(3, 2), 1);
	EXPECT_EQ(graphics::detail_level::MenuLevel(3, 0), 0);
}

TEST(ComputerSettings, TheFileIsNextToTheProfilesForAPlayerAndNoneForADeveloper)
{
	const auto playerFile = std::filesystem::path("work") / "settings.json";
	EXPECT_EQ(player_profiles::SettingsFileFor({.userFolder = std::filesystem::path("work") / "Profiles"}, playerFile),
	          playerFile);
	EXPECT_FALSE(player_profiles::SettingsFileFor({.userFolder = std::filesystem::path("work") / "Profiles", .automated = true},
	                                              playerFile)
	                 .has_value());
	const auto given = std::filesystem::path("clean");
	EXPECT_EQ(player_profiles::SettingsFileFor({.profileFolder = given, .automated = true}, playerFile),
	          given / "settings.json");
	EXPECT_FALSE(player_profiles::SettingsFileFor({}, std::nullopt).has_value());
}

TEST(ComputerSettingsSystem, AChangeIsWrittenAndReadAtTheNextStart)
{
	const TempFolder folder("change");
	const auto file = folder.Path() / "settings.json";
	WriteText(file, R"({"width": 1024, "note": 1})");
	{
		ecs::systems::ComputerSettingsSystem system(file);
		EXPECT_EQ(system.GetSettings().width, 1024);
		system.Keep({.detailLevel = 2, .maxDetailLevel = 5, .effectsVolume = 0.5f, .musicVolume = 0.75f});
		EXPECT_EQ(system.GetSettings().detailLevel, 2);
	}
	ecs::systems::ComputerSettingsSystem next(file);
	EXPECT_EQ(next.GetSettings(),
	          (Settings {.detailLevel = 2, .maxDetailLevel = 5, .effectsVolume = 0.5f, .musicVolume = 0.75f, .width = 1024}));
	EXPECT_NE(ReadText(file).find("\"note\": 1"), std::string::npos);
}

TEST(ComputerSettingsSystem, NothingNewWritesNothing)
{
	const TempFolder folder("unchanged");
	const auto file = folder.Path() / "settings.json";
	ecs::systems::ComputerSettingsSystem system(file);
	EXPECT_EQ(system.GetSettings(), Settings {});
	// A change with no value is no change: no file is made
	system.Keep({});
	EXPECT_FALSE(std::filesystem::exists(file));
	system.Keep({.detailLevel = 4});
	ASSERT_TRUE(std::filesystem::exists(file));
	WriteText(file, "{\"detail_level\": 1}");
	// The same value again is not written over the file
	system.Keep({.detailLevel = 4});
	EXPECT_EQ(ReadText(file), "{\"detail_level\": 1}");
}

TEST(ComputerSettingsSystem, ABadFileStartsWithTheDefaults)
{
	const TempFolder folder("bad");
	const auto file = folder.Path() / "settings.json";
	WriteText(file, "\xFF\xFE not json");
	ecs::systems::ComputerSettingsSystem system(file);
	EXPECT_EQ(system.GetSettings(), Settings {});
	// The next change replaces the bad file
	system.Keep({.musicVolume = 0.0f});
	EXPECT_EQ(Parse(ReadText(file)).settings, (Settings {.musicVolume = 0.0f}));
}

TEST(ComputerSettingsSystem, WithoutAFileNothingIsWritten)
{
	ecs::systems::ComputerSettingsSystem system(std::nullopt);
	system.Keep({.detailLevel = 1});
	EXPECT_EQ(system.GetSettings().detailLevel, 1);
}
