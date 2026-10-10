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
#include <map>
#include <string_view>
#include <vector>

/// The camera editor's file, Data/camera.edt: a block file ("LiOnHeAd", then blocks of a 32-byte name, a size and the
/// data) holding the scripts' numbered camera positions ("Cam<n>") and their numbered tracks ("Track<n>"), plus one
/// "EDITOR" block of the editor's own names that the game doesn't read.
namespace openblack::edt
{

enum class EDTResult : uint8_t
{
	Success = 0,
	ErrNotABlockFile,
	ErrCameraTooSmall,
	ErrTrackTooSmall,
	ErrWayMalformed,
};

std::string_view ResultToStr(EDTResult result);

/// A camera: where it is and what it looks at
struct EDTCamera
{
	std::array<float, 3> position;
	std::array<float, 3> focus;
	/// Two more floats: 0 and -1 in every camera of the game's file, read by nothing
	std::array<float, 2> unknown;
};

/// A way along a track: a cubic Bezier path through its points, each point reached at its time
struct EDTWay
{
	/// 0x63 in every way of the game's file
	uint16_t unknown2;
	/// 0.26 in every way of the game's file
	float unknown8;
	/// How long the way takes, in milliseconds: the last point's time, whole
	int32_t duration;
	std::vector<std::array<float, 3>> points;
	/// The two inner handles of the Bezier from each point to the next
	std::vector<std::array<std::array<float, 3>, 2>> handles;
	/// When each point is reached, in milliseconds
	std::vector<float> times;
	/// How fast it goes at each point, in metres a second
	std::vector<float> speeds;
};

/// A track: one way for where the camera is and one for what it looks at
struct EDTTrack
{
	/// 0 in every track of the game's file
	uint32_t unknown0;
	EDTWay position;
	EDTWay focus;
};

class EDTFile
{
public:
	/// Reads the file's cameras and tracks from its bytes
	EDTResult Open(const std::vector<uint8_t>& buffer);

	[[nodiscard]] const std::map<int32_t, EDTCamera>& GetCameras() const noexcept { return _cameras; }
	[[nodiscard]] const std::map<int32_t, EDTTrack>& GetTracks() const noexcept { return _tracks; }

private:
	std::map<int32_t, EDTCamera> _cameras;
	std::map<int32_t, EDTTrack> _tracks;
};

} // namespace openblack::edt
