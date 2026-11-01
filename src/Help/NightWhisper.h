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

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

/// The night whisper: on real nights, a ghostly voice of the guidance sound bank sometimes whispers the name of the
/// player. The game looks for a recorded name that sounds like a word of the name of the player. It checks the clock of
/// the computer every 100 game turns, on the third land and after. The whisper is a sound of this computer only and
/// changes nothing in the world.
namespace openblack::night_whisper
{

/// The number of recorded names
constexpr size_t k_NameCount = 100;
/// The help text of the first recorded name. The other names follow in order.
constexpr uint32_t k_FirstNameText = 4586;
/// The game turns from one look at the clock to the next
constexpr uint32_t k_CheckEvery = 100;
/// The volume of the first whisper, 0 to 127
constexpr uint32_t k_FirstVolume = 127;
/// The longest second name, in characters
constexpr size_t k_SecondNameLength = 48;

/// The help text of a recorded name, 0 to 99
[[nodiscard]] constexpr uint32_t NameText(size_t name)
{
	return k_FirstNameText + static_cast<uint32_t>(name);
}

/// The sample of the guidance sound bank that says a recorded name, 0 to 99. The samples follow the names, but the last
/// name has its sample between the samples of the tenth and the eleventh names.
[[nodiscard]] constexpr uint32_t NameSample(size_t name)
{
	constexpr uint32_t k_FirstSample = 30;
	constexpr size_t k_NamesBeforeTheGap = 10;
	if (name + 1 == k_NameCount)
	{
		return k_FirstSample + static_cast<uint32_t>(k_NamesBeforeTheGap);
	}
	return k_FirstSample + static_cast<uint32_t>(name) + (name < k_NamesBeforeTheGap ? 0 : 1);
}

/// The sound code of a character, as the game finds it:
/// - a e i o u and all characters that are not letters give 0;
/// - b f p v give 1, c g j k q s x z give 2, d t give 3, l gives 4, m n give 5 and r gives 6;
/// - h w y give the number of the character itself, so the case of the letter counts.
/// Only the 52 letters of the English alphabet are letters.
[[nodiscard]] uint32_t SoundCode(char16_t character);

/// Whether a recorded name sounds like the word at the start of `words`. The first characters must be the same, and the
/// next three sound codes of each must be the same. A space or the end of the text stops a word, and a word that stops
/// gives the code 0. A code that is the same as the code of the next letter gives that code plus 1, and the next letter
/// gives its own code again.
[[nodiscard]] bool SoundsAlike(std::u16string_view name, std::u16string_view words);

/// Whether a recorded name sounds like a word of a text. A word starts after each space. Thus two spaces in a row make a
/// word that starts with a space, which no name sounds like, and the next word starts after the second space.
[[nodiscard]] bool AnyWordSoundsAlike(std::u16string_view name, std::u16string_view words);

/// The sample of the first recorded name, in order, that sounds like a word of a text. None for an empty text.
[[nodiscard]] std::optional<uint32_t> FindNameSample(std::span<const std::u16string> names, std::u16string_view words);

/// The second name that the game tries for the player: the name of the profile with only the printable characters of
/// the English alphabet, at most 48 of them. The game uses the online login name of the profile first, but openblack
/// keeps no login name. When no character stays, the game makes a name that starts with an underscore, which no name
/// sounds like, so the second name is then empty.
[[nodiscard]] std::u16string SecondName(std::u16string_view profileName);

/// The sample that whispers the name of the player: the first match of the name of the profile, else of the second name.
/// None when neither sounds like a recorded name.
[[nodiscard]] std::optional<uint32_t> LookUpSample(std::span<const std::u16string> names, std::u16string_view profileName);

/// The random numbers of the whisper. Both are local to this computer.
struct Draws
{
	/// A number from 0 to x
	std::function<float(float)> floatRand;
	/// A whole number from 0 to n - 1
	std::function<uint32_t(int32_t)> rand;
};

/// A whisper to play: a sample of the guidance sound bank, played as a sound that is not in the world
struct Whisper
{
	uint32_t sample;
	/// The playback rate, in percent
	uint32_t pitchPercent;
	/// The volume. A value above 127 plays at 127.
	uint32_t volume;
	/// From the left (0) to the right (180), 90 the centre
	uint32_t pan;

	bool operator==(const Whisper&) const = default;
};

/// A real-night check that did not whisper: the evil advisor can say that the player is up too late
struct NoWhisper
{
	bool operator==(const NoWhisper&) const = default;
};

/// What a game turn does: nothing (no check), a whisper, or a check with no whisper
using TurnResult = std::variant<std::monostate, Whisper, NoWhisper>;

/// Whether a real-night check whispers: (1 - r cubed) times 1000, cut to a whole number, is below the count of checks
[[nodiscard]] bool CheckWhispers(float draw, uint32_t count);

/// The whisper of a sample. The volume is the volume that the last whisper left, and it changes to the volume of this
/// whisper.
[[nodiscard]] Whisper MakeWhisper(uint32_t sample, uint32_t& volume, const Draws& draws);

/// The state of the night whisper on this computer. A new land and a new game do not change it, only the start of the
/// program.
class Whisperer
{
public:
	/// The sample that says the name of the player, none when no recorded name sounds like it
	void SetSample(std::optional<uint32_t> sample) { _sample = sample; }
	[[nodiscard]] std::optional<uint32_t> GetSample() const { return _sample; }

	/// Once a game turn. There is no check on lands 1 and 2, and none without a sample. On the other turns, the count down
	/// goes to 0, then the clock is checked and the count down starts again. On a real night the check draws a random
	/// number. The count of checks goes to 0 at a whisper, and it increases by 1 after each real-night check.
	[[nodiscard]] TurnResult Turn(int32_t landNumber, const std::function<bool()>& clockSaysNight, const Draws& draws);

	[[nodiscard]] uint32_t GetCount() const { return _count; }
	[[nodiscard]] uint32_t GetCountdown() const { return _countdown; }
	[[nodiscard]] uint32_t GetVolume() const { return _volume; }

	/// For the testbed: the next turn that can check the clock checks it, with this count of checks
	void Stage(uint32_t count)
	{
		_count = count;
		_countdown = 0;
	}

private:
	std::optional<uint32_t> _sample;
	uint32_t _count {0};
	uint32_t _countdown {k_CheckEvery};
	uint32_t _volume {k_FirstVolume};
};

} // namespace openblack::night_whisper
