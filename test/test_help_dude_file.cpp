/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// help::HelpDudeFile (src/Help/HelpDudeFile.h) against Data\HelpSprite\markgood.hd / markevil.hd: the loader,
// the clips and their binary records; and the clip samplers (src/Help/SpiritAnimClip.h) against values of the
// original's samplers run under an x86 emulator.
// The files come from $OPENBLACK_TEST_BW_ROOT (the game root); the tests skip without it.
// The *Synthetic tests read a small .hd file built in memory and need no game data.

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Help/HelpDudeFile.h"
#include "Help/SpiritAnimClip.h"

using namespace openblack;

namespace
{
/// $OPENBLACK_TEST_BW_ROOT (the tree's convention, test_anim_effects.cpp); nullopt when unset: the tests skip
std::optional<std::filesystem::path> GameRoot()
{
	const char* root = std::getenv("OPENBLACK_TEST_BW_ROOT");
	if (root == nullptr || *root == '\0' || !std::filesystem::is_directory(root))
	{
		return std::nullopt;
	}
	return std::filesystem::path(root);
}

/// The root for the skip messages
std::string RootName()
{
	const auto root = GameRoot();
	return root ? root->string() : std::string("(OPENBLACK_TEST_BW_ROOT not set)");
}

std::optional<help::HelpDudeFile> LoadDude(const char* name, std::vector<uint8_t>* rawOut = nullptr)
{
	const auto root = GameRoot();
	if (!root)
	{
		return std::nullopt;
	}
	const auto path = *root / "Data" / "HelpSprite" / name;
	std::ifstream stream(path, std::ios::binary);
	if (!stream.is_open())
	{
		return std::nullopt;
	}
	std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	help::HelpDudeFile file;
	std::string error;
	EXPECT_TRUE(help::LoadHelpDudeFile(bytes, file, &error)) << error;
	if (rawOut != nullptr)
	{
		*rawOut = bytes;
	}
	return file;
}

struct Expected
{
	const char* file;
	uint32_t bones;
	float restHeight;
	const char* meshName;
	size_t meshSize;
	size_t fileSize;
	std::vector<size_t> emptySlots;
	float farDepth;
	float modelScale;
	float haloScale;
	glm::vec3 haloOffset;
};

const Expected k_Good {"markgood.hd",
                       97,
                       63.42295f,
                       "DATA\\Yogi_Mesh.l3d",
                       169686,
                       910358,
                       {4, 26, 41, 48, 59, 60, 61, 62, 73, 74, 75, 76, 77, 78, 79},
                       6.0266666f,
                       0.02552f,
                       0.47333333f,
                       {0.0f, -0.28f, 0.0f}};
const Expected k_Evil {"markevil.hd",
                       73,
                       79.15389f,
                       "C:\\dev\\TESTBED\\DATA\\Demon_Mesh.l3d",
                       163700,
                       776556,
                       {4, 10, 25, 26, 36, 50, 51, 73, 74, 75, 76, 77, 78, 79},
                       5.64f,
                       0.026826667f,
                       0.0f,
                       {0.0f, 0.0f, 0.0f}};

void CheckFile(const Expected& expected)
{
	std::vector<uint8_t> raw;
	const auto file = LoadDude(expected.file, &raw);
	if (!file)
	{
		GTEST_SKIP() << expected.file << " not found under " << RootName();
	}
	// exact parse: the segment read to its last byte, and the segment is the whole file after its 44-byte header
	EXPECT_EQ(raw.size(), expected.fileSize);
	EXPECT_EQ(file->segmentSize + 44, raw.size());
	EXPECT_EQ(file->bytesRead, file->segmentSize);

	EXPECT_EQ(file->boneCount, expected.bones);
	EXPECT_EQ(file->animCount, 80u);
	EXPECT_EQ(file->clipCount, 80u);
	EXPECT_NEAR(file->restHeight, expected.restHeight, 1e-4f);
	EXPECT_EQ(file->meshName, expected.meshName);
	EXPECT_EQ(file->hasData, 1u);
	ASSERT_EQ(file->mesh.size(), expected.meshSize);
	EXPECT_EQ(std::string(file->mesh.begin(), file->mesh.begin() + 4), "L3D0");

	ASSERT_EQ(file->animNames.size(), 80u);
	ASSERT_EQ(file->clips.size(), 80u);
	std::vector<size_t> empty;
	for (size_t i = 0; i < 80; ++i)
	{
		EXPECT_EQ(file->animNames[i].empty(), !file->clips[i].has_value()) << i;
		if (!file->clips[i])
		{
			empty.push_back(i);
			continue;
		}
		const auto& clip = *file->clips[i];
		EXPECT_EQ(file->clipRecordSizes[i], help::SpiritAnimClipByteSize(clip) + 4) << i;
		EXPECT_EQ(clip.boneCount, expected.bones) << i;
		EXPECT_EQ(clip.frames.size(), clip.frameCount) << i;
		EXPECT_GT(clip.durationMs, 0) << i;
		for (size_t c = 0; c < clip.rotationBones.size(); ++c)
		{
			EXPECT_LT(clip.rotationBones[c], expected.bones);
			EXPECT_TRUE(c == 0 || clip.rotationBones[c - 1] < clip.rotationBones[c]);
		}
		for (size_t c = 0; c < clip.positionBones.size(); ++c)
		{
			EXPECT_LT(clip.positionBones[c], expected.bones);
			EXPECT_TRUE(c == 0 || clip.positionBones[c - 1] < clip.positionBones[c]);
		}
	}
	EXPECT_EQ(empty, expected.emptySlots);
	EXPECT_EQ(80 - empty.size(), expected.file == k_Good.file ? 65u : 66u);

	// the stand clip has a channel of each kind for every bone (the pose fill indexes its key 0 by bone)
	const auto* stand = file->Clip(0);
	ASSERT_NE(stand, nullptr);
	EXPECT_EQ(stand->rotationBones.size(), expected.bones);
	EXPECT_EQ(stand->positionBones.size(), expected.bones);

	EXPECT_NEAR(file->nearDepth, 8.7333333f, 1e-5f);
	EXPECT_NEAR(file->farDepth, expected.farDepth, 1e-5f);
	EXPECT_NEAR(file->modelScale, expected.modelScale, 1e-6f);
	EXPECT_NEAR(file->haloScale, expected.haloScale, 1e-6f);
	EXPECT_NEAR(file->haloOffset.x, expected.haloOffset.x, 1e-6f);
	EXPECT_NEAR(file->haloOffset.y, expected.haloOffset.y, 1e-6f);
	EXPECT_NEAR(file->haloOffset.z, expected.haloOffset.z, 1e-6f);
	EXPECT_EQ(file->emotionFaces.size(), 0x200u);
	EXPECT_EQ(file->animEvents.size(), 80u);
	EXPECT_EQ(file->words2EE8[0], 1u);

	// the rest skeleton of the embedded L3D0, through l3d::L3DFile from memory
	help::RestSkeleton rest;
	std::string error;
	ASSERT_TRUE(help::LoadRestSkeleton(*file, rest, &error)) << error;
	EXPECT_EQ(rest.parents.size(), expected.bones);
	// the skeleton's height, which the original writes over the file's rest height
	EXPECT_NEAR(rest.height, file->restHeight, 1e-3f);
}

/// local = rest; SetPose(stand, standMs, fill = stand key 0); the layers; ComposeWorld under the identity
std::vector<glm::mat4> Pose(const help::HelpDudeFile& file, const help::RestSkeleton& rest, int32_t standMs,
                            const std::vector<std::tuple<size_t, int32_t, bool>>& layers)
{
	const auto& stand = *file.Clip(0);
	auto local = rest.local;
	help::SetPose(stand, standMs, rest, &stand.frames[0], local);
	for (const auto& [index, ms, middleReference] : layers)
	{
		const auto& clip = *file.Clip(index);
		const auto& reference = clip.frames[middleReference ? clip.frames.size() / 2 : 0];
		help::ApplyAdditive(clip, ms, reference, rest, local);
	}
	std::vector<glm::mat4> world;
	help::ComposeWorld(rest, local, glm::mat4(1.0f), world);
	return world;
}

struct GoldenBone
{
	size_t bone;
	glm::vec3 position; ///< LH row 3 = glm column 3
	glm::vec3 row0;     ///< LH row 0 = glm column 0
};

void CheckGolden(const std::vector<glm::mat4>& world, const std::vector<GoldenBone>& golden)
{
	for (const auto& g : golden)
	{
		ASSERT_LT(g.bone, world.size());
		const auto& m = world[g.bone];
		for (int k = 0; k < 3; ++k)
		{
			EXPECT_NEAR(m[3][k], g.position[k], 2e-3f) << "bone " << g.bone << " position " << k;
			EXPECT_NEAR(m[0][k], g.row0[k], 1e-3f) << "bone " << g.bone << " row0 " << k;
		}
	}
}
} // namespace

