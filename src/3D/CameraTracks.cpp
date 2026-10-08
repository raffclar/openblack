/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraTracks.h"

#include <cmath>
#include <cstring>

#include <exception>
#include <map>
#include <string>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack
{
namespace
{
constexpr float k_OneOver127 = 0.007874015718698502f;
constexpr float k_Milliseconds = 0.0010000000474974513f; // ms to seconds

/// The bytes of segment `name` of camera.edt, found in the file's cached bytes (the byte cache); empty when there is none
std::span<const uint8_t> FindSegment(const std::string& name)
{
	if (!Locator::filesystem::has_value() || !Locator::resources::has_value())
	{
		return {};
	}
	auto& fileSystem = Locator::filesystem::value();
	const auto& bytes = resources::LoadOptionalBlob(
	    Locator::resources::value().GetBlobs(), fileSystem.GetPath<filesystem::Path::Data>() / "camera.edt", "camera tracks");
	return camera_tracks::FindSegment(bytes, name);
}

template <typename T>
T Read(std::span<const uint8_t> bytes, size_t offset)
{
	T value {};
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}

glm::vec3 ReadVec3(std::span<const uint8_t> bytes, size_t offset)
{
	return {Read<float>(bytes, offset), Read<float>(bytes, offset + 4), Read<float>(bytes, offset + 8)};
}

/// The way at `offset` (its u16 size first, as the original copies it) and its fix-up on load
std::optional<CameraWay> ReadWay(std::span<const uint8_t> bytes, size_t offset)
{
	if (offset + 0x24 > bytes.size())
	{
		return std::nullopt;
	}
	CameraWay way {
	    .size = Read<uint16_t>(bytes, offset),
	    .unknown2 = Read<uint16_t>(bytes, offset + 2),
	};
	const auto count = Read<uint32_t>(bytes, offset + 4);
	way.unknown8 = Read<float>(bytes, offset + 8);
	way.duration = Read<int32_t>(bytes, offset + 0x10);
	if (count < 2 || way.size != 0x24 + 44 * count || offset + way.size > bytes.size())
	{
		return std::nullopt;
	}
	auto p = offset + 0x24;
	for (uint32_t i = 0; i < count; ++i, p += 12)
	{
		way.points.push_back(ReadVec3(bytes, p));
	}
	for (uint32_t i = 0; i < count; ++i, p += 24)
	{
		way.handles.push_back({ReadVec3(bytes, p), ReadVec3(bytes, p + 12)});
	}
	for (uint32_t i = 0; i < count; ++i, p += 4)
	{
		way.times.push_back(Read<float>(bytes, p));
	}
	for (uint32_t i = 0; i < count; ++i, p += 4)
	{
		way.speeds.push_back(Read<float>(bytes, p));
	}
	// the length = the sum below, 0.1 if 0
	float length = 0.0f;
	for (uint32_t i = 0; i + 1 < count; ++i)
	{
		if (way.times[i] == way.times[i + 1])
		{
			continue;
		}
		const float t0 = way.times[i] * k_Milliseconds;
		const float dt = way.times[i + 1] * k_Milliseconds - t0;
		const float v0 = way.speeds[i];
		const float a = (way.speeds[i + 1] - v0) / dt;
		length += ((v0 - a * t0) + a * 0.5f) * dt;
	}
	way.length = length == 0.0f ? 0.1f : length;
	return way;
}

float Length(const glm::vec3& v) // sqrt((z*z + y*y) + x*x), in this order
{
	return std::sqrt((v.z * v.z + v.y * v.y) + v.x * v.x);
}

/// The Bezier of BuildTable and Get: ((B3 t³ + B2 3ut²) + B1 3u²t) + B0 u³ for x, y and z
glm::vec3 BezierRunner(const CameraWay& way, uint32_t s, float t)
{
	const float u = 1.0f - t;
	const float uu = u * u;
	const float tt = t * t;
	const float uuu = u * uu;
	const float c1 = (uu * t) * 3.0f;
	const float c2 = (u * tt) * 3.0f;
	const float ttt = t * tt;
	const auto& b0 = way.points[s];
	const auto& b1 = way.handles[s][0];
	const auto& b2 = way.handles[s][1];
	const auto& b3 = way.points[s + 1];
	glm::vec3 out;
	for (int c = 0; c < 3; ++c)
	{
		out[c] = ((b3[c] * ttt + b2[c] * c2) + b1[c] * c1) + b0[c] * uuu;
	}
	return out;
}
} // namespace

glm::vec3 CameraWay::Bezier(uint32_t segment, float t) const
{
	// the same weights; x sums ((B2 3ut² + B1 3u²t) + B3 t³) + B0 u³, y and z as BezierRunner
	const float u = 1.0f - t;
	const float uu = u * u;
	const float tt = t * t;
	const float uuu = u * uu;
	const float c1 = (uu * t) * 3.0f;
	const float c2 = (u * tt) * 3.0f;
	const float ttt = tt * t;
	const auto& b0 = points[segment];
	const auto& b1 = handles[segment][0];
	const auto& b2 = handles[segment][1];
	const auto& b3 = points[segment + 1];
	return {((b2.x * c2 + b1.x * c1) + b3.x * ttt) + b0.x * uuu, ((b3.y * ttt + b2.y * c2) + b1.y * c1) + b0.y * uuu,
	        ((b3.z * ttt + b2.z * c2) + b1.z * c1) + b0.z * uuu};
}

CameraWayRunner::CameraWayRunner(const CameraWay& way)
    : _way(&way)
{
	BuildTable();
}

void CameraWayRunner::BuildTable()
{
	// the length of the segment's Bezier in 127 steps (t = 0..127 / 127, from points[s]), then the
	// running length at t = i / 127 for i = 1..126, scaled to 127 / total; [0] = 0 and [127] = 1.0
	const auto s = _segment;
	float total = 0.0f;
	glm::vec3 previous = _way->points[s];
	for (int i = 0; i < 128; ++i)
	{
		const auto point = BezierRunner(*_way, s, static_cast<float>(i) * k_OneOver127);
		total = Length(point - previous) + total;
		previous = point;
	}
	_table[0] = 0.0f;
	_table[127] = 1.0f;
	float running = 0.0f;
	previous = _way->points[s];
	for (int i = 0; i < 127; ++i)
	{
		const auto point = BezierRunner(*_way, s, static_cast<float>(i) * k_OneOver127);
		running = Length(point - previous) + running;
		previous = point;
		if (i != 0)
		{
			_table[i] = running * 127.0f / total;
		}
	}
}

glm::vec3 CameraWayRunner::Get(int32_t sample)
{
	const auto& way = *_way;
	const auto last = static_cast<uint32_t>(way.points.size() - 1);
	const auto f = static_cast<float>(sample);
	// the first segment whose end time is not before the sample (the original reads on past the array for
	// a sample after the last time; the duration is the int of that time, so the callers never ask for one)
	uint32_t segment = 0;
	while (segment + 1 < last && way.times[segment + 1] < f)
	{
		++segment;
	}
	if (segment != _segment)
	{
		_segment = segment;
		BuildTable();
	}
	if (sample >= way.duration)
	{
		return way.points[last];
	}
	if (sample <= 0)
	{
		_t = 0.0f;
		return way.points[0];
	}
	// the fraction of the chord covered at this time
	const float chord = Length(way.points[segment + 1] - way.points[segment]);
	const float v0 = way.speeds[segment];
	const float dt = (way.times[segment + 1] - way.times[segment]) * k_Milliseconds;
	const float tau = (f - way.times[segment]) * k_Milliseconds;
	float twice = chord - dt * v0;
	twice = twice + twice;
	const float a = twice / (dt * dt);
	const float u = (a * tau * 0.5f + v0) * tau / chord;
	// k = int(127 u), lo = table[k], hi = table[k + 1]; t = lo when u == lo, otherwise the
	// scans for the entry around u move a pointer but keep lo and hi, and t = w lo + (1 - w) hi with
	// w = (hi - u) / (hi - lo): u again, with the float rounding of the table's scale. With k = 127 hi is the last t.
	// Past the table (u < 0 or u > 1, when the speed at the start is more than twice the chord's mean speed)
	// the original reads the object's other fields: t = u here.
	const auto k = static_cast<int32_t>(u * 127.0f);
	float t = u;
	if (k >= 0 && k <= 127)
	{
		const float lo = _table[static_cast<size_t>(k)];
		const float hi = k < 127 ? _table[static_cast<size_t>(k) + 1] : _t;
		if (u == lo)
		{
			t = lo;
		}
		else if (hi != lo)
		{
			const float w = (hi - u) / (hi - lo);
			t = w * lo + (1.0f - w) * hi;
		}
	}
	_t = t;
	return BezierRunner(way, segment, t);
}

std::span<const uint8_t> camera_tracks::FindSegment(std::span<const uint8_t> file, std::string_view name)
{
	// "LiOnHeAd", then each segment: a 32-byte zero padded name, a u32 size, the data
	if (file.size() < 8 || std::memcmp(file.data(), "LiOnHeAd", 8) != 0)
	{
		return {};
	}
	size_t offset = 8;
	while (offset + 36 <= file.size())
	{
		std::array<char, 33> segmentName {};
		std::memcpy(segmentName.data(), file.data() + offset, 32);
		uint32_t size = 0;
		std::memcpy(&size, file.data() + offset + 32, 4);
		if (offset + 36 + size > file.size())
		{
			break;
		}
		if (name == segmentName.data())
		{
			return file.subspan(offset + 36, size);
		}
		offset += 36 + size;
	}
	return {};
}

std::optional<CameraTrack> camera_tracks::ParseTrack(std::span<const uint8_t> segment)
{
	if (segment.size() < 4)
	{
		return std::nullopt;
	}
	auto position = ReadWay(segment, 4);
	if (!position.has_value())
	{
		return std::nullopt;
	}
	auto focus = ReadWay(segment, 4 + position->size);
	if (!focus.has_value())
	{
		return std::nullopt;
	}
	return CameraTrack {
	    .unknown0 = Read<uint32_t>(segment, 0),
	    .position = std::move(*position),
	    .focus = std::move(*focus),
	};
}

std::shared_ptr<const CameraTrack> LoadCameraTrack(int32_t number)
{
	if (!Locator::resources::has_value())
	{
		return nullptr;
	}
	// parsed once into the camera track cache
	auto& tracks = Locator::resources::value().GetCameraTracks();
	const auto id = entt::hashed_string(fmt::format("camera/Track{}", number).c_str()).value();
	if (!tracks.Contains(id))
	{
		try
		{
			tracks.Load(id, resources::CameraTrackLoader::FromBufferTag {}, FindSegment(fmt::format("Track{}", number)));
		}
		catch (const std::exception&)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Cannot load track No {}", number);
			return nullptr;
		}
	}
	return tracks.Handle(id).handle();
}

std::optional<CameraBin> LoadCameraBin(int32_t number)
{
	const auto segment = FindSegment(fmt::format("Cam{}", number));
	if (segment.size() < 32)
	{
		return std::nullopt;
	}
	CameraBin camera {
	    .position = ReadVec3(segment, 0),
	    .focus = ReadVec3(segment, 12),
	    .unknown = {Read<float>(segment, 24), Read<float>(segment, 28)},
	};
	return camera;
}

} // namespace openblack
