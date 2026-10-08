/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MusicBank.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <exception>
#include <map>
#include <string_view>

#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

namespace openblack::audio
{

namespace
{
// LiOnHeAd file: 8 byte magic, then blocks { char name[32]; u32 size; u8 data[size]; } (the same layout that
// pack::PackFile reads)
constexpr std::array<char, 8> k_Magic = {'L', 'i', 'O', 'n', 'H', 'e', 'A', 'd'};
constexpr size_t k_BlockNameSize = 32;

struct BlockSpan
{
	uint64_t offset;
	uint32_t size;
};

// Record offsets used here
constexpr size_t k_RecSize = 0x10C;        // segment bytes
constexpr size_t k_RecOffset = 0x110;      // segment offset in the wave data chunk
constexpr size_t k_RecGroup = 0x118;       // music group (u16, first segment only)
constexpr size_t k_RecRate = 0x128;        // Hz
constexpr size_t k_RecDesc = 0x140;        // description: the markers
constexpr size_t k_RecDescSize = 0x100;    // up to the priority at 0x240
constexpr size_t k_RecFlags = 0x244;       // flags (first segment only)
constexpr size_t k_RecLoops = 0x248;       // loops
constexpr size_t k_RecVolume = 0x25C;      // volume
constexpr size_t k_RecMinDistance = 0x268; // 3D min distance
constexpr size_t k_RecMaxDistance = 0x26C; // 3D max distance
constexpr size_t k_RecScale = 0x270;       // 3D scale

// The distance of a bank that is not a music bank
constexpr float k_NoDistance = -1.0f;

/// `size` bytes at `position` of the stream; false, never an exception, when they cannot all be read (as a failed
/// stream read)
bool ReadAt(filesystem::Stream& stream, uint64_t position, void* out, size_t size) noexcept
{
	try
	{
		stream.Seek(static_cast<size_t>(position), filesystem::Stream::SeekMode::Begin);
		stream.Read(static_cast<uint8_t*>(out), size);
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

uint32_t ReadU32(const uint8_t* p)
{
	uint32_t v;
	std::memcpy(&v, p, sizeof(v));
	return v;
}

// The "audio" logger exists in the game, not in the tests
void LogRegisterFailure(std::string_view format, const std::string& path)
{
	if (auto logger = spdlog::get("audio"))
	{
		SPDLOG_LOGGER_WARN(logger, fmt::runtime(format), path);
	}
}
} // namespace

MusicBank::~MusicBank() = default;

std::unique_ptr<MusicBank> MusicBank::Register(const std::filesystem::path& path)
{
	// a new bank and its file opened: a failure logs and returns nullptr
	auto bank = std::unique_ptr<MusicBank>(new MusicBank());
	bank->_path = path;
	try
	{
		if (Locator::filesystem::has_value())
		{
			bank->_file = Locator::filesystem::value().Open(path, filesystem::Stream::Mode::Read);
		}
	}
	catch (const std::exception&)
	{
		bank->_file.reset();
	}
	if (bank->_file == nullptr)
	{
		LogRegisterFailure("LHBankRegister: cannot open {}", path.string());
		return nullptr;
	}

	auto& file = *bank->_file;
	std::array<char, 8> magic {};
	if (!ReadAt(file, 0, magic.data(), magic.size()) || magic != k_Magic)
	{
		LogRegisterFailure("LHBankRegister: {} is not a LiOnHeAd file", path.string());
		return nullptr;
	}

	// The library looks each block up by name; walking them once gives the same spans. Stopping at a truncated block is
	// defensive (not in the original).
	std::map<std::string, BlockSpan, std::less<>> blocks;
	const auto fileSize = static_cast<uint64_t>(file.Size());
	uint64_t pos = magic.size();
	while (pos + k_BlockNameSize + sizeof(uint32_t) <= fileSize)
	{
		std::array<char, k_BlockNameSize> name {};
		uint32_t size = 0;
		if (!ReadAt(file, pos, name.data(), name.size()) || !ReadAt(file, pos + name.size(), &size, sizeof(size)))
		{
			break;
		}
		const auto dataOffset = pos + name.size() + sizeof(size);
		if (dataOffset + size > fileSize)
		{
			break;
		}
		const auto length = std::find(name.begin(), name.end(), '\0') - name.begin();
		blocks.emplace(std::string(name.data(), static_cast<size_t>(length)), BlockSpan {dataOffset, size});
		pos = dataOffset + size;
	}

	const auto findBlock = [&blocks](std::string_view name) -> const BlockSpan* {
		const auto it = blocks.find(name);
		return it != blocks.end() ? &it->second : nullptr;
	};

	// the bank info chunk: 3 u32, the 3rd is the music flag; missing = error
	const auto* info = findBlock("LHFileSegmentBankInfo");
	if (info == nullptr || info->size < 3 * sizeof(uint32_t))
	{
		LogRegisterFailure("LHBankRegister: {} has no LHFileSegmentBankInfo", path.string());
		return nullptr;
	}
	std::array<uint32_t, 3> infoWords {};
	// the reads after the walk fail together, as on one stream: once one fails, the later ones read nothing, and the
	// sample table's check below sees it
	bool read = ReadAt(file, info->offset, infoWords.data(), sizeof(infoWords));
	bank->_musicFlag = infoWords[2];

	// the wave data chunk: not in memory, so the data is read from the file when used
	const auto* wave = findBlock("LHAudioWaveData");
	if (wave == nullptr)
	{
		LogRegisterFailure("LHBankRegister: {} has no LHAudioWaveData", path.string());
		return nullptr;
	}
	bank->_waveDataOffset = wave->offset;
	bank->_waveDataSize = wave->size;

	// the sample table chunk: u32 w, n = w & 0xFFFF samples, w >> 16 = atmos samples, then n records of 0x280 bytes
	const auto* table = findBlock("LHAudioBankSampleTable");
	if (table == nullptr || table->size < sizeof(uint32_t))
	{
		LogRegisterFailure("LHBankRegister: {} has no LHAudioBankSampleTable", path.string());
		return nullptr;
	}
	uint32_t countWord = 0;
	read = read && ReadAt(file, table->offset, &countWord, sizeof(countWord));
	const size_t count = countWord & 0xFFFF;
	// (defensive, not in the original: it reads n records without checking the block size)
	if (table->size < sizeof(uint32_t) + count * k_RecordSize)
	{
		LogRegisterFailure("LHBankRegister: {} has a short sample table", path.string());
		return nullptr;
	}
	bank->_records.resize(count);
	uint64_t recordPosition = table->offset + sizeof(countWord);
	for (auto& record : bank->_records)
	{
		read = read && ReadAt(file, recordPosition, record.data(), record.size());
		recordPosition += record.size();
	}
	if (!read)
	{
		LogRegisterFailure("LHBankRegister: cannot read the sample table of {}", path.string());
		return nullptr;
	}

	bank->_segments.reserve(count);
	for (const auto& record : bank->_records)
	{
		bank->_segments.push_back({ReadU32(&record[k_RecOffset]), ReadU32(&record[k_RecSize])});
	}

	// The music engine's group count is left to the caller: it is the maximum GetGroupId() of the registered music
	// banks.
	return bank;
}

uint32_t MusicBank::FirstRecordUint(size_t offset) const
{
	return ReadU32(&_records.front()[offset]);
}

float MusicBank::FirstRecordFloat(size_t offset) const
{
	float v;
	std::memcpy(&v, &_records.front()[offset], sizeof(v));
	return v;
}

int MusicBank::GetGroupId() const
{
	// a first sample and the music flag are needed; u32 at 0x118 & 0xFFFF
	if (_records.empty() || !IsMusic())
	{
		return -1;
	}
	return static_cast<int>(FirstRecordUint(k_RecGroup) & 0xFFFF);
}

float MusicBank::GetMinDistance() const
{
	if (_records.empty() || !IsMusic())
	{
		return k_NoDistance;
	}
	return FirstRecordFloat(k_RecMinDistance);
}

float MusicBank::GetMaxDistance() const
{
	if (_records.empty() || !IsMusic())
	{
		return k_NoDistance;
	}
	return FirstRecordFloat(k_RecMaxDistance);
}

uint32_t MusicBank::GetFlags() const
{
	// the flags of the first sample
	return _records.empty() ? 0 : FirstRecordUint(k_RecFlags);
}

uint32_t MusicBank::GetSampleRate() const
{
	return _records.empty() ? 0 : FirstRecordUint(k_RecRate);
}

int MusicBank::GetVolume() const
{
	// 127; with flag 0x20, u32 at 0x25C & 0xFFFF
	constexpr int k_DefaultVolume = 0x7F;
	if (!HasFlag(MusicBankFlag::Volume))
	{
		return k_DefaultVolume;
	}
	return static_cast<int>(FirstRecordUint(k_RecVolume) & 0xFFFF);
}

int MusicBank::GetLoops(int requestedLoops) const
{
	// the requested loops; with flag 0x40, i32 at 0x248
	if (!HasFlag(MusicBankFlag::Loops))
	{
		return requestedLoops;
	}
	return static_cast<int32_t>(FirstRecordUint(k_RecLoops));
}

MusicDistanceMapping MusicBank::GetDistanceMapping(const MusicDistanceMapping& requested) const
{
	// in this order: 0x100 max, 0x80 min, 0x200 scale
	auto mapping = requested;
	if (HasFlag(MusicBankFlag::MaxDistance))
	{
		mapping.maxDistance = FirstRecordFloat(k_RecMaxDistance);
	}
	if (HasFlag(MusicBankFlag::MinDistance))
	{
		mapping.minDistance = FirstRecordFloat(k_RecMinDistance);
	}
	if (HasFlag(MusicBankFlag::Scale))
	{
		mapping.scale = FirstRecordFloat(k_RecScale);
	}
	return mapping;
}

std::vector<MusicMarker> MusicBank::ParseMarkers() const
{
	// For i = n-1 .. 0: desc = the record's description; only if desc[0] == '!':
	//   repeat: skip the '!', take the digits (isdigit) -> sample = atoi;
	//           skip ONE character (the '='), label = up to '\0' or '!';
	//           node {next = head, sample, chunk = i + 1, label} becomes the head;
	//   while the label ended at a '!'.
	// The original holds the label in 32 bytes; the data has no longer label.
	std::vector<MusicMarker> reversed; // built in push order; the list is its reverse
	for (size_t i = _records.size(); i-- > 0;)
	{
		const auto* desc = reinterpret_cast<const char*>(&_records[i][k_RecDesc]);
		const auto descLength = strnlen(desc, k_RecDescSize);
		const std::string_view text(desc, descLength);
		if (text.empty() || text[0] != '!')
		{
			continue;
		}
		size_t cursor = 0; // at a '!'
		while (true)
		{
			const auto digitsBegin = cursor + 1;
			auto digitsEnd = digitsBegin;
			while (digitsEnd < text.size() && std::isdigit(static_cast<unsigned char>(text[digitsEnd])) != 0)
			{
				++digitsEnd;
			}
			const std::string digits(text.substr(digitsBegin, digitsEnd - digitsBegin));
			const int sample = std::atoi(digits.c_str());
			// The original skips the character after the digits even if it is the terminator, and would read past it
			// (not in the data); here the label is then empty (approximate).
			const auto labelBegin = std::min(digitsEnd + 1, text.size());
			auto labelEnd = labelBegin;
			while (labelEnd < text.size() && text[labelEnd] != '!')
			{
				++labelEnd;
			}
			reversed.push_back({static_cast<int>(i) + 1, sample, std::string(text.substr(labelBegin, labelEnd - labelBegin))});
			if (labelEnd >= text.size())
			{
				break;
			}
			cursor = labelEnd;
		}
	}
	return {reversed.rbegin(), reversed.rend()};
}

bool MusicBank::ReadSegment(size_t index, std::vector<uint8_t>& out)
{
	if (index >= _segments.size())
	{
		return false;
	}
	const auto& segment = _segments[index];
	// (defensive: these bounds checks are not in the original's music thread)
	if (static_cast<uint64_t>(segment.offset) + segment.size > _waveDataSize)
	{
		return false;
	}
	out.resize(segment.size);
	return ReadAt(*_file, _waveDataOffset + segment.offset, out.data(), segment.size);
}

} // namespace openblack::audio
