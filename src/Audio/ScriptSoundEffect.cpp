/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptSoundEffect.h"

#include <cctype>

#include <algorithm>
#include <array>

namespace openblack::audio
{

namespace
{
constexpr std::array<std::string_view, static_cast<size_t>(ScriptSoundBank::Count)> k_BankFiles = {
    "",                    // none
    "InGame.sad",          // the game's sound effects
    "editor.sad",          // the land editor's
    "spells.sad",          // the miracles'
    "creature.sad",        // every creature's
    "Scriptsfx.sad",       // the story's
    "HelpSprites.sad",     // the advisors' speech
    "villagers.sad",       // the villagers' speech
    "VillagersBanter.sad", // the villagers' chatter (the game ships without it)
    "SpellDialogue.sad",   // the miracles' announcer
    "Guidance.sad",        // the guidance voice
};

constexpr bool Is(uint16_t userParam, SoundEffectUse use)
{
	return userParam == static_cast<uint16_t>(use);
}
} // namespace

std::optional<std::string_view> ScriptSoundBankFile(int32_t bank)
{
	if (bank <= static_cast<int32_t>(ScriptSoundBank::None) || bank >= static_cast<int32_t>(ScriptSoundBank::Count))
	{
		return std::nullopt;
	}
	return k_BankFiles.at(static_cast<size_t>(bank));
}

ScriptSoundBank BankOfFile(std::string_view file)
{
	const auto sameName = [file](std::string_view name) {
		return std::ranges::equal(name, file, [](char a, char b) {
			return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		});
	};
	for (size_t bank = 1; bank < k_BankFiles.size(); ++bank)
	{
		if (sameName(k_BankFiles.at(bank)))
		{
			return static_cast<ScriptSoundBank>(bank);
		}
	}
	return ScriptSoundBank::None;
}

bool SoundEffectHeard(const SoundEffectConditions& conditions, ScriptSoundBank bank, uint16_t userParam)
{
	if (conditions.scriptWideScreen && Is(userParam, SoundEffectUse::QuietInCutscenes))
	{
		return false;
	}
	if (conditions.insideTemple && !Is(userParam, SoundEffectUse::HeardInTemple))
	{
		return false;
	}
	if (!conditions.gameSoundOn && bank != ScriptSoundBank::HelpSprites && bank != ScriptSoundBank::Villagers)
	{
		return false;
	}
	return !(conditions.creatureFightControl && Is(userParam, SoundEffectUse::QuietInFights));
}

} // namespace openblack::audio
