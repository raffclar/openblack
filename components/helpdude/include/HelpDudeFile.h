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
 * - 0x1C bytes and four words, the starting emotion, the face records (one of 16 floats per emotion)
 * - the near depth, the model scale, an unused float, the sound events and loop window of each animation
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
	std::array<uint8_t, 0x1C> unknownBytes {};
	std::array<uint32_t, 4> unknownWords {};
	uint32_t startEmotion = 0;
	/// One record of k_FaceFloats floats per emotion, as stored
	std::vector<uint8_t> faceRecords;
	float nearDepth = 0.0f;
	float modelScale = 0.0f;
	float unknownFloat = 0.0f;
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
	/// The face record of an emotion, or nullopt past the records stored
	[[nodiscard]] std::optional<std::array<float, k_FaceFloats>> FaceRecord(size_t emotion) const;
};

/// Reads a whole .hd file, its pack header included
[[nodiscard]] HelpDudeResult ReadHelpDudeFile(const std::vector<uint8_t>& bytes, HelpDudeFile& out);

/// Reads one keyframe animation at offset, moving offset past it. False when the data ends first.
[[nodiscard]] bool ReadAnimation(std::span<const uint8_t> data, size_t& offset, morph::Animation& out);

/// The bytes ReadAnimation consumes for an animation
[[nodiscard]] size_t AnimationByteSize(const morph::Animation& animation);

} // namespace openblack::helpdude
