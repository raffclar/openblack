/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <PackFile.h>
#include <gtest/gtest.h>

#include "Audio/Engine/AnimEffectBank.h"
#include "Audio/Services/SpellSounds.h"
#include "Common/Zip.h"
#include "Particles/PSysFile.h"
#include "Particles/SoundAction.h"

using namespace openblack;

namespace
{
int32_t ValueOf(const std::vector<std::pair<std::string, int32_t>>& names, const std::string& name)
{
	const auto it = std::ranges::find_if(names, [&](const auto& entry) { return entry.first == name; });
	return it == names.end() ? -2 : it->second;
}
} // namespace

TEST(SpellSounds, parseEnumHeader)
{
	const auto names = psys::ParseEnumHeader("// header\n#ifndef X\nenum\tLHSoundAction\n{\n\tA = 1, // one\n\tB\t= 2,\n"
	                                         "\t/* skipped, C */ D = 112,\n\tE,\n\tF,\n};\n#endif\n");
	ASSERT_EQ(names.size(), 5u);
	EXPECT_EQ(ValueOf(names, "A"), 1);
	EXPECT_EQ(ValueOf(names, "B"), 2);
	EXPECT_EQ(ValueOf(names, "C"), -2);
	EXPECT_EQ(ValueOf(names, "D"), 112);
	EXPECT_EQ(ValueOf(names, "E"), 113);
	EXPECT_EQ(ValueOf(names, "F"), 114);
}

TEST(SpellSounds, readSoundActionFlags)
{
	const auto file = psys::File::Parse("BEGINPROPERTIES\nENDPROPERTIES\nBEGINCLASS CreateRuleAnAtom C0\nBEGINPROPERTIES\n"
	                                    "PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 1 ONLYONE 1 SOFTRELEASE 1 "
	                                    "USESURFACE 1\nPROPERTY Other SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 1 "
	                                    "SOFTRELEASE 0 USESURFACE 1\nENDPROPERTIES\nENDCLASS\n",
	                                    "test");
	ASSERT_TRUE(file.has_value());
	const auto& object = file->objects.at(0);
	const auto a = psys::ReadSoundAction(object, "SoundOfCreate");
	EXPECT_EQ(a.action, -1);
	EXPECT_EQ(a.flags, psys::SoundAction::k_Looping | psys::SoundAction::k_SoftRelease | psys::SoundAction::k_UseSurface);
	const auto b = psys::ReadSoundAction(object, "Other");
	EXPECT_EQ(b.flags, psys::SoundAction::k_UseSurface); // ONLYONE is not kept
	// a missing property: the SoundAction defaults
	const auto none = psys::ReadSoundAction(object, "Missing");
	EXPECT_EQ(none.action, -1);
	EXPECT_EQ(none.surface, 1);
	EXPECT_EQ(none.size, 2);
	EXPECT_EQ(none.alignment, 2);
	EXPECT_EQ(none.fadeStep, 0);
	EXPECT_EQ(none.flags, 0);
}

TEST(SpellSounds, sizeClasses)
{
	using namespace audio::spell_sounds;
	EXPECT_EQ(SizeFromRadius(199.0f, 200.0f, 500.0f), 3);
	EXPECT_EQ(SizeFromRadius(200.0f, 200.0f, 500.0f), 2);
	EXPECT_EQ(SizeFromRadius(499.0f, 200.0f, 500.0f), 2);
	EXPECT_EQ(SizeFromRadius(500.0f, 200.0f, 500.0f), 1);
	// the original compares with the doubles 0.6 / 0.3: 0.6f = 0.60000002 and 0.3f = 0.30000001 are above them, the
	// floats just below are not
	EXPECT_EQ(SizeFromThrow(0.61f), 1);
	EXPECT_EQ(SizeFromThrow(0.6f), 1);
	EXPECT_EQ(SizeFromThrow(std::nextafter(0.6f, 0.0f)), 2);
	EXPECT_EQ(SizeFromThrow(0.31f), 2);
	EXPECT_EQ(SizeFromThrow(0.3f), 2);
	EXPECT_EQ(SizeFromThrow(std::nextafter(0.3f, 0.0f)), 3);
	EXPECT_EQ(SizeFromThrow(0.0f), 3);
	EXPECT_EQ(SizeFromImpactSpeed(9.0f, 10.0f, 20.0f), 3);
	EXPECT_EQ(SizeFromImpactSpeed(10.0f, 10.0f, 20.0f), 2);
	EXPECT_EQ(SizeFromImpactSpeed(20.0f, 10.0f, 20.0f), 1);
}

