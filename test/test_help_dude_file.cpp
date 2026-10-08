/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>
#include <cstring>

#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <HelpDudeFile.h>
#include <gtest/gtest.h>

using namespace openblack;

namespace
{
/// Little-endian writes for a made-up helpdude block
struct Writer
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
	void Name(const std::string& name)
	{
		std::array<char, helpdude::k_NameLength> field {};
		std::memcpy(field.data(), name.data(), std::min(name.size(), field.size() - 1));
		bytes.insert(bytes.end(), field.begin(), field.end());
	}
};

struct FakeAnimation
{
	uint32_t duration;
	uint32_t looping;
	uint32_t frames;
	std::vector<uint32_t> rotated;
	std::vector<uint32_t> translated;
};

constexpr uint32_t k_Bones = 3;
const FakeAnimation k_Stand {.duration = 1000, .looping = 1, .frames = 2, .rotated = {0, 1, 2}, .translated = {0, 1, 2}};
const FakeAnimation k_Wave {.duration = 500, .looping = 0, .frames = 3, .rotated = {1}, .translated = {0, 2}};

float RotationValue(uint32_t frame, size_t channel)
{
	return static_cast<float>(frame) + static_cast<float>(channel) * 0.25f;
}

void WriteAnimation(Writer& out, const FakeAnimation& animation)
{
	const size_t channels = animation.rotated.size() + animation.translated.size();
	// The record's size counts itself
	out.U32(static_cast<uint32_t>(4 + 0x2C + 4 * channels + 12 * channels * animation.frames));
	out.U32(animation.duration);
	out.U32(animation.looping);
	out.F32(0.0f);
	out.F32(2.0f);
	out.F32(0.0f);
	out.F32(0.0f);
	out.F32(2.0f);
	out.U32(animation.frames);
	out.U32(k_Bones);
	out.U32(static_cast<uint32_t>(animation.rotated.size()));
	out.U32(static_cast<uint32_t>(animation.translated.size()));
	for (const auto bone : animation.rotated)
	{
		out.U32(bone);
	}
	for (const auto bone : animation.translated)
	{
		out.U32(bone);
	}
	for (uint32_t frame = 0; frame < animation.frames; ++frame)
	{
		for (size_t c = 0; c < animation.rotated.size(); ++c)
		{
			out.F32(RotationValue(frame, c));
			out.F32(0.0f);
			out.F32(0.5f);
		}
		for (size_t c = 0; c < animation.translated.size(); ++c)
		{
			out.F32(static_cast<float>(frame * 10 + c));
			out.F32(1.0f);
			out.F32(2.0f);
		}
	}
}

/// Four slots: "Stand" and "Wave" with animations, an empty name, and "Spare" with a record size of 0
std::vector<uint8_t> MakeFile(bool truncateHalo = false)
{
	Writer s;
	s.U32(k_Bones);
	s.U32(4);
	s.F32(12.5f);
	s.Name("DATA\\Fake_Mesh.l3d");
	s.Name("Stand");
	s.Name("");
	s.Name("Wave");
	s.Name("Spare");
	s.U32(1);
	s.U32(8);
	for (const char c : std::string("L3D0"))
	{
		s.Value(static_cast<uint8_t>(c));
	}
	s.U32(0);
	s.U32(4);
	WriteAnimation(s, k_Stand);
	WriteAnimation(s, k_Wave);
	s.U32(0);
	for (int i = 0; i < 0x1C; ++i)
	{
		s.Value(static_cast<uint8_t>(i));
	}
	s.U32(1);
	s.F32(0.25f);
	s.F32(0.5f);
	s.F32(0.75f);
	s.U32(2);
	s.U32(0x200);
	for (int i = 0; i < 0x80; ++i)
	{
		s.F32(static_cast<float>(i));
	}
	s.F32(8.5f);
	s.F32(0.025f);
	s.F32(0.0f);
	s.U32(2);
	s.U32(0);
	s.F32(0.125f);
	s.F32(0.375f);
	s.U32(2);
	s.U32(155);
	s.U32(1);
	s.F32(0.25f);
	s.U32(154);
	s.U32(1);
	s.F32(0.5f);
	s.F32(0.0f);
	s.F32(0.0f);
	s.F32(0.35f);
	s.F32(0.0f);
	s.F32(6.0f);
	for (size_t i = 0; i < helpdude::k_AnimationSlots; ++i)
	{
		s.Value(static_cast<uint8_t>(i == 2 ? 0x20 : 0));
	}
	s.F32(0.5f);
	s.F32(0.0f);
	s.F32(-0.25f);
	if (!truncateHalo)
	{
		s.F32(0.0f);
	}

	std::vector<uint8_t> file {'L', 'i', 'O', 'n', 'H', 'e', 'A', 'd'};
	std::array<char, 32> blockName {};
	std::memcpy(blockName.data(), "helpdude", 8);
	file.insert(file.end(), blockName.begin(), blockName.end());
	const auto size = static_cast<uint32_t>(s.bytes.size());
	const auto* sizeBytes = reinterpret_cast<const uint8_t*>(&size);
	file.insert(file.end(), sizeBytes, sizeBytes + sizeof(size));
	file.insert(file.end(), s.bytes.begin(), s.bytes.end());
	return file;
}

