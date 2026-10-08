/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpMessageSets.h"

#include <algorithm>
#include <array>

#include "Common/GameRandom.h"
#include "HelpProfile.h"
#include "HelpSystem.h"

namespace openblack::help::message_sets
{
namespace
{
// clang-format off
/// The table as the original has it (first, last, mode, category, script)
constexpr std::array<Set, k_Sets> k_Table = {{
	{0x0, 0x0, 0, 0, "MultiHelpJustTalkWithText"}, // 0
	{0xC73, 0xC76, 0, 2, "MultiBanter"}, // 1
	{0xC77, 0xC7A, 0, 2, "MultiBanter"}, // 2
	{0xC7B, 0xC7E, 0, 2, "MultiBanter"}, // 3
	{0xC7F, 0xC82, 0, 2, "MultiBanter"}, // 4
	{0xC83, 0xC84, 0, 2, "MultiBanter"}, // 5
	{0xC85, 0xC88, 0, 2, "MultiBanter"}, // 6
	{0xC89, 0xC8D, 0, 2, "MultiBanter"}, // 7
	{0xC8E, 0xC91, 0, 2, "MultiBanter"}, // 8
	{0xC92, 0xC98, 0, 2, "MultiBanter"}, // 9
	{0xC99, 0xC9D, 0, 2, "MultiBanter"}, // 10
	{0xC9E, 0xCA1, 0, 2, "MultiBanter"}, // 11
	{0xCA2, 0xCA6, 0, 2, "MultiBanter"}, // 12
	{0xCA7, 0xCAB, 0, 2, "MultiBanter"}, // 13
	{0xCAC, 0xCAD, 0, 2, "MultiBanter"}, // 14
	{0xCAE, 0xCB3, 0, 2, "MultiBanter"}, // 15
	{0xCB4, 0xCB9, 0, 2, "MultiBanter"}, // 16
	{0xCBA, 0xCBE, 0, 2, "MultiBanter"}, // 17
	{0xCBF, 0xCC7, 0, 2, "MultiBanter"}, // 18
	{0xCC8, 0xCCD, 0, 2, "MultiBanter"}, // 19
	{0xCCE, 0xCD1, 0, 2, "MultiBanter"}, // 20
	{0xCD2, 0xCD5, 0, 2, "MultiBanter"}, // 21
	{0xCD6, 0xCD8, 0, 2, "MultiBanter"}, // 22
	{0xCD9, 0xCDF, 0, 2, "MultiBanter"}, // 23
	{0xCE0, 0xCE2, 0, 2, "MultiBanter"}, // 24
	{0xCE3, 0xCE7, 0, 2, "MultiBanter"}, // 25
	{0xE2D, 0xE32, 1, 3, "MultiHelpJustTalkWithText"}, // 26
	{0xE39, 0xE3E, 1, 3, "MultiHelpJustTalkWithText"}, // 27
	{0xE33, 0xE38, 1, 3, "MultiHelpJustTalkWithText"}, // 28
	{0x0, 0x0, 0, 3, "MultiHelpJustTalkWithText"}, // 29
	{0x0, 0x0, 0, 3, "MultiHelpJustTalkWithText"}, // 30
	{0x0, 0x0, 0, 3, "MultiHelpJustTalkWithText"}, // 31
	{0x0, 0x0, 0, 3, "MultiHelpJustTalkWithText"}, // 32
	{0xBAA, 0xBB7, 1, 5, "MultiHelpJustTalkWithText"}, // 33
	{0xBB8, 0xBBC, 1, 5, "MultiHelpJustTalkWithText"}, // 34
	{0xC32, 0xC37, 1, 5, "MultiHelpJustTalkWithText"}, // 35
	{0xD13, 0xD14, 0, 4, "MultiHelpJustTalkWithText"}, // 36
	{0xD15, 0xD1E, 1, 4, "MultiHelpJustTalkWithText"}, // 37
	{0xC38, 0xC38, 0, 5, "MultiHelpJustTalkWithText"}, // 38
	{0xC39, 0xC39, 0, 5, "MultiHelpJustTalkWithText"}, // 39
	{0xC3A, 0xC3A, 0, 5, "MultiHelpJustTalkWithText"}, // 40
	{0xC3B, 0xC3B, 0, 5, "MultiHelpJustTalkWithText"}, // 41
	{0xC3C, 0xC3C, 0, 5, "MultiHelpJustTalkWithText"}, // 42
	{0xC3D, 0xC3D, 0, 5, "MultiHelpJustTalkWithText"}, // 43
	{0xC3E, 0xC39, 0, 5, "MultiHelpJustTalkWithText"}, // 44
	{0xC3F, 0xC3F, 0, 5, "MultiHelpJustTalkWithText"}, // 45
	{0xC40, 0xC40, 0, 5, "MultiHelpJustTalkWithText"}, // 46
	{0xBB8, 0xBB8, 0, 4, "HelpJustTalkMaybeWithSpirits"}, // 47
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 48
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 49
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 50
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 51
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 52
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 53
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 54
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 55
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 56
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 57
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 58
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 59
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 60
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 61
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 62
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 63
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 64
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 65
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 66
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 67
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 68
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 69
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 70
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 71
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 72
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 73
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 74
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 75
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 76
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 77
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 78
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 79
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 80
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 81
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 82
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 83
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 84
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 85
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 86
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 87
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 88
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 89
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 90
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 91
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 92
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 93
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 94
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 95
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 96
	{0xAFD, 0xAFE, 0, 4, "HelpJustTalkMaybeWithSpirits"}, // 97
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 98
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 99
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 100
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 101
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 102
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 103
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 104
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 105
	{0x0, 0x0, 2, 4, "HelpJustTalkMaybeWithSpirits"}, // 106
	{0xC66, 0xC66, 0, 1, "MultiHelpJustTalkWithText"}, // 107
	{0xC67, 0xC67, 0, 1, "MultiHelpJustTalkWithText"}, // 108
	{0xC68, 0xC68, 2, 1, "MultiHelpJustTalkWithText"}, // 109
	{0xC69, 0xC69, 0, 1, "MultiHelpJustTalkWithText"}, // 110
	{0xC6A, 0xC6A, 0, 1, "MultiHelpJustTalkWithText"}, // 111
	{0xC6B, 0xC6B, 0, 1, "MultiHelpJustTalkWithText"}, // 112
	{0xC6F, 0xC6F, 0, 1, "MultiHelpJustTalkWithText"}, // 113
	{0xC6C, 0xC6C, 0, 1, "MultiHelpJustTalkWithText"}, // 114
	{0xC6D, 0xC6D, 0, 1, "MultiHelpJustTalkWithText"}, // 115
	{0xC6E, 0xC6E, 0, 1, "MultiHelpJustTalkWithText"}, // 116
	{0xC4B, 0xC4B, 0, 1, "MultiHelpJustTalkWithText"}, // 117
	{0xC4C, 0xC4C, 0, 1, "MultiHelpJustTalkWithText"}, // 118
	{0xC4D, 0xC4D, 0, 1, "MultiHelpJustTalkWithText"}, // 119
	{0xC4E, 0xC4E, 0, 1, "MultiHelpJustTalkWithText"}, // 120
	{0xC4F, 0xC50, 0, 1, "MultiHelpJustTalkWithText"}, // 121
	{0xC51, 0xC52, 0, 1, "MultiHelpJustTalkWithText"}, // 122
	{0xC53, 0xC53, 0, 1, "MultiHelpJustTalkWithText"}, // 123
	{0xC54, 0xC54, 0, 1, "MultiHelpJustTalkWithText"}, // 124
	{0xC55, 0xC55, 0, 1, "MultiHelpJustTalkWithText"}, // 125
	{0xD1F, 0xD1F, 0, 1, "MultiHelpJustTalkWithText"}, // 126
	{0xD20, 0xD20, 0, 1, "MultiHelpJustTalkWithText"}, // 127
	{0xD21, 0xD21, 0, 1, "MultiHelpJustTalkWithText"}, // 128
	{0xD22, 0xD22, 0, 1, "MultiHelpJustTalkWithText"}, // 129
	{0xD23, 0xD23, 0, 1, "MultiHelpJustTalkWithText"}, // 130
	{0xD24, 0xD24, 0, 1, "MultiHelpJustTalkWithText"}, // 131
	{0xD25, 0xD25, 0, 1, "MultiHelpJustTalkWithText"}, // 132
	{0xD26, 0xD26, 0, 1, "MultiHelpJustTalkWithText"}, // 133
	{0xD27, 0xD27, 0, 1, "MultiHelpJustTalkWithText"}, // 134
	{0xC58, 0xC59, 0, 1, "MultiHelpJustTalkWithText"}, // 135
	{0xC57, 0xC57, 0, 1, "MultiHelpJustTalkWithText"}, // 136
	{0xD0F, 0xD0F, 0, 1, "MultiHelpJustTalkWithText"}, // 137
	{0xD12, 0xD12, 0, 1, "MultiHelpJustTalkWithText"}, // 138
	{0xC5C, 0xC5C, 0, 1, "MultiHelpJustTalkWithText"}, // 139
	{0xD0A, 0xD0E, 1, 1, "MultiHelpJustTalkWithText"}, // 140
	{0xC5E, 0xC5E, 0, 1, "MultiHelpJustTalkWithText"}, // 141
	{0xC5F, 0xC5F, 0, 1, "MultiHelpJustTalkWithText"}, // 142
	{0xC5A, 0xC5A, 0, 1, "MultiHelpJustTalkWithText"}, // 143
	{0xC5B, 0xC5B, 0, 1, "MultiHelpJustTalkWithText"}, // 144
	{0xC5D, 0xC5D, 0, 1, "MultiHelpJustTalkWithText"}, // 145
	{0xD10, 0xD11, 0, 1, "MultiHelpJustTalkWithText"}, // 146
	{0xC60, 0xC65, 1, 1, "MultiHelpJustTalkWithText"}, // 147
	{0xC42, 0xC42, 0, 1, "MultiHelpJustTalkWithText"}, // 148
	{0xC43, 0xC46, 0, 7, "MultiHelpJustTalkWithText"}, // 149
	{0x0, 0x0, 3, 6, "TotemGuide"}, // 150
	{0x0, 0x0, 3, 6, "CitadelGuide"}, // 151
}};
// clang-format on
} // namespace

const Set& Get(uint32_t set)
{
	return k_Table.at(set);
}

void ProcessConditions(HelpSystem& help)
{
	auto& state = help.GetMessageSets();
	state.conditions.fill(0);
	state.conditions[1] = 1;
	// nothing more while a script holds the wide screen
	if (help.IsScriptWideScreen())
	{
		return;
	}
	// condition 2: no interface event (HelpProfile's AllInterface: every event 1..42 counts as it) for more than 120 s,
	// the player has done nothing for two minutes
	const auto idle = help_profile::TimeSince(static_cast<int32_t>(help_profile::Event::AllInterface));
	state.conditions[2] = idle.has_value() && *idle > k_IdleSeconds ? 1 : 0;
}

uint32_t GetRandomBanterSet(HelpSystem& help)
{
	auto& state = help.GetMessageSets();
	// every set 1..25 sent: their sent counts and the banter count cleared, set 1
	const auto restart = [&state]() {
		state.banterCount = 0;
		std::fill_n(state.sentCount.begin() + 1, k_BanterSets, 0u);
		return 1u;
	};
	if (state.banterCount >= k_BanterSets)
	{
		return restart();
	}
	// n = LocalRand(25 - count), the n-th set not sent yet
	uint32_t n = game_random::LocalRand(static_cast<int32_t>(k_BanterSets - state.banterCount));
	for (uint32_t set = 1; set <= k_BanterSets; ++set)
	{
		if (state.sentCount.at(set) == 0 && n-- == 0)
		{
			++state.banterCount;
			return set;
		}
	}
	return restart();
}

void MarkMessageSetSent(HelpSystem& help, uint32_t set, uint32_t turn)
{
	auto& state = help.GetMessageSets();
	state.sentTurn.at(set) = turn;
	help.TriggerCategory(static_cast<int32_t>(Get(set).category));
	++state.sentCount.at(set);
	state.lastSet = set;
	help.SetMessageTurn(turn);
	help_profile::Trigger(help_profile::Event::HelpQuery);
}

bool RunMessageSet(HelpSystem& help, uint32_t set, const script_control::Vm& vm, uint32_t turn)
{
	if (!script_control::StopHelpScriptsForNewHelp(help, vm))
	{
		return false;
	}
	MarkMessageSetSent(help, set, turn);
	const auto& entry = Get(set);
	if (entry.mode == 3) // RunMessage(script)
	{
		if (script_control::StopHelpScriptsForNewHelp(help, vm))
		{
			help.SetMessageTurn(turn);
			const bool multiplayer = vm.multiplayer && vm.multiplayer();
			if (vm.startScript)
			{
				vm.startScript(entry.script, multiplayer ? script_control::k_MultiplayerScriptTypes
				                                         : script_control::k_SinglePlayerScriptTypes);
			}
		}
		return true;
	}
	// The texts of the set: mode 0 the table's texts; 1 one of them at random (first + LocalRand(last - first + 1));
	// 2 the thing's query texts, 0 and 0 without a thing (always here: (pending) the variant with a thing)
	uint32_t first = 0;
	uint32_t last = 0;
	if (entry.mode == 0)
	{
		first = entry.first;
		last = entry.last;
	}
	else if (entry.mode == 1)
	{
		first = entry.first + game_random::LocalRand(static_cast<int32_t>(entry.last - entry.first + 1));
		last = first;
	}
	script_control::RunMessage(help, first, last, entry.script, vm, turn); // its result unused
	return true;
}

void ProcessBanter(HelpSystem& help, const script_control::Vm& vm, uint32_t turn)
{
	if (help.GetMessageSets().conditions[2] != 0)
	{
		RunMessageSet(help, GetRandomBanterSet(help), vm, turn);
	}
}

} // namespace openblack::help::message_sets
