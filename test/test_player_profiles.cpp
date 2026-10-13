/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

#include <gtest/gtest.h>

#include "Common/PlayerProfiles.h"

// Enable this define because we use a custom locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/PlayerProfileSystem.h"

using namespace openblack;
using namespace openblack::player_profiles;

namespace
{
/// A folder of its own under the system's temporary folder, gone with the test
class ProfilesFolder
{
public:
	explicit ProfilesFolder(std::string_view name)
	    : _path(std::filesystem::temp_directory_path() / "openblack_tests" / name)
	{
		std::error_code error;
		std::filesystem::remove_all(_path, error);
		std::filesystem::create_directories(_path, error);
	}
	~ProfilesFolder()
	{
		std::error_code error;
		std::filesystem::remove_all(_path, error);
	}
	ProfilesFolder(const ProfilesFolder&) = delete;
	ProfilesFolder& operator=(const ProfilesFolder&) = delete;
	ProfilesFolder(ProfilesFolder&&) = delete;
	ProfilesFolder& operator=(ProfilesFolder&&) = delete;

	[[nodiscard]] const std::filesystem::path& Path() const { return _path; }

private:
	std::filesystem::path _path;
};

/// A computer whose account is Matt and whose clock reads 0
ecs::systems::PlayerProfileSystem::Machine FakeMachine()
{
	return {.accountName = []() { return std::u16string(u"Matt"); }, .ticks = []() { return 0u; }};
}

Store StoreIn(const std::filesystem::path& folder)
{
	return {.folder = folder, .creatureMinds = folder / "Scripts" / "CreatureMind", .developer = false};
}
} // namespace

TEST(PlayerProfiles, ACreatureFileIsNamedAfterTheScrambledTime)
{
	// Each byte has the next of the C runtime's numbers from seed 0x913 added: 7624, 7051, 2652 and 32074, their low
	// bytes 0xc8, 0x8b, 0x5c and 0x4a
	EXPECT_EQ(ScrambleTicks(0), 0x4a5c8bc8u);
	EXPECT_EQ(ScrambleTicks(0x12345678), 0x5c90e140u);
	EXPECT_EQ(CreatureFileName(0), "C4a5c8bc8.erc");
	// Lower-case hexadecimal without leading zeros, as the installed game's own file C4a4f6e63.erc
	EXPECT_EQ(CreatureFileName(0xb6000000), "C5c8bc8.erc");
}

TEST(PlayerProfiles, ANewNameMustNotBeKeptAlreadyNorBeEmptyNorTooLong)
{
	Profiles profiles;
	Add(profiles, u"Matt", u"", "C1.erc", 0);
	EXPECT_EQ(CheckNewName(profiles, u"Ann"), NameCheck::Accepted);
	// Plain letters match whatever their case; anything else must be the same
	EXPECT_EQ(CheckNewName(profiles, u"mATT"), NameCheck::AlreadyExists);
	EXPECT_EQ(CheckNewName(profiles, u"Matt "), NameCheck::Accepted);
	EXPECT_FALSE(SameName(u"Émile", u"émile"));
	EXPECT_EQ(CheckNewName(profiles, u""), NameCheck::Invalid);
	EXPECT_EQ(CheckNewName(profiles, std::u16string(k_LongestName, u'a')), NameCheck::Accepted);
	EXPECT_EQ(CheckNewName(profiles, std::u16string(k_LongestName + 1, u'a')), NameCheck::Invalid);
	// Spaces alone make a name
	EXPECT_EQ(CheckNewName(profiles, u"  "), NameCheck::Accepted);
}

TEST(PlayerProfiles, TheFirstProfileMadeIsTheOnePlayed)
{
	Profiles profiles;
	EXPECT_EQ(Current(profiles), nullptr);
	const auto& made = Add(profiles, u"Matt", u"", "C1.erc", 3);
	EXPECT_EQ(made.creatureName, "Matt");
	EXPECT_EQ(made.symbol, 3);
	ASSERT_NE(Current(profiles), nullptr);
	EXPECT_EQ(Current(profiles)->creatureFile, "C1.erc");
	// Another is kept, but not played; a name already kept keeps its own file
	Add(profiles, u"Ann", u"", "C2.erc", 0);
	EXPECT_EQ(Add(profiles, u"matt", u"", "C3.erc", 0).creatureFile, "C1.erc");
	EXPECT_EQ(profiles.profiles.size(), 2u);
	EXPECT_EQ(profiles.current, "Matt");
}

