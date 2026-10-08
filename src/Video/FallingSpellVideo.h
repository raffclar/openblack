/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <functional>

#include "GameClock.h"

/// The falling spell's film (fall.bik) and the Temple fade it drives. Wiki: docs/bw1-notes/video.md.
///
/// - The script's SET_AVI_SEQUENCE(on, 2) -> KickOff (only when the local player has a creature); off -> End.
/// - KickOff: the previous one ended, the sequence mode set to 2, Init: the film played full screen (the game paused,
///   the wide screen on, as the intro) and the film's fade start moved to its end: the film has no fade of its own.
/// - Every frame in mode 2: the land is not drawn; the update runs (the state and the sounds by the film's ms), then
///   without a film or at state 4 End, else the film is drawn (base alpha 0x50 first, then the falling creature and its
///   sprites) and the liquid particles.
/// - State: 0 -> 1 at 13.45 s, 1 -> 2 at 37.75 s, 2 -> 3 at 43.9 s (a 1 s white Temple fade up, the music stopped),
///   3 -> 4 when that fade is up (the fade goes back down from white): End, whose skip is then the normal one: a
///   48-frame fade of the film with base alpha 0xFF, the pause given back. ESC ends it the same way.
///
/// Ported in src/Magic (FallingSpell, Graphics/RendererFallingSpell.cpp): the fall.cm2 path (computed, not yet applied
/// to the camera), the 16 screen-anchored smoke puffs, the one LightBurst (initialised twice) and the model light Init
/// saves, Draw puts at (0, 0, 1000) and Close puts back (the light, not the camera).
/// Still not ported: Init's fall.cm2 camera path, the falling copy of the player's creature and its hand glows, the
/// camera along the path with a pi / 4 field of view, the 16 sprites and the light bursts (with their finish frame
/// callback), everything the falling spell draws besides the film (the creature parts), the atmosphere update and the
/// other readers of the sequence mode (the camera, the player sparkles, the engine loop). The sounds and the music
/// stop are events (Hooks::sound / musicStop); GameHooks() plays them through Audio.h (PlaySoundEffect /
/// StopSoundEffect / MusicStop).
/// (approximate) In mode 2 the original draws the film inside the 3D pass, so the finish frame's bars and fade go over
/// it; openblack draws bars, film, fade (Renderer::DrawFinishFrameOverlays): the same picture while the bars are at
/// 100 % (the film's letterbox is their height). On the frame End runs, the original still shows the film with base
/// alpha 0x50 (its colour kept from before); openblack takes the colour when it draws, 0xFF (under the white fade, then
/// 1 - frame delta / 1000 opaque).
namespace openblack::video
{

class VideoPlayer;

/// The sequence mode: 2 while the falling spell plays, 1 inside the citadel, 0 otherwise (game_clock::SequenceMode)
inline constexpr int32_t k_SequenceModeNone = game_clock::k_SequenceModeNone;
inline constexpr int32_t k_SequenceModeCitadel = game_clock::k_SequenceModeCitadel;
inline constexpr int32_t k_SequenceModeFallingSpell = game_clock::k_SequenceModeFallingSpell;
/// The state that ends the falling spell
inline constexpr int32_t k_FallingSpellEndState = 4;
/// A film whose fps is <= 0 is given 24
inline constexpr int32_t k_FallingSpellFallbackFps = 0x18;
/// The state moves on when the film's ms are past these (signed)
inline constexpr int32_t k_FallingSpellStateOneMs = 0x348A;   ///< 13450
inline constexpr int32_t k_FallingSpellStateTwoMs = 0x9376;   ///< 37750
inline constexpr int32_t k_FallingSpellStateThreeMs = 0xAB7C; ///< 43900
/// The sound state moves on when the film's ms are past these
inline constexpr int32_t k_FallingSpellSoundOneMs = 0x442A;   ///< 17450
inline constexpr int32_t k_FallingSpellSoundTwoMs = 0x4BFA;   ///< 19450
inline constexpr int32_t k_FallingSpellSoundThreeMs = 0x7BA2; ///< 31650
/// The Temple fade is white
inline constexpr uint32_t k_FallingSpellFadeRgb = 0x00FFFFFF;
/// Every music channel fades out (MusicEngine.h)
inline constexpr int32_t k_FallingSpellMusicFade = 1;

/// The falling spell's film time: frame * 1000 / fps, in 32-bit signed arithmetic
[[nodiscard]] int32_t FallingSpellFilmMs(int32_t frame, int32_t fps) noexcept;

/// A sound of the update, for audio to play or stop. The banks are numbered as audio::SfxBank (BankTables.h)
struct FallingSpellSound
{
	enum class Kind : uint8_t
	{
		/// Played with the default options and the bank, 2D, the owner and the sample (and the pitch once)
		Play,
		/// Stopped by bank, owner and sample
		Stop,
	};
	enum class Bank : uint8_t
	{
		InGame = 1,    ///< audio/sfx/game/ingame.sad
		Spells = 3,    ///< audio/sfx/game/spells.sad
		ScriptSfx = 5, ///< audio/sfx/script/scriptsfx.sad
	};
	/// Which of the update's sound calls made it, in the update's order (an identifier for tests)
	enum class Cue : uint8_t
	{
		Rumble,
		LaserExplode,
		VolcanoStop,
		Creed,
		CitadelExplode,
		Volcano,
		HealChakra,
		CreedHigh,
		CreedStop,
		CreedHighStop,
		CitadelExplodeEnd,
	};
	/// The default play options' pitch (sample_play::Options): 100 percent
	static constexpr int32_t k_DefaultPitch = 100;

