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

#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/mat4x4.hpp>

namespace openblack::ecs::components
{

/// A boned mesh playing a clip of AllAnims.anm (ecs/Animations.h). The renderer draws the entity with `pose` (one model
/// matrix per bone of its mesh) instead of the mesh's rest pose.
struct SkeletalAnimation
{
	/// the clip's id in the animation manager (ecs::ClipId of its index in AllAnims.anm) and that index (ANM_ enum)
	entt::id_type clip {0};
	int32_t clipIndex {-1};
	bool hasClip {false};
	/// milliseconds into the clip
	float time {0.0f};
	float speed {1.0f};
	/// > 0: the clip advances with the ground covered at this many m/s (a moving state), not with time
	float distanceSpeed {0.0f};
	/// The villager's transition flags, kept here: 0x800 an into / out-of clip plays, 0x1000 an out-of clip plays
	/// first
	uint16_t transitionFlags {0};
	/// test hook OPENBLACK_TEST_ANIM: the clip stays whatever the villager does
	bool locked {false};
	/// The CARRIED_OBJECT drawn in its hand (ECS/CarriedProps.h): 1 none, 2 axe ... 15 tree 3
	int32_t carriedObject {1};
	/// test hook OPENBLACK_TEST_CARRY: the carried object stays
	bool carriedLocked {false};
	/// The villager's draw skips the body while the state's info.dat clip is -4 (ANM_DONT_DRAW: inside the house): the mesh
	/// is taken off meanwhile (then nothing draws or picks it) and kept here
	entt::id_type hiddenMesh {0};
	/// the bones' model matrices for the current time (empty until the first update)
	std::vector<glm::mat4> pose;
	/// the frame's local matrices of the bones (before each is put under its parent): what the skinning writes into
	/// the bone buffer, read by the shadow blobs (Renderer::DrawHumanShadows). Empty without a clip, or with a clip of
	/// another skeleton
	std::vector<glm::mat4> locals;
	/// The SuperVillager's clip cross-fade (ECS/SuperVillager.h).
	/// crossFadeMs > 0: a clip change blends the clip drawn before it (frozen at its last time) into the new one over that
	/// many ms (300); 0, every other villager: the instant cut of a plain clip change. With DrawPosition::followSnap the
	/// fade counts down but is not drawn. Set by ECS/SuperVillager every frame
	int32_t crossFadeMs {0};
	/// The on-screen region test failed this frame, so the cross-fade draw does not run: the fade state (and the last
	/// clip's time) stay as they are and only the plain pose is computed (the body is not drawn). Set by
	/// ECS/SuperVillager
	bool crossFadeFrozen {false};
	/// The pose the SuperVillager's body is drawn with while its fade is drawn (the blend goes to the SuperVillager's
	/// bone buffer only); empty otherwise, for every other villager always. `pose` keeps the plain pose: (inferred) the
	/// carried prop and the foot shadows follow it, as the villager's draw poses the object with its own pose before the
	/// cross-fade. Read through ecs::DrawnPose
	std::vector<glm::mat4> drawnPose;
	struct CrossFade
	{
		bool hasLast {false};       ///< A clip was drawn already
		entt::id_type lastClip {0}; ///< The clip drawn last frame
		float lastTime {0.0f};      ///< Its time (after the draw)
		entt::id_type oldClip {0};
		float oldTime {0.0f}; ///< Frozen during the fade
		int32_t leftMs {0};
		float weight {0.0f}; ///< The old clip's weight: left / crossFadeMs
	} crossFade;
};

} // namespace openblack::ecs::components
