/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpDudeFile.h"

#include <cstring>

#include <algorithm>

#include <PackFile.h>

namespace openblack::helpdude
{
namespace
{
constexpr std::string_view k_BlockName = "helpdude";

/// Little-endian reads over a block, refusing to go past its end
class Reader
{
public:
	explicit Reader(std::span<const uint8_t> data, size_t offset = 0)
	    : _data(data)
	    , _offset(offset)
	{
	}

	[[nodiscard]] bool Bytes(void* out, size_t count)
	{
		if (count > _data.size() - _offset)
		{
			return false;
		}
		std::memcpy(out, _data.data() + _offset, count);
		_offset += count;
		return true;
	}

	template <typename T>
	[[nodiscard]] bool Value(T& out)
	{
		return Bytes(&out, sizeof(T));
	}

	template <typename T>
	[[nodiscard]] bool Values(std::vector<T>& out, size_t count)
	{
		if (count > (_data.size() - _offset) / sizeof(T))
		{
			return false;
		}
		out.resize(count);
		return Bytes(out.data(), count * sizeof(T));
	}

	/// A char[0x80] up to its first zero
	[[nodiscard]] bool Name(std::string& out)
	{
		std::array<char, k_NameLength> name {};
		if (!Bytes(name.data(), name.size()))
		{
			return false;
		}
		out.assign(name.data(), static_cast<size_t>(std::ranges::find(name, '\0') - name.begin()));
		return true;
	}

