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

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>

#include "ECS/Systems/ParticleSystemInterface.h"
#include "Particles/ParticleEffect.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The draw of the particle effects. It reads the running effects from the rules of the simulation and never steps or
/// changes them. It keeps only what the draw needs: the texture sheets of the effects and the marks of the creatures
/// that learn.
class ParticleSystem final: public ParticleSystemInterface
{
public:
	/// Gives the rules of the simulation, none when there are none
	using FindRules = std::function<const ParticleRuleSystemInterface*()>;
	/// Finds the names of the texture sheets on the disk, by their names in lower case
	using FindTextureStems = std::function<std::map<std::string, std::string, std::less<>>()>;

	ParticleSystem(FindRules findRules, FindTextureStems findTextureStems);

	/// The names of the texture sheets in the textures folder of the game, by their names in lower case
	[[nodiscard]] static std::map<std::string, std::string, std::less<>> TextureStemsOnDisk();

	void LoadTextures() override;
	void SetLearningMarks(std::vector<particles::draw::LearningMark> marks) override { _learningMarks = std::move(marks); }
	void CollectDrawFrame(float turnFraction, particles::draw::Frame& frame, bool inTemple) const override;
	void CollectEffectDrawFrame(EffectId id, float turnFraction, particles::draw::Frame& frame) const override;
	[[nodiscard]] DrawStats GetDrawStats() const override { return _drawStats; }

private:
	using DrawnEffect = ParticleRuleSystemInterface::DrawnEffect;

	/// What the draw of the effects reads from the world
	[[nodiscard]] particles::draw::Sources DrawSources(const ParticleRuleSystemInterface& rules) const;
	/// Adds what a running effect draws to a frame
	void AddDrawn(const DrawnEffect& drawn, float turnFraction, particles::draw::Frame& frame,
	              const particles::draw::Sources& sources) const;
	/// Finds the sheets of the names that the rules added since the last frame
	void ResolveTextures(const ParticleRuleSystemInterface& rules) const;
	/// The sheets of light of the land, each in its place among the things that blend
	static void AddLightSheets(const ParticleRuleSystemInterface& rules, particles::draw::Frame& frame);

	FindRules _findRules;
	FindTextureStems _findTextureStems;
	std::vector<particles::draw::LearningMark> _learningMarks;
	/// The textures by the name that a creator spells them with: the sheet and its alpha
	mutable std::map<std::string, std::pair<entt::id_type, entt::id_type>, std::less<>> _textures;
	/// Every texture name in lower case, to its spelling on the disk
	mutable std::map<std::string, std::string, std::less<>> _textureStems;
	mutable bool _stemsFound {false};
	/// How many names of the list of the rules have a sheet
	mutable size_t _sheetsResolved {0};
	/// What the last collected frame drew, and its walk, kept for its room
	mutable DrawStats _drawStats {};
	mutable particles::Effect::DrawWalk _walk;
};

} // namespace openblack::ecs::systems
