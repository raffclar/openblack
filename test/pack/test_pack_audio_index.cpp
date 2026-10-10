/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <PackFile.h>
#include <gtest/gtest.h>

using openblack::pack::AudioBankSampleHeader;
using openblack::pack::PackFile;
using openblack::pack::PackResult;

namespace
{

void Append(std::vector<uint8_t>& bytes, const void* data, size_t size)
{
	const auto* begin = static_cast<const uint8_t*>(data);
	bytes.insert(bytes.end(), begin, begin + size);
}

void AppendBlock(std::vector<uint8_t>& bytes, std::string_view name, const std::vector<uint8_t>& body)
{
	std::array<char, 0x20> blockName {};
	std::memcpy(blockName.data(), name.data(), name.size());
	const auto size = static_cast<uint32_t>(body.size());
	Append(bytes, blockName.data(), blockName.size());
	Append(bytes, &size, sizeof(size));
	Append(bytes, body.data(), body.size());
}

struct Sample
{
	uint32_t offset;
	uint32_t size;
	int16_t group;
};

/// A sound pack of the given samples over the given sample data, with a bank info block after the data
std::vector<uint8_t> SoundPack(const std::vector<Sample>& samples, const std::vector<uint8_t>& waveData)
{
	std::vector<uint8_t> bytes;
	Append(bytes, "LiOnHeAd", 8);

	std::vector<uint8_t> table;
	const std::array<uint16_t, 2> counts = {static_cast<uint16_t>(samples.size()), 0};
	Append(table, counts.data(), sizeof(counts));
	for (const auto& sample : samples)
	{
		AudioBankSampleHeader header {};
		header.offset = sample.offset;
		header.size = sample.size;
		header.group = sample.group;
		Append(table, &header, sizeof(header));
	}
	AppendBlock(bytes, "LHAudioBankSampleTable", table);
	AppendBlock(bytes, "LHAudioWaveData", waveData);

	const std::array<uint32_t, 3> bankInfo = {0, 0, 1};
	std::vector<uint8_t> info;
	Append(info, bankInfo.data(), sizeof(bankInfo));
	info.resize(532);
	AppendBlock(bytes, "LHFileSegmentBankInfo", info);
	return bytes;
}

std::filesystem::path WriteTemp(const std::string& name, const std::vector<uint8_t>& bytes)
{
	const auto path = std::filesystem::temp_directory_path() / name;
	std::ofstream stream(path, std::ios::binary);
	stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	return path;
}

std::vector<uint8_t> ReadSpan(const std::filesystem::path& path, uint64_t offset, uint32_t size)
{
	std::ifstream stream(path, std::ios::binary);
	stream.seekg(static_cast<std::streamoff>(offset));
	std::vector<uint8_t> data(size);
	stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
	return data;
}

} // namespace

TEST(PackAudioIndex, ReadsTheHeadersAndLeavesTheSamplesInTheFile)
{
	const std::vector<uint8_t> waveData = {10, 11, 12, 20, 21, 22, 23};
	const auto path = WriteTemp("openblack_test_audio_index.sad", SoundPack({{0, 3, 4}, {3, 4, 4}}, waveData));

	PackFile pack;
	ASSERT_EQ(pack.OpenAudioIndex(path), PackResult::Success);
	ASSERT_EQ(pack.GetAudioSampleHeaders().size(), 2);
	EXPECT_EQ(pack.GetAudioSampleHeader(0).group, 4);
	// The blocks after the sample data are read too
	EXPECT_TRUE(pack.IsAudioMusicBank());
	// Nothing of the samples is read
	EXPECT_TRUE(pack.GetAudioSamplesData().empty());
	EXPECT_FALSE(pack.HasBlock("LHAudioWaveData"));

	const auto first = pack.GetAudioSampleFileSpan(0);
	const auto second = pack.GetAudioSampleFileSpan(1);
	ASSERT_TRUE(first.has_value());
	ASSERT_TRUE(second.has_value());
	EXPECT_EQ(ReadSpan(path, first->first, first->second), (std::vector<uint8_t> {10, 11, 12}));
	EXPECT_EQ(ReadSpan(path, second->first, second->second), (std::vector<uint8_t> {20, 21, 22, 23}));
	EXPECT_FALSE(pack.GetAudioSampleFileSpan(2).has_value());

	// The whole pack read the usual way holds the same samples
	PackFile whole;
	ASSERT_EQ(whole.Open(path), PackResult::Success);
	EXPECT_EQ(whole.GetAudioSampleData(1), (std::vector<uint8_t> {20, 21, 22, 23}));
	std::filesystem::remove(path);
}

TEST(PackAudioIndex, ASampleRunningPastTheDataIsAnError)
{
	const auto path = WriteTemp("openblack_test_audio_index_short.sad", SoundPack({{2, 4, 0}}, {1, 2, 3, 4}));
	PackFile pack;
	EXPECT_EQ(pack.OpenAudioIndex(path), PackResult::ErrFileTooSmall);
	std::filesystem::remove(path);
}

TEST(PackAudioIndex, APackWithoutSampleDataIsAnError)
{
	std::vector<uint8_t> bytes;
	Append(bytes, "LiOnHeAd", 8);
	AppendBlock(bytes, "Other", {1, 2, 3, 4});
	const auto path = WriteTemp("openblack_test_audio_index_none.sad", bytes);
	PackFile pack;
	EXPECT_EQ(pack.OpenAudioIndex(path), PackResult::ErrMissingAudioWaveDataBlock);
	std::filesystem::remove(path);
}
