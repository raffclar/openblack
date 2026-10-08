/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerCameraScript.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <array>
#include <exception>
#include <span>
#include <utility>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Systems/ScriptStateInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::player_camera
{
namespace
{
/// The data of one segment, read in order
class SegmentReader
{
public:
	explicit SegmentReader(std::span<const uint8_t> data)
	    : _data(data)
	{
	}

	template <typename T>
	bool Read(T& value)
	{
		if (_offset + sizeof(T) > _data.size())
		{
			return false;
		}
		std::memcpy(&value, _data.data() + _offset, sizeof(T));
		_offset += sizeof(T);
		return true;
	}

	bool Skip(size_t bytes)
	{
		if (_offset + bytes > _data.size())
		{
			return false;
		}
		_offset += bytes;
		return true;
	}

private:
	std::span<const uint8_t> _data;
	size_t _offset = 0;
};

/// The segment `name` of a Lionhead segment file (as CameraTracks.cpp reads camera.edt)
std::optional<std::span<const uint8_t>> FindSegment(const std::vector<uint8_t>& bytes, const char* name)
{
	if (bytes.size() < 8 || std::memcmp(bytes.data(), "LiOnHeAd", 8) != 0)
	{
		return std::nullopt;
	}
	size_t offset = 8;
	while (offset + 36 <= bytes.size())
	{
		std::array<char, 33> segmentName {};
		std::memcpy(segmentName.data(), bytes.data() + offset, 32);
		uint32_t size = 0;
		std::memcpy(&size, bytes.data() + offset + 32, 4);
		if (offset + 36 + size > bytes.size())
		{
			return std::nullopt;
		}
		if (std::strcmp(segmentName.data(), name) == 0)
		{
			return std::span<const uint8_t> {bytes.data() + offset + 36, static_cast<size_t>(size)};
		}
		offset += 36 + size;
	}
	return std::nullopt;
}
} // namespace

State& Get()
{
	return Locator::scriptState::value().Get<State>();
}

void ResetExclusionFile(uint32_t id)
{
	auto& zone = Get().zone;
	// Every exclusion with this id is deleted
	zone.exclusions.erase(
	    std::remove_if(zone.exclusions.begin(), zone.exclusions.end(), [id](const Exclusion& e) { return e.id == id; }),
	    zone.exclusions.end());
	zone.flag9CE6B0 = 1;
	zone.drawForceField = 0;
	zone.flagC5E14C = 0;
	zone.flagC5E148 = 0;
	zone.limit9CE6AC = k_DefaultZoneLimit;
	zone.limit9CE6A8 = k_DefaultZoneLimit;
	zone.forceFieldPoints.clear();
}

bool LoadExclusionFile(const std::vector<uint8_t>& bytes, uint32_t id)
{
	ResetExclusionFile(id);
	const auto segment = FindSegment(bytes, k_ZoneSegment);
	if (!segment.has_value())
	{
		return false;
	}
	auto& zone = Get().zone;
	SegmentReader reader(*segment);
	int32_t pointCount = 0;
	// In file order
	if (!reader.Read(zone.header) || !reader.Read(zone.flag9CE6B0) || !reader.Read(zone.drawForceField) ||
	    !reader.Read(zone.flagC5E14C) || !reader.Read(zone.flagC5E148) || !reader.Read(zone.limit9CE6AC) ||
	    !reader.Read(zone.limit9CE6A8) || !reader.Read(pointCount))
	{
		return false;
	}
	// Three floats each. (inferred) more than 1024 would overrun the original's array: clamped here
	const auto count = static_cast<size_t>(std::clamp(pointCount, 0, static_cast<int32_t>(k_MaxForceFieldPoints)));
	for (size_t i = 0; i < count; ++i)
	{
		glm::vec3 point;
		if (!reader.Read(point.x) || !reader.Read(point.y) || !reader.Read(point.z))
		{
			return false;
		}
		zone.forceFieldPoints.push_back(point);
	}
	// The record count (zeroed first) and the record size
	int32_t recordCount = 0;
	int32_t recordSize = 0;
	if (!reader.Read(recordCount) || !reader.Read(recordSize))
	{
		return false;
	}
	if (recordSize != k_ExclusionRecordSize && recordCount != 0)
	{
		// Read into a buffer of that size and thrown away
		return reader.Skip(static_cast<size_t>(std::max(recordCount, 0)) * static_cast<size_t>(std::max(recordSize, 0)));
	}
	for (int32_t i = 0; i < recordCount; ++i)
	{
		Exclusion exclusion;
		for (auto& b : exclusion.bytes)
		{
			if (!reader.Read(b))
			{
				return false;
			}
		}
		exclusion.id = id; // the id is set after the read; the list link is kept
		zone.exclusions.push_back(exclusion);
	}
	return true;
}

bool SetCameraZone(const std::string& name)
{
	ResetExclusionFile(1);
	std::vector<uint8_t> bytes;
	bool opened = false;
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		bytes = resources::LoadBlob(Locator::resources::value().GetBlobs(),
		                            fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / k_ZoneFolder / name));
		opened = true;
	}
	catch (const std::exception&)
	{
		opened = false;
	}
	if (!opened)
	{
		if (const auto logger = spdlog::get("scripting"); logger != nullptr)
		{
			SPDLOG_LOGGER_ERROR(logger, "Couldn't load zone file-{}", name);
		}
		return false;
	}
	LoadExclusionFile(bytes, 1);
	auto& zone = Get().zone;
	zone.flag9CE6B0 = 1;
	zone.drawForceField = 1;
	return true;
}

