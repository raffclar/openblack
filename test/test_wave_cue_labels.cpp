/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// audio::ReadWaveCueLabels (src/Audio/WaveCueLabels.h) on WAV files built by the test.

#include <cstdint>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/WaveCueLabels.h"

using namespace openblack::audio;

namespace
{
using Bytes = std::vector<uint8_t>;

void PutU32(Bytes& out, uint32_t v)
{
	for (int i = 0; i < 4; ++i)
	{
		out.push_back(static_cast<uint8_t>(v >> (8 * i)));
	}
}

void PutId(Bytes& out, std::string_view id)
{
	out.insert(out.end(), id.begin(), id.end());
}

/// A chunk with its body, padded to an even length
Bytes Chunk(std::string_view id, const Bytes& body)
{
	Bytes out;
	PutId(out, id);
	PutU32(out, static_cast<uint32_t>(body.size()));
	out.insert(out.end(), body.begin(), body.end());
	if ((body.size() & 1) != 0)
	{
		out.push_back(0);
	}
	return out;
}

Bytes List(std::string_view type, const std::vector<Bytes>& chunks)
{
	Bytes body;
	PutId(body, type);
	for (const auto& chunk : chunks)
	{
		body.insert(body.end(), chunk.begin(), chunk.end());
	}
	return Chunk("LIST", body);
}

Bytes Format(uint32_t rate)
{
	Bytes body;
	PutU32(body, 0x00010050); // MPEG, one channel
	PutU32(body, rate);
	PutU32(body, 4000);
	PutU32(body, 0x00000001);
	body.push_back(0);
	body.push_back(0);
	return Chunk("fmt ", body);
}

Bytes Cue(const std::vector<std::pair<uint32_t, uint32_t>>& points)
{
	Bytes body;
	PutU32(body, static_cast<uint32_t>(points.size()));
	for (const auto& [id, position] : points)
	{
		PutU32(body, id);
		PutU32(body, position);
		PutId(body, "data");
		PutU32(body, 0);
		PutU32(body, 0);
		PutU32(body, position);
	}
	return Chunk("cue ", body);
}

Bytes Label(uint32_t id, std::string_view text)
{
	Bytes body;
	PutU32(body, id);
	body.insert(body.end(), text.begin(), text.end());
	body.push_back(0);
	return Chunk("labl", body);
}

Bytes Wave(const std::vector<Bytes>& chunks)
{
	Bytes body;
	PutId(body, "WAVE");
	for (const auto& chunk : chunks)
	{
		body.insert(body.end(), chunk.begin(), chunk.end());
	}
	return Chunk("RIFF", body);
}
} // namespace

TEST(WaveCueLabels, LabelsInListOrderWithTheirCueTimes)
{
	const Bytes wave = Wave({
	    Format(22050),
	    Chunk("data", Bytes(10, 0)),
	    List("INFO", {Chunk("ICMT", Bytes(3, 'x'))}),
	    Cue({{1, 11025}, {2, 44100}, {3, 0}}),
	    List("adtl", {Label(2, "[TA pray]"), Chunk("note", Bytes(5, 'n')), Label(9, "no cue"), Label(1, "[TE sad]")}),
	});
	const auto labels = ReadWaveCueLabels(wave);
	ASSERT_EQ(labels.size(), 2u);
	EXPECT_EQ(labels[0].text, "[TA pray]");
	EXPECT_FLOAT_EQ(labels[0].time, 2.0f);
	EXPECT_EQ(labels[1].text, "[TE sad]");
	EXPECT_FLOAT_EQ(labels[1].time, 0.5f);
}

TEST(WaveCueLabels, NoCuesNoLabels)
{
	EXPECT_TRUE(ReadWaveCueLabels(Wave({Format(22050), Chunk("data", Bytes(4, 0))})).empty());
	// The cue points are only looked for after the format
	EXPECT_TRUE(ReadWaveCueLabels(Wave({Cue({{1, 0}}), Format(22050), List("adtl", {Label(1, "[TE sad]")})})).empty());
	// And the labels only after the cue points
	EXPECT_TRUE(ReadWaveCueLabels(Wave({Format(22050), List("adtl", {Label(1, "[TE sad]")}), Cue({{1, 0}})})).empty());
	EXPECT_TRUE(ReadWaveCueLabels(Bytes {}).empty());
	EXPECT_TRUE(ReadWaveCueLabels(Bytes(64, 0xFF)).empty());
}

TEST(WaveCueLabels, OnlyAsManyLabelsAsCuePoints)
{
	const Bytes wave = Wave({
	    Format(1000),
	    Cue({{1, 500}}),
	    List("adtl", {Label(1, "first"), Label(1, "second")}),
	});
	const auto labels = ReadWaveCueLabels(wave);
	ASSERT_EQ(labels.size(), 1u);
	EXPECT_EQ(labels[0].text, "first");
	EXPECT_FLOAT_EQ(labels[0].time, 0.5f);
}
