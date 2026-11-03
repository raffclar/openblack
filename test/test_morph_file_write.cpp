/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Morph files are written back as they were read. The files here are made up in the game's layout, so that no game data
// is needed: the test builds a file block by block, reads it and checks that Write gives the same bytes.

#include <cstdint>
#include <cstring>

#include <array>
#include <fstream>
#include <string>
#include <vector>

#include <MorphFile.h>
#include <gtest/gtest.h>

#include "TestTempFolder.h"

using namespace openblack;
using namespace openblack::morph;

namespace
{
class Bytes
{
public:
	template <typename T>
	void Put(T value)
	{
		const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
		_data.insert(_data.end(), bytes, bytes + sizeof(T));
	}
	/// A run of numbers that differ from each other, so that a field written in the wrong place shows
	void Numbers(size_t count)
	{
		for (size_t i = 0; i < count; ++i)
		{
			Put(static_cast<int32_t>(1000 + (++_counter)));
		}
	}
	void Text(std::string_view text, size_t size)
	{
		std::string padded(text);
		padded.resize(size, '\0');
		_data.insert(_data.end(), padded.begin(), padded.end());
	}
	void Patch(size_t at, uint32_t value) { std::memcpy(_data.data() + at, &value, sizeof(value)); }
	[[nodiscard]] size_t Size() const { return _data.size(); }
	[[nodiscard]] const std::vector<uint8_t>& Data() const { return _data; }

private:
	std::vector<uint8_t> _data;
	int32_t _counter {0};
};

/// A spec file of two animations, a cycle then a pose range, in one set
constexpr std::string_view k_Spec = "25\n=standard\nCwalk\nLpose\nE\n";

/// An animation of two frames and one joint of each kind
void PutAnimation(Bytes& bytes)
{
	bytes.Put(uint32_t {800});
	bytes.Put(uint32_t {1});
	bytes.Put(0.25f);
	bytes.Put(2.0f);
	for (const float d : {0.0f, 0.0f, -2.0f})
	{
		bytes.Put(d);
	}
	bytes.Put(uint32_t {2});
	bytes.Put(uint32_t {5});
	bytes.Put(uint32_t {1});
	bytes.Put(uint32_t {1});
	bytes.Put(uint32_t {3});
	bytes.Put(uint32_t {4});
	for (int frame = 0; frame < 2; ++frame)
	{
		for (int i = 0; i < 6; ++i)
		{
			bytes.Put(static_cast<float>(frame) + (static_cast<float>(i) * 0.5f));
		}
	}
}

/// The creature block for a version of it, with a made up value in every field
void PutCreatureBlock(Bytes& bytes, uint32_t version, uint32_t binaryVersion)
{
	if (binaryVersion <= 5)
	{
		bytes.Text("legacy name", 0x20);
	}
	size_t actions = 2 + 1 + 1 + 1 + 5;
	actions += version > 2 ? 2 : 0;
	actions += version > 11 ? 1 : 0;
	actions += version > 4 ? 2 : 0;
	actions += version > 8 ? 2 : 0;
	actions += version > 9 ? 1 : 0;
	actions += version > 15 ? 1 : 0;
	actions += version > 7 ? 1 : 0;
	bytes.Numbers(actions);
	if (version >= 14)
	{
		if (version > 7)
		{
			bytes.Put(uint32_t {1});
			bytes.Numbers(9);
		}
		if (version > 17)
		{
			bytes.Put(uint32_t {0});
		}
		// The eyes: scale, four points of a triangle, an enabled flag and a depth, then the lid angles
		bytes.Put(0.5f);
		for (uint32_t point = 0; point < 4; ++point)
		{
			bytes.Numbers(9);
			bytes.Put(point % 2 == 0 ? uint32_t {1} : uint32_t {0});
			bytes.Put(0.125f);
		}
		bytes.Numbers(9);
		if (version >= 15)
		{
			if (version > 18)
			{
				bytes.Text("TigerBank", 0x20);
			}
			bytes.Numbers(version < 11 ? 1 : 7);
			bytes.Numbers(12 * (version > 12 ? 3 : 2));
			for (uint32_t site = 0; site < 8; ++site)
			{
				if (site != 0 && site != 5)
				{
					bytes.Put(uint32_t {0});
					continue;
				}
				bytes.Put(uint32_t {1});
				// The third byte holds the skin in its top two bits and other bits that are kept
				bytes.Put(
				    std::array<uint8_t, 4> {static_cast<uint8_t>(10 + site), static_cast<uint8_t>(20 + site), 0x95, 0x5A});
				bytes.Put(0.75f);
				if (version > 16)
				{
					bytes.Put(uint32_t {site == 0 ? 1u : 0u});
					bytes.Put(uint32_t {site == 0 ? 0x105u : 2u});
				}
			}
		}
	}
	// The numbers at the end of the block
	bytes.Numbers(6);
}

/// A made up morph file: two animations in the spec file, the cycle in the base set and the pose range in the evil
/// variant, one hair group and the moments of the cycle
std::vector<uint8_t> MakeFile(uint32_t creatureVersion, uint32_t binaryVersion)
{
	Bytes bytes;
	bytes.Put(creatureVersion);
	bytes.Put(uint32_t {25});
	bytes.Put(binaryVersion);
	bytes.Text("Base_Mesh", 0x20);
	bytes.Text("Evil_Mesh", 0x20);
	for (int i = 0; i < 5; ++i)
	{
		bytes.Text("", 0x20);
	}
	// Base set: offsets of the two animations, then the offset of the next block
	const auto baseOffsets = bytes.Size();
	bytes.Put(uint32_t {0});
	bytes.Put(uint32_t {0});
	bytes.Put(uint32_t {0});
	bytes.Patch(baseOffsets, static_cast<uint32_t>(bytes.Size()));
	PutAnimation(bytes);
	bytes.Patch(baseOffsets + 8, static_cast<uint32_t>(bytes.Size()));
	// The evil variant's set has the pose range only
	const auto variantOffsets = bytes.Size();
	bytes.Put(uint32_t {0});
	bytes.Put(uint32_t {0});
	bytes.Put(uint32_t {0});
	bytes.Patch(variantOffsets + 4, static_cast<uint32_t>(bytes.Size()));
	PutAnimation(bytes);
	bytes.Patch(variantOffsets + 8, static_cast<uint32_t>(bytes.Size()));
	// Hair: the sound object and one group of one hair
	bytes.Put(uint32_t {3});
	bytes.Put(uint32_t {1});
	bytes.Put(uint32_t {2});
	bytes.Put(uint32_t {1});
	bytes.Put(uint32_t {4});
	bytes.Put(uint32_t {1});
	for (int variant = 0; variant < 3; ++variant)
	{
		bytes.Put(int32_t {200 + variant});
		bytes.Put(int32_t {100});
		bytes.Put(int32_t {50});
		for (const float value : {1.5f, 0.9f, 0.1f, 0.02f})
		{
			bytes.Put(value);
		}
	}
	bytes.Put(uint32_t {1});
	bytes.Numbers(9);
	for (int i = 0; i < 9; ++i)
	{
		bytes.Put(0.25f * static_cast<float>(i));
	}
	// What happens at moments of the cycle: two entries, then the end of the list
	for (uint32_t i = 0; i < 2; ++i)
	{
		bytes.Put(uint32_t {1});
		bytes.Put(i);
		bytes.Put(uint32_t {100 * (i + 1)});
		bytes.Put(uint32_t {7 + i});
		bytes.Put(uint32_t {0});
	}
	bytes.Put(uint32_t {0});
	if (creatureVersion != 0)
	{
		PutCreatureBlock(bytes, creatureVersion, binaryVersion);
	}
	return bytes.Data();
}

/// Reads the made up file and writes it back
void ExpectRoundTrip(uint32_t creatureVersion, uint32_t binaryVersion)
{
	const auto original = MakeFile(creatureVersion, binaryVersion);
	openblack::test::TestTempFolder folder("morph_write_" + std::to_string(creatureVersion) + "_" +
	                                       std::to_string(binaryVersion));
	const auto specName = (creatureVersion != 0 ? "ctrspec25.txt" : "hndspec25.txt");
	{
		std::ofstream spec(folder.Path() / specName);
		spec << k_Spec;
	}
	MorphFile file;
	ASSERT_EQ(file.Open(original, folder.Path()), MorphResult::Success);
	EXPECT_EQ(file.Write(), original) << "version " << creatureVersion << ", binary version " << binaryVersion;
}
} // namespace