	[[nodiscard]] size_t Offset() const { return _offset; }
	[[nodiscard]] size_t Remaining() const { return _data.size() - _offset; }

private:
	std::span<const uint8_t> _data;
	size_t _offset;
};
} // namespace

std::string_view ResultToStr(HelpDudeResult result)
{
	switch (result)
	{
	case HelpDudeResult::Success:
		return "Success";
	case HelpDudeResult::ErrNotAPack:
		return "Not a LiOnHeAd pack";
	case HelpDudeResult::ErrNoBlock:
		return "No helpdude block";
	case HelpDudeResult::ErrTruncated:
		return "The block ends early";
	case HelpDudeResult::ErrTooManyNames:
		return "More than 80 animation names";
	case HelpDudeResult::ErrTooManyAnimations:
		return "More than 80 animations";
	case HelpDudeResult::ErrTooManyEventLists:
		return "More than 80 animation event lists";
	}
	return "Unknown";
}

bool ReadAnimation(std::span<const uint8_t> data, size_t& offset, morph::Animation& out)
{
	Reader in(data, offset);
	if (!in.Value(out.header) || !in.Values(out.rotatedJointIndices, out.header.rotatedJointCount) ||
	    !in.Values(out.translatedJointIndices, out.header.translatedJointCount))
	{
		return false;
	}
	const size_t frameBytes = (out.header.rotatedJointCount + out.header.translatedJointCount) * sizeof(float) * 3;
	if (frameBytes != 0 && out.header.frameCount > in.Remaining() / frameBytes)
	{
		return false;
	}
	out.keyframes.resize(out.header.frameCount);
	for (auto& frame : out.keyframes)
	{
		if (!in.Values(frame.eulerAngles, out.header.rotatedJointCount) ||
		    !in.Values(frame.translations, out.header.translatedJointCount))
		{
			return false;
		}
	}
	offset = in.Offset();
	return true;
}

size_t AnimationByteSize(const morph::Animation& animation)
{
	const size_t channels = animation.header.rotatedJointCount + animation.header.translatedJointCount;
	return sizeof(morph::AnimationHeader) + channels * sizeof(uint32_t) +
	       channels * animation.header.frameCount * sizeof(float) * 3;
}

HelpDudeResult ReadHelpDudeFile(const std::vector<uint8_t>& bytes, HelpDudeFile& out)
{
	out = {};
	pack::PackFile pack;
	if (pack.Open(bytes) != pack::PackResult::Success)
	{
		return HelpDudeResult::ErrNotAPack;
	}
	if (!pack.HasBlock(std::string(k_BlockName)))
	{
		return HelpDudeResult::ErrNoBlock;
	}
	const auto& block = pack.GetBlock(std::string(k_BlockName));
	Reader in(block);

	uint32_t nameCount = 0;
	if (!in.Value(out.boneCount) || !in.Value(nameCount) || !in.Value(out.restHeight) || !in.Name(out.meshName))
	{
		return HelpDudeResult::ErrTruncated;
	}
	if (nameCount > k_AnimationSlots)
	{
		return HelpDudeResult::ErrTooManyNames;
	}
	for (uint32_t i = 0; i < nameCount; ++i)
	{
		if (!in.Name(out.animationNames.at(i)))
		{
			return HelpDudeResult::ErrTruncated;
		}
	}

	uint32_t hasData = 0;
	if (!in.Value(hasData))
	{
		return HelpDudeResult::ErrTruncated;
	}
	out.hasData = hasData != 0;
	if (out.hasData)
	{
		uint32_t meshSize = 0;
		if (!in.Value(meshSize) || !in.Values(out.mesh, meshSize))
		{
			return HelpDudeResult::ErrTruncated;
		}
		uint32_t animationCount = 0;
		if (!in.Value(animationCount))
		{
			return HelpDudeResult::ErrTruncated;
		}
		if (animationCount > k_AnimationSlots)
		{
			return HelpDudeResult::ErrTooManyAnimations;
		}
		for (uint32_t i = 0; i < animationCount; ++i)
		{
			// A slot without a name has no record at all
			if (out.animationNames.at(i).empty())
			{
				continue;
			}
			uint32_t recordSize = 0;
			if (!in.Value(recordSize))
			{
				return HelpDudeResult::ErrTruncated;
			}
			if (recordSize == 0)
			{
				continue;
			}
			morph::Animation animation {};
			animation.name = out.animationNames.at(i);
			size_t offset = in.Offset();
			if (!ReadAnimation(block, offset, animation))
			{
				return HelpDudeResult::ErrTruncated;
			}
			in = Reader(block, offset);
			out.animations.at(i) = std::move(animation);
		}
	}

	int32_t faceBytes = 0;
	if (!in.Bytes(out.faceBones.data(), out.faceBones.size() * sizeof(uint32_t)) || !in.Value(out.unusedWord) ||
	    !in.Value(out.pupilCentreU) || !in.Value(out.pupilCentreV) || !in.Value(out.pupilScale) ||
	    !in.Value(out.startEmotion) || !in.Value(faceBytes))
	{
		return HelpDudeResult::ErrTruncated;
	}
	constexpr auto k_AllRecords = static_cast<int32_t>(sizeof(out.faceRecords));
	if (faceBytes == k_AllRecords)
	{
		if (!in.Bytes(out.faceRecords.data(), sizeof(out.faceRecords)))
		{
			return HelpDudeResult::ErrTruncated;
		}
	}
	else
	{
		for (int32_t i = 0; i < faceBytes; ++i)
		{
			if (!in.Bytes(&out.faceRecords[0][0], 1))
			{
				return HelpDudeResult::ErrTruncated;
			}
		}
	}

	uint32_t eventLists = 0;
	if (!in.Value(out.nearDepth) || !in.Value(out.modelScale) || !in.Value(out.pitchOffset) || !in.Value(eventLists))
	{
		return HelpDudeResult::ErrTruncated;
	}
	if (eventLists > k_AnimationSlots)
	{
		return HelpDudeResult::ErrTooManyEventLists;
	}
	out.events.resize(eventLists);
	for (auto& list : out.events)
	{
		uint32_t count = 0;
		if (!in.Value(count) || !in.Values(list.sounds, count) || !in.Value(list.loopStart) || !in.Value(list.loopEnd))
		{
			return HelpDudeResult::ErrTruncated;
		}
	}

	if (!in.Value(out.fingertipAcross) || !in.Value(out.fingertipAhead) || !in.Value(out.farDepth) ||
	    !in.Bytes(out.animationFlags.data(), out.animationFlags.size()) || !in.Value(out.haloScale) ||
	    !in.Bytes(out.haloOffset.data(), out.haloOffset.size() * sizeof(float)))
	{
		return HelpDudeResult::ErrTruncated;
	}
	return HelpDudeResult::Success;
}

} // namespace openblack::helpdude
