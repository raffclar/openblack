/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <list>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "Graphics/ShadowMath.h"
#include "Graphics/UniqueHandle.h"

/// The list of projected shadows (wiki: docs/bw1-notes/rendering.md, "Projected shadows (ShadowInfo)"): one entry per
/// caster, the newest first, each with its own 32 x 32 texture that the CPU rasterizes every frame (shadow_math) and
/// the renderer draws over the land blocks it touches (RendererShadows.cpp) and, for the ones with `onObjects`, over
/// the objects.
///
/// The casters (the producers in Frame): the hand (made with the hand, the complex update), the flying physics objects
/// (generic update, removed with the physics) and the objects with a components::DynamicShadow (the launched boat; the
/// SuperVillagers' temporary shadow) and the particle mesh atoms with CastHumanShadow (made with the particle, updated
/// with the particle's object; psys::mesh_atoms::HumanShadows).
/// Not here yet: the creature and the prediction object.
namespace openblack::graphics::shadow_list
{

/// The hand's shadow as the original draws it (checked against captures of the original game: the hand's silhouette
/// with its fingers, light and see-through, a held orb darker and round under it): the hand's shadow uses half rows,
/// so the raster skips the even subrows (4/15 at most), 32 x 32, projected onto the hand's own y, and the held object
/// is rasterized into the same texture at full density (from its own base y, with half rows off around it). false
/// gives back openblack's look from before the list (64 x 64 at full density, projected onto the ground under the
/// hand, nothing of the held object), kept only to compare.
inline constexpr bool k_HandShadowAsOriginal = true;

/// The generic update (the holder casters) or the complex one (complex objects: the hand, the creature)
enum class Update : uint8_t
{
	Generic,
	Complex,
};

/// Where the shadow's light comes from
enum class LightKind : uint8_t
{
	Vertical, ///< the caster + (0, 15000, 0)
	Sun,      ///< the holder's: the fixed sun
	Hand,     ///< the caster + (0, 200, 0)
	Creature, ///< the current light brought within 3 radii
};

struct ShadowInfo
{
	entt::entity caster {entt::null}; ///< its owner: the hand, the physics object, the boat's hull
	/// A particle mesh atom instead (caster stays entt::null): its key, and its object of this frame
	/// (psys::mesh_atoms::HumanShadow: the mesh, the drawn matrix, the scale)
	const void* psysAtom {nullptr};
	entt::id_type particleMesh {0};
	glm::mat4 particleMatrix {1.0f};
	float particleScale {1.0f};
	entt::entity held {entt::null}; ///< the held object rasterized with the caster
	Update update {Update::Generic};
	LightKind light {LightKind::Vertical};
	/// Set: the land's t' takes H = the caster's altitude; the generic update writes it, the complex one never does,
	/// so the hand has H = the caster's y and t' = 1
	bool emitter {true};
	bool active {true};     ///< the complex update clears it when the object is hidden
	bool onObjects {false}; ///< drawn over the objects too (the hand, the boat)
	bool halfRows {false};  ///< only the odd subrows (the hand)
	int texels {shadow_math::k_Texels};
	int alpha {0}; ///< 0 = not drawn
	int baseAlpha {255};
	shadow_math::Projection projection;                  ///< the light, caster - light, the caster's y
	shadow_math::Box box;                                ///< {x0, z0, x1, z1} and kMin
	float landT {1.0f};                                  ///< the land draw's t' of this frame (one H per shadow)
	shadow_math::Texels texels16 {};                     ///< the alpha nibbles n of the texture, n / 15
	graphics::UniqueHandle<bgfx::TextureHandle> texture; ///< R8 of n x 17, CLAMP; destroyed with the entry
	bool seen {false};                                   ///< the producers found the caster this frame
};

struct FrameInputs
{
	glm::vec3 camera {0.0f}; ///< the camera position
	/// The world-to-clipping matrix (billboard::CameraFrame::clipMatrices): the visibility of the blocks
	affine::AffineMatrix worldToClipping;
	float nearW {1.0f};   ///< the near clip distance
	bool landRef {false}; ///< the LandRef detail key (the blocks' corners and centre)
};

/// The block states of this frame for the shadows' land draw: each block's visibility (its outcodes with the main
/// camera) and |centre - camera|. Flat as the block index (32 x 32 blocks, bx * 32 + bz), refilled each frame
struct Blocks
{
	static constexpr int k_Side = 32;
	std::array<shadow_math::BlockState, static_cast<size_t>(k_Side* k_Side)> states {};
	[[nodiscard]] shadow_math::BlockState At(int x, int z) const
	{
		if (x < 0 || z < 0 || x >= k_Side || z >= k_Side)
		{
			return {};
		}
		return states[static_cast<size_t>(x * k_Side + z)];
	}
};

class List
{
public:
	List();
	~List();
	List(const List&) = delete;
	List& operator=(const List&) = delete;

	/// A new entry in front of the others, its texture cleared
	ShadowInfo& Add(entt::entity caster, Update update, LightKind light, bool onObjects, bool halfRows, int texels);
	/// Out of the list, its texture released
	void Remove(entt::entity caster);
	void Clear();
	/// One frame: the producers, then every entry's update (fade, alpha, light), its silhouette (projection, raster,
	/// resolve, chroma blur, baked fade) and its upload
	void Frame(const FrameInputs& inputs);

	/// The active entries with a non-zero alpha, newest first
	template <typename F>
	void ForEachActive(F&& function) const
	{
		for (const auto& shadow : _shadows)
		{
			if (shadow.active && shadow.alpha != 0 && shadow.texture.IsValid())
			{
				function(shadow);
			}
		}
	}
	[[nodiscard]] const std::list<ShadowInfo>& GetShadows() const { return _shadows; }

private:
	std::list<ShadowInfo> _shadows;
	int _frame {0};
	/// The block states, kept between frames (one list, updated once a frame on the render thread)
	Blocks _blocks;
};

} // namespace openblack::graphics::shadow_list
