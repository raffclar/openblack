/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <filesystem>
#include <vector>

#include <gtest/gtest.h>

#include "Sound/CustomMusicPlaylist.h"

using namespace openblack::audio::custom_music;

namespace
{
Playlist ThreeTracks()
{
	const std::vector<std::filesystem::path> files {"c.mp3", "a.mp3", "b.mp3"};
	Playlist playlist;
	playlist.SetTracks(ListTracks(files));
	return playlist;
}

Random Always(uint32_t roll)
{
	return [roll](uint32_t) { return roll; };
}
} // namespace

TEST(CustomMusicPlaylist, OnlyMp3FilesAreTracksInNameOrder)
{
	const std::vector<std::filesystem::path> files {"folder/Song B.MP3", "folder/notes.txt", "folder/song a.mp3",
	                                                "folder/cover.jpg"};
	const auto tracks = ListTracks(files);
	ASSERT_EQ(tracks.size(), 2u);
	EXPECT_EQ(tracks.at(0).fileName, "song a.mp3");
	EXPECT_EQ(tracks.at(0).songName, "song a");
	EXPECT_EQ(tracks.at(1).fileName, "Song B.MP3");
	EXPECT_EQ(tracks.at(1).songName, "Song B");
}

TEST(CustomMusicPlaylist, TheFirstTrackIsCurrentAndNoneWithoutTracks)
{
	Playlist empty;
	empty.SetTracks({});
	EXPECT_FALSE(empty.GetCurrent().has_value());
	EXPECT_EQ(empty.GetCurrentTrack(), nullptr);
	EXPECT_EQ(ThreeTracks().GetCurrent(), 0u);
}

TEST(CustomMusicPlaylist, NextAndPreviousGoRound)
{
	auto playlist = ThreeTracks();
	playlist.Previous(Always(0));
	EXPECT_EQ(playlist.GetCurrent(), 2u);
	playlist.Next(Always(0));
	EXPECT_EQ(playlist.GetCurrent(), 0u);
	playlist.Next(Always(0));
	EXPECT_EQ(playlist.GetCurrent(), 1u);
}

TEST(CustomMusicPlaylist, ShuffledTracksAreRandom)
{
	auto playlist = ThreeTracks();
	playlist.SetShuffle(true);
	playlist.Next(Always(2));
	EXPECT_EQ(playlist.GetCurrent(), 2u);
	EXPECT_TRUE(playlist.AfterEnd(Always(1)));
	EXPECT_EQ(playlist.GetCurrent(), 1u);
}

TEST(CustomMusicPlaylist, TheMusicStopsAfterTheLastTrackUnlessItRepeats)
{
	auto playlist = ThreeTracks();
	playlist.SetCurrent(1);
	EXPECT_TRUE(playlist.AfterEnd(Always(0)));
	EXPECT_EQ(playlist.GetCurrent(), 2u);
	EXPECT_FALSE(playlist.AfterEnd(Always(0)));
	EXPECT_EQ(playlist.GetCurrent(), 0u);
	playlist.SetCurrent(2);
	playlist.SetRepeat(true);
	EXPECT_TRUE(playlist.AfterEnd(Always(0)));
	EXPECT_EQ(playlist.GetCurrent(), 0u);
}

TEST(CustomMusicPlaylist, ATrackOutsideTheListIsNotMadeCurrent)
{
	auto playlist = ThreeTracks();
	playlist.SetCurrent(7);
	EXPECT_EQ(playlist.GetCurrent(), 0u);
}