std::optional<std::vector<uint8_t>> ReadGameFile(const char* name)
{
	const char* root = std::getenv("OPENBLACK_GAME_PATH");
	if (root == nullptr)
	{
		return std::nullopt;
	}
	for (const auto* folder : {"Data/HelpSprite", "data/helpsprite", "Data/Helpsprite"})
	{
		const auto path = std::filesystem::path(root) / folder / name;
		std::ifstream stream(path, std::ios::binary);
		if (stream)
		{
			return std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {});
		}
	}
	return std::nullopt;
}
} // namespace

TEST(HelpDudeFile, ReadsEveryField)
{
	helpdude::HelpDudeFile file;
	ASSERT_EQ(helpdude::ReadHelpDudeFile(MakeFile(), file), helpdude::HelpDudeResult::Success);

	EXPECT_EQ(file.boneCount, k_Bones);
	EXPECT_FLOAT_EQ(file.restHeight, 12.5f);
	EXPECT_EQ(file.meshName, "DATA\\Fake_Mesh.l3d");
	EXPECT_EQ(file.animationNames[0], "Stand");
	EXPECT_TRUE(file.animationNames[1].empty());
	EXPECT_EQ(file.animationNames[3], "Spare");
	EXPECT_TRUE(file.animationNames[4].empty());
	EXPECT_TRUE(file.hasData);
	ASSERT_EQ(file.mesh.size(), 8u);

	const auto* stand = file.AnimationAt(0);
	ASSERT_NE(stand, nullptr);
	EXPECT_EQ(stand->header.duration, 1000u);
	EXPECT_EQ(stand->keyframes.size(), 2u);
	EXPECT_EQ(stand->rotatedJointIndices, (std::vector<uint32_t> {0, 1, 2}));
	EXPECT_EQ(file.AnimationAt(1), nullptr);
	const auto* wave = file.AnimationAt(2);
	ASSERT_NE(wave, nullptr);
	EXPECT_EQ(wave->translatedJointIndices, (std::vector<uint32_t> {0, 2}));
	ASSERT_EQ(wave->keyframes.size(), 3u);
	EXPECT_FLOAT_EQ(wave->keyframes[2].eulerAngles[0][0], RotationValue(2, 0));
	EXPECT_FLOAT_EQ(wave->keyframes[2].translations[1][0], 21.0f);
	EXPECT_EQ(file.AnimationAt(3), nullptr);

	EXPECT_EQ(file.unknownBytes[0x1B], 0x1Bu);
	EXPECT_EQ(file.unknownWords[0], 1u);
	EXPECT_EQ(file.startEmotion, 2u);
	const auto face = file.FaceRecord(7);
	ASSERT_TRUE(face.has_value());
	EXPECT_FLOAT_EQ((*face)[15], 127.0f);
	EXPECT_FALSE(file.FaceRecord(8).has_value());
	EXPECT_FLOAT_EQ(file.nearDepth, 8.5f);
	EXPECT_FLOAT_EQ(file.modelScale, 0.025f);
	ASSERT_EQ(file.events.size(), 2u);
	EXPECT_TRUE(file.events[0].sounds.empty());
	EXPECT_FLOAT_EQ(file.events[0].loopEnd, 0.375f);
	ASSERT_EQ(file.events[1].sounds.size(), 2u);
	EXPECT_EQ(file.events[1].sounds[1].sample, 154u);
	EXPECT_FLOAT_EQ(file.events[1].sounds[1].phase, 0.5f);
	EXPECT_FLOAT_EQ(file.fingertipAcross, 0.35f);
	EXPECT_FLOAT_EQ(file.farDepth, 6.0f);
	EXPECT_EQ(file.animationFlags[2], 0x20u);
	EXPECT_FLOAT_EQ(file.haloScale, 0.5f);
	EXPECT_FLOAT_EQ(file.haloOffset[1], -0.25f);
}

