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

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Common/Zoomer.h"
#include "Help/SpiritAnimClip.h"

// The two advisor spirits as pure logic: AdvisorSpiritController, the two help spirits that hold the targets (good and evil)
// and AdvisorSpirit's motion and anim stack. Nothing here draws, plays a sound or reads the Locator: the camera, the voice
// and the random streams come in through Queries, and each frame gives, per dude, the hover position, the model matrix,
// the alpha, the in-world blend and the list of clips the renderer turns into a pose with SpiritAnimClip.h (SetPose /
// ApplyAdditive).
//
// Conventions: "hover space" is hx = (px - W/2) / (W/2), hy = (py - H/2) / (W/2) (both axes over the half width).
// W / H are the screen size (unsigned 16 bits; W/2 and H/2 by a shift). Dude 0 is the good spirit (MarkGood.Hd), dude 1
// the evil one (help spirit type 1 -> 0, any other -> 1). Marks: (inferred) deduced from use, (approximate) not checked
// exactly, (pending) not read.

namespace openblack::help
{
struct HelpDudeFile;
}

namespace openblack::help::spirits
{

constexpr int k_Dudes = 2;
constexpr int k_GoodDude = 0;
constexpr int k_EvilDude = 1;
constexpr size_t k_AnimSlots = 80; ///< The size of the anim name table
constexpr size_t k_Zones = 6;      ///< The hover zones the dude feels

/// The anim indices AdvisorSpirit uses by number
namespace anim
{
constexpr uint32_t k_Stand = 0;
constexpr uint32_t k_Hover = 1;
constexpr uint32_t k_HoverLeft = 2;
constexpr uint32_t k_HoverRight = 3;
constexpr uint32_t k_Wingflap = 4;
constexpr uint32_t k_LeftEyeShut = 5;
constexpr uint32_t k_RightEyeShut = 6;
constexpr uint32_t k_VowelE = 7;
constexpr uint32_t k_FirstEmotion = 10; ///< 10..17 Normal..Furious
constexpr uint32_t k_LookLR = 18;
constexpr uint32_t k_LookUD = 19;
constexpr uint32_t k_PointLIn = 27;
constexpr uint32_t k_PointL = 28;
constexpr uint32_t k_PointRIn = 29;
constexpr uint32_t k_PointR = 30;
constexpr uint32_t k_HoverStable = 31;
constexpr uint32_t k_AvoidL = 32; ///< 32..35 L / R / U / D
constexpr uint32_t k_PointAtCamera = 42;
constexpr uint32_t k_GoInvisible = 46;
constexpr uint32_t k_ClingL = 65; ///< 65..68 L / R / U / D
constexpr uint32_t k_ClingR = 66;
constexpr uint32_t k_ClingU = 67;
constexpr uint32_t k_ClingD = 68;
constexpr uint32_t k_GimmeFive = 69;
constexpr uint32_t k_LookLRStable = 70;
constexpr uint32_t k_LookUDStable = 71;
constexpr uint32_t k_Last = 0x50; ///< PLAY_SPIRIT_ANIM accepts 0..0x50
} // namespace anim

/// AdvisorSpirit's current and queued states
namespace dude_state
{
constexpr uint32_t k_Hover = 4;
constexpr uint32_t k_Point = 8;        ///< request; resolved to an intro 0x19 / 0x1A
constexpr uint32_t k_PointHoldL = 9;   ///< &1 left arm (Point L 28)
constexpr uint32_t k_PointHoldR = 0xA; ///< &2 right arm (Point R 30)
constexpr uint32_t k_PointIntroL = 0x19;
constexpr uint32_t k_PointIntroR = 0x1A;
constexpr uint32_t k_PointOutroL = 0x29;
constexpr uint32_t k_PointOutroR = 0x2A;
constexpr uint32_t k_Avoid = 0x40;
constexpr uint32_t k_FlyToAnim = 0x80;
constexpr uint32_t k_Cling = 0x100;
constexpr uint32_t k_ClingArrive = 0x110;
constexpr uint32_t k_ClingLeave = 0x120;
constexpr uint32_t k_ScriptedAnim = 0x200;
} // namespace dude_state

/// AdvisorSpiritController's state of each dude
enum class ControlState : int32_t
{
	Home = 0, ///< hidden at the home point
	GoingHome = 1,
	Out = 2,
	Clinging = 3,
};

/// The screen edge a clinging dude holds on to
enum class Edge : int32_t
{
	Bottom = 0,
	Left = 1,
	Top = 2,
	Right = 3,
};

/// (1 - cos(pi t)) / 2, 0 below 0 and 1 above 1
[[nodiscard]] float Smooth(float t);

/// The screen size in pixels
struct Screen
{
	uint16_t width {640};
	uint16_t height {480};
	[[nodiscard]] int32_t HalfWidth() const { return width >> 1; }
	[[nodiscard]] int32_t HalfHeight() const { return height >> 1; }
};

/// A point projected to the screen
struct ProjectedPoint
{
	int32_t x {0};
	int32_t y {0};
	float depth {0.0f};
};

/// What a script's game thing gives the per-turn refresh
struct ObjectInfo
{
	glm::vec3 position {0.0f}; ///< (x / 6553.6, altitude + the y offset, z / 6553.6)
	float height {0.0f};       ///< The object's height (added for a point, not for a look)
};

/// One audio tag (a 20-byte record): time, who, action, index, value
struct AudioTag
{
	float time {0.0f};
	int32_t who {0};    ///< 0 the speaker (T), 1 the other (O, !), 2 the good one (G), 3 the evil one (E), 4 both (B, *)
	int32_t action {0}; ///< 1 A play, 2 AS loop, 3 AR / R release, 4 E emotion, 5 L / 6 LS look mode, 7 LR restore
	int32_t index {0};  ///< anim (prefix match), emotion or look mode
	int32_t value {100};
};
/// The audio tag builder's per-label loop: one tag parsed at a time until the NUL, "[" then
/// "<talker><type> <name>[digits]" entries; each tag gets the cue's time. `errors` counts the "unrecognised ..." cases.
[[nodiscard]] std::vector<AudioTag> ParseAudioTags(std::string_view label, float time, int* errors = nullptr);

/// The per-dude data AdvisorSpirit takes from its .hd
struct DudeData
{
	std::array<const SpiritAnimClip*, k_AnimSlots> clips {}; ///< nullptr = empty slot
	std::array<float, k_AnimSlots> loopStart {};
	std::array<float, k_AnimSlots> loopEnd {};
	std::array<uint8_t, k_AnimSlots> flags {};
	std::array<std::array<float, 16>, 8> faces {}; ///< 8 emotion records of 16 floats
	uint32_t startEmotion {0};
	float modelSize {0.0f};  ///< 63.423 / 79.154 in the files
	float nearDepth {15.0f}; ///< 8.7333 in the file
	float farDepth {10.0f};  ///< x 1.2 on load
	float scale {5.0f};      ///< S, the model scale
	float pitchOffset {0.0f};
	float fingertipOffsetRow0 {0.12f};
	float fingertipOffsetRow2 {0.0f};