bool InsideInclusion(const Zone& zone, const glm::vec3& p, const glm::vec3& dir, glm::vec3* hit, glm::vec3* normal)
{
	if (zone.drawForceField == 0)
	{
		return true;
	}
	if (hit != nullptr)
	{
		*hit = p;
	}
	const auto& points = zone.forceFieldPoints;
	if (points.size() < 3)
	{
		return true;
	}
	int32_t crossings = 0;
	float bestAhead = k_InclusionFar;
	float bestBehind = k_InclusionFar;
	bool foundAhead = false;
	bool foundBehind = false;
	glm::vec3 hitAhead = p;
	glm::vec3 hitBehind = p;
	glm::vec2 normalAhead(0.0f, 1.0f);
	glm::vec2 normalBehind(0.0f, 1.0f);
	const glm::vec3* a = &points.back(); // the last point
	for (const auto& b : points)
	{
		const float ex = a->x - b.x;
		const float ez = a->z - b.z;
		const float wx = p.x - b.x;
		const float wz = p.z - b.z;
		const float length2 = ex * ex + ez * ez;
		if (wz * wz + wx * wx < k_InclusionSquaredEpsilon) // on a point
		{
			return true;
		}
		const glm::vec3* next = &b;
		if (length2 > k_InclusionSquaredEpsilon)
		{
			const float nex = -ex;
			const float denominator = dir.z * nex + dir.x * ez;
			if (static_cast<double>(std::abs(denominator)) > k_InclusionEpsilon)
			{
				const float numerator = ((b.x * ez + b.z * nex) - p.x * ez) - p.z * nex;
				const float t = numerator / denominator;
				const glm::vec3 q(t * dir.x + p.x, dir.y * t + p.y, t * dir.z + p.z);
				const float s = ((q.x - b.x) * ex + (q.z - b.z) * ez) / length2;
				// 0 <= s < 1
				if (!(s < 0.0f) && s < 1.0f)
				{
					if (static_cast<double>(std::abs(t)) < k_InclusionEpsilon) // on the edge
					{
						return true;
					}
					if (t > 0.0f)
					{
						++crossings;
						if (t < bestAhead)
						{
							bestAhead = t;
							hitAhead = q;
							normalAhead = {ez, nex};
							foundAhead = true;
						}
					}
					else if (t < 0.0f && -t < bestBehind)
					{
						bestBehind = -t;
						hitBehind = q;
						normalBehind = {ez, nex};
						foundBehind = true;
					}
				}
			}
		}
		a = next; // the edge's end becomes the next start
	}
	bool found = false;
	if (foundAhead)
	{
		if (hit != nullptr)
		{
			*hit = hitAhead;
		}
		if (normal != nullptr)
		{
			*normal = {normalAhead.x, 0.0f, normalAhead.y};
		}
		found = true;
	}
	else if (foundBehind)
	{
		if (hit != nullptr)
		{
			*hit = hitBehind;
		}
		if (normal != nullptr)
		{
			*normal = {normalBehind.x, 0.0f, normalBehind.y};
		}
		found = true;
	}
	if (hit != nullptr) // the nearest polygon point when nearer in x / z
	{
		const float hx = hit->x - p.x;
		const float hz = hit->z - p.z;
		float best = hx * hx + hz * hz;
		for (const auto& v : points)
		{
			const float vx = v.x - p.x;
			const float vz = v.z - p.z;
			const float d = vz * vz + vx * vx;
			if (d < best || !found)
			{
				best = d;
				*hit = v;
				found = true;
			}
		}
	}
	return (crossings & 1) != 0;
}

void ForceRotateAboutPoint(const std::optional<glm::vec3>& point)
{
	auto& rotation = Get().fixedRotation;
	rotation.on = point.has_value();
	if (point.has_value())
	{
		rotation.point = *point;
	}
}

void Reset()
{
	Get() = State {};
}

} // namespace openblack::player_camera
