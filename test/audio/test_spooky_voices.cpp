/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Audio/Audio.h"
#include "Audio/Device/SampleOutput.h"
#include "Audio/Device/Sound.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/Guidance.h"
#include "Audio/Services/SpookyVoices.h"
#include "Audio/Services/Voices.h"
#include "Common/HelpText.h"

// The spooky voices. The Soundex is checked against the original's letter code table and against an independent
// emulation over the installation's 100 names (the installed-game test needs OPENBLACK_TEST_BW_ROOT).

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::audio::spooky;

namespace
{
std::optional<std::filesystem::path> GameRoot()
{
	const char* root = std::getenv("OPENBLACK_TEST_BW_ROOT");
	if (root == nullptr || *root == '\0' || !std::filesystem::is_directory(root))
	{
		return std::nullopt;
	}
	return std::filesystem::path(root);
}

std::vector<uint8_t> ReadAll(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::optional<std::filesystem::path> FindNoCase(const std::filesystem::path& root, std::string_view relative)
{
	const auto lower = [](std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	};
	auto current = root;
	for (const auto& part : std::filesystem::path(relative))
	{
		const auto wanted = lower(part.string());
		std::optional<std::filesystem::path> found;
		std::error_code ec;
		for (const auto& entry : std::filesystem::directory_iterator(current, ec))
		{
			if (lower(entry.path().filename().string()) == wanted)
			{
				found = entry.path();
				break;
			}
		}
		if (!found)
		{
			return std::nullopt;
		}
		current = *found;
	}
	return current;
}

class FakeOutput final: public SampleOutput
{
public:
	std::array<bool, 16> playing {};
	std::array<Start, 16> starts {};
	bool Play(size_t channel, Sound&, const Start& start) override
	{
		playing[channel] = true;
		starts[channel] = start;
		return true;
	}
	void Stop(size_t channel) override { playing[channel] = false; }
	void StopRamped(size_t channel) override { playing[channel] = false; }
	[[nodiscard]] int64_t PlayPositionMs(size_t channel) const override { return playing[channel] ? 0 : -1; }
	[[nodiscard]] bool Playing(size_t channel) const override { return playing[channel]; }
	void SetGain(size_t, float) override {}
	void SetPitch(size_t, float) override {}
	void SetPosition(size_t, glm::vec3) override {}
	void ReleaseLoop(size_t) override {}
	void SetListener(glm::vec3) override {}
	void Update() override {}
	[[nodiscard]] size_t Sources() const override { return 0; }
	void DeleteAll() override { playing.fill(false); }
};

/// The codes of a word as three GetNextSoundexCode calls from its second character
std::array<int, 3> Codes(const std::u16string& word)
{
	const char16_t* p = word.c_str() + 1;
	std::array<int, 3> codes {};
	for (auto& code : codes)
	{
		code = GetNextSoundexCode(p);
	}
	return codes;
}
} // namespace

TEST(SpookySoundex, LetterCodes)
{
	// a=0 b=1 c=2 d=3 e=0 f=1 g=2 h=char i=0 j=2 k=2 l=4 m=5 n=5 o=0 p=1 q=2 r=6 s=2 t=3 u=0 v=1 w=char x=2 y=char z=2
	const std::array<int, 26> dump {0, 1, 2, 3, 0, 1, 2, -1, 0, 2, 2, 4, 5, 5, 0, 1, 2, 6, 2, 3, 0, 1, -1, 2, -1, 2};
	for (int i = 0; i < 26; ++i)
	{
		const auto lower = static_cast<char16_t>(u'a' + i);
		const auto upper = static_cast<char16_t>(u'A' + i);
		const int expected = dump.at(static_cast<size_t>(i));
		EXPECT_EQ(SoundexDigit(lower), expected < 0 ? static_cast<int>(lower) : expected) << static_cast<char>(lower);
		EXPECT_EQ(SoundexDigit(upper), expected < 0 ? static_cast<int>(upper) : expected) << static_cast<char>(upper);
	}
	// not a letter (_isalpha): 0, accented letters too in the "C" locale (inferred)
	EXPECT_EQ(SoundexDigit(u' '), 0);
	EXPECT_EQ(SoundexDigit(u'1'), 0);
	EXPECT_EQ(SoundexDigit(u'\u00E9'), 0);
}

TEST(SpookySoundex, NextCodeKeepsTheW120Quirk)
{
	// "Pablo": a 0, b 1 (l differs), l 4, o 0 -> end
	EXPECT_EQ(Codes(u"Pablo"), (std::array<int, 3> {1, 4, 0}));
	// "Curro": the first r is 6 and the next r too -> 7, without passing it; then that r gives 6
	EXPECT_EQ(Codes(u"Curro"), (std::array<int, 3> {7, 6, 0}));
	// 'h' codes as itself
	EXPECT_EQ(Codes(u"Nacho"), (std::array<int, 3> {2, 'h', 0}));
	// a space ends the word
	EXPECT_EQ(Codes(u"Ana Maria"), (std::array<int, 3> {5, 0, 0}));
}

TEST(SpookySoundex, ComparisonAndOverlap)
{
	EXPECT_TRUE(SoundsAlike(u"Manuel", u"Manolo"));  // M; 5 4 0 against 5 4 0
	EXPECT_FALSE(SoundsAlike(u"Manuel", u"manuel")); // the first character exactly
	EXPECT_FALSE(SoundsAlike(u"", u""));             // not the end
	EXPECT_TRUE(SoundsAlike(u"Jos\u00E9", u"Jose"));
	// the name's words in turn
	EXPECT_TRUE(SoundexOverlap(u"Jos\u00E9", u"Maria Jose"));
	EXPECT_FALSE(SoundexOverlap(u"Jos\u00E9", u"Maria "));
	EXPECT_FALSE(SoundexOverlap(u"Jos\u00E9", u""));
}

TEST(SpookySoundex, NightByTheClock)
{
	// >= 23 or <= 5, or 20:45..20:59
	std::tm t {};
	const auto at = [&t](int h, int m) {
		t.tm_hour = h;
		t.tm_min = m;
		return NightNow(t);
	};
	EXPECT_TRUE(at(23, 0));
	EXPECT_TRUE(at(0, 30));
	EXPECT_TRUE(at(5, 59));
	EXPECT_FALSE(at(6, 0));
	EXPECT_FALSE(at(20, 44));
	EXPECT_TRUE(at(20, 45));
	EXPECT_TRUE(at(20, 59));
	EXPECT_FALSE(at(21, 0));
	EXPECT_FALSE(at(22, 59));
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(SpookySoundex, TheInstallationsNames)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	const auto script = FindNoCase(*root, "Scripts/InfoScript2.txt");
	ASSERT_TRUE(script);
	const auto entries = helptext::Parse(ReadAll(*script));
	ASSERT_GT(entries.size(), k_FirstName + k_Names);
	SetNameTexts([&entries](uint32_t id) { return entries.at(id > 0 && id < entries.size() ? id : 0).text; });
	// the voice table: the names' waves are Guidance 30..129 in the installation: here sample =
	// the name's index + 1
	std::vector<std::string> names;
	for (const auto& entry : entries)
	{
		names.push_back(entry.name);
	}
	VoiceTable::SampleNames guidance;
	for (size_t k = 0; k < k_Names; ++k)
	{
		guidance.push_back(fmt::format("K:\\x\\{}.wav", entries.at(k_FirstName + k).name));
	}
	voices::SetTable(VoiceTable::Build(names, {}, {}, guidance));
	// each name finds itself, or the first earlier one it overlaps
	const std::map<size_t, size_t> earlier {{24, 16}, {35, 16}, {36, 5}, {41, 1},  {68, 64}, {77, 63},
	                                        {82, 9},  {83, 65}, {85, 9}, {89, 14}, {97, 80}};
	for (size_t k = 0; k < k_Names; ++k)
	{
		const auto& name = entries.at(k_FirstName + k).text;
		const auto found = earlier.find(k);
		const auto expected = static_cast<uint32_t>((found != earlier.end() ? found->second : k) + 1);
		EXPECT_EQ(FindSoundAlikeName(name), expected) << k;
	}
	// the profile on the installation's disk (Profiles\_J_a_h_o_v_i_a) has no spooky name; "Maria Jose" is Jose
	EXPECT_EQ(FindSoundAlikeName(u"Jahovia"), 0u);
	EXPECT_EQ(FindSoundAlikeName(u"Maria Jose"), 1u);
	EXPECT_EQ(FindSoundAlikeName(u"Mario"), 65u);
	EXPECT_EQ(FindSoundAlikeName(u""), 0u);
	SetNameTexts({});
	voices::SetTable({});
}

TEST(SpookySoundex, TheInstallationsNamesSynthetic)
{
	// Four names in the first slots and empty texts in the others (an empty text matches nothing). Name k is the
	// Guidance wave k + 1 through the voice table.
	const std::array<std::u16string, 4> texts = {u"Jose", u"Manuel", u"Manolo", u"Pablo"};
	SetNameTexts([&texts](uint32_t id) {
		const auto k = static_cast<size_t>(id - k_FirstName);
		return id >= k_FirstName && k < texts.size() ? texts.at(k) : std::u16string();
	});
	std::vector<std::string> names(k_FirstName + k_Names);
	VoiceTable::SampleNames guidance;
	for (size_t k = 0; k < k_Names; ++k)
	{
		names.at(k_FirstName + k) = fmt::format("HELP_TEXT_SPOOKY_NAMES_{}", k + 1);
		guidance.push_back(fmt::format("K:\\x\\{}.wav", names.at(k_FirstName + k)));
	}
	voices::SetTable(VoiceTable::Build(names, {}, {}, guidance));
	// each name finds itself, or the first earlier one it overlaps: Manolo sounds like Manuel
	EXPECT_EQ(FindSoundAlikeName(u"Jose"), 1u);
	EXPECT_EQ(FindSoundAlikeName(u"Manuel"), 2u);
	EXPECT_EQ(FindSoundAlikeName(u"Manolo"), 2u);
	EXPECT_EQ(FindSoundAlikeName(u"Pablo"), 4u);
	// any word of the name counts; no match and an empty name give 0
	EXPECT_EQ(FindSoundAlikeName(u"Maria Jose"), 1u);
	EXPECT_EQ(FindSoundAlikeName(u"Jahovia"), 0u);
	EXPECT_EQ(FindSoundAlikeName(u"Maria"), 0u);
	EXPECT_EQ(FindSoundAlikeName(u""), 0u);
	SetNameTexts({});
	voices::SetTable({});
}

namespace
{
class SpookyTest: public ::testing::Test
{
protected:
	FakeOutput output;
	std::map<entt::id_type, Sound> sounds;
	BankId guidance {k_NoBank};
	static inline int s_Land = 3;
	static inline uint32_t s_Seed = 1;

