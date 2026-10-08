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
#include <optional>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include "Graphics/RenderModes.h"
#include "Particles/PSys.h"

// Mesh particles: ParticleMeshCreator, ParticleMeshCreatorAnimTextured (a UV frame offset) and ParticleAnimCreator (a
// skinned mesh playing a .anm). The atoms are drawn as mesh instances with the objects (ECS RenderingSystem), the
// animated ones with their bones. Wiki: docs/bw1-notes/magic.md.

namespace openblack::psys
{

/// The frames of a cycle of a ParticleAnimCreator atom (and the / 1000 of AnimCycleTime)
inline constexpr int k_AnimFrames = 1000;

/// The common mesh creator properties + ParticleMeshCreator / ParticleMeshCreatorAnimTextured / ParticleAnimCreator
struct MeshCreator: Creator
{
	entt::id_type meshId {0}; ///< MeshEnum (a mesh of the pack) or MeshFileName (a shared mesh file)
	bool faceCamera {false};
	bool faceCameraSprite {false};
	float heightStretch {1.0f};
	bool scriptHighlightPulse {false}; ///< UseScriptHightlightPulse
	// ParticleMeshCreator (double-sided and MeshChangeMaterialProps default to 1, the other flags 0)
	bool additive {false};           ///< UseAdditiveAlpha (applied with MeshChangeMaterialProps)
	bool writeDepth {false};         ///< MaterialUpdateZBuffer
	bool doubleSided {true};         ///< MaterialSetDoubleSided
	bool changeMaterialProps {true}; ///< MeshChangeMaterialProps
	/// The material properties' alpha flag (no property; always 1): 0 would make every material TexturedAlpha
	bool materialAlpha {true};
	/// UseGlobalAlpha: default 0 for ParticleMeshCreator / AnimTextured, 1 for ParticleAnimCreator. Only AnimTextured
	/// and ParticleAnimCreator pass it to their particles; ParticleMeshCreator never does, so its atoms never use their
	/// object's alpha table (see globalAlpha below)
	bool useGlobalAlpha {false};
	/// The material properties are set on the mesh once, at the creator's first particle, as the original does when it
	/// first fetches the mesh
	mutable bool materialsSet {false};
	bool neverClip {false};
	/// CastHumanShadow (ParticleMeshCreator only: AnimTextured reads NeverClip in its place). Each atom's particle gets
	/// a shadow node; every drawn particle puts its object in the node and pushes it on a list, every frame each node's
	/// shadow is updated with its object (the generic update of the physics objects) and the list is emptied. The
	/// particle's destruction takes the shadow out. Here: mesh_atoms::HumanShadows, the list of the last Collect, which
	/// graphics::shadow_list reads. The property is 0 in all 20 mesh creators of the spell files
	bool castHumanShadow {false};
	/// Whether the atoms' objects receive the projected shadows: 0, no property. The atoms never receive
	static constexpr bool k_ReceivesShadow = false;
	bool drawWithLandscapeColour {false}; ///< DrawWithLandscapeColor
	bool drawCutByPlane {false};          ///< DrawCutByPlane
	// ParticleMeshCreatorAnimTextured
	bool animTextured {false};
	int textureWidth {64};
	int textureHeight {64};
	bool slideU {false};
	bool slideV {false};
	bool randomiseInitFrame {false};
	bool randomiseFrameRate {false};
	float frameRate {1.0f};
	float frameRateMax {10.0f};
	int numFrames {1};
	bool playAnimation {false};
	float initialOffsetFrac {0.0f};
	float stretchY {1.0f};
	// ParticleAnimCreator: a skinned mesh playing a .anm
	bool animated {false};
	entt::id_type animId {0};         ///< AnimFileName (AnimEnum is -1 in every spell file)
	float speedUpFactor {1.0f};       ///< SpeedUpFactor (default 1.0)
	bool animPlay {false};            ///< PlayAnim (default 0)
	bool animRandomInitFrame {false}; ///< RandomiseInitFrame (default 0)