	/// From a loaded file: the clips point into `file` (keep it alive), the far depth x 1.2
	static DudeData FromFile(const HelpDudeFile& file);
};

/// One entry of the per-frame anim list, in the original's order
struct AnimLayer
{
	enum class Kind : uint8_t
	{
		Set,      ///< SetPose(clip, ms) with the stand clip's key 0 as fill
		SetBlend, ///< Set(clip, ms) and Set(clipB, msB) lerped per float by `blend` in absolute bone space
		Add,      ///< ApplyAdditive(clip, ms, frames[referenceKey]) (ApplyAnim and the direct calls)
	};
	Kind kind {Kind::Add};
	uint32_t clip {0};
	int32_t milliseconds {0};
	uint32_t referenceKey {0};
	uint32_t clipB {0};
	int32_t millisecondsB {0};
	float blend {0.0f};
};

/// One particle of the puff (16 of them, made by the first draw of a puff)
struct PuffParticle
{
	glm::vec3 velocity {0.0f}; ///< Random(-0.8, 0.8), Random(-1.9, 1.5), Random(-1, 1) (z unused)
	float sizeBase {0.0f};     ///< Random(4, 8)
	float age {0.0f};          ///< In the puff's time units (ms x 0.0032)
	float spin {0.0f};         ///< Random(-2, 2)
	float k {0.0f};            ///< (1 - vy) + 1
	uint32_t grey {0};         ///< (g, g, g), g = trunc(Random(116, 250)), the first draw of the six
	/// This frame's draw: trunc(clamp(trunc(255 - 32 age) fade, 0, 255)) with the age before its step; <= 0 is not
	/// drawn
	int32_t drawAlpha {0};
	/// vy before this frame's drift (the drift comes after the sprite's position)
	float drawVelocityY {0.0f};
};
constexpr size_t k_PuffParticles = 16;

/// A sound effect request (anim, phase, in-game bank): the call, not its effect ((pending) the crossing rule)
struct AnimSound
{
	uint32_t anim {0};
	float phase {0.0f};
};

/// The inputs of one frame
struct FrameInput
{
	float dt {0.0f};     ///< The frame time x 0.001
	int32_t frameMs {0}; ///< For the alpha fade and the puff: the frame time (<= 500) / the game time step
	Screen screen {};
	glm::ivec2 mouse {0};    ///< The mouse position in pixels
	uint32_t tickMs {0};     ///< GetTickCount (the GoInvisible flicker)
	bool wideScreen {false}; ///< Read by Feel and the mouse zone
};

/// What the lip sync left for the mouth
struct LipSyncFrame
{
	/// (GetTickCount - the sentence's start tick) x 0.001, then the play position x 0.001 when it is >= 0. The tag
	/// walker runs on it in both cases, so the tags fire on the tick time while the play position is still -1 (Say's
	/// delay of up to 500 ms)
	float time {0.0f};
	/// The play position was >= 0: the vowels run
	bool playing {false};
	/// The key the last lip sync key step left (stale when it was skipped, no PCM)
	std::array<float, 3> weights {};
};

/// What the logic reads from the rest of the game; unset gives the value written next to each
struct Queries
{
	/// Unset: game_random::LocalRand
	std::function<uint32_t(int32_t n)> localRand;
	/// Unset: game_random::LocalFloatRand
	std::function<float(float x)> localFloatRand;
	/// The CRT rand stream. Unset: game_random::crt::Random
	std::function<float(float a, float b)> random;

