/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <MorphFile.h>

/**
 * The advisors' files, Data/HelpSprite/MarkGood.hd (the good advisor) and MarkEvil.hd (the evil one).
 *
 * A file is a LiOnHeAd pack with a single block, "helpdude", holding in this order:
 * - the bone count, the number of animation names (80) and the height of the rest skeleton
 * - the mesh's name (char[0x80]) and the animation names (char[0x80] each); an empty name has no animation
 * - a flag saying whether the mesh and the animations follow
 * - the mesh: its size then a plain L3D0 mesh
 * - the animations: a count, then for each slot with a name the record's size (0 for none) and the animation, in the
 *   same keyframe format as the creatures' and the hand's
 * - the face's bones, a word nothing reads and the pupils' texture centre and scale, the starting emotion and the face
 *   records (one of 16 floats per emotion)
 * - the near depth, the model scale, the pitch offset, the sound events and loop window of each animation
 * - the fingertip offsets of the pointing arm, the far depth, a flag byte per animation, the halo's scale and offset
 */
namespace openblack::helpdude
{

enum class HelpDudeResult : uint8_t
{
	Success = 0,
	ErrNotAPack,
	ErrNoBlock,
	ErrTruncated,
	ErrTooManyNames,
	ErrTooManyAnimations,
	ErrTooManyEventLists,
};

[[nodiscard]] std::string_view ResultToStr(HelpDudeResult result);

constexpr size_t k_AnimationSlots = 80;
constexpr size_t k_NameLength = 0x80;
constexpr size_t k_FaceFloats = 16;
constexpr size_t k_Emotions = 8;
constexpr size_t k_FaceBones = 7;

/// A face record: how an emotion sets the eyes, lids and pupils
using FaceRecord = std::array<float, k_FaceFloats>;

/// What a face record holds when the file doesn't give it: blinks every 5 to 10 seconds lasting half a second, eyes and
/// pupils at their natural size, the lids open and the base pose at its own pace
constexpr FaceRecord k_DefaultFaceRecord {5.0f, 10.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
                                          0.0f, 0.0f,  0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f};

/// A sound an animation plays when its phase passes a point
struct SoundEvent
{
	/// The in-game sound bank's sample
	uint32_t sample;
	uint32_t flag;
	/// The point of the animation, 0 at its start and 1 at its end
	float phase;
};

/// The sounds of one animation, and the part of it an animation held in a loop plays round
struct AnimationEvents
{
	std::vector<SoundEvent> sounds;
	float loopStart;
	float loopEnd;
};

struct HelpDudeFile
{
	HelpDudeFile() { faceRecords.fill(k_DefaultFaceRecord); }

	/// The bones as the file states them; the mesh's own count is the one used
	uint32_t boneCount = 0;
	/// The height of the rest skeleton, which is measured again once the mesh loads
	float restHeight = 0.0f;
	std::string meshName;
	/// One per slot; an empty name has no animation
	std::array<std::string, k_AnimationSlots> animationNames;
	/// The mesh and the animations were in the file
	bool hasData = false;
	/// A plain L3D0 mesh
	std::vector<uint8_t> mesh;
	std::array<std::optional<morph::Animation>, k_AnimationSlots> animations;
	/// The first eye's bones (0 for none), the second eye's, then the head: the eyes are scaled and their pupils moved by
	/// the face records, and the head turns to look between the two eyes
	std::array<uint32_t, k_FaceBones> faceBones {};
	/// A word nothing reads
	uint32_t unusedWord = 0;
	/// Where the pupils' texture is centred and how far it moves
	float pupilCentreU = 0.0f;
	float pupilCentreV = 0.0f;
	float pupilScale = 0.0f;
	uint32_t startEmotion = 0;
	/// One record per emotion. Only a block of exactly 0x200 bytes replaces them; any other count of bytes is read one
	/// byte at a time over the first byte of the first record, so only the last byte read stays there.
	std::array<FaceRecord, k_Emotions> faceRecords {};
	float nearDepth = 0.0f;
	float modelScale = 0.0f;
	/// Added to the advisor's pitch
	float pitchOffset = 0.0f;
	std::vector<AnimationEvents> events;
	/// How far the pointing finger's tip is along the model's first and third axes, in model sizes
	float fingertipAcross = 0.0f;
	float fingertipAhead = 0.0f;
	float farDepth = 0.0f;
	std::array<uint8_t, k_AnimationSlots> animationFlags {};
	/// The halo's size, none when 0, and where it sits from the head
	float haloScale = 0.0f;
	std::array<float, 3> haloOffset {};

	[[nodiscard]] const morph::Animation* AnimationAt(size_t slot) const
	{
		return slot < animations.size() && animations[slot].has_value() ? &*animations[slot] : nullptr;
	}
};

/// Reads a whole .hd file, its pack header included
[[nodiscard]] HelpDudeResult ReadHelpDudeFile(const std::vector<uint8_t>& bytes, HelpDudeFile& out);

/// Reads one keyframe animation at offset, moving offset past it. False when the data ends first.
[[nodiscard]] bool ReadAnimation(std::span<const uint8_t> data, size_t& offset, morph::Animation& out);

/// The bytes ReadAnimation consumes for an animation
[[nodiscard]] size_t AnimationByteSize(const morph::Animation& animation);

} // namespace openblack::helpdude
