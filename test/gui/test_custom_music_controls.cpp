/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/CustomMusicPlayerInterface.h"
#include "Gui/CustomMusicControlsRules.h"
#include "Gui/ScreenFit.h"

using namespace openblack;
using namespace openblack::gui::custom_music_controls;

namespace
{
/// A player that keeps what it is told
class FakePlayer final: public audio::CustomMusicPlayerInterface
{
public:
	void SetFiles(const std::vector<std::filesystem::path>& /*files*/) override {}
	[[nodiscard]] bool IsRunning() const override { return !tracks.empty(); }
	[[nodiscard]] PlayState GetPlayState() const override { return state; }
	[[nodiscard]] std::string GetSongName() const override
	{
		return current.has_value() ? tracks.at(*current).songName : std::string();
	}
	[[nodiscard]] const std::vector<audio::custom_music::Track>& GetTracks() const override { return tracks; }
	[[nodiscard]] std::optional<size_t> GetCurrentTrack() const override { return current; }
	void SetCurrentTrack(size_t track) override { current = track; }
	void Play() override
	{
		state = PlayState::Playing;
		++plays;
	}
	void Pause() override { state = PlayState::Paused; }
	void Stop() override { state = PlayState::Stopped; }
	void NextTrack() override { ++nexts; }
	void PreviousTrack() override { ++previous; }
	[[nodiscard]] uint32_t GetPosition() const override { return position; }
	[[nodiscard]] uint32_t GetLength() const override { return length; }
	void SetPosition(uint32_t milliseconds) override { position = milliseconds; }
	void SetVolume(uint32_t value) override { volume = value; }
	[[nodiscard]] uint32_t GetVolume() const override { return volume; }
	void SetShuffle(bool value) override { shuffle = value; }
	void SetRepeat(bool value) override { repeat = value; }
	[[nodiscard]] bool GetShuffle() const override { return shuffle; }
	[[nodiscard]] bool GetRepeat() const override { return repeat; }
	void Update() override {}

	std::vector<audio::custom_music::Track> tracks;
	std::optional<size_t> current;
	PlayState state {PlayState::Stopped};
	uint32_t position {0};
	uint32_t length {0};
	uint32_t volume {100};
	bool shuffle {true};
	bool repeat {true};
	int plays {0};
	int nexts {0};
	int previous {0};
};

FakePlayer TwoTracks()
{
	FakePlayer player;
	player.tracks = {{.path = "a.mp3", .fileName = "Tone A.mp3", .songName = "Tone A"},
	                 {.path = "b.mp3", .fileName = "Tone B.mp3", .songName = "Tone B"}};
	return player;
}
} // namespace

TEST(CustomMusicControls, TheButtonsSitAgainstTheTopRightAsOnTheDefaultScreen)
{
	// On the 800 by 600 screen the controls are the game's pixels from its right edge
	const auto play = ToRect(k_Buttons.at(static_cast<size_t>(Button::Play)).place);
	EXPECT_EQ(play.min, glm::ivec2(640, 20));
	EXPECT_EQ(play.max, glm::ivec2(680, 40));
	EXPECT_EQ(ToRect(k_Buttons.at(static_cast<size_t>(Button::Music)).place).min, glm::ivec2(760, 0));
	EXPECT_EQ(ToRect(k_PlayList).max, glm::ivec2(800, 400));
	EXPECT_EQ(ToRect(k_SongName).min, glm::ivec2(400, 0));
	EXPECT_EQ(Origin({800, 600}), glm::vec2(0.0f, 0.0f));
}

TEST(CustomMusicControls, TheLayoutsRightEdgeIsOnTheScreensRightEdge)
{
	for (const auto size : {glm::u16vec2 {1280, 1024}, glm::u16vec2 {1920, 1080}, glm::u16vec2 {2560, 1440},
	                        glm::u16vec2 {3440, 1440}, glm::u16vec2 {5120, 1440}})
	{
		const auto space = gui::FrontEndSpace(size.x, size.y);
		const auto origin = Origin(size);
		EXPECT_FLOAT_EQ(space.ToScreen({800.0f, 0.0f}).x + origin.x, static_cast<float>(size.x));
		EXPECT_FLOAT_EQ(space.ToScreen({800.0f, 0.0f}).y + origin.y, 0.0f);
	}
}

TEST(CustomMusicControls, IconsComeFromTheAtmosphereTexture)
{
	const gui::DialogRect rect {.min = {760, 0}, .max = {780, 20}};
	const auto music = IconOf(10, rect, false);
	EXPECT_EQ(music.corner, glm::ivec2(761, 1));
	EXPECT_FLOAT_EQ(music.uvMin.x, 0.25f + (2.0f / 16.0f));
	EXPECT_FLOAT_EQ(music.uvMin.y, 3.0f / 16.0f);
	EXPECT_FLOAT_EQ(music.uvMax.x, 0.25f + (3.0f / 16.0f));
	const auto play = IconOf(0, rect, true);
	EXPECT_EQ(play.corner, glm::ivec2(762, 2));
	EXPECT_FLOAT_EQ(play.uvMin.x, 0.25f);
	EXPECT_FLOAT_EQ(play.uvMin.y, 1.0f / 16.0f);
}

