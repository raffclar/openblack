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

#include <vector>

#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "Particles/ParticleDrawFrame.h"

namespace openblack::ecs::systems
{

/// The draw of the particle effects, in the presentation. The rules of the simulation step the effects. This system only
/// reads them and collects what they draw between the turns. A computer that shows nothing, such as the dedicated
/// server, does not have it.
class ParticleSystemInterface
{
public:
	using EffectId = ParticleRuleSystemInterface::EffectId;

	/// How many things the last collected frame drew, for the debug window
	struct DrawStats
	{
		size_t sprites;
		size_t chains;
		size_t meshes;
		size_t mists;
		size_t lightStamps;
		size_t effects;
	};

	virtual ~ParticleSystemInterface() = default;

	/// Finds the names of the texture sheets on the disk. Call it behind the loading screen, so that the first draw of an
	/// effect does not read the folder in a frame.
	virtual void LoadTextures() {}
	/// The marks over the creatures that learn, drawn with the effects of the world until set again
	virtual void SetLearningMarks(std::vector<particles::draw::LearningMark> /*marks*/) {}
	/// Everything the effects draw this frame, t of the way through the game turn, into a frame that is cleared first.
	/// The frame does not depend on the camera: each pass orders it for its own (particles::draw::Order). In the temple
	/// only the effects of the temple draw, and outside it only the effects of the world.
	virtual void CollectDrawFrame(float turnFraction, particles::draw::Frame& frame, bool inTemple) const = 0;
	/// Clears the frame and fills it with what one effect draws, none if the effect does not run
	virtual void CollectEffectDrawFrame(EffectId id, float turnFraction, particles::draw::Frame& frame) const = 0;
	[[nodiscard]] virtual DrawStats GetDrawStats() const = 0;
};

} // namespace openblack::ecs::systems
