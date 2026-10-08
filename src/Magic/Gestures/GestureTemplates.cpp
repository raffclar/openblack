/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureTemplates.h"

#include <cstring>

#include <optional>

#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::magic::gestures;

namespace
{
constexpr size_t k_RecordSize = 0x65C;
constexpr size_t k_SampleSize = 0x14;

template <class T>
T Read(const std::vector<uint8_t>& bytes, size_t offset)
{
	T value;
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}
} // namespace

void GestureData::SetToZero()
{
	samples.fill(KeySample {});
	count = 0;
	gesture = k_None;
	positionMode = 0;
	aspect = 0.0f;
	checkDirection = false;
	allowReverse = false;
	checkAspect = false;
}

void GestureData::Append(const KeySample& sample)
{
	if (count < k_MaxSamples)
	{
		samples[count++] = sample;
	}
}

bool openblack::magic::gestures::LoadTemplates(const std::vector<uint8_t>& bytes, std::vector<GestureData>& out)
{
	out.clear();
	if (bytes.size() < 4)
	{
		return false;
	}
	const auto count = Read<uint32_t>(bytes, 0);
	if (bytes.size() < 4 + static_cast<size_t>(count) * k_RecordSize)
	{
		return false;
	}
	out.resize(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		const size_t base = 4 + static_cast<size_t>(i) * k_RecordSize;
		auto& data = out[i];
		for (size_t s = 0; s < k_MaxSamples; ++s)
		{
			const size_t at = base + s * k_SampleSize;
			data.samples[s] = {Read<float>(bytes, at), Read<float>(bytes, at + 4), Read<float>(bytes, at + 8),
			                   Read<float>(bytes, at + 0xC), Read<uint32_t>(bytes, at + 0x10)};
		}
		// the original reads 4 bytes into each of the three byte fields (only the low byte stays)
		data.count = static_cast<uint8_t>(Read<uint32_t>(bytes, base + 0x640));
		data.gesture = static_cast<Gesture>(Read<uint32_t>(bytes, base + 0x644));
		data.positionMode = static_cast<uint8_t>(Read<uint32_t>(bytes, base + 0x648));
		data.checkDirection = Read<uint32_t>(bytes, base + 0x64C) != 0;
		data.allowReverse = Read<uint32_t>(bytes, base + 0x650) != 0;
		data.checkAspect = Read<uint32_t>(bytes, base + 0x654) != 0;
		data.aspect = Read<float>(bytes, base + 0x658);
	}
	return true;
}

const std::vector<GestureData>& openblack::magic::gestures::Templates()
{
	// read once from the game's files through the resource cache; without a file system there is no list to read
	if (!Locator::filesystem::has_value())
	{
		static const std::vector<GestureData> k_NoTemplates;
		return k_NoTemplates;
	}
	auto& cache = Locator::resources::value().GetGestureTemplates();
	const auto id = entt::hashed_string("gestures/templates").value();
	if (!cache.Contains(id))
	{
		cache.Load(id, resources::GestureTemplatesLoader::FromDiskTag {});
	}
	return *cache.Handle(id);
}
