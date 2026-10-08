/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The debug GUI's Audio Player window: the switches that silence the game's sounds and music (audio::OutputSwitches), and the
// format the Audio banks window shows for a bank's sample (debug::gui::SampleFormatName), from waves written by hand.

#include <cstdint>

#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/Device/OutputSwitches.h"
#include "Debug/AudioBankSample.h"

using namespace openblack;

namespace
{
void Put16(std::vector<uint8_t>& b, uint32_t v)
{
	b.push_back(static_cast<uint8_t>(v));
	b.push_back(static_cast<uint8_t>(v >> 8));
}

void Put32(std::vector<uint8_t>& b, uint32_t v)
{
	Put16(b, v & 0xFFFF);
	Put16(b, v >> 16);
}

/// A RIFF wave of one "fmt " chunk (tag, 1 channel, 22050 Hz) and a data chunk of `data` bytes
std::vector<uint8_t> Wave(uint16_t tag, uint16_t blockAlign, uint16_t bits, uint32_t data)
{
	std::vector<uint8_t> file = {'R', 'I', 'F', 'F'};
	Put32(file, 4 + 8 + 16 + 8 + data);
	file.insert(file.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
	Put32(file, 16);
	Put16(file, tag);
	Put16(file, 1);
	Put32(file, 22050);
	Put32(file, 22050 * blockAlign);
	Put16(file, blockAlign);
	Put16(file, bits);
	file.insert(file.end(), {'d', 'a', 't', 'a'});
	Put32(file, data);
	file.resize(file.size() + data, 0);
	return file;
}
} // namespace

TEST(AudioMenu, SwitchesAreOnByDefault)
{
	const audio::OutputSwitches switches;
	EXPECT_TRUE(switches.sounds);
	EXPECT_TRUE(switches.music);
}

TEST(AudioMenu, AnOffSwitchSilencesAndAnOnOneKeepsTheGain)
{
	EXPECT_EQ(audio::SwitchedGain(true, 0.75f), 0.75f);
	EXPECT_EQ(audio::SwitchedGain(true, 0.0f), 0.0f);
	EXPECT_EQ(audio::SwitchedGain(false, 0.75f), 0.0f);
	EXPECT_EQ(audio::SwitchedGain(false, 1.0f), 0.0f);
}

TEST(AudioMenu, TheFormatOfAWave)
{
	EXPECT_EQ(debug::gui::SampleFormatName(Wave(1, 2, 16, 64)), "PCM");
	EXPECT_EQ(debug::gui::SampleFormatName(Wave(1, 1, 8, 64)), "PCM");
	EXPECT_EQ(debug::gui::SampleFormatName(Wave(2, 256, 4, 256)), "MS-ADPCM");
	EXPECT_EQ(debug::gui::SampleFormatName(Wave(0x11, 256, 4, 256)), "IMA ADPCM");
}

TEST(AudioMenu, TheFormatOfAnMpegWave)
{
	// The game's MPEG waves say 0 bits per sample, which the wave decoder does not open: the tag is read on its own
	EXPECT_EQ(debug::gui::SampleFormatName(Wave(0x50, 1, 0, 64)), "MPEG");
	EXPECT_EQ(debug::gui::SampleFormatName(Wave(0x50, 1, 16, 64)), "MPEG");
	EXPECT_EQ(debug::gui::SampleFormatName(Wave(0x55, 1, 0, 64)), "MPEG layer III");
}

TEST(AudioMenu, BytesThatAreNotAWave)
{
	// The music segments: MPEG frames with no wave header
	const std::vector<uint8_t> frames = {0xFF, 0xFD, 0x94, 0x00, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(debug::gui::SampleFormatName(frames), "raw MPEG");
	EXPECT_EQ(debug::gui::SampleFormatName(std::vector<uint8_t> {}), "raw MPEG");
	// A RIFF wave without a "fmt " chunk
	const std::vector<uint8_t> broken = {'R', 'I', 'F', 'F', 4, 0, 0, 0, 'W', 'A', 'V', 'E'};
	EXPECT_EQ(debug::gui::SampleFormatName(broken), "broken wave");
}