	Kind kind;
	Bank bank;
	int32_t sample;
	/// 0, 1 or 2 (no game thing: three "owners" so the three loops can be stopped one by one)
	uint32_t owner;
	int32_t pitch;
	Cue cue;

	bool operator==(const FallingSpellSound&) const = default;
};

/// The Temple fade: a full screen colour fade at 1.0 a second of frame time, shared by the citadel and the falling
/// spell; its colour goes to the screen fade
struct TempleFade
{
	/// The target
	float target {0.0f};
	/// The current value. The original starts at 1.0f, but the logo screen (the first pass of the game loop, single
	/// player) sets current = target = 0, the colour to 0xFF000000 and done to 0, and a new game sets current =
	/// target = 0: at rest, as here
	float current {0.0f};
	/// The colour's RGB (the logo screen's 0xFF000000 is 0 once masked)
	uint32_t rgb {0};
	/// + 1 each time the fade reaches its target
	int32_t done {0};

	/// Whether the fade updates this frame: unless target == current and the mode is not the citadel's (then the
	/// script fade runs); mode 3 runs neither
	[[nodiscard]] bool Runs(int32_t mode) const noexcept;
	/// One step of `deltaMs`: returns the screen fade's colour. Float arithmetic, as the original's single precision
	uint32_t Update(uint32_t deltaMs) noexcept;
};

/// The falling spell's film sequence and the sequence mode it sets
class FallingSpellVideo
{
public:
	struct Hooks
	{
		/// Whether the local player has a creature
		std::function<bool()> hasCreature;
		/// "data\spells\fall\fall.bik" as a path to open
		std::function<std::filesystem::path()> filmPath;
		/// The sounds of the update. Unset: nothing
		std::function<void(const FallingSpellSound& sound)> sound;
		/// The music stop. Unset: nothing
		std::function<void(int32_t fade)> musicStop;
		/// Sets the screen fade to the Temple fade's colour (ScreenFade::SetColour)
		std::function<void(uint32_t argb)> setScreenFadeColour;
	};
	/// openblack's: the local player's creature (PLAYER_ONE, inferred), FindPath("Data/Spells/fall/fall.bik"), the
	/// sounds through audio::PlaySoundEffect(PlayOptions) / StopSoundEffect (2D, owners None / Key(1) / Key(2)),
	/// audio::MusicStop and the screen fade (Locator::screenFade)
	[[nodiscard]] static Hooks GameHooks();

	FallingSpellVideo(VideoPlayer& player, Hooks hooks);

	/// Nothing without the local player's creature, else End() and Start()
	void KickOff();
	/// Sets the sequence mode to 2, marks the sequence active and plays the film. (openblack) OPENBLACK_TEST_VIDEO=fall
	/// calls it directly
	void Start();
	/// Nothing when not active; else the sequence mode back to 0, Close(), inactive and the normal skip of the film
	/// (VideoPlayer::Skip)
	void End();
	/// Each frame, after the film's service (VideoPlayer::Process): in mode 2 the update, then End() without a film or
	/// at state 4; then the Temple fade when it runs, `realMs` = the frame time (game_clock::FrameRealMs)
	void ProcessFrame(uint32_t realMs);
	/// The falling spell's update alone
	void Update();

	/// Whether the falling spell sequence is running
	[[nodiscard]] bool IsActive() const { return _active; }
	/// The sequence mode (game_clock::SequenceMode)
	[[nodiscard]] int32_t Mode() const { return game_clock::SequenceMode(); }
	/// In mode 2 the land, the objects, the hand and the interface are not drawn (the normal pass is skipped): only the
	/// film (and, not ported, the falling creature, its sprites and the liquid particles)
	[[nodiscard]] bool HidesWorld() const { return Mode() == k_SequenceModeFallingSpell; }
	/// 0..4
	[[nodiscard]] int32_t State() const { return _state; }
	/// 0..3
	[[nodiscard]] int32_t SoundState() const { return _soundState; }
	/// On from state 1. (not ported) The falling spell's draw rewrites it with whether a sprite is still seen
	[[nodiscard]] bool SparklesOn() const { return _sparklesOn; }
	/// The film ms of the last update (100 after Init)
	[[nodiscard]] int32_t LastMs() const { return _lastMs; }
	[[nodiscard]] const TempleFade& GetTempleFade() const { return _fade; }
	[[nodiscard]] TempleFade& GetTempleFade() { return _fade; }

private:
	/// The falling spell's init (the film's part)
	void Init();
	/// The falling spell's close (the film's part)
	void Close();
	void Sound(FallingSpellSound::Kind kind, FallingSpellSound::Bank bank, int32_t sample, uint32_t owner,
	           FallingSpellSound::Cue cue, int32_t pitch = FallingSpellSound::k_DefaultPitch) const;

	VideoPlayer& _player;
	Hooks _hooks;
	bool _active {false}; ///< The sequence is running
	int32_t _lastMs {0};
	bool _sparklesOn {false};
	int32_t _state {0};
	int32_t _soundState {0};
	TempleFade _fade;
};

/// The game's one, over video::Get() with GameHooks()
[[nodiscard]] FallingSpellVideo& GetFallingSpell();

} // namespace openblack::video