TEST(PlayerProfiles, AProfileMadeWithoutANameTakesTheAccountsName)
{
	Profiles profiles;
	EXPECT_EQ(Add(profiles, u"", u"Matt", "C1.erc", 0).name, "Matt");
	EXPECT_EQ(Add(profiles, u"", u"", "C2.erc", 0).name, k_UnnamedPlayer);
}

TEST(PlayerProfiles, ProfilesAreReadBackAsWritten)
{
	Profiles profiles;
	Add(profiles, u"Matt Smith", u"", "C4a4f6e63.erc", 5);
	Add(profiles, u"Émile", u"", "C2.erc", 15);
	EXPECT_EQ(Read(Write(profiles)), profiles);
	// Lines it doesn't understand are passed over, and Windows line ends are read too
	const auto read = Read("current\tAnn\r\nnonsense\nprofile\tAnn\tC2.erc\t7\tRex\r\nprofile\tBob\n");
	ASSERT_EQ(read.profiles.size(), 1u);
	EXPECT_EQ(read.profiles.front().creatureFile, "C2.erc");
	EXPECT_EQ(read.profiles.front().symbol, 7);
	EXPECT_EQ(read.profiles.front().creatureName, "Rex");
	EXPECT_EQ(read.current, "Ann");
}

TEST(PlayerProfiles, TheAlignmentKeptIsReadBackAsWrittenAndIsNeitherGoodNorEvilUntilKept)
{
	Profiles profiles;
	Add(profiles, u"Ann", u"", "C2.erc", 1);
	EXPECT_EQ(profiles.profiles.front().alignment, 0.0);
	profiles.profiles.front().alignment = static_cast<double>(-0.123456789f);
	const auto read = Read(Write(profiles));
	ASSERT_EQ(read.profiles.size(), 1u);
	EXPECT_EQ(read.profiles.front().alignment, profiles.profiles.front().alignment);
	// A value that can't be read leaves the profile neither good nor evil
	const auto bad = Read("profile\tAnn\tC2.erc\t7\tRex\nalignment\tAnn\tgood\n");
	ASSERT_EQ(bad.profiles.size(), 1u);
	EXPECT_EQ(bad.profiles.front().alignment, 0.0);
}

TEST(PlayerProfiles, TheListIsInTheOrderOfTheNamesWhateverTheirCase)
{
	Profiles profiles;
	Add(profiles, u"matt", u"", "C1.erc", 0);
	Add(profiles, u"Ann", u"", "C2.erc", 0);
	Add(profiles, u"bob", u"", "C3.erc", 0);
	Add(profiles, u"Ab", u"", "C4.erc", 0);
	ASSERT_EQ(profiles.profiles.size(), 4u);
	EXPECT_EQ(profiles.profiles[0].name, "Ab");
	EXPECT_EQ(profiles.profiles[1].name, "Ann");
	EXPECT_EQ(profiles.profiles[2].name, "bob");
	EXPECT_EQ(profiles.profiles[3].name, "matt");
	EXPECT_EQ(IndexOf(profiles, u"BOB"), 2u);
	EXPECT_FALSE(IndexOf(profiles, u"Zed").has_value());
	// Read back in the list's order, whatever the file's
	const auto read = Read("profile\tzed\tC1.erc\t0\tz\nprofile\tAl\tC2.erc\t0\ta\n");
	ASSERT_EQ(read.profiles.size(), 2u);
	EXPECT_EQ(read.profiles[0].name, "Al");
}

TEST(PlayerProfiles, PickingAndForgettingProfiles)
{
	Profiles profiles;
	Add(profiles, u"Matt", u"", "C1.erc", 0);
	Add(profiles, u"Ann", u"", "C2.erc", 0);
	EXPECT_EQ(profiles.current, "Matt");
	EXPECT_TRUE(Select(profiles, u"ann"));
	EXPECT_EQ(profiles.current, "Ann");
	EXPECT_FALSE(Select(profiles, u"Zed"));
	EXPECT_EQ(profiles.current, "Ann");
	// Forgetting another leaves the one played; forgetting the one played leaves none
	EXPECT_TRUE(Remove(profiles, u"Matt"));
	EXPECT_EQ(profiles.current, "Ann");
	EXPECT_FALSE(Remove(profiles, u"Matt"));
	EXPECT_TRUE(Remove(profiles, u"ANN"));
	EXPECT_TRUE(profiles.current.empty());
	EXPECT_TRUE(profiles.profiles.empty());
}

