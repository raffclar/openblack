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
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "Audio/Engine/MusicBank.h"
#include "Audio/Game/BankTables.h"
#include "support/TestServices.h"

// MusicBank over the installed music banks. Expected values: each bank's segments, group, flags, loops, volume and
// min/max/scale of the first segment, and the music type table; markers and segment layout from the data. Needs
// OPENBLACK_TEST_BW_ROOT (the game folder with Audio\); without it the tests skip.
// The *Synthetic tests check the same reading rules on small banks written by the test, so they always run.

using namespace openblack::audio;

namespace
{
/// MusicBank::Register reads through the file system: a DefaultFileSystem for the call, the one before put back
/// after it (the bank keeps its own open file)
std::unique_ptr<MusicBank> RegisterMusicBank(const std::filesystem::path& path)
{
	const openblack::test::ScopedDefaultFileSystem fileSystem;
	return MusicBank::Register(path);
}

std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

std::optional<std::filesystem::path> GameRoot()
{
	const char* root = std::getenv("OPENBLACK_TEST_BW_ROOT");
	if (root == nullptr || *root == '\0' || !std::filesystem::is_directory(root))
	{
		return std::nullopt;
	}
	return std::filesystem::path(root);
}

// The original runs on a case-insensitive file system: resolve each component without case
std::optional<std::filesystem::path> FindNoCase(const std::filesystem::path& root, std::string_view relative)
{
	auto current = root;
	for (const auto& part : std::filesystem::path(relative))
	{
		const auto wanted = Lower(part.string());
		std::optional<std::filesystem::path> found;
		std::error_code ec;
		for (const auto& entry : std::filesystem::directory_iterator(current, ec))
		{
			if (Lower(entry.path().filename().string()) == wanted)
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

struct Expected
{
	const char* path;
	size_t segments;
	int group;
	uint32_t flags;
	int loops;  // GetLoops(0): -1 with flag 0x40, else the 0 requested
	int volume; // 127 unless flag 0x20
	float minD; // GetDistanceMapping({-1, -1, -1}): -1 = not overridden
	float maxD;
	float scale;
};

// the banks of Audio\Music + the three verses (Dialogue, music types 51..53)
const std::array<Expected, 80> k_Expected = {{
    {"Audio/Music/Intro/intro.sad", 177, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/Intro/trailer.sad", 97, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Aztc_Evil.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Aztc_Good.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Aztc_Neutral.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Celt_Evil.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Celt_Good.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Celt_Neutral.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Egpt_Evil.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Egpt_Good.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Egpt_Neutral.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Evil.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Good.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Grek_Evil.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Grek_Good.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Grek_Neutral.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Indn_Evil.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Indn_Good.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Indn_Neutral.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Japn_Evil.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Japn_Good.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Japn_Neutral.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Neutral.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Tbtn_Evil.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Tbtn_Good.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/align/Tbtn_Neutral.sad", 430, 1, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/chant/aztc_chant.sad", 34, 6, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/aztc_chant_vox.sad", 34, 6, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/celt_chant.sad", 47, 7, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/celt_chant_vox.sad", 47, 7, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/egpt_chant.sad", 57, 8, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/egpt_chant_vox.sad", 57, 8, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/grek_chant.sad", 44, 9, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/grek_chant_vox.sad", 44, 9, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/indn_chant.sad", 37, 10, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/indn_chant_vox.sad", 37, 10, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/japn_chant.sad", 32, 13, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/japn_chant_vox.sad", 32, 13, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/nrse_chant.sad", 81, 11, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/nrse_chant_vox.sad", 81, 11, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/tbtn_chant.sad", 31, 12, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/chant/tbtn_chant_vox.sad", 31, 12, 0x3C0, -1, 127, 30.0f, 120.0f, 2.0f},
    {"Audio/Music/citadel/citadel.sad", 273, 4, 0x40, -1, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/outro/Outro.sad", 180, 0, 0x40, -1, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Christmas.sad", 16, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Circus.sad", 27, 0, 0x60, -1, 60, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Circus3D.sad", 27, 0, 0x3E0, -1, 60, 30.0f, 120.0f, 3.0f},
    {"Audio/Music/script/CreatureBigFight.sad", 127, 0, 0x40, -1, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/CreatureChosen.sad", 11, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/CreatureEndSequence.sad", 55, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/CreatureFight.sad", 83, 5, 0x40, -1, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/CreatureGuide.sad", 142, 0, 0x60, -1, 70, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Epic01.sad", 29, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Epic02.sad", 28, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Epic03.sad", 30, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Epic04.sad", 28, 0, 0x20, 0, 80, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Failure.sad", 17, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Funeral.sad", 15, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Gregorian.sad", 35, 0, 0x60, -1, 40, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Gregorian3D.sad", 35, 0, 0x3E0, -1, 50, 60.0f, 120.0f, 4.0f},
    {"Audio/Music/script/GuardianStone.sad", 20, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Hermit.sad", 50, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/MissionariesBackground.sad", 37, 0, 0x3C0, -1, 127, 30.0f, 100.0f, 4.0f},
    {"Audio/Music/script/MissionariesSad.sad", 19, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Nemesis.sad", 115, 0, 0x40, -1, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/PiperTune_M.sad", 107, 0, 0x3C0, -1, 127, 15.0f, 100.0f, 4.0f},
    {"Audio/Music/script/Pipercave_M.sad", 107, 0, 0x3C0, -1, 127, 25.0f, 80.0f, 4.0f},
    {"Audio/Music/script/Script01.sad", 50, 0, 0x20, 0, 60, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Script02.sad", 51, 0, 0x20, 0, 65, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Script03.sad", 57, 0, 0x20, 0, 65, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Script04.sad", 51, 0, 0x20, 0, 65, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/SingingStonesA.sad", 80, 0, 0x3C0, -1, 127, 100.0f, 200.0f, 4.0f},
    {"Audio/Music/script/Sleg.sad", 31, 0, 0x20, 0, 65, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/Twinkle.sad", 14, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Music/script/WhistleFuneral.sad", 7, 0, 0x3C0, -1, 127, 30.0f, 80.0f, 2.0f},
    {"Audio/Music/script/WhistleTwinkle.sad", 10, 0, 0x3C0, -1, 127, 30.0f, 80.0f, 2.0f},
    {"Audio/Music/script/khazar.sad", 63, 0, 0x60, -1, 65, -1.0f, -1.0f, -1.0f},
    {"Audio/Dialogue/MissionariesVerse1.sad", 36, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Dialogue/MissionariesVerse2.sad", 37, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
    {"Audio/Dialogue/MissionariesVerse3.sad", 37, 0, 0x0, 0, 127, -1.0f, -1.0f, -1.0f},
}};

// MPEG audio frame walk (ISO/IEC 11172-3 / 13818-3 header). Every music segment is MPEG-2 (LSF) Layer II at 22050 Hz:
// frame bytes = 144 * bitrate / 22050 + padding, 1152 samples per frame.
constexpr std::array<int, 16> k_Mpeg2LayerIIKbps = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};

// Number of frames if the segment is exactly a run of MPEG-2 Layer II 22050 Hz frames, else -1
int CountFrames(const std::vector<uint8_t>& data)
{
	size_t pos = 0;
	int frames = 0;
	while (pos < data.size())
	{
		if (pos + 4 > data.size())
		{
			return -1;
		}
		const uint8_t* h = &data[pos];
		const bool sync = h[0] == 0xFF && (h[1] & 0xF0) == 0xF0;
		const bool mpeg2 = ((h[1] >> 3) & 1) == 0;
		const bool layer2 = ((h[1] >> 1) & 3) == 2;
		const bool rate22050 = ((h[2] >> 2) & 3) == 0;
		const int kbps = k_Mpeg2LayerIIKbps[h[2] >> 4];
		if (!sync || !mpeg2 || !layer2 || !rate22050 || kbps == 0)
		{
			return -1;
		}
		pos += static_cast<size_t>(144 * kbps * 1000 / 22050 + ((h[2] >> 1) & 1));
		++frames;
	}
	return pos == data.size() ? frames : -1;
}

// ---- synthetic banks ----------------------------------------------------------------------------------------------

/// The running test's suite and name and the process id: parallel test processes never share a temporary file
std::string UniqueSuffix()
{
	const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
#ifdef _WIN32
	const auto pid = _getpid();
#else
	const auto pid = getpid();
#endif
	return std::string(info->test_suite_name()) + "_" + info->name() + "_" + std::to_string(pid);
}

/// Removes the file the test wrote when it goes out of scope (declare it before the bank that keeps the file open)
struct TempFile
{
	std::filesystem::path path;
	explicit TempFile(std::filesystem::path p)
	    : path(std::move(p))
	{
	}
	TempFile(const TempFile&) = delete;
	TempFile& operator=(const TempFile&) = delete;
	~TempFile()
	{
		std::error_code ec;
		std::filesystem::remove(path, ec);
	}
};

// Byte offsets inside one sample table record, as the music bank reads them
constexpr size_t k_RecordSegmentSize = 0x10C;
constexpr size_t k_RecordSegmentOffset = 0x110;
constexpr size_t k_RecordGroup = 0x118;
constexpr size_t k_RecordRate = 0x128;
constexpr size_t k_RecordDescription = 0x140;
constexpr size_t k_RecordFlags = 0x244;
constexpr size_t k_RecordLoops = 0x248;
constexpr size_t k_RecordVolume = 0x25C;
constexpr size_t k_RecordMinDistance = 0x268;
constexpr size_t k_RecordMaxDistance = 0x26C;
constexpr size_t k_RecordScale = 0x270;

/// What the first record of a synthetic bank says. The later records get other values on purpose, so a test can
/// see that only the first one counts.
struct BankSpec
{
	uint32_t musicFlag {1};
	std::vector<std::vector<uint8_t>> segments;
	std::vector<std::string> descriptions;
	uint32_t group {0};
	uint32_t flags {0};
	int32_t loops {0};
	uint32_t volumeWord {127};
	float minDistance {0.0f};
	float maxDistance {0.0f};
	float scale {0.0f};
	uint32_t sampleRate {22050};
	int badOffsetSegment {-1}; ///< a segment whose offset points past the wave data
};

template <typename T>
void Put(std::vector<uint8_t>& bytes, size_t offset, T value)
{
	std::memcpy(&bytes[offset], &value, sizeof(value));
}

void WriteBlock(std::ofstream& out, const char* name, const std::vector<uint8_t>& data)
{
	std::array<char, 32> blockName {};
	std::strncpy(blockName.data(), name, blockName.size() - 1);
	out.write(blockName.data(), blockName.size());
	const auto size = static_cast<uint32_t>(data.size());
	out.write(reinterpret_cast<const char*>(&size), sizeof(size));
	out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

/// A LiOnHeAd file with the bank info, the wave data (the segments back to back) and the sample table
std::filesystem::path WriteBank(const std::string& name, const BankSpec& spec)
{
	const auto path = std::filesystem::path(TEST_BINARY_DIR) / ("music_bank_" + name + "_" + UniqueSuffix() + ".sad");
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write("LiOnHeAd", 8);
	std::vector<uint8_t> info(12, 0);
	Put(info, 8, spec.musicFlag);
	WriteBlock(out, "LHFileSegmentBankInfo", info);

	std::vector<uint8_t> wave;
	std::vector<uint32_t> offsets;
	for (const auto& segment : spec.segments)
	{
		offsets.push_back(static_cast<uint32_t>(wave.size()));
		wave.insert(wave.end(), segment.begin(), segment.end());
	}
	WriteBlock(out, "LHAudioWaveData", wave);

	const size_t count = spec.segments.size();
	std::vector<uint8_t> table(4 + count * MusicBank::k_RecordSize, 0);
	Put(table, 0, static_cast<uint32_t>(count));
	for (size_t i = 0; i < count; ++i)
	{
		const size_t r = 4 + i * MusicBank::k_RecordSize;
		Put(table, r + k_RecordSegmentSize, static_cast<uint32_t>(spec.segments[i].size()));
		const bool bad = static_cast<int>(i) == spec.badOffsetSegment;
		Put(table, r + k_RecordSegmentOffset, bad ? static_cast<uint32_t>(wave.size() + 1) : offsets[i]);
		Put(table, r + k_RecordRate, i == 0 ? spec.sampleRate : 11025u);
		if (i < spec.descriptions.size())
		{
			std::memcpy(&table[r + k_RecordDescription], spec.descriptions[i].data(), spec.descriptions[i].size());
		}
		const bool first = i == 0;
		Put(table, r + k_RecordGroup, first ? spec.group : spec.group + 1);
		Put(table, r + k_RecordFlags, first ? spec.flags : (~spec.flags & 0x3E0u));
		Put(table, r + k_RecordLoops, first ? spec.loops : 99);
		Put(table, r + k_RecordVolume, first ? spec.volumeWord : 1u);
		Put(table, r + k_RecordMinDistance, first ? spec.minDistance : 1.0f);
		Put(table, r + k_RecordMaxDistance, first ? spec.maxDistance : 2.0f);
		Put(table, r + k_RecordScale, first ? spec.scale : 3.0f);
	}
	WriteBlock(out, "LHAudioBankSampleTable", table);
	return path;
}

/// A silent MPEG-2 layer II frame at 22050 Hz with no CRC: the header, then zero bits (no subband is allocated)
std::vector<uint8_t> SilentFrame(int kbps, bool mono)
{
	const auto index = std::find(k_Mpeg2LayerIIKbps.begin(), k_Mpeg2LayerIIKbps.end(), kbps) - k_Mpeg2LayerIIKbps.begin();
	std::vector<uint8_t> frame(static_cast<size_t>(144 * kbps * 1000 / 22050), 0);
	frame[0] = 0xFF;
	frame[1] = 0xF5;
	frame[2] = static_cast<uint8_t>(index << 4);
	frame[3] = mono ? 0xC0 : 0x00;
	return frame;
}

std::vector<uint8_t> SilentFrames(int count)
{
	std::vector<uint8_t> bytes;
	for (int i = 0; i < count; ++i)
	{
		const auto frame = SilentFrame(8, false);
		bytes.insert(bytes.end(), frame.begin(), frame.end());
	}
	return bytes;
}
} // namespace

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(MusicBank, MissingFile)
{
	EXPECT_EQ(RegisterMusicBank("this/file/does/not/exist.sad"), nullptr);
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// MUSIC_TYPE_SCRIPT_WELCOME_DANCE: registering fails, the bank stays null
	EXPECT_EQ(RegisterMusicBank(*root / std::string(MusicBankFor(MusicType::ScriptWelcomeDance).path)), nullptr);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(MusicBank, FirstSegmentFields)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	for (const auto& e : k_Expected)
	{
		const auto path = FindNoCase(*root, e.path);
		ASSERT_TRUE(path.has_value()) << e.path;
		const auto bank = RegisterMusicBank(*path);
		ASSERT_NE(bank, nullptr) << e.path;
		EXPECT_TRUE(bank->IsMusic()) << e.path;
		EXPECT_EQ(bank->GetSegmentCount(), e.segments) << e.path;
		EXPECT_EQ(bank->GetGroupId(), e.group) << e.path;
		EXPECT_EQ(bank->GetFlags(), e.flags) << e.path;
		EXPECT_EQ(bank->GetSampleRate(), 22050u) << e.path;
		EXPECT_EQ(bank->GetVolume(), e.volume) << e.path;
		EXPECT_EQ(bank->GetLoops(0), e.loops) << e.path;
		if (!bank->HasFlag(MusicBankFlag::Loops))
		{
			EXPECT_EQ(bank->GetLoops(5), 5) << e.path;
		}
		const auto mapping = bank->GetDistanceMapping({-1.0f, -1.0f, -1.0f});
		EXPECT_FLOAT_EQ(mapping.minDistance, e.minD) << e.path;
		EXPECT_FLOAT_EQ(mapping.maxDistance, e.maxD) << e.path;
		EXPECT_FLOAT_EQ(mapping.scale, e.scale) << e.path;
		if (bank->HasFlag(MusicBankFlag::MaxDistance))
		{
			EXPECT_FLOAT_EQ(bank->GetMaxDistance(), e.maxD) << e.path;
			EXPECT_FLOAT_EQ(bank->GetMinDistance(), e.minD) << e.path;
		}
	}
}

TEST(MusicBank, FirstSegmentFieldsSynthetic)
{
	// The same reads as above on banks of three segments; the expected values follow from each spec
	struct Case
	{
		const char* name;
		BankSpec spec;
		int loops0; // GetLoops(0)
		int loops5; // GetLoops(5)
		int volume;
		MusicDistanceMapping mapping; // GetDistanceMapping({-1, -1, -1})
	};
	const auto segments = std::vector<std::vector<uint8_t>> {{1, 2}, {3, 4}, {5, 6}};
	std::vector<Case> cases;
	// no flag: the requested loops, volume 127, the requested mapping
	{
		BankSpec spec;
		spec.segments = segments;
		spec.group = 1;
		spec.loops = 7;
		spec.volumeWord = 40;
		spec.minDistance = 10.0f;
		spec.maxDistance = 20.0f;
		spec.scale = 0.5f;
		cases.push_back({"plain", spec, 0, 5, 127, {-1.0f, -1.0f, -1.0f}});
	}
	// volume and loops flags: the record's volume (low 16 bits only) and loops, the requested mapping
	{
		BankSpec spec;
		spec.segments = segments;
		spec.group = 2;
		spec.flags = static_cast<uint32_t>(MusicBankFlag::Volume) | static_cast<uint32_t>(MusicBankFlag::Loops);
		spec.loops = -1;
		spec.volumeWord = (3u << 16) | 60u;
		cases.push_back({"volume_loops", spec, -1, -1, 60, {-1.0f, -1.0f, -1.0f}});
	}
	// every distance flag with the volume and loops ones: the whole mapping from the record
	{
		BankSpec spec;
		spec.segments = segments;
		spec.flags = static_cast<uint32_t>(MusicBankFlag::Volume) | static_cast<uint32_t>(MusicBankFlag::Loops) |
		             static_cast<uint32_t>(MusicBankFlag::MinDistance) | static_cast<uint32_t>(MusicBankFlag::MaxDistance) |
		             static_cast<uint32_t>(MusicBankFlag::Scale);
		spec.loops = 2;
		spec.volumeWord = 50;
		spec.minDistance = 60.0f;
		spec.maxDistance = 120.0f;
		spec.scale = 4.0f;
		cases.push_back({"three_d", spec, 2, 2, 50, {60.0f, 120.0f, 4.0f}});
	}
	// the max distance flag alone: only that field of the mapping is replaced
	{
		BankSpec spec;
		spec.segments = segments;
		spec.flags = static_cast<uint32_t>(MusicBankFlag::MaxDistance);
		spec.minDistance = 15.0f;
		spec.maxDistance = 80.0f;
		spec.scale = 2.0f;
		cases.push_back({"max_only", spec, 0, 5, 127, {-1.0f, 80.0f, -1.0f}});
	}
	for (const auto& c : cases)
	{
		const TempFile file {WriteBank(c.name, c.spec)};
		const auto bank = RegisterMusicBank(file.path);
		ASSERT_NE(bank, nullptr) << c.name;
		EXPECT_TRUE(bank->IsMusic()) << c.name;
		EXPECT_EQ(bank->GetSegmentCount(), segments.size()) << c.name;
		EXPECT_EQ(bank->GetGroupId(), static_cast<int>(c.spec.group)) << c.name;
		EXPECT_EQ(bank->GetFlags(), c.spec.flags) << c.name;
		EXPECT_EQ(bank->GetSampleRate(), c.spec.sampleRate) << c.name;
		EXPECT_EQ(bank->GetVolume(), c.volume) << c.name;
		EXPECT_EQ(bank->GetLoops(0), c.loops0) << c.name;
		EXPECT_EQ(bank->GetLoops(5), c.loops5) << c.name;
		const auto mapping = bank->GetDistanceMapping({-1.0f, -1.0f, -1.0f});
		EXPECT_FLOAT_EQ(mapping.minDistance, c.mapping.minDistance) << c.name;
		EXPECT_FLOAT_EQ(mapping.maxDistance, c.mapping.maxDistance) << c.name;
		EXPECT_FLOAT_EQ(mapping.scale, c.mapping.scale) << c.name;
		// a music bank gives the first record's distances whatever its flags
		EXPECT_FLOAT_EQ(bank->GetMinDistance(), c.spec.minDistance) << c.name;
		EXPECT_FLOAT_EQ(bank->GetMaxDistance(), c.spec.maxDistance) << c.name;
	}
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(MusicBank, GroupsOfTheTypeTable)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// the music type table: the alignment music is group 1, citadel 4, CreatureFight 5, the chants 6..13 in pairs,
	// rest 0
	int maxGroup = -1;
	for (size_t i = 1; i < k_MusicBanks.size(); ++i)
	{
		const auto type = static_cast<MusicType>(i);
		const auto path = FindNoCase(*root, k_MusicBanks[i].path);
		if (type == MusicType::ScriptWelcomeDance)
		{
			EXPECT_FALSE(path.has_value());
			continue;
		}
		ASSERT_TRUE(path.has_value()) << k_MusicBanks[i].path;
		const auto bank = RegisterMusicBank(*path);
		ASSERT_NE(bank, nullptr) << k_MusicBanks[i].path;
		int expected = 0;
		if (i >= 1 && i <= 27)
		{
			expected = 1;
		}
		else if (i >= 44 && i <= 46)
		{
			expected = 4;
		}
		else if (type == MusicType::CreatureFight)
		{
			expected = 5;
		}
		else if (i >= 28 && i <= 43)
		{
			// celt 7, aztc 6, japn 13, indn 10, egpt 8, grek 9, nrse 11, tbtn 12 (normal and _vox share it)
			constexpr std::array<int, 8> k_ChantGroups = {7, 6, 13, 10, 8, 9, 11, 12};
			expected = k_ChantGroups[(i - 28) / 2];
		}
		EXPECT_EQ(bank->GetGroupId(), expected) << k_MusicBanks[i].name;
		maxGroup = std::max(maxGroup, bank->GetGroupId());
	}
	// 13 groups in all: the audio side keeps 13 group positions
	EXPECT_EQ(maxGroup, 13);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(MusicBank, SegmentsAreContiguousFrameAlignedChunks)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	size_t total = 0;
	std::vector<uint8_t> data;
	for (const auto& e : k_Expected)
	{
		const auto bank = RegisterMusicBank(*FindNoCase(*root, e.path));
		ASSERT_NE(bank, nullptr) << e.path;
		uint32_t expectedOffset = 0;
		const auto& segments = bank->GetSegments();
		for (size_t i = 0; i < segments.size(); ++i)
		{
			EXPECT_EQ(segments[i].offset, expectedOffset) << e.path << " segment " << i;
			expectedOffset = segments[i].offset + segments[i].size;
			ASSERT_TRUE(bank->ReadSegment(i, data)) << e.path << " segment " << i;
			ASSERT_EQ(data.size(), segments[i].size);
			const int frames = CountFrames(data);
			// 21 frames = 24192 samples (k_MusicSamplesPerSegment); only the last segment may be shorter
			if (i + 1 < segments.size())
			{
				EXPECT_EQ(frames * 1152, k_MusicSamplesPerSegment) << e.path << " segment " << i;
			}
			else
			{
				EXPECT_GE(frames, 1) << e.path;
				EXPECT_LE(frames * 1152, k_MusicSamplesPerSegment) << e.path;
			}
		}
		EXPECT_EQ(expectedOffset, bank->GetWaveDataSize()) << e.path;
		EXPECT_FALSE(bank->ReadSegment(segments.size(), data));
		total += segments.size();
	}
	EXPECT_EQ(total, 13457u);
}

TEST(MusicBank, SegmentsAreContiguousFrameAlignedChunksSynthetic)
{
	// Two full segments of 21 silent frames and a last one of 4: the same walk as above
	BankSpec spec;
	spec.segments = {SilentFrames(21), SilentFrames(21), SilentFrames(4)};
	const TempFile file {WriteBank("frames", spec)};
	const auto bank = RegisterMusicBank(file.path);
	ASSERT_NE(bank, nullptr);
	const auto& segments = bank->GetSegments();
	ASSERT_EQ(segments.size(), spec.segments.size());
	uint32_t expectedOffset = 0;
	std::vector<uint8_t> data;
	for (size_t i = 0; i < segments.size(); ++i)
	{
		EXPECT_EQ(segments[i].offset, expectedOffset) << i;
		expectedOffset = segments[i].offset + segments[i].size;
		ASSERT_TRUE(bank->ReadSegment(i, data)) << i;
		EXPECT_EQ(data, spec.segments[i]) << i;
		const int frames = CountFrames(data);
		if (i + 1 < segments.size())
		{
			EXPECT_EQ(frames * 1152, k_MusicSamplesPerSegment) << i;
		}
		else
		{
			EXPECT_EQ(frames, 4);
		}
	}
	EXPECT_EQ(expectedOffset, bank->GetWaveDataSize());
	EXPECT_FALSE(bank->ReadSegment(segments.size(), data));

	// a segment whose bytes are not inside the wave data cannot be read
	BankSpec broken;
	broken.segments = {SilentFrames(1), SilentFrames(1)};
	broken.badOffsetSegment = 1;
	const TempFile brokenFile {WriteBank("bad_offset", broken)};
	const auto brokenBank = RegisterMusicBank(brokenFile.path);
	ASSERT_NE(brokenBank, nullptr);
	EXPECT_TRUE(brokenBank->ReadSegment(0, data));
	EXPECT_FALSE(brokenBank->ReadSegment(1, data));
}

TEST(MusicBank, ATruncatedLastBlockEndsTheWalk)
{
	// a block whose size runs past the end of the file stops the walk of the blocks; the blocks before it are kept and
	// the segments read as from a whole bank (the reads after a stopped walk work)
	BankSpec spec;
	spec.segments = {SilentFrames(1), SilentFrames(2)};
	const TempFile file {WriteBank("truncated", spec)};
	{
		std::ofstream out(file.path, std::ios::binary | std::ios::app);
		std::array<char, 32> blockName {};
		std::strncpy(blockName.data(), "LHTruncated", blockName.size() - 1);
		out.write(blockName.data(), blockName.size());
		const uint32_t size = 1000; // only 3 bytes follow
		out.write(reinterpret_cast<const char*>(&size), sizeof(size));
		out.write("abc", 3);
	}
	const auto bank = RegisterMusicBank(file.path);
	ASSERT_NE(bank, nullptr);
	ASSERT_EQ(bank->GetSegmentCount(), spec.segments.size());
	std::vector<uint8_t> data;
	for (size_t i = 0; i < spec.segments.size(); ++i)
	{
		ASSERT_TRUE(bank->ReadSegment(i, data)) << i;
		EXPECT_EQ(data, spec.segments[i]) << i;
	}
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(MusicBank, GoodSadLastSegment)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// align/good.sad = 429 x 24192 samples + a shorter last segment of 15865 bytes
	const auto bank = RegisterMusicBank(*FindNoCase(*root, MusicBankFor(MusicType::GenericGood).path));
	ASSERT_NE(bank, nullptr);
	ASSERT_EQ(bank->GetSegmentCount(), 430u);
	EXPECT_EQ(bank->GetSegments().front().size, 17535u);
	EXPECT_EQ(bank->GetSegments().back().size, 15865u);
	std::vector<uint8_t> data;
	ASSERT_TRUE(bank->ReadSegment(429, data));
	EXPECT_EQ(CountFrames(data), 19);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(MusicBank, Markers)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	const auto load = [&root](MusicType type) { return RegisterMusicBank(*FindNoCase(*root, MusicBankFor(type).path)); };

	// MissionariesVerse1: segment 1 says "!20063=P!39=L1"; the list puts L1 (39) before P (20063)
	{
		const auto bank = load(MusicType::ScriptMissionariesVerse1);
		ASSERT_NE(bank, nullptr);
		const auto markers = bank->ParseMarkers();
		ASSERT_EQ(markers.size(), 55u);
		EXPECT_EQ(markers[0].chunk, 1);
		EXPECT_EQ(markers[0].sample, 39);
		EXPECT_EQ(markers[0].label, "L1");
		EXPECT_EQ(markers[1].chunk, 1);
		EXPECT_EQ(markers[1].sample, 20063);
		EXPECT_EQ(markers[1].label, "P");
		EXPECT_EQ(markers[2].chunk, 2);
		EXPECT_EQ(markers[2].sample, 719);
		EXPECT_EQ(markers.back().chunk, 35);
		EXPECT_EQ(markers.back().sample, 3775);
		EXPECT_EQ(markers.back().label, "P");
		// LAST_MUSIC_LINE 1..12 (TheMissionaries.txt): the lines L1..L12, the last one in chunk 32
		std::vector<int> lines;
		for (const auto& m : markers)
		{
			ASSERT_FALSE(m.label.empty());
			if (m.label[0] == 'L')
			{
				lines.push_back(std::atoi(m.label.c_str() + 1));
				if (lines.back() == 12)
				{
					EXPECT_EQ(m.chunk, 32);
					EXPECT_EQ(m.sample, 9023);
				}
			}
			else
			{
				EXPECT_EQ(m.label, "P");
			}
		}
		const std::vector<int> expectedLines = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
		EXPECT_EQ(lines, expectedLines);
	}
	{
		const auto v2 = load(MusicType::ScriptMissionariesVerse2);
		const auto v3 = load(MusicType::ScriptMissionariesVerse3);
		ASSERT_NE(v2, nullptr);
		ASSERT_NE(v3, nullptr);
		EXPECT_EQ(v2->ParseMarkers().size(), 73u);
		EXPECT_EQ(v3->ParseMarkers().size(), 58u);
	}
	// Other music with a stray marker text: the outro's last segment and two "Record Take 001"
	{
		const auto bank = load(MusicType::Outro);
		ASSERT_NE(bank, nullptr);
		const auto markers = bank->ParseMarkers();
		ASSERT_EQ(markers.size(), 1u);
		EXPECT_EQ(markers[0].chunk, 180);
		EXPECT_EQ(markers[0].sample, 12679);
		EXPECT_EQ(markers[0].label, "Marker 4,343,047");
	}
	{
		const auto bank = load(MusicType::ScriptMissionariesBackground);
		ASSERT_NE(bank, nullptr);
		const auto markers = bank->ParseMarkers();
		ASSERT_EQ(markers.size(), 1u);
		EXPECT_EQ(markers[0].chunk, 1);
		EXPECT_EQ(markers[0].sample, 0);
		EXPECT_EQ(markers[0].label, "Record Take 001");
	}
	{
		const auto bank = load(MusicType::GenericGood);
		ASSERT_NE(bank, nullptr);
		EXPECT_TRUE(bank->ParseMarkers().empty());
	}
}

TEST(MusicBank, MarkersSynthetic)
{
	// Segment by segment, and inside a segment in the reverse order of its text; a description that does not start
	// with '!' has no marker, and a label may be empty
	BankSpec spec;
	spec.segments = {{0}, {0}, {0}, {0}, {0}};
	spec.descriptions = {"!20063=P!39=L1", "", "Record Take 001", "!0=Record Take 001", "!7=L2!8="};
	const TempFile file {WriteBank("markers", spec)};
	const auto bank = RegisterMusicBank(file.path);
	ASSERT_NE(bank, nullptr);
	const auto markers = bank->ParseMarkers();
	ASSERT_EQ(markers.size(), 5u);
	EXPECT_EQ(markers[0].chunk, 1);
	EXPECT_EQ(markers[0].sample, 39);
	EXPECT_EQ(markers[0].label, "L1");
	EXPECT_EQ(markers[1].chunk, 1);
	EXPECT_EQ(markers[1].sample, 20063);
	EXPECT_EQ(markers[1].label, "P");
	EXPECT_EQ(markers[2].chunk, 4);
	EXPECT_EQ(markers[2].sample, 0);
	EXPECT_EQ(markers[2].label, "Record Take 001");
	EXPECT_EQ(markers[3].chunk, 5);
	EXPECT_EQ(markers[3].sample, 8);
	EXPECT_EQ(markers[3].label, "");
	EXPECT_EQ(markers[4].chunk, 5);
	EXPECT_EQ(markers[4].sample, 7);
	EXPECT_EQ(markers[4].label, "L2");

	// no description at all: no marker
	BankSpec plain;
	plain.segments = {{0}, {0}};
	const TempFile plainFile {WriteBank("no_markers", plain)};
	const auto plainBank = RegisterMusicBank(plainFile.path);
	ASSERT_NE(plainBank, nullptr);
	EXPECT_TRUE(plainBank->ParseMarkers().empty());
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(MusicBank, EffectBankIsNotMusic)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	const auto bank = RegisterMusicBank(*FindNoCase(*root, SfxBankPath(SfxBank::InGame)));
	ASSERT_NE(bank, nullptr);
	EXPECT_FALSE(bank->IsMusic());
	EXPECT_EQ(bank->GetGroupId(), -1);
	EXPECT_FLOAT_EQ(bank->GetMinDistance(), -1.0f);
	EXPECT_FLOAT_EQ(bank->GetMaxDistance(), -1.0f);
	EXPECT_EQ(bank->GetSegmentCount(), 210u);
}

TEST(MusicBank, EffectBankIsNotMusicSynthetic)
{
	// The bank info's music flag is 0: no group and no distances, even with them set in the first record
	BankSpec spec;
	spec.musicFlag = 0;
	spec.segments = {{1}, {2}, {3}};
	spec.group = 5;
	spec.minDistance = 30.0f;
	spec.maxDistance = 120.0f;
	const TempFile file {WriteBank("effects", spec)};
	const auto bank = RegisterMusicBank(file.path);
	ASSERT_NE(bank, nullptr);
	EXPECT_FALSE(bank->IsMusic());
	EXPECT_EQ(bank->GetGroupId(), -1);
	EXPECT_FLOAT_EQ(bank->GetMinDistance(), -1.0f);
	EXPECT_FLOAT_EQ(bank->GetMaxDistance(), -1.0f);
	EXPECT_EQ(bank->GetSegmentCount(), 3u);
}
