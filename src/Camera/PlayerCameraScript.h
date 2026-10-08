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
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

/// What the scripts set on the player's camera mode: the camera zone of SET_CAMERA_ZONE (the exclusions and the force
/// field), GET_INCLUSION_DISTANCE and SET_FIXED_CAM_ROTATION (ForceRotateAboutPoint). The script camera
/// (Camera/ScriptCamera.h) is not affected by any of them.
///
/// Only the data and the pure InsideInclusion are ported: openblack's player camera (DefaultWorldCameraModel) reads
/// none of this yet
namespace openblack::player_camera
{

/// The zone file's segment
constexpr const char* k_ZoneSegment = "cameraexc";
/// SET_CAMERA_ZONE reads ".\Data\Zones\<name>"
constexpr const char* k_ZoneFolder = "Zones";
/// Both zone limits after a reset
constexpr float k_DefaultZoneLimit = 500.0f;
/// The original's force field array has room for 1024 points
constexpr size_t k_MaxForceFieldPoints = 1024;
/// The size of an exclusion record LoadExclusionFile reads (else the records are skipped)
constexpr int32_t k_ExclusionRecordSize = 0x28;
/// InsideInclusion: 1e-8 for squared distances, about 1e-4 (a double) for the rest
constexpr float k_InclusionSquaredEpsilon = 1e-08f;
constexpr double k_InclusionEpsilon = 9.9999997473787516e-05;
/// InsideInclusion: the best ray parameters start at 1e20
constexpr float k_InclusionFar = 1.0e20f;
/// The inclusion distance: FLT_MAX at start and when the player camera resets; the player camera's update writes 1e10
/// while there is no force field
constexpr float k_NoInclusionDistance = std::numeric_limits<float>::max();
constexpr float k_NoForceFieldDistance = 1.0e10f;

/// A camera exclusion record (0x28 bytes): kept as read. None of the nine zone files of Data\Zones has one (all have
/// count 0)
struct Exclusion
{
	uint32_t id = 0;                    ///< The file's id (1 from SET_CAMERA_ZONE)
	std::array<uint8_t, 0x28> bytes {}; ///< the record; its first field (next) is the list's
};

/// The camera zone state a zone file sets (LoadExclusionFile, in file order)
struct Zone
{
	int32_t header = 0;                      ///< the first int32, read into a local and overwritten (1 in all nine files)
	int32_t flag9CE6B0 = 1;                  ///< Meaning unknown; the player camera's altitude reads it
	int32_t drawForceField = 0;              ///< InsideInclusion only checks with it on
	int32_t flagC5E14C = 0;                  ///< Meaning unknown; the player camera's update reads it
	int32_t flagC5E148 = 0;                  ///< Idem
	float limit9CE6AC = k_DefaultZoneLimit;  ///< Idem
	float limit9CE6A8 = k_DefaultZoneLimit;  ///< Idem
	std::vector<glm::vec3> forceFieldPoints; ///< The inclusion polygon (x, z), with heights
	std::vector<Exclusion> exclusions;       ///< the exclusion list
};

/// The point the player camera is forced to turn about (ForceRotateAboutPoint)
struct FixedRotation
{
	bool on = false;
	glm::vec3 point {0.0f};
};

struct State
{
	Zone zone;
	float inclusionDistance = k_NoInclusionDistance;
	FixedRotation fixedRotation;
};

State& Get();

/// Removes the records of that id; flag9CE6B0 = 1, drawForceField = flagC5E14C = flagC5E148 = 0, both limits = 500 and
/// no force field points
void ResetExclusionFile(uint32_t id);

/// On the bytes of a whole Lionhead segment file ("LiOnHeAd", then segments of a 32-byte name, a u32 size and the
/// data): ResetExclusionFile(id), then from segment "cameraexc" the int32 header, flag9CE6B0, drawForceField,
/// flagC5E14C, flagC5E148, the floats limit9CE6AC, limit9CE6A8, the point count and the points (12 bytes each), the
/// record count and the record size; records of 0x28 bytes become exclusions of `id` (the list link and id kept), any
/// other size skips them. False when the file or the segment is not there or is short (inferred: the original's own
/// segment checks are unknown; what was read before stays)
bool LoadExclusionFile(const std::vector<uint8_t>& bytes, uint32_t id);

/// SET_CAMERA_ZONE: ResetExclusionFile(1), ".\Data\Zones\<name>" opened and LoadExclusionFile(file, 1), then
/// flag9CE6B0 = 1 and drawForceField = 1 whatever the file says; a file that does not open -> "Couldn't load zone
/// file-%s" with the zone left reset. Returns false then
bool SetCameraZone(const std::string& name);

/// True without the force field or with fewer than 3
/// points; else the parity of the polygon's edges crossed by the ray p + t dir, t > 0, in x / z (an edge from each point
/// to the one before, the last before the first). True at once when p is on a point (squared distance < 1e-8) or on an
/// edge (|t| < 1e-4). `hit` gets the nearest crossing ahead (else behind, else p), then the nearest polygon point if
/// that one is nearer in x / z; `normal` the crossed edge's (e.z, 0, -e.x), e = the point before - the point
[[nodiscard]] bool InsideInclusion(const Zone& zone, const glm::vec3& p, const glm::vec3& dir, glm::vec3* hit,
                                   glm::vec3* normal);

/// Turns the fixed rotation on about *point, or off for null. The player's camera then turns about that point (its
/// update copies the point over its turning centre); reinitialising the camera clears it. Not read by
/// DefaultWorldCameraModel yet
void ForceRotateAboutPoint(const std::optional<glm::vec3>& point);

/// Not original: everything as at start (a new game, the tests)
void Reset();

} // namespace openblack::player_camera