	void SetUp() override
	{
		s_Land = 3;
		GameQueries queries;
		queries.landNumber = []() { return s_Land; };
		queries.camera = []() -> std::optional<CameraState> { return CameraState {}; };
		audio::Init(std::move(queries));
		guidance = RegisterBank("audio/dialogue/Guidance.sad", "Guidance.sad");
		sample_play::Backend backend;
		backend.output = &output;
		backend.sound = [this](entt::id_type id) -> Sound* {
			const auto found = sounds.find(id);
			return found != sounds.end() ? &found->second : nullptr;
		};
		backend.rand = []() { return 16383; };
		backend.camera = []() -> std::optional<glm::vec3> { return glm::vec3(0.0f); };
		sample_play::SetBackend(std::move(backend));
		sample_play::SetMainVolume(127);
		audio::ClearMap();
		output = FakeOutput {};
		Sound sound;
		sound.name = "Guidance 30";
		sound.id = 30;
		sound.bank = guidance;
		sound.priority = 20;
		sound.sampleRate = 22050;
		sound.pitch = 100;
		sound.pitchDeviation = 0;
		sound.duration = 1.0f;
		sounds.emplace(SampleId(guidance, 30), std::move(sound));
		// 2026-10-01 23:30 local: night
		SetClock([]() {
			std::tm t {};
			t.tm_year = 126;
			t.tm_mon = 9;
			t.tm_mday = 1;
			t.tm_hour = 23;
			t.tm_min = 30;
			t.tm_isdst = -1;
			return static_cast<int64_t>(std::mktime(&t));
		});
		guidance::SetRandom([](uint32_t n) { return guidance::SeededRandom(n, s_Seed); });
		guidance::ResetForTests();
	}
	void TearDown() override
	{
		SetClock({});
		guidance::SetRandom({});
		audio::ClearMap();
		sample_play::SetMainVolume(127); // back to its default (QMixer's maximum)
		sample_play::SetBackend({});
		audio::Shutdown();
	}
};
} // namespace

TEST_F(SpookyTest, CountdownAndCounter)
{
	SetForTests(30, 0, 100);
	// lands 1 and 2: nothing
	s_Land = 1;
	Process();
	EXPECT_EQ(GetState().countdown, 100u);
	s_Land = 3;
	// the countdown runs to 0: 100 calls
	for (int i = 0; i < 100; ++i)
	{
		Process();
	}
	EXPECT_EQ(GetState().countdown, 0u);
	EXPECT_EQ(GetState().counter, 0u);
	// at 0, at night: (1 - r^3) x 1000, truncated, is never < 0, so no voice but guidance::OneOff(1); the counter + 1, the
	// countdown 99
	Process();
	EXPECT_EQ(GetState().counter, 1u);
	EXPECT_EQ(GetState().countdown, 99u);
	// no sample: nothing at all
	SetForTests(0, 0, 0);
	Process();
	EXPECT_EQ(GetState().counter, 0u);
}

TEST_F(SpookyTest, PlaySpookyVoiceBuildsTheVolumeUp)
{
	// draws: LocalFloatRand(0.65) -> 0, LocalRand(2) -> 1 (p = 1), LocalRand(180), LocalFloatRand(0.8) -> 0, LocalRand(2)
	guidance::SetRandom([](uint32_t n) { return n == 0xFFFF ? 0u : (n == 2 ? 1u : 7u); });
	SetForTests(30, 5, 0);
	PlaySpookyVoice();
	EXPECT_EQ(GetState().counter, 0u);
	EXPECT_EQ(GetState().options.pitch, 100);
	EXPECT_EQ(GetState().options.volume, 127);
	EXPECT_EQ(GetState().field2C, 7);
	EXPECT_FALSE(GetState().options.is3D);
	// a = LocalFloatRand(0.65) at its top and LocalRand(2) = 0: p = 1 / (1 + a^3); the volume is multiplied by q every
	// time and kept
	guidance::SetRandom([](uint32_t n) { return n == 0xFFFF ? 0xFFFEu : 0u; });
	PlaySpookyVoice();
	// LocalFloatRand returns a float (the caller stores it as one)
	const double a = static_cast<float>(static_cast<double>(0xFFFE) * 0.65f * 1.5259022e-5f);
	const double b = static_cast<float>(static_cast<double>(0xFFFE) * 0.8f * 1.5259022e-5f);
	const auto p = static_cast<float>(1.0 / (1.0 + a * a * a));
	EXPECT_EQ(GetState().options.pitch, static_cast<int>(static_cast<double>(p) * 100.0f)); // 78
	const int volume = static_cast<int>(127.0 / (1.0 + b * b * b));
	EXPECT_EQ(GetState().options.volume, volume);
	PlaySpookyVoice();
	EXPECT_EQ(GetState().options.volume, static_cast<int>(volume / (1.0 + b * b * b)));
}
