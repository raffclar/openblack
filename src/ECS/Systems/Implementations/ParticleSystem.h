/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include <entt/entity/entity.hpp>

#include "ECS/Systems/ParticleSystemInterface.h"
#include "Particles/ParticleClassRegistry.h"
#include "Particles/ParticleCreators.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleMaths.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The particle effects' world: the land's height, the players' colours, the camera and the objects miracles act on
class GameParticleWorld final: public particles::ParticleWorldInterface
{
public:
	[[nodiscard]] float LandHeight(glm::vec2 xz) const override;
	[[nodiscard]] uint32_t PlayerColour(int player) const override;
	[[nodiscard]] glm::vec3 CameraRight() const override;
	[[nodiscard]] glm::vec3 CameraUp() const override;
	[[nodiscard]] std::optional<TargetInfo> Target(entt::entity target, bool centre) const override;
	[[nodiscard]] bool IsTargetHeld(entt::entity target) const override;
	[[nodiscard]] bool IsTargetClaimed(entt::entity target) const override { return _claimed.contains(target); }
	void ClaimTarget(entt::entity target, bool claimed) override;
	void Reset() { _claimed.clear(); }

private:
	std::set<entt::entity> _claimed;
};

/// The models and light maps of the particle creators, found by their names and loaded once through the resource caches
class GameCreatorResources final: public particles::CreatorResourcesInterface
{
public:
	[[nodiscard]] std::optional<entt::id_type> MeshByName(std::string_view name) override;
	[[nodiscard]] std::optional<entt::id_type> MeshByFile(std::string_view path) override;
	[[nodiscard]] std::optional<entt::id_type> LightMap(std::string_view path, int pitch, int channels, int framesInFile,
	                                                    int framesInUse) override;

private:
	/// The game's list of models by name, read when first needed
	std::optional<std::map<std::string, int32_t, std::less<>>> _meshNames;
};

class ParticleSystem final: public ParticleSystemInterface
{
public:
	ParticleSystem();
	~ParticleSystem() override;
	ParticleSystem(const ParticleSystem&) = delete;
	ParticleSystem& operator=(const ParticleSystem&) = delete;
	ParticleSystem(ParticleSystem&&) = delete;
	ParticleSystem& operator=(ParticleSystem&&) = delete;

	EffectId Start(std::string_view file, glm::vec3 origin, float magnitude, bool synced) override;
	EffectId Start(ParticleType type, glm::vec3 origin, float magnitude, bool synced) override;
	EffectId StartForSpell(ParticleType type, glm::vec3 origin, glm::vec3 direction, float magnitude,
	                       particles::SpellSink& sink) override;
	bool ProcessForSpell(EffectId id, const particles::ProcessInfo& info, float seconds) override;
	EffectId StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> turns, entt::entity owner,
	                         float magnitude) override;

	void SetOrigin(EffectId id, glm::vec3 origin) override;
	void SetPlayer(EffectId id, int player) override;
	void SetDrawPath(EffectId id, particles::draw::DrawPath path) override;
	void AddTarget(EffectId id, entt::entity target) override;
	void CloseDown(EffectId id) override;
	void Delete(EffectId id) override;
	[[nodiscard]] bool IsRunning(EffectId id) const override;
	[[nodiscard]] particles::Effect* Find(EffectId id) override;

	void ProcessTurn() override;
	void Reset() override;

	void CollectDrawFrame(float turnFraction, particles::draw::Frame& frame) const override;
	[[nodiscard]] DrawStats GetDrawStats() const override { return _drawStats; }
	[[nodiscard]] std::vector<EffectInfo> GetEffects() const override;
	[[nodiscard]] std::vector<std::string> GetFileNames() const override;
	void SetPaused(bool paused) override { _paused = paused; }
	[[nodiscard]] bool IsPaused() const override { return _paused; }

private:
	struct Running
	{
		EffectId id;
		std::string file;
		std::unique_ptr<particles::Effect> effect;
		/// Stepped by its miracle, not by ProcessTurn
		bool ownedBySpell {false};
		/// A spot visual's turns left, negative for ever
		std::optional<int> turnsLeft;
		/// An object it follows and ends with
		entt::entity owner {entt::null};
		particles::draw::DrawPath path {particles::draw::DrawPath::Sorted};
	};

	std::deque<Running>::iterator FindRunning(EffectId id);
	[[nodiscard]] std::deque<Running>::const_iterator FindRunning(EffectId id) const;
	/// Steps an effect; true once it has ended
	bool StepEffect(particles::Effect& effect, float seconds);
	/// The sheets its sprites are drawn from, looked up whatever case the files spell their names in
	void ResolveTextures(const particles::Effect& effect);

	GameCreatorResources _resources;
	particles::ParticleClassRegistry _classes;
	GameParticleWorld _world;
	particles::maths::ValueNoise _noise;
	/// Newest first, as they are stepped and drawn
	std::deque<Running> _effects;
	EffectId _nextId {1};
	bool _paused {false};
	/// The textures by the name a creator spells them with: the sheet and its alpha
	std::map<std::string, std::pair<entt::id_type, entt::id_type>, std::less<>> _textures;
	/// Every texture's name in lower case, to its spelling on disk
	std::map<std::string, std::string, std::less<>> _textureStems;
	/// The particle classes already reported as not run yet
	std::set<std::string, std::less<>> _reportedUnported;
	/// What the last collected frame drew, and its walk, kept for its room
	mutable DrawStats _drawStats {};
	mutable particles::Effect::DrawWalk _walk;
};

} // namespace openblack::ecs::systems