TEST(HelpDudeFile, AnimationSizeMatchesItsRecord)
{
	helpdude::HelpDudeFile file;
	ASSERT_EQ(helpdude::ReadHelpDudeFile(MakeFile(), file), helpdude::HelpDudeResult::Success);
	EXPECT_EQ(helpdude::AnimationByteSize(*file.AnimationAt(0)), 0x2Cu + 4u * 6u + 12u * 6u * 2u);
	EXPECT_EQ(helpdude::AnimationByteSize(*file.AnimationAt(2)), 0x2Cu + 4u * 3u + 12u * 3u * 3u);
}

TEST(HelpDudeFile, RefusesAShortBlock)
{
	helpdude::HelpDudeFile file;
	EXPECT_EQ(helpdude::ReadHelpDudeFile(MakeFile(true), file), helpdude::HelpDudeResult::ErrTruncated);
	EXPECT_EQ(helpdude::ReadHelpDudeFile({'n', 'o'}, file), helpdude::HelpDudeResult::ErrNotAPack);
}

// Needs the original game's files; skipped without OPENBLACK_GAME_PATH
TEST(HelpDudeFile, GameFiles)
{
	struct Expected
	{
		const char* name;
		uint32_t bones;
		const char* mesh;
		size_t animations;
		float farDepth;
		float modelScale;
		float haloScale;
	};
	const std::array<Expected, 2> expected {{
	    {"markgood.hd", 97, "DATA\\Yogi_Mesh.l3d", 65, 6.0266666f, 0.02552f, 0.47333333f},
	    {"markevil.hd", 73, "C:\\dev\\TESTBED\\DATA\\Demon_Mesh.l3d", 66, 5.64f, 0.026826667f, 0.0f},
	}};
	for (const auto& e : expected)
	{
		const auto bytes = ReadGameFile(e.name);
		if (!bytes)
		{
			GTEST_SKIP() << e.name << " is not under OPENBLACK_GAME_PATH";
		}
		helpdude::HelpDudeFile file;
		ASSERT_EQ(helpdude::ReadHelpDudeFile(*bytes, file), helpdude::HelpDudeResult::Success) << e.name;
		EXPECT_EQ(file.boneCount, e.bones);
		EXPECT_EQ(file.meshName, e.mesh);
		EXPECT_EQ(std::ranges::count_if(file.animations, [](const auto& a) { return a.has_value(); }),
		          static_cast<std::ptrdiff_t>(e.animations));
		EXPECT_NEAR(file.nearDepth, 8.7333333f, 1e-5f);
		EXPECT_NEAR(file.farDepth, e.farDepth, 1e-5f);
		EXPECT_NEAR(file.modelScale, e.modelScale, 1e-6f);
		EXPECT_NEAR(file.haloScale, e.haloScale, 1e-6f);
		EXPECT_EQ(file.faceRecords.size(), 0x200u);
		EXPECT_EQ(file.events.size(), 80u);
		for (size_t i = 0; i < helpdude::k_AnimationSlots; ++i)
		{
			if (const auto* animation = file.AnimationAt(i))
			{
				EXPECT_EQ(animation->header.meshBoneCount, e.bones) << i;
				EXPECT_EQ(animation->keyframes.size(), animation->header.frameCount) << i;
			}
		}
	}
}
