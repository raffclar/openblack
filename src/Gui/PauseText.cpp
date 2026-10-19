/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PauseText.h"

#include <cmath>

#include <algorithm>

#include <glm/mat4x4.hpp>

#include "Common/X87.h"
#include "Common/X87Matrix.h"

namespace openblack::gui::pause_text
{

namespace
{

/// The angle in a half turn either side of none that shows the letter's face: a letter that turns past a quarter turn
/// shows its face again from the other side
float FrontAngle(float angle)
{
	if (angle > k_Pi)
	{
		angle = angle - k_Pi;
	}
	if (angle >= k_HalfPi)
	{
		angle = angle - k_Pi;
	}
	return angle;
}

/// A row turned about the camera's right axis by an angle of a cosine and a sine kept wide. The up part is the forward
/// part times the sine added to the up part times the cosine. The forward part is the forward part times the cosine
/// less the up part times the sine.
glm::vec3 TurnRow(glm::vec3 row, double cosine, double sine)
{
	const float up = x87::Mul(sine, row.z) + x87::Mul(cosine, row.y);
	const float forward = x87::Mul(cosine, row.z) - x87::Mul(sine, row.y);
	return {row.x, up, forward};
}

/// A direction made one long, unless it is none. The squares are added in the given order of the axes.
glm::vec3 Unit(glm::vec3 v, bool zFirst)
{
	if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
	{
		return v;
	}
	const float squares = zFirst ? ((v.z * v.z) + (v.y * v.y)) + (v.x * v.x) : ((v.x * v.x) + (v.y * v.y)) + (v.z * v.z);
	const float inverse = 1.0f / std::sqrt(squares);
	return {inverse * v.x, v.y * inverse, v.z * inverse};
}

} // namespace

Kind KindOf(bool paused, float speed)
{
	if (paused)
	{
		return Kind::Paused;
	}
	if (speed < 1.0f)
	{
		return Kind::Slower;
	}
	return speed > 1.0f ? Kind::Faster : Kind::None;
}

float Ease(float fade)
{
	if (fade < 0.0f)
	{
		return 0.0f;
	}
	if (fade > 1.0f)
	{
		return 1.0f;
	}
	return x87::Sub(1.0, x87::Cos(fade * k_Pi)) * 0.5f;
}

std::optional<Shown> Fader::Step(const Situation& situation, uint32_t milliseconds)
{
	const auto kind = KindOf(situation.paused, situation.speed);
	const auto elapsed = static_cast<float>(milliseconds);
	// The words that show fade in while the game stays in their state, and fade out otherwise
	if (kind == _kind && kind != Kind::None)
	{
		_fade = (elapsed * k_FadeInPerMillisecond) + _fade;
	}
	else
	{
		_fade = _fade - (elapsed * k_FadeOutPerMillisecond);
	}
	// Faded out fully, the words of the game's state take their place
	if (_fade <= 0.0f)
	{
		_fade = 0.0f;
		_kind = kind;
	}
	else if (_fade >= 1.0f)
	{
		_fade = 1.0f;
	}
	if (_kind == Kind::None || !situation.started)
	{
		return std::nullopt;
	}
	// The temple, a video, the editor and a box with a background hide the words at once. A script that holds the
	// game's speed hides the slower and quicker words, but not the paused words. The fade then starts again from none.
	if (situation.insideTemple || (situation.scriptHasSpeed && kind != Kind::Paused) || situation.editing ||
	    situation.videoPlaying || situation.boxWithBackground)
	{
		_kind = kind;
		_fade = 0.0f;
		return std::nullopt;
	}
	const float eased = Ease(_fade);
	if (eased == 0.0f)
	{
		return std::nullopt;
	}
	// The letters turn only while the words show
	_turn = (elapsed * k_TurnPerMillisecond) + _turn;
	if (_turn > k_FourPi)
	{
		_turn = _turn - k_FourPi;
	}
	return Shown {.kind = _kind, .eased = eased, .turn = _turn};
}

int32_t LayoutWidth(logo_effect::Screen screen)
{
	return std::min(screen.width, (screen.height * 4) / 3);
}

float Tilt(const logo_effect::View& view, const logo_effect::Lens& lens, logo_effect::Screen screen)
{
	const int32_t middle = screen.width >> 1;
	const auto centre = logo_effect::ScreenToWorld(view, lens, screen, {middle, screen.height >> 1}, k_TiltDepth);
	const auto row = logo_effect::ScreenToWorld(view, lens, screen, {middle, screen.height / k_RowDivisor}, k_TiltDepth);
	const auto toCentre = Unit(centre - view.position, false);
	const auto toRow = Unit(row - view.position, true);
	const float cosine = ((toCentre.z * toRow.z) + (toCentre.y * toRow.y)) + (toRow.x * toCentre.x);
	return x87::RoundToFloat(x87::Acos(cosine));
}

LetterPlace PlaceLetter(size_t index, size_t length, const Shown& shown, logo_effect::Screen screen, int32_t layoutWidth,
                        float tilt)
{
	const float depth = (shown.eased + 1.0f) * k_NearDepth;
	// The letters share half the width at the spacing depth, and come closer together as the words go away
	const auto half = static_cast<float>(layoutWidth >> 1);
	const float spacing = ((half / static_cast<float>(length)) / depth) * k_SpacingDepth;
	const auto middle = static_cast<float>(screen.width >> 1);
	const auto number = static_cast<float>(index);
	const float x = (middle - ((static_cast<float>(static_cast<int32_t>(length) - 1) * spacing) * 0.5f)) + (number * spacing);

	// The letter turns after the letters before it, by an angle that goes round two full turns
	float angle = shown.turn - (number * k_LetterLag);
	while (angle < 0.0f)
	{
		angle = angle + k_FourPi;
	}
	while (angle > k_FourPi)
	{
		angle = angle - k_FourPi;
	}
	std::array<glm::vec3, 3> rows {};
	if (angle < k_TwoPi)
	{
		// For the first turn the letter tips over about the camera's right axis
		const float front = FrontAngle(angle);
		const auto cosine = static_cast<float>(x87::Cos(front));
		const auto sine = static_cast<float>(x87::Sin(front));
		rows = {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, cosine, -sine), glm::vec3(0.0f, sine, cosine)};
	}
	else
	{
		// For the second turn the letter turns round the camera's up axis
		const float front = FrontAngle(angle - k_TwoPi);
		const auto cosine = static_cast<float>(x87::Cos(front));
		const auto sine = static_cast<float>(x87::Sin(front));
		rows = {glm::vec3(cosine, 0.0f, sine), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-sine, 0.0f, cosine)};
	}
	// The letter tilts to face the camera from its row, then turns a half turn about the right axis so that it stands
	// upright on the screen
	const double tiltCosine = x87::Cos(tilt);
	const double tiltSine = x87::Sin(tilt);
	const double halfCosine = x87::Cos(static_cast<double>(k_Pi));
	const double halfSine = x87::Sin(static_cast<double>(k_Pi));
	for (auto& row : rows)
	{
		row = TurnRow(TurnRow(row, tiltCosine, tiltSine), halfCosine, halfSine);
	}
	return {.pixel = {static_cast<int32_t>(x), screen.height / k_RowDivisor}, .depth = depth, .turn = rows};
}

TextFrame FrameOf(const LetterPlace& letter, const logo_effect::View& view, const logo_effect::Lens& lens,
                  logo_effect::Screen screen)
{
	// The turn of the letter in the camera's axes, then through the camera's axes into the world
	glm::mat4 turn {0.0f};
	glm::mat4 camera {0.0f};
	for (glm::length_t row = 0; row < 3; ++row)
	{
		turn[row] = glm::vec4(letter.turn.at(static_cast<size_t>(row)), 0.0f);
	}
	camera[0] = glm::vec4(view.right, 0.0f);
	camera[1] = glm::vec4(view.up, 0.0f);
	camera[2] = glm::vec4(view.forward, 0.0f);
	const auto world = x87::Product(turn, camera);
	return {.origin = logo_effect::ScreenToWorld(view, lens, screen, letter.pixel, letter.depth),
	        .axes = {glm::vec3(world[0]), glm::vec3(world[1]), glm::vec3(world[2])}};
}

uint8_t AlphaOf(float eased)
{
	return static_cast<uint8_t>(static_cast<int32_t>(eased * 255.0f));
}

float LetterOffset(float advance)
{
	return ((advance * k_LetterSize) * k_FontUnit) * -0.5f;
}

} // namespace openblack::gui::pause_text
