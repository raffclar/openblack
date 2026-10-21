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
#include <optional>
#include <sstream>
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

/// A sound pack of the samples over the sample data, with a bank info block after the data. With no sample data, the
/// pack has no block of sample data. With no samples, the pack has no table of samples.
std::string SoundPack(const std::vector<Sample>& samples, const std::optional<std::vector<uint8_t>>& waveData)
{
	std::vector<uint8_t> bytes;
	Append(bytes, "LiOnHeAd", 8);
	if (!samples.empty())
	{
		std::vector<uint8_t> table;
		const std::array<uint16_t, 2> counts = {static_cast<uint16_t>(samples.size()), 3};
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
	}
	if (waveData.has_value())
	{
		AppendBlock(bytes, "LHAudioWaveData", *waveData);
	}
	const std::array<uint32_t, 3> bankInfo = {0, 0, 1};
	std::vector<uint8_t> info;
	Append(info, bankInfo.data(), sizeof(bankInfo));
	info.resize(532);
	AppendBlock(bytes, "LHFileSegmentBankInfo", info);
	return {bytes.begin(), bytes.end()};
}

/// Reads the pack in full and with the headers only, and expects the same result and the same headers
PackResult ExpectSameAsAFullRead(const std::string& bytes)
{
	PackFile whole;
	std::istringstream wholeStream(bytes, std::ios::binary);
	const auto wholeResult = whole.ReadFile(wholeStream);
	PackFile headers;
	std::istringstream headersStream(bytes, std::ios::binary);
	const auto headersResult = headers.ReadAudioHeaders(headersStream);
	EXPECT_EQ(headersResult, wholeResult);
	if (wholeResult == PackResult::Success)
	{
		EXPECT_TRUE(headers.GetAudioSamplesData().empty());
		EXPECT_EQ(headers.GetAudioBankAtmosCount(), whole.GetAudioBankAtmosCount());
		const auto& a = headers.GetAudioSampleHeaders();
		const auto& b = whole.GetAudioSampleHeaders();
		EXPECT_EQ(a.size(), b.size());
		for (size_t i = 0; i < a.size() && i < b.size(); ++i)
		{
			EXPECT_EQ(std::memcmp(&a[i], &b[i], sizeof(AudioBankSampleHeader)), 0);
		}
	}
	return wholeResult;
}

} // namespace

TEST(PackAudioHeaders, AGoodPackGivesTheHeadersOfAFullRead)
{
	EXPECT_EQ(ExpectSameAsAFullRead(SoundPack({{0, 3, 4}, {3, 4, 5}}, std::vector<uint8_t> {1, 2, 3, 4, 5, 6, 7})),
	          PackResult::Success);
}

TEST(PackAudioHeaders, ASampleThatRunsPastTheDataFailsAsInAFullRead)
{
	EXPECT_EQ(ExpectSameAsAFullRead(SoundPack({{2, 4, 0}}, std::vector<uint8_t> {1, 2, 3, 4})), PackResult::ErrFileTooSmall);
	EXPECT_EQ(ExpectSameAsAFullRead(SoundPack({{9, 0, 0}}, std::vector<uint8_t> {1, 2, 3, 4})), PackResult::ErrFileTooSmall);
}

TEST(PackAudioHeaders, APackWithNoSampleDataFailsAsInAFullRead)
{
	EXPECT_EQ(ExpectSameAsAFullRead(SoundPack({{0, 1, 0}}, std::nullopt)), PackResult::ErrMissingAudioWaveDataBlock);
}

TEST(PackAudioHeaders, APackWithNoSamplesReadsAsInAFullRead)
{
	EXPECT_EQ(ExpectSameAsAFullRead(SoundPack({}, std::nullopt)), PackResult::Success);
	EXPECT_EQ(ExpectSameAsAFullRead(SoundPack({}, std::vector<uint8_t> {1, 2})), PackResult::Success);
}
