/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/spdlog.h>

#include "Audio/AudioPlayer.h"

using openblack::audio::AudioPlayer;

namespace
{

/// Put a new "audio" logger with a sink that keeps the last messages
std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> RegisterAudioLogger()
{
	auto sink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(8);
	auto logger = std::make_shared<spdlog::logger>("audio", sink);
	logger->set_level(spdlog::level::trace);
	logger->set_pattern("%l %v");
	spdlog::drop("audio");
	spdlog::register_logger(logger);
	return sink;
}

} // namespace

// The audio library can send a message from its own thread after the program removed the logger, for example when
// the system adds or removes a sound device. Before, the program then used a null logger and stopped.
TEST(AudioLibraryLog, AMessageWithoutTheLoggerDoesNothing)
{
	spdlog::drop("audio");
	AudioPlayer::ForwardLibraryLog('E', "error");
	AudioPlayer::ForwardLibraryLog('W', "warning");
	AudioPlayer::ForwardLibraryLog('I', "information");
	SUCCEED();
}

TEST(AudioLibraryLog, AMessageFromTheLibraryThreadWithoutTheLoggerDoesNothing)
{
	spdlog::drop("audio");
	std::thread library([] { AudioPlayer::ForwardLibraryLog('I', "OnDeviceStateChanged(device, 0x1)"); });
	library.join();
	SUCCEED();
}

TEST(AudioLibraryLog, MessagesGoToTheAudioLoggerAtTheirLevel)
{
	const auto sink = RegisterAudioLogger();
	AudioPlayer::ForwardLibraryLog('E', "first");
	AudioPlayer::ForwardLibraryLog('W', "second");
	AudioPlayer::ForwardLibraryLog('I', "third");
	AudioPlayer::ForwardLibraryLog('?', "unknown");
	const auto lines = sink->last_formatted();
	spdlog::drop("audio");
	ASSERT_EQ(lines.size(), 3u);
	EXPECT_NE(lines[0].find("error first"), std::string::npos);
	EXPECT_NE(lines[1].find("warning second"), std::string::npos);
	EXPECT_NE(lines[2].find("info third"), std::string::npos);
}
