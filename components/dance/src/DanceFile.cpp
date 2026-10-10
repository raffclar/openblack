/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DanceFile.h"

#include <cstring>

#include <bit>

using namespace openblack::dance;

namespace
{
class Reader
{
public:
	explicit Reader(std::span<const uint8_t> buffer)
	    : _buffer(buffer)
	{
	}

	bool U32(uint32_t& value)
	{
		if (_buffer.size() - _offset < sizeof(value))
		{
			return false;
		}
		std::memcpy(&value, _buffer.data() + _offset, sizeof(value));
		_offset += sizeof(value);
		return true;
	}

	bool F32(float& value)
	{
		uint32_t bits = 0;
		if (!U32(bits))
		{
			return false;
		}
		value = std::bit_cast<float>(bits);
		return true;
	}

	bool Bytes(std::string& text, uint32_t count)
	{
		if (_buffer.size() - _offset < count)
		{
			return false;
		}
		text.assign(reinterpret_cast<const char*>(_buffer.data() + _offset), count);
		_offset += count;
		return true;
	}

	[[nodiscard]] bool AtEnd() const { return _offset == _buffer.size(); }

private:
	std::span<const uint8_t> _buffer;
	std::size_t _offset {0};
};

bool ReadAction(Reader& reader, DanceAction& action)
{
	uint32_t groups = 0;
	if (!reader.U32(groups))
	{
		return false;
	}
	action.groups.resize(groups);
	for (auto& group : action.groups)
	{
		if (!reader.U32(group))
		{
			return false;
		}
	}
	if (!reader.U32(action.type))
	{
		return false;
	}
	for (auto& argument : action.arguments)
	{
		if (!reader.U32(argument))
		{
			return false;
		}
	}
	return true;
}

bool ReadKeyFrame(Reader& reader, DanceKeyFrame& keyFrame)
{
	uint32_t actions = 0;
	if (!reader.F32(keyFrame.time) || !reader.U32(keyFrame.flags) || !reader.U32(actions))
	{
		return false;
	}
	keyFrame.actions.resize(actions);
	for (auto& action : keyFrame.actions)
	{
		if (!ReadAction(reader, action))
		{
			return false;
		}
	}
	return true;
}
} // namespace

std::string_view openblack::dance::ResultToStr(DanceResult result)
{
	switch (result)
	{
	case DanceResult::Success:
		return "Success";
	case DanceResult::ErrTruncated:
		return "The dance file ends too soon";
	case DanceResult::ErrTrailingBytes:
		return "The dance file has bytes after its end";
	}
	return "Unknown";
}

float DanceAction::Float(std::size_t index) const
{
	return std::bit_cast<float>(arguments.at(index));
}

DanceResult DanceFile::Open(std::span<const uint8_t> buffer)
{
	Reader reader(buffer);
	uint32_t keyFrameCount = 0;
	if (!reader.U32(version) || !reader.U32(keyFrameCount))
	{
		return DanceResult::ErrTruncated;
	}
	keyFrames.resize(keyFrameCount);
	for (auto& keyFrame : keyFrames)
	{
		if (!ReadKeyFrame(reader, keyFrame))
		{
			return DanceResult::ErrTruncated;
		}
	}
	if (!reader.F32(beat) || !reader.U32(groupCount) || !reader.U32(unknown44) || !reader.U32(unknown48) || !reader.U32(loops))
	{
		return DanceResult::ErrTruncated;
	}
	if (version >= 2)
	{
		float facing = 0.0f;
		if (!reader.F32(facing))
		{
			return DanceResult::ErrTruncated;
		}
		angle = facing;
	}
	if (version >= 1)
	{
		groupNames.resize(groupCount);
		for (auto& name : groupNames)
		{
			uint32_t length = 0;
			if (!reader.U32(length) || !reader.Bytes(name, length))
			{
				return DanceResult::ErrTruncated;
			}
		}
	}
	return reader.AtEnd() ? DanceResult::Success : DanceResult::ErrTrailingBytes;
}
