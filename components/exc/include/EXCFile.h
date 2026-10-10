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
#include <string_view>
#include <vector>

/// A land's camera zone file, Data/Zones/*.exc: a block file ("LiOnHeAd", then blocks of a 32-byte name, a size and the
/// data) whose "cameraexc" block holds the fence the camera is kept inside, the limits on its height and the domes and
/// cylinders it is kept out of
namespace openblack::exc
{

enum class EXCResult : uint8_t
{
	Success = 0,
	ErrNotABlockFile,
	ErrNoZoneBlock,
	ErrZoneTooSmall,
};

std::string_view ResultToStr(EXCResult result);

/// A place the camera is kept out of
struct EXCExclusion
{
	enum class Kind : uint32_t
	{
		Dome = 0,
		Cylinder = 1,
	};
	std::array<float, 3> position;
	float radius;
	float height;
	Kind kind;
};

struct EXCZones
{
	/// 1 in every file of the game's
	uint32_t version {0};
	/// Whether the camera is kept out of the exclusions and under its height limits
	bool exclusionsOn {false};
	/// Whether the camera is kept inside the fence
	bool fenceOn {false};
	/// Whether the highest the camera may go is `maxAltitude`, and the highest above the land `heightAboveLand`
	bool useMaxAltitude {false};
	bool useHeightAboveLand {false};
	float maxAltitude {0.0f};
	float heightAboveLand {0.0f};
	/// The fence's corners in order, of which only the ground's directions count
	std::vector<std::array<float, 3>> fence;
	std::vector<EXCExclusion> exclusions;
};

class EXCFile
{
public:
	/// Reads the zones from the file's bytes
	EXCResult Open(const std::vector<uint8_t>& buffer);

	[[nodiscard]] const EXCZones& GetZones() const noexcept { return _zones; }

private:
	EXCZones _zones;
};

} // namespace openblack::exc