TEST(SpellSounds, findAttribRow)
{
	constexpr int32_t w = audio::AnimEffectBank::k_Wildcard;
	audio::AnimEffectBank bank;
	bank.rows = {{w, w, w, w, 52, 0}, {1, w, w, w, 52, 2}, {1, w, w, 7, 52, 5}, {w, w, w, 7, 52, 7}};
	bank.waves = {1, 10, 2, 11, 12, 1, 13, 1, 14};
	EXPECT_EQ(bank.FindList({3, 2, 1, 1, 52}), std::vector<int32_t>({10}));
	EXPECT_EQ(bank.FindList({1, 2, 1, 1, 52}), std::vector<int32_t>({11, 12}));
	EXPECT_EQ(bank.FindList({1, 2, 1, 7, 52}), std::vector<int32_t>({13})); // the most exact row
	EXPECT_EQ(bank.FindList({2, 2, 1, 7, 52}), std::vector<int32_t>({14}));
	EXPECT_TRUE(bank.FindList({1, 2, 1, 1, 40}).empty());
}

/// With OPENBLACK_GAME_PATH set to the install: Data\SoundAction.h, spells.sad and SF_TeleportVortex
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(SpellSounds, realData)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const std::filesystem::path root(game);
	std::ifstream header(root / "Data" / "SoundAction.h", std::ios::binary);
	ASSERT_TRUE(header.is_open());
	const std::string text((std::istreambuf_iterator<char>(header)), std::istreambuf_iterator<char>());
	const auto names = psys::ParseEnumHeader(text);
	EXPECT_EQ(ValueOf(names, "SOUND_SPELL_TELEPORT_POOL"), 76);
	EXPECT_EQ(ValueOf(names, "SOUND_SPELL_HAND_LIGHTNING_LEVEL_3"), 113);
	EXPECT_EQ(ValueOf(names, "SOUND_SPELL_CREATURE_SPELL_CAST"), 117);
	EXPECT_EQ(ValueOf(names, "SOUND_SPELL_WATER"), 141);
	EXPECT_EQ(ValueOf(names, "SOUND_ACTION_VORTEX"), 156);

	pack::PackFile pack;
	ASSERT_EQ(pack.Open(root / "Audio" / "Sfx" / "Game" / "spells.sad"), pack::PackResult::Success);
	audio::AnimEffectBank bank;
	bank.name = "spells.sad";
	bank.Load(pack);
	EXPECT_EQ(bank.rows.size(), 75u);
	// the default slots {size 2, alignment 2, 1, surface 1}: the "*" rows
	EXPECT_EQ(bank.FindList({2, 2, 1, 1, 76}), std::vector<int32_t>({39}));
	EXPECT_EQ(bank.FindList({2, 2, 1, 1, 48}), std::vector<int32_t>({1}));
	EXPECT_EQ(bank.FindList({1, 2, 1, 3, 52}), std::vector<int32_t>({28, 29}));
	EXPECT_EQ(bank.FindList({3, 2, 1, 7, 52}), std::vector<int32_t>({23}));
	EXPECT_EQ(bank.FindList({3, 2, 1, 1, 43}), std::vector<int32_t>({16}));
	EXPECT_TRUE(bank.FindList({2, 2, 1, 1, 57}).empty()); // CAST_FAILURE has no row
	const auto* pool = bank.FindSample(39);
	ASSERT_NE(pool, nullptr);
	EXPECT_EQ(pool->loops, -1);
	EXPECT_EQ(pool->playMode, 2);
	EXPECT_FLOAT_EQ(pool->maxDistance, 170.0f);

	std::ifstream zzz(root / "Data" / "Spells" / "ZSpellFiles" / "SF_TeleportVortex_txt.zzz", std::ios::binary);
	ASSERT_TRUE(zzz.is_open());
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(zzz)), std::istreambuf_iterator<char>());
	ASSERT_GT(bytes.size(), 4u);
	uint32_t size = 0;
	std::memcpy(&size, bytes.data(), 4);
	const auto inflated = zip::Inflate(std::vector<uint8_t>(bytes.begin() + 4, bytes.end()), size);
	const auto file = psys::File::Parse(std::string(inflated.begin(), inflated.end()), "SF_TeleportVortex");
	ASSERT_TRUE(file.has_value());
	const auto* create = file->Find("CreateRuleAnAtom0");
	ASSERT_NE(create, nullptr);
	EXPECT_EQ(create->String("SoundOfCreate"), "SOUND_SPELL_TELEPORT_POOL");
	EXPECT_EQ(psys::ReadSoundAction(*create, "SoundOfCreate").flags,
	          psys::SoundAction::k_Looping | psys::SoundAction::k_SoftRelease);
}
