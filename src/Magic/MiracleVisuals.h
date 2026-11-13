/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "Visual/RenderModes.h"

/// How the miracles look outside their effects: the one-shot globes and the miracle floating in each, the rings round
/// an extreme miracle, the bands and bracelets on the hand that holds a miracle and its glow. Pure rules, which the
/// systems step and the renderer draws.
namespace openblack::magic::visuals
{

// The globe

/// The glint on the globe's dome runs through the 16 cells of a 4 by 4 sheet, 18 a second, round and round
inline constexpr float k_GlintFrameRate = 18.0f;
inline constexpr int k_GlintCells = 16;
/// The globe's own alpha is 150 of 255. The land's light, which is fully opaque, multiplies it by 255 and divides it by
/// 256, so the globe is added over what is behind it at 149.
inline constexpr uint8_t k_GlobeOwnAlpha = 150;
inline constexpr uint8_t k_GlobeAlpha = static_cast<uint8_t>((255u * k_GlobeOwnAlpha) >> 8u);
static_assert(k_GlobeAlpha == 149);
/// The game changes the materials of the globe's dome when it makes the globe. The dome adds its texture by the alpha of
/// the texture and writes its depth, also where the texture has no alpha. It shows only its front faces.
inline constexpr graphics::render_modes::Properties k_GlobeMaterials {
    .additive = true,
    .writesDepth = true,
    .bothSides = false,
    .textureAlpha = true,
};
/// The miracle in a globe is drawn at this share of the globe's size
inline constexpr float k_GlobeSeedScale = 0.6f;

/// The point by which the globe's dome is sorted among the things that blend: the middle of the dome, moved towards
/// the camera by the dome's radius. The game finds the way to the camera with the inverse square root of its 3D engine.
[[nodiscard]] glm::vec3 GlobeSortPoint(const glm::vec3& middle, const glm::vec3& camera, float radius);

/// The glint's frame after some seconds more
[[nodiscard]] float StepGlint(float frame, float seconds);
/// The corner of the glint's cell in the sheet: a quarter across for each of its column and row
[[nodiscard]] glm::vec2 GlintUvOffset(float frame);

// The rings round an extreme miracle in a globe or above a worship icon

/// They spin at 10.3 radians a second
inline constexpr float k_RingSpin = 10.3f;
/// Their alpha is 60 out of 256 of what they are drawn in
inline constexpr uint8_t k_RingAlpha = 60;
/// The ring's model is this much of the miracle's size
inline constexpr float k_RingScale = 0.2f;
/// And it is grey in its specular, 20 of 255 in each colour
inline constexpr uint32_t k_RingSpecular = 0x141414u;

/// One ring for the first power-up, two for the second; none for a plain miracle
[[nodiscard]] int RingCount(int powerUp);
/// A ring's alpha in something drawn at the given alpha: 34 in a globe, 59 above an icon
[[nodiscard]] uint8_t RingAlpha(uint8_t drawnAlpha);
/// The ring's turn before the camera's: laid flat, turned about the vertical by the spin (and half a radian more for the
/// second ring), tipped by 0.3, turned back or on by one radian, and tipped by 0.2 more
[[nodiscard]] glm::mat3 RingTurn(int ring, float spin);
/// Where a ring is drawn round a point: its turn, then turned to face the camera from where it is, scaled by the ring's
/// share times the band scale times the miracle's size
[[nodiscard]] glm::mat4 RingModel(int ring, float spin, const glm::vec3& point, float scale, const glm::vec3& camera);
/// The turn that faces something at a point towards the camera, from the way from the camera to it and world up made
/// square to that way. The rings' first axis points to the camera, their second up and their third across; the globe's
/// first is across, its second points to the camera and its third up.
enum class Facing : uint8_t
{
	Ring,
	Globe,
};
[[nodiscard]] glm::mat3 FacingCamera(const glm::vec3& point, const glm::vec3& camera, Facing facing);

// The creature spell phials in a globe

/// The creature spells' seeds, shown as phials that pulse
[[nodiscard]] bool IsPhialSeed(SpellSeedType seed);

/// Their texture runs through the 32 cells of a sheet of 8 by 4, an eighth each way, backwards, 15 a second
inline constexpr float k_PhialFrameRate = -15.0f;
inline constexpr int k_PhialCells = 32;
[[nodiscard]] float StepPhialFrame(float frame, float seconds);
/// The corner of the phial texture's cell, an eighth across for each of its column and row
[[nodiscard]] glm::vec2 PhialUvOffset(float frame);
/// How fast a phial pulses, in pulses a second: the freeze's slower
[[nodiscard]] float PhialPulseSpeed(int receiveType);
/// How far through its pulse a phial is at a phase: 0 to 1 and back over a turn
[[nodiscard]] float PhialPulse(float phase);
/// A phial's shape at a phase of its pulse, by how its creature takes the spell: swelling across and through to two and a
/// half times for one kind, squashing across and through for another, never changing its height; no spell the game
/// ships takes either kind
[[nodiscard]] glm::vec3 PhialScale(int receiveType, float phase);
/// The ice colour a frozen phial is tinted towards by its pulse, 0xRRGGBB
[[nodiscard]] uint32_t FreezeTint(float pulse);
/// A phial's draws: how big, of its size, and how faint, of 255, each of its one or two draws is, by how its creature
/// takes the spell, at a phase and pulse of its animation, in a globe drawn at an alpha
struct PhialDraw
{
	float size;
	uint8_t alpha;
	/// Takes the land's light and colour where it stands; otherwise lit in white by the sun, with none of the land's
	/// colour added, as a phial shrinking or growing is
	bool landLit;
};
/// The one or two draws of a phial, in an array of fixed size
struct PhialDrawList
{
	std::array<PhialDraw, 2> draws {};
	size_t count {0};

