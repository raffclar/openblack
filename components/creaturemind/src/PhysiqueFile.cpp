/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PhysiqueFile.h"

#include <cstddef>
#include <cstring>

#include <type_traits>
#include <utility>

#include "MindFile.h"

using namespace openblack::creaturemind;

namespace
{
constexpr std::string_view k_PhysiquePrefix = "Physique";

template <typename T>
void Put(std::vector<uint8_t>& bytes, T value)
{
	static_assert(sizeof(T) == 4 && std::is_trivially_copyable_v<T>);
	const auto at = bytes.size();
	bytes.resize(at + sizeof(T));
	std::memcpy(bytes.data() + at, &value, sizeof(T));
}

void PutMarks(std::vector<uint8_t>& bytes, const std::vector<uint32_t>& marks)
{
	Put(bytes, static_cast<uint32_t>(marks.size()));
	for (const auto mark : marks)
	{
		Put(bytes, mark);
	}
}

class Reader
{
public:
	explicit Reader(std::span<const uint8_t> bytes)
	    : _bytes(bytes)
	{
	}

	template <typename T>
	[[nodiscard]] std::optional<T> Get()
	{
		if (_bytes.size() - _position < sizeof(T))
		{
			return std::nullopt;
		}
		T value {};
		std::memcpy(&value, _bytes.data() + _position, sizeof(T));
		_position += sizeof(T);
		return value;
	}

	[[nodiscard]] std::optional<std::vector<uint32_t>> GetMarks()
	{
		const auto count = Get<uint32_t>();
		if (!count.has_value() || *count > (_bytes.size() - _position) / sizeof(uint32_t))
		{
			return std::nullopt;
		}
		std::vector<uint32_t> marks(*count);
		for (auto& mark : marks)
		{
			mark = *Get<uint32_t>();
		}
		return marks;
	}

private:
	std::span<const uint8_t> _bytes;
	size_t _position {0};
};
} // namespace

std::vector<uint8_t> openblack::creaturemind::WritePhysique(const PhysiqueFileData& data)
{
	std::vector<uint8_t> bytes;
	Put(bytes, data.speciesRow);
	Put(bytes, data.drawnSize);
	Put(bytes, data.strength);
	Put(bytes, data.fatness);
	Put(bytes, data.alignment);
	PutMarks(bytes, data.blood);
	PutMarks(bytes, data.wounds);
	return bytes;
}

std::optional<PhysiqueFileData> openblack::creaturemind::ReadPhysique(std::span<const uint8_t> bytes)
{
	Reader reader(bytes);
	const auto speciesRow = reader.Get<uint32_t>();
	const auto drawnSize = reader.Get<float>();
	const auto strength = reader.Get<float>();
	const auto fatness = reader.Get<float>();
	const auto alignment = reader.Get<float>();
	if (!speciesRow.has_value() || !drawnSize.has_value() || !strength.has_value() || !fatness.has_value() ||
	    !alignment.has_value())
	{
		return std::nullopt;
	}
	auto blood = reader.GetMarks();
	if (!blood.has_value())
	{
		return std::nullopt;
	}
	auto wounds = reader.GetMarks();
	if (!wounds.has_value())
	{
		return std::nullopt;
	}
	return PhysiqueFileData {.speciesRow = *speciesRow,
	                         .drawnSize = *drawnSize,
	                         .strength = *strength,
	                         .fatness = *fatness,
	                         .alignment = *alignment,
	                         .blood = std::move(*blood),
	                         .wounds = std::move(*wounds)};
}

std::string openblack::creaturemind::PhysiqueFileName(std::string_view mindFile)
{
	return std::string(k_PhysiquePrefix) + std::string(mindFile);
}

bool openblack::creaturemind::IsPhysiqueFileName(std::string_view name)
{
	return name.starts_with(k_PhysiquePrefix);
}

std::optional<PhysiqueFileData> openblack::creaturemind::ReadPhysiqueFile(const std::filesystem::path& path)
{
	const auto bytes = ReadFileBytes(path);
	return bytes.has_value() ? ReadPhysique(*bytes) : std::nullopt;
}

bool openblack::creaturemind::WritePhysiqueFile(const std::filesystem::path& path, const PhysiqueFileData& data)
{
	return WriteFileBytes(path, WritePhysique(data));
}