TEST(CustomMusicControls, TheMusicButtonBlendsTowardsTheMood)
{
	EXPECT_EQ(MusicButtonColour(std::nullopt), 0xffffffffu);
	EXPECT_EQ(MusicButtonColour(music_mood::Tint {.colour = 0xff00ff00, .amount = 0}), 0xffffffffu);
	// Half way to green: red and blue fall to 0x7f, green stays
	EXPECT_EQ(MusicButtonColour(music_mood::Tint {.colour = 0xff00ff00, .amount = 128}), 0xff7fff7fu);
	EXPECT_EQ(MusicButtonColour(music_mood::Tint {.colour = 0xffff0000, .amount = 255}), 0xffff0000u);
	EXPECT_EQ(Blend(0x00102030, 0x80302010, 128), 0x80202020u);
}

TEST(CustomMusicControls, TimesShowAsMinutesAndSeconds)
{
	EXPECT_EQ(TimeText(0), u"0:00");
	EXPECT_EQ(TimeText(-5000), u"0:00");
	EXPECT_EQ(TimeText(61999), u"1:01");
	EXPECT_EQ(PositionLabel(5000, 600000), u"0:05 / 10:00");
}

TEST(CustomMusicControls, ShowOnlyOverPlay)
{
	EXPECT_TRUE(Shows({}));
	EXPECT_FALSE(Shows({.inTemple = true}));
	EXPECT_FALSE(Shows({.videoPlays = true}));
	EXPECT_FALSE(Shows({.wideScreen = true}));
	EXPECT_FALSE(Shows({.creatureHeld = true}));
	EXPECT_FALSE(Shows({.dialogOpen = true}));
}

TEST(CustomMusicControls, StartGivesThePlayerFullVolumeNoShuffleNoRepeat)
{
	auto player = TwoTracks();
	Panel panel;
	Start(panel, player);
	EXPECT_EQ(player.volume, 255u);
	EXPECT_FALSE(player.shuffle);
	EXPECT_FALSE(player.repeat);
	player.volume = 7;
	Start(panel, player);
	EXPECT_EQ(player.volume, 7u);
}

TEST(CustomMusicControls, ButtonsTellThePlayer)
{
	auto player = TwoTracks();
	Panel panel;
	Click(panel, player, Button::Music);
	EXPECT_TRUE(panel.controlsShown);
	Click(panel, player, Button::Play);
	EXPECT_EQ(player.state, audio::CustomMusicPlayerInterface::PlayState::Playing);
	Click(panel, player, Button::Pause);
	EXPECT_EQ(player.state, audio::CustomMusicPlayerInterface::PlayState::Paused);
	Click(panel, player, Button::Stop);
	EXPECT_EQ(player.state, audio::CustomMusicPlayerInterface::PlayState::Stopped);
	Click(panel, player, Button::Next);
	Click(panel, player, Button::Previous);
	EXPECT_EQ(player.nexts, 1);
	EXPECT_EQ(player.previous, 1);
	player.shuffle = false;
	Click(panel, player, Button::Shuffle);
	EXPECT_TRUE(panel.shuffle);
	EXPECT_TRUE(player.shuffle);
	player.repeat = false;
	Click(panel, player, Button::Repeat);
	EXPECT_TRUE(panel.repeat);
	EXPECT_TRUE(player.repeat);
	Click(panel, player, Button::PlayList);
	EXPECT_TRUE(panel.playListShown);
	// The player has no rewind or fast forward
	EXPECT_FALSE(CanDo(Button::Rewind));
	EXPECT_FALSE(CanDo(Button::FastForward));
	EXPECT_TRUE(CanDo(Button::Play));
}

TEST(CustomMusicControls, NothingHappensWithoutTracks)
{
	FakePlayer player;
	Panel panel;
	Click(panel, player, Button::Music);
	EXPECT_FALSE(panel.controlsShown);
	panel.controlsShown = true;
	const auto view = Refresh(panel, player, std::nullopt);
	EXPECT_FALSE(view.running);
	EXPECT_FALSE(panel.controlsShown);
}

TEST(CustomMusicControls, APickedTrackThatIsNotCurrentPlays)
{
	auto player = TwoTracks();
	player.current = 0;
	PickTrack(player, 0);
	EXPECT_EQ(player.plays, 0);
	PickTrack(player, std::nullopt);
	EXPECT_EQ(player.plays, 0);
	PickTrack(player, 1);
	EXPECT_EQ(player.current, 1u);
	EXPECT_EQ(player.plays, 1);
}

TEST(CustomMusicControls, SlidersSetTheVolumeAndThePosition)
{
	auto player = TwoTracks();
	Panel panel;
	DragVolume(panel, player, 0.5f);
	EXPECT_EQ(panel.volume, 127u);
	EXPECT_EQ(player.volume, 127u);
	player.length = 60000;
	player.current = 0;
	(void)Refresh(panel, player, std::nullopt);
	DragPosition(panel, player, 0.25f);
	EXPECT_EQ(player.position, 15000u);
}

TEST(CustomMusicControls, ALookAtThePlayer)
{
	auto player = TwoTracks();
	player.current = 1;
	player.position = 30000;
	player.length = 120000;
	Panel panel;
	panel.volume = 51;
	const auto view = Refresh(panel, player, music_mood::Emotion {.mood = -1.0f, .fear = 0.0f});
	EXPECT_TRUE(view.running);
	EXPECT_EQ(view.positionLabel, u"0:30 / 2:00");
	EXPECT_FLOAT_EQ(view.position, 0.25f);
	EXPECT_FLOAT_EQ(view.volume, 0.2f);
	EXPECT_EQ(view.songName, u"Tone B");
	ASSERT_EQ(view.playList.size(), 2u);
	EXPECT_EQ(view.playList.at(0), u"Tone A.mp3");
	EXPECT_EQ(view.currentTrack, 1u);
	EXPECT_EQ(view.musicColour, 0xffff0000u);
	EXPECT_EQ(panel.length, 120000u);
}