TEST(PlayerProfiles, TheCreaturesNameIsThePlayersUntilOneIsGivenAndTheSymbolOneOfSixteen)
{
	Profile profile {.name = "Matt", .creatureFile = "C1.erc", .creatureName = "Matt", .symbol = 0, .settings = {}};
	Edit(profile, u"Rex", 15);
	EXPECT_EQ(profile.creatureName, "Rex");
	EXPECT_EQ(profile.symbol, 15);
	Edit(profile, u"", 16);
	EXPECT_EQ(profile.creatureName, "Matt");
	EXPECT_EQ(profile.symbol, 0);
	Edit(profile, u"Rex", -1);
	EXPECT_EQ(profile.symbol, 0);
}

TEST(PlayerProfiles, SettingsAreTheGamesOwnUntilKeptAndAreReadBackAsWritten)
{
	// The game's own: intelligent tooltips, the important texts, help level 3, creature help, middle reading speed, the
	// newest text at the top, the right hand, no push scrolling, saving automatically
	constexpr Settings k_Default;
	EXPECT_EQ(k_Default.toolTips, 2);
	EXPECT_EQ(k_Default.storyText, 1);
	EXPECT_EQ(k_Default.helpLevel, 3);
	EXPECT_TRUE(k_Default.creatureHelp);
	EXPECT_FLOAT_EQ(k_Default.readSpeed, 0.5f);
	EXPECT_FALSE(k_Default.textFromBottom);
	EXPECT_FALSE(k_Default.leftHanded);
	EXPECT_FALSE(k_Default.pushScrolling);
	EXPECT_TRUE(k_Default.autoSave);

	Profiles profiles;
	Add(profiles, u"Matt", u"", "C1.erc", 0);
	Add(profiles, u"Ann", u"", "C2.erc", 0);
	auto& matt = profiles.profiles.at(*IndexOf(profiles, u"Matt"));
	matt.settings = {.toolTips = 0,
	                 .storyText = 2,
	                 .helpLevel = 4,
	                 .creatureHelp = false,
	                 .readSpeed = 0.25f,
	                 .textFromBottom = true,
	                 .leftHanded = true,
	                 .pushScrolling = true,
	                 .autoSave = false};
	EXPECT_EQ(Read(Write(profiles)), profiles);
	// A file kept before the settings were has the game's own; a value not understood is passed over
	const auto read = Read("profile\tAnn\tC2.erc\t7\tRex\nsetting\tAnn\thelp_level\tlots\nsetting\tAnn\ttooltips\t3\n");
	ASSERT_EQ(read.profiles.size(), 1u);
	EXPECT_EQ(read.profiles.front().settings.helpLevel, 3);
	EXPECT_EQ(read.profiles.front().settings.toolTips, 3);
}

TEST(PlayerProfiles, ToolsAndDevelopersKeepTheirProfileApartFromThePlayers)
{
	const std::filesystem::path user = "user";
	// The player's are kept in the game's own folder
	const auto player = StoreFor({.profileFolder = std::nullopt, .userFolder = user, .automated = false});
	EXPECT_FALSE(player.developer);
	EXPECT_EQ(player.folder, user);
	EXPECT_EQ(player.creatureMinds, user / "Scripts" / "CreatureMind");
	// A developer's run has its own
	const auto developer = StoreFor({.profileFolder = std::nullopt, .userFolder = user, .automated = true});
	EXPECT_TRUE(developer.developer);
	EXPECT_EQ(developer.creatureMinds, user / "developer" / "Scripts" / "CreatureMind");
	// A folder given is kept as a player's, whatever started the game
	const auto given = StoreFor({.profileFolder = std::filesystem::path("clean"), .userFolder = user, .automated = true});
	EXPECT_FALSE(given.developer);
	EXPECT_EQ(given.folder, std::filesystem::path("clean"));
}

