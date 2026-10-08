/*******************************************************************************
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
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>

namespace openblack
{

/// The scripted cameras and camera tracks of Data\camera.edt (docs/bw1-notes/camera-tracks.md). The file is a
/// Lionhead segment file ("LiOnHeAd", then segments of a 32-byte name, a u32 size and the data), opened once by the
/// camera editor and read by segment name: "EDITOR" (the editor's names, not read by the game), "Cam%d" (CameraBin,
/// 32 bytes) and "Track%d".

/// Segment "Cam%d", copied as is: CONVERT_CAMERA_POSITION pushes the floats 0..2, CONVERT_CAMERA_FOCUS the floats
/// 3..5.
struct CameraBin
{
	glm::vec3 position;
	glm::vec3 focus;
	std::array<float, 2> unknown; ///< 0 and -1 in all 556 cameras of the file; not read by those two functions
};

/// A way: a cubic Bezier path of `count` points, copied from the file with its u16 size in front and fixed up on load
/// (the four array pointers are rebuilt from the data after the header).
struct CameraWay
{
	uint16_t size;     ///< the bytes of the way = 0x24 + 44 * count
	uint16_t unknown2; ///< 0x63 in every track
	float unknown8;    ///< 0.26 in every track
	/// Rebuilt on load: Σ over the segments with times[i] != times[i + 1] of
	/// dt * (speeds[i] - a * t0 + a / 2) with t0 = times[i] * 0.001, dt = times[i + 1] * 0.001 - t0,
	/// a = (speeds[i + 1] - speeds[i]) / dt; 0.1 if that is 0. Not read by the walk path.
	float length;
	int32_t duration;                              ///< the samples (milliseconds) = int(times.back())
	std::vector<glm::vec3> points;                 ///< count points
	std::vector<std::array<glm::vec3, 2>> handles; ///< the two inner Bezier handles of segment i (after i)
	std::vector<float> times;                      ///< the time of each point, in ms
	std::vector<float> speeds;                     ///< the speed at each point, units per second

	/// The Bezier (points[s], handles[s][0], handles[s][1], points[s + 1]) at t, with
	/// the original's order of the sums (x differs from y and z).
	[[nodiscard]] glm::vec3 Bezier(uint32_t segment, float t) const;
};

/// Segment "Track%d", read into a scripted camera: the first u32, a runner on the first way (the camera's position)
/// and a runner on the second way (its focus).
struct CameraTrack
{
	uint32_t unknown0; ///< 0 in every track
	CameraWay position;
	CameraWay focus;
};

/// A runner along a way: the current segment, an arc-length table of that segment and the last Bezier parameter.
class CameraWayRunner
{
public:
	explicit CameraWayRunner(const CameraWay& way); // segment 0, t 0, the table built

	/// The point at `sample` ms. The segment is the first i with times[i + 1] >= sample
	/// (a new one rebuilds the table); sample >= duration gives the last point (segment and t unchanged), sample <= 0
	/// the first one (t = 0). In between the distance along the chord of the segment grows with a constant
	/// acceleration from speeds[i] such that the whole chord is covered at times[i + 1]; the fraction of the chord is
	/// the Bezier parameter (the table lookup gives it back, see the .cpp).
	glm::vec3 Get(int32_t sample);

	[[nodiscard]] uint32_t Segment() const { return _segment; }
	[[nodiscard]] float Parameter() const { return _t; }

private:
	void BuildTable();

	const CameraWay* _way;
	uint32_t _segment {0};
	std::array<float, 128> _table {}; ///< 127 * arc length / total at t = i / 127, [127] = 1.0
	float _t {0.0f};
};

/// The track `number` ("Track%d"), nullptr if the file or segment is missing
/// ("Cannot load track No %d"). The tracks are kept once loaded.
std::shared_ptr<const CameraTrack> LoadCameraTrack(int32_t number);

namespace camera_tracks
{
/// The bytes of segment `name` of a "LiOnHeAd" segment file (camera.edt), a view into `file`; empty when it is not there
[[nodiscard]] std::span<const uint8_t> FindSegment(std::span<const uint8_t> file, std::string_view name);
/// A track segment ("Track%d": a u32, the position way, the focus way); nothing when it is cut short or malformed
[[nodiscard]] std::optional<CameraTrack> ParseTrack(std::span<const uint8_t> segment);
} // namespace camera_tracks

/// The camera `number` ("Cam%d"); the original leaves the caller's buffer
/// as it was when the segment is missing, here std::nullopt.
std::optional<CameraBin> LoadCameraBin(int32_t number);

} // namespace openblack
