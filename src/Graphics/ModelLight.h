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

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// The one model light of the original, CPU side; the GPU twin is assets/shaders/model_light.sh and the two must stay
/// the same. See docs/bw1-notes/rendering-objects.md, "Luz de los modelos".
///
/// The engine keeps a single point light and lights every model vertex with it in integers: I = round(255 (n . l))
/// with the light brought into the mesh's own space, then f = I < 0 ? amb : amb + ((255 - amb) I >> 8) with amb = 90,
/// and each colour channel times f >> 8. That integer rule colours every normal model (buildings, villagers, trees,
/// rocks): the original's float formula for hardware transform and lighting is never reached, because the game never
/// turns hardware T&L on.
///
/// The light itself moves with the hour: once a frame, before the models are drawn, it is left at the default sun by
/// day and taken next to the player's hand in full night.
namespace openblack::model_light
{

/// The default sun, set at start-up and never changed again; it is copied into the light at start-up. The static
/// shadows read it directly, so they keep the day sun all night.
constexpr glm::vec3 k_DefaultSun {-500000.0f, 500000.0f, -500000.0f};
/// The ambient at start-up
constexpr int k_DefaultAmbient = 90;
/// The ambient the mists put on while they draw the clouds and the shrinking mists (put back to 90 afterwards)
constexpr int k_MistAmbient = 210;

/// The engine's one light, in world coordinates
[[nodiscard]] glm::vec3 Light();
/// Moves the light. Saving and restoring is the caller's job: ScopedLight.
void SetLight(const glm::vec3& position);

/// A caller that moves the light for a few draws and puts the old one back
class ScopedLight
{
public:
	explicit ScopedLight(const glm::vec3& position)
	    : _previous(Light())
	{
		SetLight(position);
	}
	~ScopedLight() { SetLight(_previous); }
	ScopedLight(const ScopedLight&) = delete;
	ScopedLight(ScopedLight&&) = delete;
	ScopedLight& operator=(const ScopedLight&) = delete;
	ScopedLight& operator=(ScopedLight&&) = delete;

private:
	glm::vec3 _previous;
};

/// The ambient, 0..255
[[nodiscard]] int Ambient();
void SetAmbient(int ambient);

/// The mists' 210 and back
class ScopedAmbient
{
public:
	explicit ScopedAmbient(int ambient)
	    : _previous(Ambient())
	{
		SetAmbient(ambient);
	}
	~ScopedAmbient() { SetAmbient(_previous); }
	ScopedAmbient(const ScopedAmbient&) = delete;
	ScopedAmbient(ScopedAmbient&&) = delete;
	ScopedAmbient& operator=(const ScopedAmbient&) = delete;
	ScopedAmbient& operator=(ScopedAmbient&&) = delete;

private:
	int _previous;
};

/// Once a frame, before the models: the focus is the player hand's position, lifted to at least 10 over the land under
/// it. With a sky type of 1.5 or less (the sky type of the visual time, 2 = night, 0 = day) the light is the default
/// sun, so only the darker half of the night moves it: there it goes 3 units from the focus towards the camera.
void UpdateFrameLight(glm::vec3 focus, const glm::vec3& cameraPosition, float skyType);

/// The light's position brought into the mesh's own space by the inverse of the drawn matrix (the general inverse, so
/// a non-uniform scale is kept) and normalised. The boned path inverts each bone matrix over the light in camera
/// space, the same per bone if those matrices go from the bone to the camera (inferred).
[[nodiscard]] glm::vec3 LightInMeshSpace(const glm::mat4& model);

/// The uniform both sides share, u_modelLight of assets/shaders/model_light.sh: xyz the light, w the ambient
[[nodiscard]] glm::vec4 Uniform();

/// The CPU side of the rule: the exploded pieces (mesh_pieces::AppendPiece) and the broken buildings and their fragments
/// (FragMesh::AppendDraw, through TwoSided); still waiting: the special primitives, which use the truncating variant.
/// The GPU paths use their twins in model_light.sh.
///
/// I = round(255 (n . l)): to the nearest, halves to even. `truncate` is the truncating variant of the same rule.
[[nodiscard]] int Intensity(float dot, bool truncate = false);
/// f = I < 0 ? amb : amb + ((255 - amb) I >> 8)
[[nodiscard]] int Factor(int intensity, int ambient);
/// The whole rule on one colour: each channel of c times f >> 8, the alpha untouched
[[nodiscard]] uint32_t Apply(uint32_t colour, int intensity, int ambient);

/// The two sides of one face, lit once: the colours of the front and the back copy of a FragMesh triangle
struct TwoSidedColours
{
	uint32_t front {0};
	uint32_t back {0};
};
/// One I = round(255 dot) for the whole face, the factor of I for the front and the factor of -I for the back (the same
/// rule as Factor), each channel of the colour times its factor >> 8 with the colour's alpha kept (as Apply). So a face
/// lit at I > 0 has its back at the bare ambient
[[nodiscard]] TwoSidedColours TwoSided(uint32_t colour, float dot, int ambient);

} // namespace openblack::model_light
