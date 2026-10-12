/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpMessages.h"

#include <algorithm>
#include <array>

using namespace openblack;
using namespace openblack::help_messages;

namespace
{
// first text, last text, how a text is picked, category, script
constexpr std::array<MessageSet, k_SetCount> k_Sets {{
    {0, 0, Pick::Fixed, 0, Script::JustTalkWithText},               // 0
    {3187, 3190, Pick::Fixed, 2, Script::Banter},                   // 1
    {3191, 3194, Pick::Fixed, 2, Script::Banter},                   // 2
    {3195, 3198, Pick::Fixed, 2, Script::Banter},                   // 3
    {3199, 3202, Pick::Fixed, 2, Script::Banter},                   // 4
    {3203, 3204, Pick::Fixed, 2, Script::Banter},                   // 5
    {3205, 3208, Pick::Fixed, 2, Script::Banter},                   // 6
    {3209, 3213, Pick::Fixed, 2, Script::Banter},                   // 7
    {3214, 3217, Pick::Fixed, 2, Script::Banter},                   // 8
    {3218, 3224, Pick::Fixed, 2, Script::Banter},                   // 9
    {3225, 3229, Pick::Fixed, 2, Script::Banter},                   // 10
    {3230, 3233, Pick::Fixed, 2, Script::Banter},                   // 11
    {3234, 3238, Pick::Fixed, 2, Script::Banter},                   // 12
    {3239, 3243, Pick::Fixed, 2, Script::Banter},                   // 13
    {3244, 3245, Pick::Fixed, 2, Script::Banter},                   // 14
    {3246, 3251, Pick::Fixed, 2, Script::Banter},                   // 15
    {3252, 3257, Pick::Fixed, 2, Script::Banter},                   // 16
    {3258, 3262, Pick::Fixed, 2, Script::Banter},                   // 17
    {3263, 3271, Pick::Fixed, 2, Script::Banter},                   // 18
    {3272, 3277, Pick::Fixed, 2, Script::Banter},                   // 19
    {3278, 3281, Pick::Fixed, 2, Script::Banter},                   // 20
    {3282, 3285, Pick::Fixed, 2, Script::Banter},                   // 21
    {3286, 3288, Pick::Fixed, 2, Script::Banter},                   // 22
    {3289, 3295, Pick::Fixed, 2, Script::Banter},                   // 23
    {3296, 3298, Pick::Fixed, 2, Script::Banter},                   // 24
    {3299, 3303, Pick::Fixed, 2, Script::Banter},                   // 25
    {3629, 3634, Pick::Random, 3, Script::JustTalkWithText},        // 26
    {3641, 3646, Pick::Random, 3, Script::JustTalkWithText},        // 27
    {3635, 3640, Pick::Random, 3, Script::JustTalkWithText},        // 28
    {0, 0, Pick::Fixed, 3, Script::JustTalkWithText},               // 29
    {0, 0, Pick::Fixed, 3, Script::JustTalkWithText},               // 30
    {0, 0, Pick::Fixed, 3, Script::JustTalkWithText},               // 31
    {0, 0, Pick::Fixed, 3, Script::JustTalkWithText},               // 32
    {2986, 2999, Pick::Random, 5, Script::JustTalkWithText},        // 33
    {3000, 3004, Pick::Random, 5, Script::JustTalkWithText},        // 34
    {3122, 3127, Pick::Random, 5, Script::JustTalkWithText},        // 35
    {3347, 3348, Pick::Fixed, 4, Script::JustTalkWithText},         // 36
    {3349, 3358, Pick::Random, 4, Script::JustTalkWithText},        // 37
    {3128, 3128, Pick::Fixed, 5, Script::JustTalkWithText},         // 38
    {3129, 3129, Pick::Fixed, 5, Script::JustTalkWithText},         // 39
    {3130, 3130, Pick::Fixed, 5, Script::JustTalkWithText},         // 40
    {3131, 3131, Pick::Fixed, 5, Script::JustTalkWithText},         // 41
    {3132, 3132, Pick::Fixed, 5, Script::JustTalkWithText},         // 42
    {3133, 3133, Pick::Fixed, 5, Script::JustTalkWithText},         // 43
    {3134, 3129, Pick::Fixed, 5, Script::JustTalkWithText},         // 44
    {3135, 3135, Pick::Fixed, 5, Script::JustTalkWithText},         // 45
    {3136, 3136, Pick::Fixed, 5, Script::JustTalkWithText},         // 46
    {3000, 3000, Pick::Fixed, 4, Script::JustTalkMaybeWithSpirits}, // 47
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 48
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 49
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 50
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 51
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 52
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 53
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 54
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 55
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 56
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 57
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 58
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 59
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 60
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 61
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 62
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 63
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 64
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 65
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 66
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 67
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 68
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 69
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 70
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 71
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 72
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 73
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 74
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 75
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 76
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 77
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 78
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 79
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 80
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 81
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 82
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 83
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 84
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 85
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 86
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 87
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 88
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 89
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 90
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 91
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 92
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 93
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 94
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 95
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 96
    {2813, 2814, Pick::Fixed, 4, Script::JustTalkMaybeWithSpirits}, // 97
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 98
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 99
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 100
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 101
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 102
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 103
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 104
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 105
    {0, 0, Pick::Thing, 4, Script::JustTalkMaybeWithSpirits},       // 106
}};
} // namespace