TEST(MorphFileWrite, AHandFileWritesBackByteForByte)
{
	ExpectRoundTrip(0, 6);
}

TEST(MorphFileWrite, ACreatureFileWithoutEyesWritesBackByteForByte)
{
	ExpectRoundTrip(12, 6);
}

TEST(MorphFileWrite, ACreatureFileWithEyesButNoTattoosWritesBackByteForByte)
{
	ExpectRoundTrip(14, 6);
}

TEST(MorphFileWrite, CreatureFilesWithTattoosWriteBackByteForByte)
{
	ExpectRoundTrip(15, 6);
	ExpectRoundTrip(16, 6);
	ExpectRoundTrip(17, 6);
	ExpectRoundTrip(19, 6);
}

TEST(MorphFileWrite, ACreatureFileWithALegacyNameWritesBackByteForByte)
{
	ExpectRoundTrip(19, 5);
}

TEST(MorphFileWrite, ChangedTattooSitesAreWritten)
{
	const auto original = MakeFile(19, 6);
	openblack::test::TestTempFolder folder;
	{
		std::ofstream spec(folder.Path() / "ctrspec25.txt");
		spec << k_Spec;
	}
	MorphFile file;
	ASSERT_EQ(file.Open(original, folder.Path()), MorphResult::Success);
	ASSERT_TRUE(file.GetTattooSites().has_value());
	const auto& site = (*file.GetTattooSites())[0];
	EXPECT_TRUE(site.enabled);
	EXPECT_EQ(site.u, 10);
	EXPECT_EQ(site.v, 20);
	EXPECT_EQ(site.skin, 2);
	EXPECT_TRUE(site.mirror);
	// Only the low two bits of the stored turn count
	EXPECT_EQ(site.rotation, 1u);
	EXPECT_EQ(file.GetSoundBankName(), "TigerBank");
	ASSERT_EQ(file.GetHairGroups().size(), 1u);
	EXPECT_EQ(file.GetExtraData().front().size(), 2u);
	EXPECT_EQ(file.GetBaseAnimationSet().size(), 1u);
	EXPECT_EQ(file.GetVariantAnimationSet(0).size(), 1u);
	EXPECT_NE(file.GetVariantAnimation(0, 1), nullptr);
	EXPECT_EQ(file.GetVariantAnimation(0, 0), nullptr);
}