	/// audio::advisor::IsTalking. Unset: false
	std::function<bool(int dude)> isTalking;
	/// Talking or just stopped (audio::advisor::TalkingOrJustStopped). Unset: isTalking
	std::function<bool(int dude)> talkedRecently;
	/// Saying a sentence (audio::advisor::Active). Unset: false
	std::function<bool(int dude)> sayActive;
	/// The lip sync's sound part this frame (audio::advisor::LipSyncThisFrame): nullopt when it stopped at IsTalking
	/// or at no sentence, the only case where the tag walker does not run. Unset: nullopt (no vowels, no tags)
	std::function<std::optional<LipSyncFrame>(int dude)> lipSync;

	/// The world point under a pixel at a camera depth. Unset: (px.x, px.y, depth)
	std::function<glm::vec3(glm::vec2 pixel, float depth)> pointFromScreen;
	/// A world point in pixels, nullopt on failure (WorldToHover; `force` also keeps points off the screen).
	/// Unset: (p.x, p.y)
	std::function<std::optional<glm::vec2>(const glm::vec3& p, bool force)> worldToPixel;
	/// The engine's point projection, nullopt when it fails. Unset: (p.x, p.y, p.z)
	std::function<std::optional<ProjectedPoint>(const glm::vec3& p)> projectPoint;
	/// The near clip. Unset: 0
	std::function<float()> nearClip;
	/// The rows of the inverse view matrix with the translation zeroed: the camera's world axes (inferred). Every
	/// glm::mat3 here holds the original's row i in [i]. Unset: identity
	std::function<glm::mat3()> cameraAxes;
	/// The camera position (look mode 2). Unset: (0, 0, 0)
	std::function<glm::vec3()> cameraPosition;
	/// The land's altitude at world (x, z). Unset: 0
	std::function<float(float x, float z)> altitude;
	/// The head angles for the look target: the yaw / pitch already clamped to +-1.0472 and x 0.477465 ([-0.5, 0.5]);
	/// nullopt when the target is behind the head (local y < 0), where the original writes nothing and the previous
	/// values stay. Needs the head bone, so it is the renderer's. Unset: (0, 0)
	std::function<std::optional<glm::vec2>(int dude, const glm::mat3& rows, const glm::vec3& position, const glm::vec3& target)>
	    headAngles;
	/// The fingertip of the point arm (bone 0 x M + r0 (fingertipOffsetRow0 x size) + r2 (fingertipOffsetRow2 x size));
	/// needs the pose. Unset: the model position
	std::function<glm::vec3(int dude, const glm::mat3& rows, const glm::vec3& position)> fingertip;
	/// A script's game thing (available, position, height). Unset: never available
	std::function<std::optional<ObjectInfo>(uint32_t object)> object;
};

class AdvisorSpiritController;

/// One AdvisorSpirit: its motion, its states and its anim stack
class AdvisorSpirit
{
public:
	/// Set up as the good or the evil one, then SetState(4)
	AdvisorSpirit(int index, const DudeData& data, AdvisorSpiritController& control);
	/// It keeps references to its data and its control: neither copied nor moved
	AdvisorSpirit(const AdvisorSpirit&) = delete;
	AdvisorSpirit(AdvisorSpirit&&) = delete;
	AdvisorSpirit& operator=(const AdvisorSpirit&) = delete;
	AdvisorSpirit& operator=(AdvisorSpirit&&) = delete;
	~AdvisorSpirit() = default;

