/*******************************************************************************
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

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// The players who play on this computer, each with a profile of their own: the name they go by, the symbol they
/// picked, their creature's name and the file their creature is kept in between lands
namespace openblack::player_profiles
{

struct Profile
{
	/// The name the player typed, as UTF-8
	std::string name;
	/// The file, in the creature-mind folder, the profile's creature is kept in
	std::string creatureFile;
	/// The creature's name, the player's own until it is changed
	std::string creatureName;
	/// The player's symbol, one of the sixteen
	int symbol {0};

	bool operator==(const Profile&) const = default;
};

/// Every profile, and the one being played, by its name
struct Profiles
{
	std::vector<Profile> profiles;
	std::string current;

	bool operator==(const Profiles&) const = default;
};

/// The most characters a name typed into the new player's box can have
constexpr size_t k_NameBoxLength = 29;
/// A name longer than this is turned down
constexpr size_t k_LongestName = 64;
/// The name a profile made without one takes when the computer's account has no name either
constexpr std::string_view k_UnnamedPlayer = "Player";

/// What becomes of a name given for a new profile
enum class NameCheck : uint8_t
{
	Accepted,
	/// A profile of that name is already kept
	AlreadyExists,
	/// Empty, or too long
	Invalid,
};

/// A new profile's creature file is named after the time on the system's clock in milliseconds, scrambled so the names
/// look unlike each other: each of its four bytes, lowest first, has the C runtime's next random number added to it,
/// the random numbers always starting from the same seed
[[nodiscard]] uint32_t ScrambleTicks(uint32_t ticks);
/// The creature file of a profile made at that time: "C", the scrambled time in lower-case hexadecimal, ".erc"
[[nodiscard]] std::string CreatureFileName(uint32_t ticks);

/// Two names are one profile's when they are the same but for the case of their plain (ASCII) letters: profiles were
/// kept by name in a store that ignores the case of those letters
[[nodiscard]] bool SameName(std::u16string_view a, std::u16string_view b);
/// Whether a new profile may take the name: the check for a name already kept comes first, then its length
[[nodiscard]] NameCheck CheckNewName(const Profiles& profiles, std::u16string_view name);

/// The profile being played, if there is one by its name
[[nodiscard]] const Profile* Current(const Profiles& profiles);
/// The profile of a name, if one is kept
[[nodiscard]] const Profile* Find(const Profiles& profiles, std::u16string_view name);
/// Makes a profile: an empty name is the account's (or k_UnnamedPlayer); a name already kept is used as it is, else a
/// new profile is made with the creature file, the player's name for its creature and the symbol. The first profile
/// becomes the one played. The profile of that name
const Profile& Add(Profiles& profiles, std::u16string_view name, std::u16string_view accountName, std::string_view creatureFile,
                   int symbol);

/// The profiles as they are kept on disk, UTF-8: one line naming the current profile, then a line for each profile with
/// its name, its creature file, its symbol and its creature's name, separated by tabs (no name can hold a tab)
[[nodiscard]] std::string Write(const Profiles& profiles);
/// The profiles read back; lines that aren't understood are passed over
[[nodiscard]] Profiles Read(std::string_view text);

/// Where the profiles and their creatures are kept for a run
struct Store
{
	/// The folder of the profiles' file, none when they can't be kept
	std::optional<std::filesystem::path> folder;
	/// The folder the profiles' creatures are kept in
	std::optional<std::filesystem::path> creatureMinds;
	/// A developer's run plays as one profile made for it, never kept, whose creature is kept in a folder of its own
	bool developer {false};
};

/// How the game was started, as far as it decides where the profiles are kept
struct RunOptions
{
	/// A folder given for the profiles
	std::optional<std::filesystem::path> profileFolder;
	/// The game's own folder for the player's data, none when it can't be had
	std::optional<std::filesystem::path> userFolder;
	/// Started by a developer or a tool: on a level of its own, the testbed or a scenario, with the opening skipped, the
	/// logos left out, the inspector on, a seed, a video or a number of frames to run
	bool automated {false};
};

/// The name of a developer's profile, and the file its creature is kept in
constexpr std::string_view k_DeveloperName = "Developer";
constexpr std::string_view k_DeveloperCreatureFile = "Player.erc";
/// The folder, in the game's own, that a developer's run keeps its creature in
constexpr std::string_view k_DeveloperFolder = "developer";

/// A folder given keeps the profiles there, as a player's are, whatever started the game, so a first start can be had
/// on a clean folder; a developer's or a tool's run has its own profile, kept apart from the player's; a player's are kept
/// in the game's own folder
[[nodiscard]] Store StoreFor(const RunOptions& options);

} // namespace openblack::player_profiles