namespace
{
/// Little-endian writes for the synthetic .hd segment
struct SegmentWriter
{
	std::vector<uint8_t> bytes;

	template <typename T>
	void Value(const T& value)
	{
		const auto* raw = reinterpret_cast<const uint8_t*>(&value);
		bytes.insert(bytes.end(), raw, raw + sizeof(T));
	}
	void U32(uint32_t value) { Value(value); }
	void F32(float value) { Value(value); }
	void Vec3(const glm::vec3& value)
	{
		F32(value.x);
		F32(value.y);
		F32(value.z);
	}
	void Name(const std::string& name)
	{
		std::array<char, 0x80> field {};
		std::memcpy(field.data(), name.data(), std::min(name.size(), field.size() - 1));
		bytes.insert(bytes.end(), field.begin(), field.end());
	}
};

/// A clip's record: its size (counting itself) then the clip, keys whose values depend on the key and the channel
struct SyntheticClip
{
	uint32_t durationMs;
	uint32_t loopWord;
	glm::vec3 displacement;
	uint32_t frames;
	std::vector<uint32_t> rotationBones;
	std::vector<uint32_t> positionBones;
};

constexpr uint32_t k_SyntheticBones = 3;

glm::vec3 SyntheticRotation(uint32_t key, size_t channel)
{
	return {static_cast<float>(key), static_cast<float>(channel), 0.5f};
}

glm::vec3 SyntheticPosition(uint32_t key, size_t channel)
{
	return {static_cast<float>(key * 10 + channel), 1.0f, 2.0f};
}

void WriteClip(SegmentWriter& out, const SyntheticClip& clip)
{
	const size_t channels = clip.rotationBones.size() + clip.positionBones.size();
	// eleven header words, the channel tables, then 12 bytes per channel and key; and the size word itself
	out.U32(static_cast<uint32_t>(4 + 11 * 4 + 4 * channels + 12 * channels * clip.frames));
	out.U32(clip.durationMs);
	out.U32(clip.loopWord);
	out.F32(glm::length(clip.displacement) / static_cast<float>(clip.durationMs)); // speed
	out.F32(glm::length(clip.displacement));                                       // distance
	out.Vec3(clip.displacement);
	out.U32(clip.frames);
	out.U32(k_SyntheticBones);
	out.U32(static_cast<uint32_t>(clip.rotationBones.size()));
	out.U32(static_cast<uint32_t>(clip.positionBones.size()));
	for (const auto bone : clip.rotationBones)
	{
		out.U32(bone);
	}
	for (const auto bone : clip.positionBones)
	{
		out.U32(bone);
	}
	for (uint32_t key = 0; key < clip.frames; ++key)
	{
		for (size_t c = 0; c < clip.rotationBones.size(); ++c)
		{
			out.Vec3(SyntheticRotation(key, c));
		}
		for (size_t c = 0; c < clip.positionBones.size(); ++c)
		{
			out.Vec3(SyntheticPosition(key, c));
		}
	}
}

const SyntheticClip k_StandClip {1000, 1, {0.0f, 0.0f, 2.0f}, 2, {0, 1, 2}, {0, 1, 2}};
const SyntheticClip k_WaveClip {500, 0, {0.0f, 0.0f, 0.0f}, 3, {1}, {0, 2}};

/// A whole .hd file: 4 anim slots ("Stand" with a clip, an empty name, "Wave" with a clip, "Spare" with a record size
/// of 0), an 8-byte mesh, 3 event entries and the tail
std::vector<uint8_t> MakeSyntheticDude()
{
	SegmentWriter s;
	s.U32(k_SyntheticBones);
	s.U32(4);     // anim names
	s.F32(12.5f); // rest height
	s.Name("DATA\\Synthetic_Mesh.l3d");
	s.Name("Stand");
	s.Name("");
	s.Name("Wave");
	s.Name("Spare");
	s.U32(1); // has data
	s.U32(8); // mesh size
	for (const char c : std::string("L3D0"))
	{
		s.Value(static_cast<uint8_t>(c));
	}
	s.U32(0);
	s.U32(4); // clip slots
	WriteClip(s, k_StandClip);
	// slot 1: an empty name, no record at all
	WriteClip(s, k_WaveClip);
	s.U32(0); // slot 3: a record size of 0, no clip
	for (int i = 0; i < 0x1C; ++i)
	{
		s.Value(static_cast<uint8_t>(i));
	}
	s.U32(1);
	s.F32(0.25f);
	s.F32(0.5f);
	s.F32(0.75f);
	s.U32(0);     // starting emotion
	s.U32(0x200); // face records
	for (int i = 0; i < 0x200; ++i)
	{
		s.Value(static_cast<uint8_t>(i & 0xFF));
	}
	s.F32(8.5f);   // near depth
	s.F32(0.025f); // model scale
	s.F32(0.0f);
	s.U32(3); // event entries
	// entry 0: no events
	s.U32(0);
	s.F32(0.125f);
	s.F32(0.375f);
	// entry 1: two events of sample 155
	s.U32(2);
	s.U32(155);
	s.U32(1);
	s.F32(0.25f);
	s.U32(155);
	s.U32(1);
	s.F32(0.5f);
	s.F32(0.0f);
	s.F32(0.0f);
	// entry 2: one event of sample 154
	s.U32(1);
	s.U32(154);
	s.U32(1);
	s.F32(0.75f);
	s.F32(0.25f);
	s.F32(0.625f);
	s.F32(0.25f); // the two pending values
	s.F32(0.0f);
	s.F32(6.0f); // far depth
	for (size_t i = 0; i < help::HelpDudeFile::k_AnimSlots; ++i)
	{
		s.Value(static_cast<uint8_t>(i == 2 ? 0x20 : 0));
	}
	s.F32(0.5f); // halo scale
	s.Vec3({0.0f, -0.25f, 0.0f});

	// the LiOnHeAd container with the one "helpdude" segment
	std::vector<uint8_t> file {'L', 'i', 'O', 'n', 'H', 'e', 'A', 'd'};
	std::array<char, 32> blockName {};
	std::strncpy(blockName.data(), "helpdude", blockName.size() - 1);
	file.insert(file.end(), blockName.begin(), blockName.end());
	const auto size = static_cast<uint32_t>(s.bytes.size());
	const auto* sizeBytes = reinterpret_cast<const uint8_t*>(&size);
	file.insert(file.end(), sizeBytes, sizeBytes + sizeof(size));
	file.insert(file.end(), s.bytes.begin(), s.bytes.end());
	return file;
}

void CheckSyntheticClip(const help::SpiritAnimClip& clip, const SyntheticClip& expected)
{
	EXPECT_EQ(clip.durationMs, static_cast<int32_t>(expected.durationMs));
	EXPECT_EQ(clip.looping, (expected.loopWord & 1u) != 0);
	EXPECT_FLOAT_EQ(clip.distance, glm::length(expected.displacement));
	EXPECT_EQ(clip.displacement, expected.displacement);
	EXPECT_EQ(clip.frameCount, expected.frames);
	EXPECT_EQ(clip.boneCount, k_SyntheticBones);
	EXPECT_EQ(clip.rotationBones, expected.rotationBones);
	EXPECT_EQ(clip.positionBones, expected.positionBones);
	ASSERT_EQ(clip.frames.size(), expected.frames);
	for (uint32_t key = 0; key < expected.frames; ++key)
	{
		ASSERT_EQ(clip.frames[key].rotations.size(), expected.rotationBones.size());
		ASSERT_EQ(clip.frames[key].positions.size(), expected.positionBones.size());
		for (size_t c = 0; c < expected.rotationBones.size(); ++c)
		{
			EXPECT_EQ(clip.frames[key].rotations[c], SyntheticRotation(key, c)) << key << " " << c;
		}
		for (size_t c = 0; c < expected.positionBones.size(); ++c)
		{
			EXPECT_EQ(clip.frames[key].positions[c], SyntheticPosition(key, c)) << key << " " << c;
		}
	}
}
} // namespace

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(HelpDudeFile, MarkGood)
{
	CheckFile(k_Good);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(HelpDudeFile, MarkEvil)
{
	CheckFile(k_Evil);
}

TEST(HelpDudeFile, MarkGoodSynthetic)
{
	const auto raw = MakeSyntheticDude();
	help::HelpDudeFile file;
	std::string error;
	ASSERT_TRUE(help::LoadHelpDudeFile(raw, file, &error)) << error;
	// exact parse: the segment read to its last byte, and the segment is the whole file after its 44-byte header
	EXPECT_EQ(file.segmentSize + 44, raw.size());
	EXPECT_EQ(file.bytesRead, file.segmentSize);

	EXPECT_EQ(file.boneCount, k_SyntheticBones);
	EXPECT_EQ(file.animCount, 4u);
	EXPECT_EQ(file.clipCount, 4u);
	EXPECT_FLOAT_EQ(file.restHeight, 12.5f);
	EXPECT_EQ(file.meshName, "DATA\\Synthetic_Mesh.l3d");
	EXPECT_EQ(file.hasData, 1u);
	ASSERT_EQ(file.mesh.size(), 8u);
	EXPECT_EQ(std::string(file.mesh.begin(), file.mesh.begin() + 4), "L3D0");

	// every table keeps its 80 slots, the names past the count empty
	ASSERT_EQ(file.animNames.size(), help::HelpDudeFile::k_AnimSlots);
	ASSERT_EQ(file.clips.size(), help::HelpDudeFile::k_AnimSlots);
	ASSERT_EQ(file.clipRecordSizes.size(), help::HelpDudeFile::k_AnimSlots);
	EXPECT_EQ(file.animNames[0], "Stand");
	EXPECT_TRUE(file.animNames[1].empty());
	EXPECT_EQ(file.animNames[2], "Wave");
	EXPECT_EQ(file.animNames[3], "Spare");
	EXPECT_TRUE(file.animNames[4].empty());

	// the record sizes count themselves: 4 + 0x2C + 4 per channel + 12 per channel and key
	ASSERT_NE(file.Clip(0), nullptr);
	EXPECT_EQ(file.clipRecordSizes[0], 4u + 0x2Cu + 4u * 6u + 12u * 6u * 2u);
	EXPECT_EQ(file.clipRecordSizes[0], help::SpiritAnimClipByteSize(*file.Clip(0)) + 4);
	CheckSyntheticClip(*file.Clip(0), k_StandClip);
	// an empty name has no record and no clip
	EXPECT_EQ(file.Clip(1), nullptr);
	EXPECT_EQ(file.clipRecordSizes[1], 0u);
	ASSERT_NE(file.Clip(2), nullptr);
	EXPECT_EQ(file.clipRecordSizes[2], 4u + 0x2Cu + 4u * 3u + 12u * 3u * 3u);
	EXPECT_EQ(file.clipRecordSizes[2], help::SpiritAnimClipByteSize(*file.Clip(2)) + 4);
	CheckSyntheticClip(*file.Clip(2), k_WaveClip);
	// a record size of 0 has no clip
	EXPECT_EQ(file.Clip(3), nullptr);
	EXPECT_EQ(file.Clip(4), nullptr);

	EXPECT_EQ(file.faceBoneIndices[0], 0u);
	EXPECT_EQ(file.faceBoneIndices[0x1B], 0x1Bu);
	EXPECT_EQ(file.words2EE8[0], 1u);
	EXPECT_EQ(file.startEmotion, 0u);
	ASSERT_EQ(file.emotionFaces.size(), 0x200u);
	EXPECT_EQ(file.emotionFaces[0x1FF], 0xFFu);
	EXPECT_FLOAT_EQ(file.nearDepth, 8.5f);
	EXPECT_FLOAT_EQ(file.modelScale, 0.025f);
	EXPECT_EQ(file.animEvents.size(), 3u);
	EXPECT_FLOAT_EQ(file.value35C4, 0.25f);
	EXPECT_FLOAT_EQ(file.farDepth, 6.0f);
	EXPECT_EQ(file.animFlags[2], 0x20u);
	EXPECT_EQ(file.animFlags[0], 0u);
	EXPECT_FLOAT_EQ(file.haloScale, 0.5f);
	EXPECT_EQ(file.haloOffset, glm::vec3(0.0f, -0.25f, 0.0f));

	// the same segment four bytes short (the halo's z missing) fails instead of reading past the end
	auto cut = raw;
	cut.resize(raw.size() - 4);
	const auto size = static_cast<uint32_t>(cut.size() - 44);
	std::memcpy(cut.data() + 8 + 32, &size, sizeof(size));
	help::HelpDudeFile shorter;
	EXPECT_FALSE(help::LoadHelpDudeFile(cut, shorter, &error));
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(HelpDudeFile, Events)
{
	const auto good = LoadDude("markgood.hd");
	const auto evil = LoadDude("markevil.hd");
	if (!good || !evil)
	{
		GTEST_SKIP() << "HelpSprite files not found under " << RootName();
	}
	// KnockScreen (58): InGame 155 twice for good, seven times for evil; HandGun (60): evil 154 at 0.48
	ASSERT_EQ(good->animEvents[58].events.size(), 2u);
	EXPECT_EQ(good->animEvents[58].events[0].sample, 155u);
	EXPECT_NEAR(good->animEvents[58].events[0].phase, 0.34053656f, 1e-6f);
	EXPECT_NEAR(good->animEvents[58].events[1].phase, 0.36297652f, 1e-6f);
	EXPECT_EQ(evil->animEvents[58].events.size(), 7u);
	ASSERT_EQ(evil->animEvents[60].events.size(), 1u);
	EXPECT_EQ(evil->animEvents[60].events[0].sample, 154u);
	EXPECT_NEAR(evil->animEvents[60].events[0].phase, 0.48095238f, 1e-6f);
	// the loop window of the nod (20)
	EXPECT_NEAR(good->animEvents[20].loopStart, 0.25238097f, 1e-6f);
	EXPECT_NEAR(good->animEvents[20].loopEnd, 0.5714286f, 1e-6f);
}

TEST(HelpDudeFile, EventsSynthetic)
{
	help::HelpDudeFile file;
	std::string error;
	ASSERT_TRUE(help::LoadHelpDudeFile(MakeSyntheticDude(), file, &error)) << error;
	ASSERT_EQ(file.animEvents.size(), 3u);
	// an entry without events still has its loop window
	EXPECT_TRUE(file.animEvents[0].events.empty());
	EXPECT_FLOAT_EQ(file.animEvents[0].loopStart, 0.125f);
	EXPECT_FLOAT_EQ(file.animEvents[0].loopEnd, 0.375f);
	// the events in the file's order: sample, flag, phase
	ASSERT_EQ(file.animEvents[1].events.size(), 2u);
	EXPECT_EQ(file.animEvents[1].events[0].sample, 155u);
	EXPECT_EQ(file.animEvents[1].events[0].flag, 1u);
	EXPECT_FLOAT_EQ(file.animEvents[1].events[0].phase, 0.25f);
	EXPECT_FLOAT_EQ(file.animEvents[1].events[1].phase, 0.5f);
	ASSERT_EQ(file.animEvents[2].events.size(), 1u);
	EXPECT_EQ(file.animEvents[2].events[0].sample, 154u);
	EXPECT_FLOAT_EQ(file.animEvents[2].events[0].phase, 0.75f);
	EXPECT_FLOAT_EQ(file.animEvents[2].loopStart, 0.25f);
	EXPECT_FLOAT_EQ(file.animEvents[2].loopEnd, 0.625f);
}

TEST(SpiritAnimClip, Keys)
{
	help::SpiritAnimClip loop;
	loop.durationMs = 1000;
	loop.looping = true;
	loop.frameCount = 10;
	loop.frames.resize(10);
	// a looping clip: 10 keys over 1000 ms, the last one wraps to key 0
	auto keys = help::SampleKeys(loop, 950, false);
	EXPECT_EQ(keys.key0, 9u);
	EXPECT_EQ(keys.key1, 0u);
	EXPECT_NEAR(keys.fraction, 0.5f, 1e-6f);
	// a one-shot clip: period 1000 * 10 / 9 = 1111 (integer), key 9 at the end, key1 still wraps to 0
	auto once = loop;
	once.looping = false;
	keys = help::SampleKeys(once, 1000, true);
	EXPECT_EQ(keys.key0, 9u);
	EXPECT_EQ(keys.key1, 0u);
	EXPECT_NEAR(keys.fraction, 10.0f / 1111.0f * 1000.0f - 9.0f, 1e-5f);
	keys = help::SampleKeys(once, 5000, true);
	EXPECT_EQ(keys.key0, 9u);
}

TEST(SpiritAnimClip, ApplyAnimArguments)
{
	help::SpiritAnimClip clip;
	clip.durationMs = 8066;
	clip.frameCount = 72;
	clip.frames.resize(72);
	clip.displacement = {1.0f, 2.0f, 3.0f};
	auto args = help::ApplyAnimArguments(clip, 1.3f, 0.5f, true);
	EXPECT_NEAR(args.phase, 0.3f, 1e-5f);
	EXPECT_EQ(args.milliseconds, static_cast<int32_t>(8066.0f * args.phase));
	EXPECT_EQ(args.referenceKey, 35u);
	EXPECT_NEAR(args.rootMove.z, 3.0f * args.phase, 1e-5f);
	args = help::ApplyAnimArguments(clip, 1.3f, 0.0f, false);
	EXPECT_EQ(args.phase, 1.0f);
	EXPECT_EQ(args.milliseconds, 8065);
	EXPECT_EQ(args.referenceKey, 0u);
	args = help::ApplyAnimArguments(clip, -0.25f, 0.0f, false);
	EXPECT_EQ(args.phase, 0.0f);
	EXPECT_EQ(args.milliseconds, 0);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(SpiritAnimClip, SampledPoses)
{
	const auto good = LoadDude("markgood.hd");
	const auto evil = LoadDude("markevil.hd");
	if (!good || !evil)
	{
		GTEST_SKIP() << "HelpSprite files not found under " << RootName();
	}
	help::RestSkeleton goodRest;
	help::RestSkeleton evilRest;
	std::string error;
	ASSERT_TRUE(help::LoadRestSkeleton(*good, goodRest, &error)) << error;
	ASSERT_TRUE(help::LoadRestSkeleton(*evil, evilRest, &error)) << error;

	// sanity: the good stand clip is the rest pose (its angles are below 2e-7, its positions the L3D's)
	std::vector<glm::mat4> local;
	help::SampleLocal(*good->Clip(0), 0.0f, goodRest, local);
	std::vector<glm::mat4> world;
	help::ComposeWorld(goodRest, local, glm::mat4(1.0f), world);
	ASSERT_EQ(world.size(), goodRest.world.size());
	for (size_t b = 0; b < world.size(); ++b)
	{
		for (int c = 0; c < 4; ++c)
		{
			for (int r = 0; r < 3; ++r)
			{
				EXPECT_NEAR(world[b][c][r], goodRest.world[b][c][r], 2e-3f) << "bone " << b;
			}
		}
	}
	// every row of a sampled rotation is a unit vector, whatever the clip and time
	for (size_t index : {1u, 24u, 58u, 65u})
	{
		help::SampleLocal(*good->Clip(index), 777.0f, goodRest, local);
		for (const auto& m : local)
		{
			for (int c = 0; c < 3; ++c)
			{
				const glm::vec3 row(m[c]);
				EXPECT_NEAR(std::sqrt(row.x * row.x + row.y * row.y + row.z * row.z), 1.0f, 1e-3f);
			}
		}
	}

	// golden values from the original (emulated): good = stand at 100 ms, KnockOnScreen (58) at 1234 ms and the vowel
	// E (7) at 133 ms, both from their key 0
	CheckGolden(Pose(*good, goodRest, 100, {{58, 1234, false}, {7, 133, false}}),
	            {
	                {0, {0.25844f, -11.67703f, 9.58045f}, {-1e-06f, 0.999989f, 0.0f}},
	                {5, {-0.19207f, 11.70354f, 1.36303f}, {-0.041051f, 0.839877f, -0.541074f}},
	                {48, {0.05562f, 0.12191f, -21.90013f}, {0.997793f, 0.062059f, -0.013403f}},
	                {96, {24.98054f, -18.67578f, -22.01774f}, {0.426977f, -0.263047f, -0.865082f}},
	            });
	// evil = stand at 50 ms, HandGun (60) at 700 ms from key 0, Look L/R (18) at 400 ms from its middle key
	CheckGolden(Pose(*evil, evilRest, 50, {{60, 700, false}, {18, 400, true}}),
	            {
	                {0, {-5.63581f, -18.47319f, 22.14791f}, {0.303281f, -0.462248f, -0.833254f}},
	                {5, {-5.76097f, -8.014f, 22.37665f}, {0.182539f, -0.844113f, -0.504022f}},
	                {36, {11.52593f, 9.25333f, -5.37187f}, {0.733394f, 0.196337f, -0.65061f}},
	                {72, {-22.54125f, -39.05022f, 26.48773f}, {-0.190044f, -0.756841f, -0.625002f}},
	            });
}