TEST(PlayerProfileSystem, AFirstStartHasNoProfileUntilOneIsMade)
{
	const ProfilesFolder folder("player_profiles_made");
	const auto store = StoreIn(folder.Path());
	ecs::systems::PlayerProfileSystem system(store, FakeMachine());
	EXPECT_EQ(system.GetProfileCount(), 0u);
	EXPECT_FALSE(system.CreatureFile().has_value());
	EXPECT_FALSE(system.CurrentProfileHasCreature());
	// Nothing is written until a profile is made
	EXPECT_FALSE(std::filesystem::exists(folder.Path() / ecs::systems::PlayerProfileSystem::k_ProfilesFile));

	system.CreateProfile(u"Ann", 4);
	EXPECT_EQ(system.GetProfileCount(), 1u);
	EXPECT_EQ(system.CreatureFile(), "C4a5c8bc8.erc");
	ASSERT_TRUE(system.GetCurrentProfile().has_value());
	EXPECT_EQ(system.GetCurrentProfile()->symbol, 4);
	EXPECT_EQ(system.CheckNewName(u"ann"), NameCheck::AlreadyExists);

	// A later game reads the profile back, and knows its creature once its file is there
	ecs::systems::PlayerProfileSystem later(store, FakeMachine());
	EXPECT_EQ(later.GetProfileCount(), 1u);
	EXPECT_EQ(later.CreatureFile(), "C4a5c8bc8.erc");
	EXPECT_FALSE(later.CurrentProfileHasCreature());
	std::filesystem::create_directories(*store.creatureMinds);
	std::ofstream(*store.creatureMinds / "C4a5c8bc8.erc") << "mind";
	EXPECT_TRUE(later.CurrentProfileHasCreature());
}

TEST(PlayerProfileSystem, PickingDeletingAndEditingProfilesIsKept)
{
	const ProfilesFolder folder("player_profiles_kept");
	const auto store = StoreIn(folder.Path());
	ecs::systems::PlayerProfileSystem system(store, FakeMachine());
	system.CreateProfile(u"Matt", 0);
	system.CreateProfile(u"Ann", 0);
	// The profile made is the one played
	EXPECT_EQ(system.GetProfiles().current, "Ann");
	EXPECT_TRUE(system.SelectProfile(u"matt"));
	system.EditCurrentProfile(u"Rex", 9);
	auto settings = system.GetCurrentProfile()->settings;
	settings.helpLevel = 0;
	system.SetCurrentSettings(settings);
	system.SetCurrentAlignment(0.75);
	EXPECT_TRUE(system.DeleteProfile(u"Ann"));
	EXPECT_FALSE(system.DeleteProfile(u"Ann"));

	ecs::systems::PlayerProfileSystem later(store, FakeMachine());
	ASSERT_EQ(later.GetProfileCount(), 1u);
	const auto profile = later.GetCurrentProfile();
	ASSERT_TRUE(profile.has_value());
	EXPECT_EQ(profile->name, "Matt");
	EXPECT_EQ(profile->creatureName, "Rex");
	EXPECT_EQ(profile->symbol, 9);
	EXPECT_EQ(profile->settings.helpLevel, 0);
	EXPECT_EQ(profile->alignment, 0.75);
}

TEST(PlayerProfileSystem, ADevelopersRunPlaysAsAProfileNeverKept)
{
	const ProfilesFolder folder("player_profiles_developer");
	auto store = StoreIn(folder.Path());
	store.developer = true;
	ecs::systems::PlayerProfileSystem system(store, FakeMachine());
	EXPECT_EQ(system.GetProfileCount(), 1u);
	EXPECT_EQ(system.CreatureFile(), k_DeveloperCreatureFile);
	system.CreateProfile(u"Ann", 0);
	EXPECT_FALSE(std::filesystem::exists(folder.Path() / ecs::systems::PlayerProfileSystem::k_ProfilesFile));
}

TEST(PlayerProfileSystem, WithNowhereToKeepThemThereAreNoProfiles)
{
	ecs::systems::PlayerProfileSystem system({}, FakeMachine());
	EXPECT_FALSE(system.CreatureFile().has_value());
	EXPECT_EQ(system.GetProfileCount(), 0u);
}