	// --- small members of the original ---
	/// The hover snapped to a pixel (both Zoomers SetPosition), the trail reset, SetState(4, 0)
	void SetPosition(glm::ivec2 pixel);
	/// SetHoverX / SetHoverY to a pixel in `seconds`, the cling target = the target, SnapClingEdge, SetState(4, 0)
	void FlyTo(glm::ivec2 pixel, float seconds, bool clamp);
	/// Ignored while the puff runs; clamp to [-0.75, 0.75]; then
	/// Zoomer::SetDestinationWithSpeedAndTime(target, 0, T) inline (T < 0.001 snaps)
	void SetHoverX(float target, float seconds, bool clamp);
	/// The same on y, clamp [-0.6, 0.6]
	void SetHoverY(float target, float seconds, bool clamp);
	/// The target emotion, peak = max(peak, 0), the emotion time reset
	void SetEmotion(uint32_t emotion, float peak);
	/// The 32 trail points reset to the hover (the trail is drawn by the renderer: a counter here)
	void ResetTrail() { ++_trailResets; }
	/// Every anim slot's mode = 0
	void ClearAnims();
	/// The cling target and the fly target = (hx, hy), SnapClingEdge, from home a snap to the edge's off-screen point
	/// (Anchor) and a trail reset, then SetState(0x100, 0)
	void Cling(float hx, float hy, bool fromHome);
	/// Not in 0x120; |x| > 1.28205 |y| -> right (x = 1.04) or left (-1.04), else bottom (y = 0.78) or top
	/// (-0.78)
	void SnapClingEdge();
	/// 0x80 (current or queued) heading to 0x200, or 0x200 current or queued
	[[nodiscard]] bool IsPlayingAnim() const;
	/// Anim 0 while playing -> SetState(4, 0); else the fly target, the anim, the speed and 0x200 after the flight,
	/// SetState(0x80, 0)
	void PlayAnim(float hx, float hy, uint32_t anim, float speed);
	/// Not off screen, the point target = the pixel in hover space, SetState(8, 0)
	void ScreenPoint(glm::ivec2 pixel);
	/// (side 8.0, height 5.0): world Zoomers (snap out of the world, else 0.5 s), the in-world target, side and height,
	/// the projection test, the hover target or (0, 1) off screen, SetState(8, 0), the look target
	void PointAt(const glm::vec3& position, bool inWorld, float side, float height);
	void SetState(uint32_t next, bool force);
	/// The puff starts (running, time 0), the 16 particles drawn again with Random if the draw has made them, alpha
	/// target = alpha > 0.5 ? 0 : 1
	void StartPuff();

	// --- per frame ---
	void UpdateMotion(float dt, bool focus, float zMin, bool sfx);
	/// The look mode and target
	void UpdateLookTarget(float dt, bool engaged, bool partnerOut, bool talkOrPoint, const glm::vec3* lookPosition);
	/// Head angles, their smoothing and the look layers 18 / 19 or 70 / 71
	void UpdateHead(float dt);
	/// The draw-side part of the dude's draw: the alpha byte (with the GoInvisible flicker), the fade +3/s / -2/s, the
	/// puff (its particles made on its first draw, their ages, alphas and drift)
	void UpdateDraw(int32_t frameMs, uint32_t tickMs);
	/// Fires one audio tag
	void FireTag(const AudioTag& tag, bool resolve, bool apply);

