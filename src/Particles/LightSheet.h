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

#include <functional>
#include <numbers>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::particles
{

/// A sheet of light standing along a row of points, as the game raises one along a recognised gesture's trail. Over each
/// point it rises as a wall, its top spread out from the middle of the points a tenth further, its light brightest a
/// quarter of the way up and dark at the bottom and the top, a sheet of stars sliding up through it. Its height comes
/// from a strength fed in at its first point that runs along it one point every few hundredths of a second, times a wave
/// that rolls along it: each point's height is the strength it was given that long ago times the wave.
class LightSheet
{
public:
	/// The top of the sheet is spread out from the middle of its points by this, unless it is given another spread
	static constexpr float k_Spread = 1.1f;
	/// By default its strength moves one point along this often, in seconds
	static constexpr float k_ShiftSeconds = 0.03f;
	/// A ring of light stands on this many points by default
	static constexpr uint32_t k_RingPoints = 32;
	/// How far up the sheet its brightest line is
	static constexpr float k_BrightLine = 0.25f;
	/// The stars slide up the sheet this many times a second
	static constexpr float k_SlideSpeed = 1.0f;
	/// The wave has three crests along the sheet and rolls along it at three radians a second, between 0.5 and 1.1 of
	/// the sheet's height
	static constexpr float k_WaveSpan = 6.0f * std::numbers::pi_v<float>;
	static constexpr float k_WaveSpeed = 3.0f;
	/// The game holds the depth and the middle as doubles with the bits of the floats 0.3 and 0.8
	static constexpr double k_WaveDepth = static_cast<double>(0.3f);
	static constexpr double k_WaveMiddle = static_cast<double>(0.8f);
	/// A point's light is its height times this, at most 255
	static constexpr float k_LightPerHeight = 50.0f;
	/// The stars repeat this often along the sheet, by distance
	static constexpr float k_StarsPerUnit = 0.1f;
	/// The texture's row of the brightest line
	static constexpr float k_BrightRow = 0.75f;

	/// A pulsed sheet (see StartPulsed): its pulses fade out with a time constant of a second, it shows only while one of
	/// its points has more than this, a point's light is its strength times this, its middle line is halfway up, and its
	/// texture runs a tenth as fast along it as the stars do, repeating every hundred units
	static constexpr float k_PulseFadeRate = 1.0f;
	static constexpr float k_PulseShowsAbove = 0.02f;
	static constexpr float k_PulseLightPerStrength = 255.0f;
	static constexpr float k_PulseMiddleLine = 0.5f;
	/// Each frame a pulsed sheet's texture keeps only a fifth of how far it slid
	static constexpr float k_PulseSlideKept = 0.2f;

	/// What it is drawn with
	enum class Look : uint8_t
	{
		/// A sheet of stars sliding up it
		Stars,
		/// The force field's texture, sliding along it both ways at once
		ForceField,
	};

	/// A corner of the sheet in the world, its colour and the colour added after its texture as 0xAARRGGBB
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t argb;
		uint32_t specularArgb;
	};

	/// The height of the land at a point across the ground
	using GroundHeight = std::function<float(glm::vec2)>;
	/// The points of a ring round a centre across the ground, each on the land's height there. The first point is due
	/// along x from the centre, and the last point is on the first, so that the ring closes.
	[[nodiscard]] static std::vector<glm::vec3> RingPoints(glm::vec2 centre, float radius, uint32_t count,
	                                                       const GroundHeight& ground);

	/// Stands it on its points, its light of a colour (0xRRGGBB), its height and how often its strength moves one point
	/// along
	void Start(std::vector<glm::vec3> points, uint32_t rgb, float height, float shiftSeconds);
	[[nodiscard]] bool Started() const { return !_points.empty(); }
	/// Stands it as a pulsed sheet, as the camera's force field is: it rises the same height everywhere, unseen until
	/// pulsed where the camera hits it, each pulse fading out. It can stand on up to `capacity` points (see SetPoints).
	void StartPulsed(uint32_t rgb, float height, size_t capacity);
	[[nodiscard]] bool Pulsed() const { return _pulsed; }
	/// A pulsed sheet moves onto other points, as many as its capacity; each keeps the strength the point in its place had
	void SetPoints(std::vector<glm::vec3> points);
	/// A pulse at a point: each point of a pulsed sheet nearer than the radius across the ground gains strength, a whole
	/// one at the point and none at the radius
	void Pulse(const glm::vec3& centre, float radius);
	/// Whether a pulsed sheet has a point strong enough to be seen; an unpulsed sheet always shows
	[[nodiscard]] bool Showing() const;
	[[nodiscard]] Look GetLook() const { return _pulsed ? Look::ForceField : Look::Stars; }
	/// How far its top is spread out from the middle of its points
	void SetSpread(float spread) { _spread = spread; }
	/// A hidden sheet is neither drawn nor moved on by time: it waits as it is until it is shown again
	void SetHidden(bool hidden) { _hidden = hidden; }
	[[nodiscard]] bool Hidden() const { return _hidden; }
	/// The strength fed in at its first point from now on, 0 to 1
	void SetStrength(float strength) { _strength = strength; }
	/// The colour of its light from now on (0xRRGGBB)
	void SetRgb(uint32_t rgb) { _rgb = rgb & 0xFFFFFFu; }
	[[nodiscard]] uint32_t Rgb() const { return _rgb; }
	/// How many times a second the stars slide up the sheet from now on. A negative speed slides them down.
	void SetSlideSpeed(float speed) { _slideSpeed = speed; }
	/// A sheet with its own clock is moved on by time only by its owner, which sets its values first. The particles
	/// draw it but do not move it on.
	void SetOwnClock(bool own) { _ownClock = own; }
	[[nodiscard]] bool OwnClock() const { return _ownClock; }
	/// Time passes: the wave rolls on, the stars slide and the strengths move along
	void Update(float seconds);
	/// Its corners, three over each point (the land, the bright line and the top), and its triangles, four between each
	/// pair of points; nothing for fewer than two points. Building it also settles how far the stars have slid and where
	/// its middle is. A pulsed sheet is built twice over, its texture sliding one way along the first and the other way
	/// along the second, and not at all while it doesn't show.
	void Build(std::vector<Vertex>& vertices, std::vector<uint32_t>& triangles);
	/// The middle of its points when it was last built, where it takes its place among what blends
	[[nodiscard]] glm::vec3 Middle() const { return _middle; }

	[[nodiscard]] const std::vector<float>& Strengths() const { return _strengths; }
	[[nodiscard]] size_t Points() const { return _points.size(); }
	[[nodiscard]] const std::vector<float>& Heights() const { return _heights; }

	/// Gives the values that a saved game keeps to a visitor
	template <typename Self, typename Visitor>
	static void VisitSaved(Self& self, Visitor&& visit)
	{
		visit(self._points, self._pulsed, self._strengths, self._heights, self._rgb, self._spread, self._hidden, self._height,
		      self._shiftSeconds, self._strength, self._time, self._sinceShift, self._slide, self._middle);
	}

private:
	void UpdatePulsed(float seconds);
	void BuildPulsed(std::vector<Vertex>& vertices, std::vector<uint32_t>& triangles);

	std::vector<glm::vec3> _points;
	bool _pulsed {false};
	/// The strength each point was given, its first point's the newest
	std::vector<float> _strengths;
	/// Each point's height as the wave has it
	std::vector<float> _heights;
	uint32_t _rgb {0};
	float _spread {k_Spread};
	bool _hidden {false};
	float _height {0.0f};
	float _shiftSeconds {0.0f};
	float _strength {0.0f};
	float _time {0.0f};
	float _sinceShift {0.0f};
	float _slide {0.0f};
	glm::vec3 _middle {0.0f};
	/// Only a sheet with its own clock changes these, and its owner sets them again before each move. A saved game does
	/// not keep them.
	float _slideSpeed {k_SlideSpeed};
	bool _ownClock {false};
};

} // namespace openblack::particles
