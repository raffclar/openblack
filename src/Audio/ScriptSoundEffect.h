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

#include <optional>
#include <string_view>

namespace openblack::audio
{

/// The sound effect banks a script names by number when it plays a sound
enum class ScriptSoundBank : int32_t
{
	None,
	InGame,
	Editor,
	Spells,
	CreatureGeneric,
	ScriptSfx,
	HelpSprites,
	Villagers,
	VillagersBanter,
	SpellDialogue,
	Guidance,
	Count,
};

/// What a bank's sample says about when it may be heard (the sample's user parameter in its bank header)
enum class SoundEffectUse : uint16_t
{
	Any = 0,
	/// Not heard while a script holds the widescreen bars
	QuietInCutscenes = 1,
	/// The only sounds heard while the camera is inside the temple
	HeardInTemple = 2,
	/// Not heard while the player controls a creature fight
	QuietInFights = 4,
};

/// The game state that decides whether a sound effect is heard
struct SoundEffectConditions
{
	/// The widescreen bars are up and a script owns them
	bool scriptWideScreen {false};
	/// The camera is inside the temple
	bool insideTemple {false};
	/// The scripts haven't turned the game's sound effects off
	bool gameSoundOn {true};
	/// The interface is in one of the player's creature fight controls
	bool creatureFightControl {false};
};

/// The file of a bank a script names by number, without its folder, or none for a number that names no bank
[[nodiscard]] std::optional<std::string_view> ScriptSoundBankFile(int32_t bank);

/// The bank a sound group's file is, by its file name whatever its case, None for a file that is no such bank
[[nodiscard]] ScriptSoundBank BankOfFile(std::string_view file);

/// Whether the interface is in the player's creature fight controls: the player's creature fights while the camera
/// watches the fight and the hand holds nothing (pressing on either fighter keeps the controls)
[[nodiscard]] constexpr bool InCreatureFightControls(bool playersCreatureFights, bool cameraWatchesFight, bool handEmpty)
{
	return playersCreatureFights && cameraWatchesFight && handEmpty;
}

/// Whether a sound effect of this bank, with this user parameter, is heard in these conditions. With the game's sound
/// turned off by a script, only the advisors' and the villagers' speech still play.
[[nodiscard]] bool SoundEffectHeard(const SoundEffectConditions& conditions, ScriptSoundBank bank, uint16_t userParam);

} // namespace openblack::audio