	// --- state, read by the renderer and the tests ---
	[[nodiscard]] int Index() const { return _index; }
	[[nodiscard]] uint32_t State() const { return _state; }
	[[nodiscard]] uint32_t QueuedState() const { return _queuedState; }
	[[nodiscard]] float StateTime() const { return _stateTime; }
	[[nodiscard]] glm::vec2 Hover() const { return {_hoverX.value, _hoverY.value}; }
	[[nodiscard]] const Zoomer& HoverX() const { return _hoverX; }
	[[nodiscard]] const Zoomer& HoverY() const { return _hoverY; }
	[[nodiscard]] const Zoomer& DepthChannel() const { return _depth; }
	[[nodiscard]] float Closeness() const { return _closeness; }
	[[nodiscard]] float ModelScale() const { return _modelScale; }
	void SetModelScale(float scale) { _modelScale = scale; }
	[[nodiscard]] float Alpha() const { return _alpha; }
	[[nodiscard]] float AlphaTarget() const { return _alphaTarget; }
	void SetAlpha(float alpha) { _alpha = alpha; }
	void SetAlphaTarget(float alpha) { _alphaTarget = alpha; }
	[[nodiscard]] int32_t AlphaByte() const { return _alphaByte; }
	[[nodiscard]] float InWorld() const { return _inWorld; }
	void SetInWorld(float blend) { _inWorld = blend; }
	[[nodiscard]] float InWorldTarget() const { return _inWorldTarget; }
	void SetInWorldTarget(float target) { _inWorldTarget = target; }
	[[nodiscard]] glm::vec3 WorldTarget() const { return _worldTarget.GetCurrentValue(); }
	[[nodiscard]] float WorldSide() const { return _worldSide; }
	[[nodiscard]] float WorldHeight() const { return _worldHeight; }
	[[nodiscard]] const glm::mat3& Rows() const { return _rows; }
	[[nodiscard]] const glm::vec3& Position() const { return _position; }
	[[nodiscard]] float Roll() const { return _roll; }
	[[nodiscard]] Edge ClingEdge() const { return _edge; }
	[[nodiscard]] glm::vec2 ClingTarget() const { return {_clingX, _clingY}; }
	[[nodiscard]] glm::vec2 PointTarget() const { return {_pointX, _pointY}; }
	[[nodiscard]] bool PointOffScreen() const { return _pointOffScreen; }
	[[nodiscard]] bool PuffRunning() const { return _puffRunning; }
	/// The puff's time (its draw side)
	[[nodiscard]] float PuffTime() const { return _puffTime; }
	/// clamp(PuffTime() x 4, 0, 1) of this frame's draw
	[[nodiscard]] float PuffFade() const { return _puffFade; }
	/// nullopt until the first draw of a puff makes them
	[[nodiscard]] const std::optional<std::array<PuffParticle, k_PuffParticles>>& PuffParticles() const
	{
		return _puffParticles;
	}
	[[nodiscard]] uint32_t TrailResets() const { return _trailResets; }
	[[nodiscard]] uint32_t Emotion() const { return _emotion; }
	[[nodiscard]] uint32_t EmotionTarget() const { return _emotionTarget; }
	[[nodiscard]] float EmotionWeight() const { return _emotionWeight; }
	[[nodiscard]] float EmotionPeak() const { return _emotionPeak; }
	[[nodiscard]] int32_t LookMode() const { return _lookMode; }
	[[nodiscard]] int32_t TagLookMode() const { return _tagLookMode; }
	[[nodiscard]] const std::optional<glm::vec3>& LookTarget() const { return _lookTarget; }
	[[nodiscard]] glm::vec2 HeadAngles() const { return {_head.x, _head.y}; }
	[[nodiscard]] const std::array<float, 16>& Face() const { return _face; }
	[[nodiscard]] uint32_t ActiveFlags() const { return _activeFlags; }
	[[nodiscard]] int32_t SlotMode(size_t i) const { return _slotMode[i]; }
	[[nodiscard]] float SlotPhase(size_t i) const { return _slotPhase[i]; }
	void SetSlot(size_t i, int32_t mode, float phase)
	{
		_slotMode[i] = mode;
		_slotPhase[i] = phase;
	}
	[[nodiscard]] const std::vector<AnimLayer>& Layers() const { return _layers; }
	[[nodiscard]] const std::vector<AnimSound>& Sounds() const { return _sounds; }
	/// During this frame's GoInvisible windows
	[[nodiscard]] bool Flicker() const { return _flicker; }
	[[nodiscard]] float PartnerDistance() const { return _partnerDistance; }
	[[nodiscard]] float HoverZoneStrength(size_t zone) const { return _zones[zone].strength; }