std::string_view help_messages::ScriptName(Script script)
{
	switch (script)
	{
	case Script::JustTalkWithText:
		return "MultiHelpJustTalkWithText";
	case Script::JustTalkWithNoText:
		return "MultiHelpJustTalkWithNoText";
	case Script::Banter:
		return "MultiBanter";
	case Script::JustTalkMaybeWithSpirits:
		return "HelpJustTalkMaybeWithSpirits";
	case Script::Town:
		return "HelpTown";
	case Script::TownNeeds:
		return "HelpTownNeeds";
	}
	return {};
}

const MessageSet& help_messages::Set(size_t set)
{
	return k_Sets.at(set);
}

std::optional<Texts> help_messages::TextsOf(size_t set, std::optional<Texts> thing,
                                            const std::function<uint32_t(int32_t)>& localRand)
{
	const auto& row = Set(set);
	switch (row.pick)
	{
	case Pick::Fixed:
		return Texts {row.first, row.last};
	case Pick::Random:
	{
		const auto text = localRand(static_cast<int32_t>(row.last - row.first + 1)) + row.first;
		return Texts {text, text};
	}
	case Pick::Thing:
		return thing.has_value() ? *thing : Texts {0, 0};
	case Pick::NoText:
		return std::nullopt;
	}
	return std::nullopt;
}

size_t help_messages::PickBanter(SetCounts& sent, uint16_t& banterUsed, const std::function<uint32_t(int32_t)>& localRand)
{
	const auto clear = [&sent, &banterUsed]() {
		std::fill_n(sent.begin() + k_FirstBanter, k_BanterCount, 0u);
		banterUsed = 0;
		return k_FirstBanter;
	};
	if (banterUsed >= k_BanterCount)
	{
		return clear();
	}
	auto skip = localRand(static_cast<int32_t>(k_BanterCount - banterUsed));
	for (size_t set = k_FirstBanter; set < k_FirstBanter + k_BanterCount; ++set)
	{
		if (sent.at(set) != 0)
		{
			continue;
		}
		if (skip == 0)
		{
			++banterUsed;
			return set;
		}
		--skip;
	}
	return clear();
}

float help_messages::TownNeed(float rawDesire)
{
	constexpr float k_Percent = 100.0f;
	return (rawDesire < 1.0f ? rawDesire : 1.0f) * k_Percent;
}

bool help_messages::BanterDue(float secondsSinceHelp, bool scriptCutScene)
{
	return !scriptCutScene && secondsSinceHelp > k_BanterIdleSeconds;
}
