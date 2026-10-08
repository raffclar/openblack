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
#include <vector>

#include <glm/vec3.hpp>

namespace openblack::ecs::disappear_smoke
{

/// A puff of 15 camera-facing smoke.raw sprites (render mode 6) flying apart from a point while they grow and fade.
/// Made for the boat's dust and splashes, an object that goes (DisappearSmoke::Create), the ground marks' dust...; moved
/// and drawn once per frame.
struct Puff
{
	glm::vec3 position;  ///< sprite position
	glm::vec3 velocity;  ///< units per second
	float angle {0.0f};  ///< the turn on the screen
	float half {1.0f};   ///< half the side
	uint8_t cell {0x10}; ///< cell of smoke.raw's 8 x 8 sheet
	uint32_t argb {0xFFFFFFFFu};
};

struct Cloud
{
	static constexpr size_t k_Puffs = 15;
	std::array<Puff, k_Puffs> puffs {};
	float life {1.0f}; ///< 1 when made, gone below 0
	int32_t mode {0};  ///< the second argument of Create (0 for every caller here)
	float size {1.0f};
	uint32_t colour {0}; ///< 0xAARRGGBB, its rgb replaces the puffs' unless it is -1
};

/// Mode 0: each puff at pos + (c, b, a) with a, b, c =
/// Random(-size, size), turned Random(0, 2 pi), cell 0x10, flying along norm(e, size, d) (d, e = Random(-size, size)) at
/// Random(0.3, 1) x size units per second. Mode != 0 (the ground marks' dust of ECS/GroundMarks): 1.5 x size
/// along that direction. Random is game_random::crt::Random: ((rand() x k) x (b - a)) + a, k just over 1 / 32768,
/// on the CRT stream the rest of the game shares.
void Create(const glm::vec3& position, int32_t mode, float size, uint32_t colour);

/// dt = the game time step x 0.001, per cloud, then the ones with life < 0 are freed:
/// life -= dt / 3 (mode 0; 2 dt / 3 otherwise); nothing is drawn once life <= 0. Colour: mode 0 (life x 100) << 24 |
/// 0x808080 (else min(life / 0.7, 1) x 255 << 24 | 0x68503D), the rgb of `colour` over it; per puff angle = +-5 life +
/// velocity.x (- when velocity.x > velocity.z), half = ((1 - life) x 2 + 1) x size / 2 (at least 0.0001),
/// position += velocity x dt, cell = (int)(life x 15).
void Update(float seconds);

[[nodiscard]] const std::vector<Cloud>& Get();

/// Every DisappearSmoke goes (a new map: the list is emptied with the landscape)
void Clear();

} // namespace openblack::ecs::disappear_smoke

namespace openblack::ecs
{

/// The puff an object leaves when it goes,
/// DisappearSmoke::Create(point, type 0, size, colour 0xFFFFFFFF, so the grey 0x808080 of type 0 stays). The point is
/// usually half the object's height up; `size` is the original's size argument (an animal corpse: 1). Used by the
/// animal corpses that time out; the original also uses it for villagers, buildings and spells. The puffs live in
/// disappear_smoke (moved in ecs::missionary_boat's frame step, drawn with the boat's sprites).
class DisappearSmoke
{
public:
	static void Create(glm::vec3 at, float size) { disappear_smoke::Create(at, 0, size, 0xFFFFFFFFu); }
	static void Clear() { disappear_smoke::Clear(); }
	DisappearSmoke() = delete;
};

} // namespace openblack::ecs
