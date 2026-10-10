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
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

/// The rules of the scrolls and signs scripts put up (see ScriptHighlight), free of the game so that tests can check them
namespace openblack::ecs::script_highlights
{

/// A sign gives a tip; the scrolls mark challenges
[[nodiscard]] constexpr bool IsTipSign(HighlightInfo kind)
{
	return kind == HighlightInfo::DidYouKnowSign;
}

/// The sounds of the game's own bank a highlight plays: a scroll as it starts, a sign as the player taps it
constexpr uint32_t k_ScrollStartedSound = 134;
constexpr uint32_t k_SignTappedSound = 173;
/// The help script the advisors explain the signs with, the first time the player reads one
constexpr std::string_view k_FirstTipScript = "FirstDYKExplained";

/// The beat all highlights pulse to together: it goes round once every 1.26 seconds of turns, from 0 up to 1 and back
struct Pulse
{
	float phase {0.0f};
	float level {0.0f};
	/// The level a turn before, which the frames between turns blend from
	float previous {0.0f};
};

/// A turn of the beat, of a turn's milliseconds
void StepPulse(Pulse& pulse, float millisecondsPerTurn);
/// The beat between two turns, from 0.4 to 1
[[nodiscard]] float PulseShare(const Pulse& pulse, float turnFraction);

/// The colour of the beam a started scroll stands in, as 0xAARRGGBB: a pale blue one for silver; a yellow one for the
/// others, whose alpha is the whole part of the beat (so nearly always none)
[[nodiscard]] uint32_t BeamColour(HighlightInfo kind, float pulseShare);

/// An angle brought to a turn, as the game brings it, by whole turns towards none
[[nodiscard]] float WrapAngle(float radians);
/// A highlight turns half a turn each second of the frames' game time
[[nodiscard]] float Spin(float yAngle, float frameMilliseconds);
/// The angle that turns a highlight standing at a point to face the camera
[[nodiscard]] float FacingAngle(glm::vec3 camera, glm::vec3 at);
/// A scroll is drawn larger the further the camera is, from a third of its size at 10 or nearer to all of it at 30 or
/// further; a sign keeps its size
[[nodiscard]] float DrawnScale(HighlightInfo kind, float scale, float cameraDistance);

/// The glow before a highlight: shown unless it is a started sign
[[nodiscard]] constexpr bool GlowShown(HighlightInfo kind, bool active)
{
	return !(IsTipSign(kind) && active);
}
/// Its half size, from how far the highlight's model reaches, and where it is, part way from the model's middle to the
/// camera
[[nodiscard]] float GlowHalfSize(float radius);
[[nodiscard]] glm::vec3 GlowPosition(glm::vec3 centre, glm::vec3 camera, float radius);
/// How much it adds of its colour, out of 255, by the highlight's kind
[[nodiscard]] uint8_t GlowAlpha(HighlightInfo kind);
/// The picture of the sprite sheet it is: the first of 8 by 8
constexpr uint32_t k_GlowPicture = 0;
constexpr uint32_t k_GlowSheetPictures = 8;

/// A scroll is picked under the cursor by a ball a little larger than its model; a sign by its model
[[nodiscard]] constexpr bool PickedByBall(HighlightInfo kind)
{
	return !IsTipSign(kind);
}
constexpr float k_PickBallMargin = 0.5f;

/// Whether the hand's tap does anything: a sign once it has its tip, a scroll once it has started and has a challenge
[[nodiscard]] constexpr bool ValidToTap(HighlightInfo kind, uint32_t scriptId, bool active)
{
	return IsTipSign(kind) ? scriptId != 0 : active && scriptId != 0;
}

/// What a tap does
struct TapOutcome
{
	/// The help system hears of the tap of the player at this computer: one event for a highlight with a challenge or tip,
	/// another for one without
	std::optional<uint32_t> helpEvent;
	/// It starts (and nothing more happens without a challenge or tip)
	bool starts {false};
	/// A sign plays its sound when the player at this computer taps it
	bool signSound {false};
	/// A sign shows its tip
	bool showsTip {false};
	/// A scroll plays its challenge's last message again
	bool replaysChallenge {false};
};
[[nodiscard]] TapOutcome Tap(HighlightInfo kind, uint32_t scriptId, bool byThisPlayer);

/// A sign checks each eighth turn, counted from its own number, whether its tip has been read, to start
[[nodiscard]] constexpr bool ChecksTipThisTurn(uint32_t ownNumber, uint32_t turn)
{
	return ((ownNumber + turn) & 7u) == 0;
}

/// The tips the player has read, in each of their five categories, at most 48 in each
class TipsRead
{
public:
	static constexpr size_t k_Categories = 5;
	static constexpr size_t k_MostInCategory = 48;

	[[nodiscard]] bool Has(uint32_t text, uint32_t category) const;
	/// A tip read; nothing for a category past the last, a full one or one already there
	void Add(uint32_t text, uint32_t category);
	[[nodiscard]] bool Empty() const;
	void Clear();

private:
	std::array<std::vector<uint32_t>, k_Categories> _texts;
};

/// A thing in the cell a highlight stands in, as the highlight weighs whether it stands on it
struct ThingBelow
{
	/// Its place within the cell, in metres from the cell's corner
	glm::vec2 inCell {0.0f};
	/// How far it reaches across, and how high its top is above the land
	float radius {0.0f};
	float top {0.0f};
	/// A living thing, or one that moves, is never stood on
	bool livingOrMoving {false};
};
/// The height above the land a highlight stands at: on top of the highest thing of its cell whose reach overlaps its own
/// (by the sum of their squared reaches), else on the land
[[nodiscard]] float HeightOnThings(glm::vec2 inCell, float radius, std::span<const ThingBelow> things);
/// A point's place within its map cell, in metres from the cell's corner, as the map's fixed point keeps it
[[nodiscard]] glm::vec2 InCell(glm::vec2 point);

} // namespace openblack::ecs::script_highlights
