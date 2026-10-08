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
#include <functional>
#include <span>
#include <string>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

/// The mesh eyes of the intro's SuperVillagers (ECS/SuperVillager.h; docs/bw1-notes/intro.md): an eyeball sphere and
/// four eyelids from Data\MISC\Eyes, put on bone 8 of the HD mesh through its EBone block, with a random blink, a
/// squint and a glance shared by every SuperVillager.
///
/// The pure part (timers, matrices, shade, the L3D byte patches) has no Locator and is tested in
/// test/test_super_villager_eyes.cpp; the entities part draws them as six mesh entities.
namespace openblack::ecs::components
{
/// One of the eye objects: Transform + Mesh + ObjectColour (+ UvScroll for the eyeballs), moved every frame
struct SuperVillagerEye
{
	entt::entity owner {entt::null};
	/// Slot: 0 r_up, 1 r_down, 2 l_up, 3 l_down; the eyeball is one object drawn twice by the original (once per eye):
	/// two entities here, 4 right and 5 left
	uint8_t slot {0};
	entt::id_type mesh {0};
};
} // namespace openblack::ecs::components

namespace openblack::ecs::super_villager::eyes
{

/// The eye timers, one per SuperVillager
struct Timers
{
	float glance {0.0f}; ///< The eyeballs' yaw (radians)
	bool blinking {false};
	int32_t field28 {0};      ///< Zeroed when a blink ends; no reader found
	int32_t untilBlinkMs {0}; ///< 0 at creation: the first frame with time blinks
	float closure {0.0f};     ///< The lids, 0 open .. 1 shut
	int32_t blinkMs {0};
	int32_t holdMs {200}; ///< 200 at creation
};

/// The globals every Eyes reads and moves (one step per Eyes per frame)
struct Shared
{
	float glanceTarget {0.0f};
	float squint {0.0f};
	uint32_t squintMs {0};
};

/// A random float in a range (game_random::crt::Random); the tests give their own
using RandomFn = std::function<float(float, float)>;

/// The per-frame step, before the matrices: closure = 0; the blink (Random(100, 200) when one ends,
/// Random(1000, 5000) when one starts), the glance towards the shared target at 0.7 rad/s (a new Random(-0.25, 0.25)
/// target when it is there), the shared squint (Random(0, 0.3) every 300 ms of this counter) as the closure's floor
void Step(Timers& timers, Shared& shared, int32_t milliseconds, const RandomFn& random);

/// The 180-degree matrix: c = cos(pi), s = sin(pi) of the double 3.1415927410125732, rows (-c, 0, -s), (0, 1, 0),
/// (-s, 0, c) = (1, 0, -s), (0, 1, 0), (-s, 0, -1): a mirror of z (det -1), not a turn
[[nodiscard]] glm::mat4 Mirror();
/// An EBone block's matrix (L3DEBone::matrices[k]: 3 rows then the position) as glm (its rows are glm's columns)
[[nodiscard]] glm::mat4 EBoneMatrix(const std::array<float, 12>& cells);
/// The eye k matrix: v R E_k bone in rows, the bone in the world (the original's camera-space bone x the camera to
/// world) = boneWorld E_k R in glm
[[nodiscard]] glm::mat4 EyeMatrix(const glm::mat4& boneWorld, const std::array<float, 12>& eBoneCells);
/// The eyeball turned by the glance about its own Y (rows 0 and 2, affine::RotateY), when the glance is not 0;
/// c rounded to a float, s kept at full precision
[[nodiscard]] glm::mat4 Glance(const glm::mat4& eye, float glance);
/// Rows 1 and 2 turned by closure x 0.47 (upper) or closure x -0.35 (lower): r1' = c r1 - s r2, r2' = c r2 + s r1
/// (affine::RotateX), c rounded to a float, s kept at full precision
[[nodiscard]] glm::mat4 Lid(const glm::mat4& eye, float closure, bool upper);
/// A table by type and eye gives the yaw a and pitch b of the eye's lighting normal n = (sin a cos b, -sin b,
/// -cos a cos b); n M normalised (the original's inverse square root and sum orders, which differ between the two
/// eyes), dot the normalised default sun, I = round(255 dot) (model_light::Intensity)
[[nodiscard]] int ShadeIntensity(int type, int eye, const glm::mat3& eyeRows);
/// The iris cell of misc0, u = k / 8 with k = {4, 3, 5}[type], v = 0.75
[[nodiscard]] float IrisU(int type);
/// The file of slot 0..4 (r_up, r_down, l_up, l_down, eye_ball) for an eye type: types 0 and 1 the
/// same files, type 2 the "*2" ones. No extension
[[nodiscard]] std::string File(int type, int slot);

/// In-memory edits of an L3D file (the layout of components/l3d L3DFile: header dwords 3 / 4 submeshes, 14 / 15 skins;
/// submesh +4 / +8 the primitives' offsets; primitive +8 the material's skin id, +16 / +20 the vertices; vertex 32 bytes,
/// the uv at +12). False when the file does not have what is asked
namespace l3d_patch
{
/// The first material's texture: primitive 0 of submesh 0
[[nodiscard]] bool FirstSkinId(const std::vector<uint8_t>& file, uint32_t& skinId);
bool SetFirstSkinId(std::vector<uint8_t>& file, uint32_t skinId);
/// Every vertex uv of every primitive doubled
bool DoubleUvs(std::vector<uint8_t>& file);
/// The embedded skin with that id (its id dword and 256 x 256 texels), empty when it has none
[[nodiscard]] std::vector<uint8_t> FindSkin(const std::vector<uint8_t>& file, uint32_t skinId);
/// The file gets that skin as its only one, appended after its data (the skins of the HD files also come after the size
/// the header gives); the other offsets are absolute and stay
bool AppendSkin(std::vector<uint8_t>& file, std::span<const uint8_t> skin);
} // namespace l3d_patch

/// What an Eyes needs of its host's HD file (ECS/SuperVillager.cpp loads it)
struct HostModel
{
	std::string name;                                 ///< "nors_man", for the lids' mesh ids
	std::array<std::array<float, 12>, 2> matrices {}; ///< EBone matrices 0 and 1
	std::array<int32_t, 2> bones {};                  ///< The EBone block's two bone indices
	uint32_t skinId {0};                              ///< its first material's texture
	std::vector<uint8_t> skin;                        ///< that skin's bytes, for the lids
};

/// The six eye entities of one Eyes (slots 0..3 the lids, 4 / 5 the eyeballs)
using Objects = std::array<entt::entity, 6>;

/// One SuperVillager's eyes
struct Eyes
{
	int32_t type {0}; ///< 0 man, 1 woman, 2 boy
	std::array<std::array<float, 12>, 2> matrices {};
	std::array<int32_t, 2> bones {};
	Timers timers;
	Objects objects {entt::null, entt::null, entt::null, entt::null, entt::null, entt::null};
};

/// The six eye entities (meshes loaded once into the mesh manager: the eyeball with the misc0 texture, the
/// lids with the host's skin and doubled uv)
[[nodiscard]] Eyes Create(entt::entity host, int32_t type, const HostModel& model);
/// The same with the five meshes given (slots 0..4; 0: none): the entities only, no loading
[[nodiscard]] Eyes Create(entt::entity host, int32_t type, const HostModel& model, const std::array<entt::id_type, 5>& meshes);
/// The entities go (whatever the host's state: ECS/SuperVillager keeps their ids in its list entry, so that
/// a host destroyed while it is a SuperVillager does not leave them drawn)
void Destroy(Objects& objects);
/// Not drawn this frame (no Mesh): the host has no mesh or no pose, or it is off screen
void Hide(const Eyes& eyes);
/// One SuperVillager on screen: Step, then each eye's matrix from the host's body matrix (ecs::DrawnBodyModel, the
/// turned copy) and drawn pose (ecs::DrawnPose, the cross-fade), its shade (the host's land light x the eye's own
/// intensity) and the eyeball and lids moved. Hidden while the host has no mesh or no pose
void Update(Eyes& eyes, Shared& shared, entt::entity host, int32_t milliseconds);

} // namespace openblack::ecs::super_villager::eyes