	/// The creator's part of a new atom (Effect::NewAtom does the rest): frame, frame rate, StretchY; for
	/// ParticleAnimCreator the clip's frame rate, PlayAnim, the random first frame
	void InitAtom(Effect& effect, Atom& atom) const override;
	/// The frame count of an atom: NumFrames, or 1000 for the sliding textures and the animated meshes
	[[nodiscard]] int FramesPerAtom() const override
	{
		return animated || slideU || slideV ? k_AnimFrames : std::max(1, numFrames);
	}
	/// The UV offset of an AnimTextured frame
	[[nodiscard]] glm::vec2 UvOffset(int frame) const;
};

/// The atom's frame rate = 1000 / the clip's ms x SpeedUpFactor x 1000: one cycle of 1000 frames in the clip's length,
/// faster by the factor
[[nodiscard]] float AnimFrameRate(int32_t clipMs, float speedUpFactor) noexcept;
/// The clip's ms x frame / 1000 in integers (the / 1000 rounds towards 0): the time the clip is posed at
[[nodiscard]] int32_t AnimCycleTime(int32_t clipMs, int frame) noexcept;

namespace mesh_atoms
{
/// One mesh atom to draw this frame (the atom's rotation x the scale, the Y axis x the stretch)
struct Instance
{
	entt::id_type meshId;
	glm::mat4 model;
	float alpha;                   ///< 0..1 (atom alpha x collection alpha)
	glm::vec2 uv;                  ///< the AnimTextured offset (0, 0 otherwise)
	bool translucent;              ///< additive or alpha < 1: drawn with the blended objects
	bool additive;                 ///< material mode 13: SRCALPHA / ONE, no Z write
	std::array<uint8_t, 3> colour; ///< the atom colour's r, g, b (or x the land light)
	bool landscapeColour;          ///< DrawWithLandscapeColor: the colour x the land light, else the colour alone
	/// The atom's specular colour (D3DCOLOR) given to the object with the colour: added to the land's specular with
	/// DrawWithLandscapeColor, else set as it is
	uint32_t specular {0};
	/// The object's global alpha: its draw then takes the global alpha mode table and so blends the opaque modes 0, 2,
	/// 4, 9, 17 with the colour's alpha. Without it the materials' own modes: the alpha only shows in the modes that
	/// blend
	bool globalAlpha {true};
	/// A ParticleAnimCreator atom's bones (graphics::ComputePose at the time of its frame); empty for the still meshes
	std::vector<glm::mat4> pose {};
	/// DrawCutByPlane: the atom is drawn cut by the default plane instead of whole
	bool cutByPlane {false};
	/// Its effect's draw path: Sorted, its own Z object at the model's translation (opaque too); Queued /
	/// Immediate, drawn at its place in its effect's items (manager::OrderedEffect, matched by `atom`)
	DrawPath path {DrawPath::Sorted};
	uint32_t effect {0};        ///< the effect's id (manager::Drawable::effect)
	const Atom* atom {nullptr}; ///< the atom (Effect::DrawAtom::atom), the key into its effect's items
	/// A render mode every material of the draw takes instead of its own (none: the materials' own, or 13 for an
	/// additive atom); the atoms leave it empty, an object's ghost sets it on its second draw (ecs::object_ghosts)
	std::optional<graphics::render_modes::Mode> mode {};
};
/// Every mesh atom of the running effects, interpolated since the last turn
[[nodiscard]] std::vector<Instance> Collect();
/// Whether any effect has a mesh atom (without interpolating them or working out their poses)
[[nodiscard]] bool Any();
/// One node of the human shadow list (CastHumanShadow, MeshCreator::castHumanShadow): an atom drawn this frame whose
/// particle's object casts a shadow list shadow, updated as the physics objects' (the light straight above, not over
/// the objects)
struct HumanShadow
{
	const Atom* atom;     ///< the key of its shadow (the particle, from its creation to its destruction)
	entt::id_type meshId; ///< the particle's object's mesh (and its radius)
	glm::mat4 model;      ///< the object's matrix as drawn (Instance::model): the vertices and the position
	float scale;          ///< the atom's drawn scale
};
/// The atoms of the last Collect with CastHumanShadow, in Collect's order (the original walks the list from its head,
/// the last pushed first: (approximate) the order only changes which shadow is updated first)
[[nodiscard]] const std::vector<HumanShadow>& HumanShadows();
} // namespace mesh_atoms

} // namespace openblack::psys