	/// Zone::Feel summed over the 6 zones minus 250 e^2
	[[nodiscard]] float Feel(float x, float y) const;
	/// The hover point in 3D, at depth nearFlag ? 2 near : lerp(nearDepth, farDepth, smooth(closeness))
	/// (1 + 0.3 k)
	[[nodiscard]] glm::vec3 HoverTo3D(float hx, float hy, float k, bool nearFlag) const;
	[[nodiscard]] std::optional<glm::vec2> WorldToHover(const glm::vec3& p, bool force) const;

	AdvisorSpirit* partner {nullptr}; ///< Set by the control's Process

private:
	friend class AdvisorSpiritController;

	struct Zone
	{
		float strength {0.0f};
		float x {0.0f};
		float y {0.0f};
		float inner {0.0f};
		float outer {0.0f};
		[[nodiscard]] float Feel(float px, float py) const;
	};

	[[nodiscard]] const SpiritAnimClip* Clip(uint32_t i) const { return i < k_AnimSlots ? _data.clips[i] : nullptr; }
	/// The layer and the root move added through the rows
	void ApplyAnim(uint32_t anim, float phase, float referencePhase, bool wrap);
	/// ApplyAdditive called directly at ms = clamp(trunc(dur * w), 0, dur - 1) with the key `referenceKey`
	void AddAt(uint32_t anim, float weight, uint32_t referenceKey);
	void Sound(uint32_t anim, float phase, bool sfx);
	void UpdateHoverPosition(float probability);
	void UpdateDepthSpacing(float zMin);
	void UpdateZones(float dt, bool focus);
	void UpdateBasePose(bool stable, float rate, bool sfx);
	/// Face record, blink, lids, emotion clip
	void UpdateFace(float dt);
	/// The lip sync: vowels, the tag walker and the 80 slots
	void UpdateAnimStack(float dt, bool sfx);
	/// The model matrix of UpdateMotion step 8 and the in-world pose of step 9
	void UpdateMatrix(float dt);
	/// The six Random draws of one puff particle in the original's order: grey, vx, vy, vz, size, spin; the age 0
	void RandomisePuffParticle(PuffParticle& particle) const;

	int _index;
	const DudeData& _data;
	AdvisorSpiritController& _control;