	[[nodiscard]] size_t size() const { return count; }
	[[nodiscard]] const PhialDraw& operator[](size_t index) const { return draws.at(index); }
	[[nodiscard]] const PhialDraw& back() const { return draws.at(count - 1); }
	[[nodiscard]] std::span<const PhialDraw> All() const { return std::span(draws).first(count); }
};
[[nodiscard]] PhialDrawList PhialDraws(int receiveType, float phase, float pulse, uint8_t globeAlpha);
/// The size of a phial in its last draw, of its own size: the growing copy of the one kind that draws two phials, else the
/// full phial
[[nodiscard]] float PhialLastDrawSize(int receiveType, float phase);

/// Where a seed's model is drawn above a globe or an icon: its middle raised by the seed's model height at the drawn
/// scale, turned about the vertical by its spin, at the seed's scale times the drawn scale, a phial's size and its shape.
/// The glints of the seed's effect sparkle on the points of the model in this place.
[[nodiscard]] glm::mat4 SeedModelPlacement(glm::vec3 middle, float meshHeight, float seedScale, float drawnScale, float spin,
                                           float size, glm::vec3 shape);

// A seed's picture in the temple

/// How far a seed's picture has turned, and how far its phial has gone through its texture and its pulse
struct SeedTurn
{
	float spin {0.0f};
	float phialFrame {0.0f};
	float phialPhase {0.0f};
};
/// A seed's picture after more seconds of the game's time. It turns two radians a second and stays between 0 and a
/// full turn. A phial (a receive type is given) goes through its texture and its pulse by the same time. A paused game
/// gives 0 seconds, so the picture does not move.
[[nodiscard]] SeedTurn TurnSeed(SeedTurn turn, std::optional<int> phialReceiveType, float gameSeconds);

// The seed above a stone hand of a village centre or above an icon at a worship site

/// How far the seed above an icon has turned, and the rings round it
struct IconSeedTurn
{
	SeedTurn seed;
	float ringSpin {0.0f};
};
/// The seed above an icon after a frame of the game's time. The seed turns as a seed's picture does, and its rings turn
/// at 10.3 radians a second. They move only in a frame that draws the icon's hand on the screen. In other frames, the
/// seed and its rings stay as they are.
[[nodiscard]] IconSeedTurn TurnIconSeed(IconSeedTurn turn, std::optional<int> phialReceiveType, float gameSeconds, bool drawn);

// The hand that holds a miracle

/// The bands that fly onto the hand, the bracelets that stay on it and the one that flies off
enum class BandKind : uint8_t
{
	/// Stays round the hand while it holds the miracle: one more for each power-up
	Bracelet,
	/// Flies from in front of the camera onto the hand, and goes
	FlyIn,
	/// Flies from the hand back to the camera, and goes
	FlyOff,
	/// Flies from in front of the camera onto the hand while an icon charges a miracle for the hand, and goes
	Pulse,
};

struct HandBand
{
	BandKind kind {BandKind::Bracelet};
	/// Its place along the hand: the further on, the further up the arm
	int index {0};
	float delay {0.0f};
	float age {0.0f};
	float duration {0.85f};
	uint8_t alphaFrom {20};
	uint8_t alphaTo {130};
	/// Its turn about the hand
	float spin {0.0f};
	/// It has reached the end of its flight
	bool done {false};
};

/// A band's radius round the hand, and its spin a second (a little faster the further up the arm)
inline constexpr float k_BandRadius = 10.0f;
inline constexpr float k_BandSpin = 12.0f;
inline constexpr float k_BandSpinPerIndex = 0.2f;
/// Where along the hand the bands sit, in the hand's own units
inline constexpr float k_BandFirstOffset = 10.0f;
inline constexpr float k_BandOffsetPerIndex = 40.0f;
/// A band flies from this far in front of the camera, at this share of its size
inline constexpr float k_BandFlyDistance = 4.0f;
inline constexpr float k_BandFlyScale = 0.5f;
/// The most bracelets the hand wears
inline constexpr int k_MostBracelets = 5;
/// The bands that fly in come this far apart, the first after this long when they wait for the miracle to settle
inline constexpr float k_FlyInSpacing = 0.1f;
inline constexpr float k_BandSettleDelay = 2.4f;

struct HandBands
{
	std::vector<HandBand> bracelets;
	std::vector<HandBand> flying;
};

/// The hand wears so many bracelets, adding or taking off the newest; new ones wait for the miracle to settle when
/// asked
void SetBracelets(HandBands& bands, int count, bool delayed);
/// Five bands fly onto the hand a tenth of a second apart
void AddFlyIn(HandBands& bands, bool delayed);
/// One band flies off the hand to the camera over a second
void AddFlyOff(HandBands& bands);
/// The bands age and spin; those that have flown go
void StepBands(HandBands& bands, float seconds);

/// While an icon at a worship site charges a miracle for the hand, bands pulse onto the hand. The more charge, the more
/// often they come, the faster they fly and the brighter they are.
struct ChargePulse
{
	/// An icon charged for the hand in the last frame
	bool charging {false};
	/// The seconds of game time since the last pulse
	float sinceLast {0.0f};
};
/// The share of the charge that the pulse shows, between none and all. Less than none, or no number, is none.
[[nodiscard]] float PulseShare(float charge);
/// One band pulses onto the hand for a share of the charge
void AddPulse(HandBands& bands, float share);
/// A frame of the game's time. The charge is the largest share of the icons that charge for the hand, none when no
/// icon charges for it. The first pulse comes in the frame that the charge starts. Then a pulse comes when more time
/// than the gap for the charge went by since the last pulse.
void StepChargePulse(HandBands& bands, ChargePulse& pulse, std::optional<float> charge, float seconds);

/// Where a band is in its flight and how it looks
struct BandPose
{
	/// Shown at all: it has stopped waiting
	bool shown;
	/// 0 in front of the camera, 1 round the hand
	float t;
	uint8_t alpha;
};
[[nodiscard]] BandPose PoseOf(const HandBand& band);
/// The band round the hand, in the hand's frame: about its length, at its place along it, turned by its spin
[[nodiscard]] glm::mat4 BandOnHand(const HandBand& band);
/// The band round the hand before it starts spinning, where the bands fly to
[[nodiscard]] glm::mat4 BandOnHandUnspun(const HandBand& band);
/// The band in front of the camera, in the camera's frame (right, up, forward), the near plane permitting
[[nodiscard]] glm::mat4 BandAtCamera(float nearPlane);
/// A band flying between the camera and the hand: turned as the camera is, its size and place eased between the two
[[nodiscard]] glm::mat4 BandBetween(const glm::mat4& atCamera, const glm::mat4& onHand, float t);
/// A bracelet settling: every part of its matrix eased between the two
[[nodiscard]] glm::mat4 BandBlended(const glm::mat4& atCamera, const glm::mat4& onHand, float t);

/// The hand's glow while it holds a miracle: the flow's sheet over the hand at this share, its 32 cells backwards at 20
/// a second
inline constexpr float k_GlowShare = 0.8f;
/// The glow's alpha, of 1: its share kept in whole 255ths in the colour the hand is drawn in, which drops the fraction
[[nodiscard]] constexpr float GlowAlpha()
{
	constexpr float k_ByteMax = 255.0f;
	return static_cast<float>(static_cast<int>(k_GlowShare * k_ByteMax)) / k_ByteMax;
}
inline constexpr float k_GlowFrameRate = -20.0f;
inline constexpr int k_GlowCells = 32;
/// The flow's texture is an eighth of the sheet across and down
inline constexpr float k_GlowCellSize = 0.125f;
[[nodiscard]] float StepGlowFrame(float frame, float seconds);
[[nodiscard]] glm::vec2 GlowUvOffset(float frame);

/// The announcer's voice as a miracle comes to the hand or powers up: the first, second or third power-up's, by its
/// level; none for a plain miracle. The sample's number in the spell dialogue bank.
[[nodiscard]] std::optional<int> PowerUpVoiceSample(int powerUp);

} // namespace openblack::magic::visuals
