/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Reading the cues a music bank marks in its chunks' header texts

#include <algorithm>
#include <string_view>
#include <vector>

#include <MusicCues.h>
#include <PackFile.h>
#include <gtest/gtest.h>

using namespace openblack::pack;

namespace
{
AudioBankSampleHeader HeaderWithText(std::string_view text)
{
	AudioBankSampleHeader header {};
	std::ranges::copy(text.substr(0, header.description.size() - 1), header.description.begin());
	return header;
}
} // namespace

TEST(MusicCues, ReadsEachCueAsWritten)
{
	EXPECT_EQ(ParseChunkCues("!0=L1"), (std::vector<ChunkCue> {{.frame = 0, .text = "L1"}}));
	EXPECT_EQ(ParseChunkCues("!20095=L3!7423=p!2815=p"), (std::vector<ChunkCue> {
	                                                         {.frame = 20095, .text = "L3"},
	                                                         {.frame = 7423, .text = "p"},
	                                                         {.frame = 2815, .text = "p"},
	                                                     }));
	// Text after the frame runs to the next cue, spaces and all
	EXPECT_EQ(ParseChunkCues("!0=Record Take 001"), (std::vector<ChunkCue> {{.frame = 0, .text = "Record Take 001"}}));
}

TEST(MusicCues, TextNotStartingWithAMarkHasNone)
{
	EXPECT_TRUE(ParseChunkCues("").empty());
	EXPECT_TRUE(ParseChunkCues("sect0001.mpg !0=L1").empty());
}

TEST(MusicCues, OddlyWrittenCues)
{
	// Without digits the frame is 0, and whatever follows them is skipped before the text
	EXPECT_EQ(ParseChunkCues("!=L2"), (std::vector<ChunkCue> {{.frame = 0, .text = "L2"}}));
	EXPECT_EQ(ParseChunkCues("!12:p"), (std::vector<ChunkCue> {{.frame = 12, .text = "p"}}));
	// A cue cut short has no text
	EXPECT_EQ(ParseChunkCues("!5"), (std::vector<ChunkCue> {{.frame = 5, .text = ""}}));
	EXPECT_EQ(ParseChunkCues("!5=!6=p"), (std::vector<ChunkCue> {{.frame = 5, .text = ""}, {.frame = 6, .text = "p"}}));
}

TEST(MusicCues, ABanksCuesGoChunkByChunkLastWrittenFirst)
{
	const std::vector<AudioBankSampleHeader> headers {
	    HeaderWithText("!0=L1"),
	    HeaderWithText("no cues here"),
	    HeaderWithText("!2303=p!18815=L2"),
	};
	EXPECT_EQ(ReadMusicCues(headers), (std::vector<MusicCue> {
	                                      {.chunk = 1, .frame = 0, .text = "L1"},
	                                      {.chunk = 3, .frame = 18815, .text = "L2"},
	                                      {.chunk = 3, .frame = 2303, .text = "p"},
	                                  }));
}

TEST(MusicCues, ATextFillingTheWholeFieldStopsAtItsEnd)
{
	AudioBankSampleHeader header {};
	std::ranges::fill(header.description, 'p');
	header.description[0] = '!';
	header.description[1] = '1';
	header.description[2] = '=';
	const auto cues = ReadMusicCues(std::vector<AudioBankSampleHeader> {header});
	ASSERT_EQ(cues.size(), 1u);
	EXPECT_EQ(cues[0].text.size(), header.description.size() - 3);
}