	uint32_t _state {0};
	uint32_t _queuedState {0};
	float _totalTime {0.0f};
	float _stateTime {0.0f};
	float _emotionTime {0.0f};
	float _stableBlend {0.0f};
	float _hoverClock {0.0f};
	float _hoverClock2 {0.0f};
	Zoomer _hoverX;
	Zoomer _hoverY;
	Zoomer _depth;
	Zoomer3 _worldTarget;
	float _closeness {0.0f};
	float _closenessTarget {1.0f};
	float _closenessRate {1.0f};
	float _modelScale {1.0f};
	bool _gimme {false};
	float _alpha {1.0f};
	float _alphaTarget {1.0f};
	int32_t _alphaByte {255};
	bool _flicker {false};
	bool _puffRunning {false};
	float _puffTime {0.0f};
	float _puffFade {0.0f}; ///< This frame's clamp(_puffTime x 4, 0, 1)
	std::optional<std::array<PuffParticle, k_PuffParticles>> _puffParticles;
	float _inWorld {0.0f};
	float _inWorldTarget {0.0f};
	float _worldSide {0.0f};
	float _worldHeight {0.0f};
	float _clingX {0.0f};
	float _clingY {0.0f};
	Edge _edge {Edge::Bottom};
	float _clingClock {0.0f};
	float _targetX {0.0f};
	float _targetY {0.0f};
	float _roll {0.0f};
	float _animSpeed {0.0f};
	uint32_t _scriptAnim {0};
	uint32_t _afterFly {0};
	bool _pointOffScreen {false};
	float _pointBlend {0.0f};
	float _pointX {0.0f};
	float _pointY {0.0f};
	uint32_t _avoidDir {0};
	uint32_t _activeFlags {0};
	float _partnerDistance {100.0f};
	std::array<Zone, k_Zones> _zones {};
	float _mouseSpeed {0.0f};
	glm::ivec2 _lastMouse {0};
	float _mouseInterest {0.0f};
	bool _lookToggle {false};
	float _lookTimer {0.0f};
	int32_t _lookMode {0}; ///< The mode UpdateLookTarget chose this frame
	int32_t _tagLookMode {0};
	int32_t _savedLookMode {0};
	std::optional<glm::vec3> _lookTarget;
	glm::vec3 _head {0.0f};
	glm::vec3 _headTarget {0.0f};
	uint32_t _emotion {0};
	float _emotionWeight {0.0f};
	uint32_t _emotionTarget {0};
	float _emotionPeak {0.0f};
	std::array<float, 16> _face {};
	float _blinkTimer {0.0f};
	std::array<int32_t, k_AnimSlots> _slotMode {};
	std::array<float, k_AnimSlots> _slotPhase {};
	std::array<float, k_AnimSlots> _slotLast {}; ///< Starts at -1
	std::vector<AudioTag> _tags;
	size_t _nextTag {0};
	glm::mat3 _rows {1.0f}; ///< Rows r0, r1, r2
	glm::vec3 _position {0.0f};
	uint32_t _trailResets {0};
	std::vector<AnimLayer> _layers;
	std::vector<AnimSound> _sounds;
};

/// AdvisorSpiritController and the two help spirits around it. The CHL-facing calls take the help spirit type
/// (1 good, 2 evil: ResolveScriptAdvisor of HelpSystem.h first).
class AdvisorSpiritController
{
public:
	/// Both dudes, then both placed at their home point, state 0
	AdvisorSpiritController(const DudeData& good, const DudeData& evil, Queries queries, Screen screen);
	/// The dudes hold a reference to it: neither copied nor moved
	AdvisorSpiritController(const AdvisorSpiritController&) = delete;
	AdvisorSpiritController(AdvisorSpiritController&&) = delete;
	AdvisorSpiritController& operator=(const AdvisorSpiritController&) = delete;
	AdvisorSpiritController& operator=(AdvisorSpiritController&&) = delete;
	~AdvisorSpiritController() = default;

	/// Type 1 -> dude 0, any other -> 1
	[[nodiscard]] static int DudeOf(int32_t helpSpirit) { return helpSpirit != 1 ? 1 : 0; }

	// --- AdvisorSpiritController, by dude ---
	/// The nearest of the four off-screen anchors, or (3W/2, -H/2) / (-W/2, -H/2) when within 8 px
	[[nodiscard]] glm::ivec2 HomePoint(int dude) const;
	/// (W/2, 3H/2), (-W/2, H/2), (W/2, -H/2), (3W/2, H/2)
	[[nodiscard]] glm::ivec2 Anchor(int edge) const;
	void Eject(int dude);
	void Appear(int dude);
	void Home(int dude);
	void Vanish(int dude);
	void Cling(int dude, float px, float py);
	/// Only out of home, FlyTo(trunc(px), trunc(py), 1 s, clamp)
	void Fly(int dude, float px, float py);
	void PlayAnim(int dude, float px, float py, uint32_t anim, float speed);
	/// Point mode 1
	void PointAtPosition(int dude, const glm::vec3& position, bool inWorld, float side, float height);
	/// Point mode 2
	void PointAtPixel(int dude, glm::ivec2 pixel);
	/// Point mode 0
	void StopPointing(int dude) { _pointMode[dude] = 0; }
	void LookAt(int dude, const glm::vec3& position);
	void StopLooking(int dude) { _lookOn[dude] = false; }
	/// State 0 or 1
	[[nodiscard]] bool IsHome(int dude) const;
	/// The delay Say gives the sentence: |hx| - 0.95 < 0 ? 0 : min((v + 1) 250, 500) ms (for audio::advisor::Say,
	/// which has no hover)
	[[nodiscard]] uint32_t SayDelayMs(int dude) const;

	// --- help spirits (type 1 / 2) as the CHL calls them ---
	/// CHL 7 SPIRIT_EJECT with isHelp, 300 SPIRIT_APPEAR with 1
	void SpiritEject(int32_t type, bool isHelp);
	/// CHL 8 SPIRIT_HOME with isHelp, 301 SPIRIT_DISAPPEAR with 1, END_DIALOGUE
	void SpiritHome(int32_t type, bool isHelp);
	/// CHL 9 SPIRIT_POINT_POS
	void SpiritPointPosition(int32_t type, const glm::vec3& position, bool inWorld);
	/// CHL 10 SPIRIT_POINT_GAME_THING: nothing for an unavailable object
	void SpiritPointObject(int32_t type, uint32_t object, bool inWorld);
	/// CHL 418 SPIRIT_SCREEN_POINT: `pixel` = (x W, y H) as the script computes it
	void SpiritScreenPoint(int32_t type, glm::ivec2 pixel);
	/// CHL 137 STOP_POINTING
	void SpiritStopPointing(int32_t type);
	/// CHL 139 LOOK_AT_POSITION
	void SpiritLookAtPosition(int32_t type, const glm::vec3& position);
	/// CHL 100 LOOK_GAME_THING: nothing for an unavailable object
	void SpiritLookObject(int32_t type, uint32_t object);
	/// CHL 138 STOP_LOOKING
	void SpiritStopLooking(int32_t type);
	/// CHL 140 PLAY_SPIRIT_ANIM, after the script's checks: x, y in 0..1
	void SpiritPlayAnim(int32_t type, float x, float y, uint32_t anim, float speed);
	/// CHL 165 SPIRIT_PLAYED pushes the negation
	[[nodiscard]] bool SpiritPlayingAnim(int32_t type) const;
	/// CHL 166 CLING_SPIRIT: x, y in 0..1
	void SpiritCling(int32_t type, float x, float y);
	/// CHL 167 FLY_SPIRIT: x, y in 0..1
	void SpiritFly(int32_t type, float x, float y);
	/// Both spirits, once a game turn: the point object, then the look object
	void ProcessTurn();

	// --- per frame ---
	/// The control's Process(dt, 0.4), then the draw-side update of every dude the original draws (state != 0)
	void Update(const FrameInput& input);
	/// The sentence's tags when it starts (from the WAV's cue labels, ParseAudioTags)
	void SetSentenceTags(int dude, std::vector<AudioTag> tags);

	[[nodiscard]] AdvisorSpirit& Dude(int dude) { return *_dudes[dude]; }
	[[nodiscard]] const AdvisorSpirit& Dude(int dude) const { return *_dudes[dude]; }
	[[nodiscard]] ControlState State(int dude) const { return _state[dude]; }
	[[nodiscard]] float Timer(int dude) const { return _timer[dude]; }
	[[nodiscard]] int Focus() const { return _focus; }
	[[nodiscard]] int32_t PointMode(int dude) const { return _pointMode[dude]; }
	[[nodiscard]] bool LookOn(int dude) const { return _lookOn[dude]; }
	[[nodiscard]] const Screen& GetScreen() const { return _frame.screen; }
	[[nodiscard]] const FrameInput& Frame() const { return _frame; }
	[[nodiscard]] const Queries& GetQueries() const { return _queries; }
	/// The in-world bob, one static for both dudes
	float worldBob {0.0f};

	// The random streams of the original call sites
	[[nodiscard]] uint32_t LocalRand(int32_t n) const;
	[[nodiscard]] float LocalFloatRand(float x) const;
	[[nodiscard]] float Random(float a, float b) const;

private:
	void Process(float dt, float focusBias);

	struct Spirit
	{
		int32_t type {1};
		uint32_t pointObject {0};
		uint32_t lookObject {0};
		bool pointInWorld {false};
	};

	Queries _queries;
	FrameInput _frame;
	std::array<std::unique_ptr<AdvisorSpirit>, k_Dudes> _dudes;
	std::array<ControlState, k_Dudes> _state {};
	std::array<float, k_Dudes> _timer {};
	std::array<float, k_Dudes> _pointSide {};   ///< 8.0
	std::array<float, k_Dudes> _pointHeight {}; ///< 5.0
	std::array<bool, k_Dudes> _pointInWorld {};
	int _focus {0};
	std::array<glm::vec3, k_Dudes> _pointPosition {};
	std::array<int32_t, k_Dudes> _pointMode {};
	std::array<glm::vec3, k_Dudes> _lookPosition {};
	std::array<bool, k_Dudes> _lookOn {};
	std::array<Spirit, k_Dudes> _spirits {}; ///< Good / evil, by dude
	uint32_t _flickerNext {0};
	int32_t _flickerMultiplier {0};

	friend class AdvisorSpirit;
};

} // namespace openblack::help::spirits
