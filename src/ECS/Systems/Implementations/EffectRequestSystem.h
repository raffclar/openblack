/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <unordered_set>

#include "ECS/Systems/EffectRequestSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class EffectRequestSystem final: public EffectRequestSystemInterface
{
public:
	/// Finds the side that shows the effects, none on a computer that shows nothing
	using FindServer = std::function<EffectServerInterface*()>;

	/// Tells the side that shows the effects, if any, to tell this record of each start and end of an effect
	explicit EffectRequestSystem(FindServer findServer);
	~EffectRequestSystem() override;
	EffectRequestSystem(const EffectRequestSystem&) = delete;
	EffectRequestSystem& operator=(const EffectRequestSystem&) = delete;
	EffectRequestSystem(EffectRequestSystem&&) = delete;
	EffectRequestSystem& operator=(EffectRequestSystem&&) = delete;

	EffectHandle Start(const EffectRequest& request) override;
	void Show(const EffectRequest& request) override;
	EffectHandle StartHere(const EffectRequest& request) override;
	void Move(EffectHandle handle, glm::vec3 position) override;
	void Stop(EffectHandle handle) override;
	void Remove(EffectHandle handle) override;
	void RemoveOwnedBy(entt::entity owner) override;
	[[nodiscard]] bool IsRunning(EffectHandle handle) const override;
	void SetPlayer(EffectHandle handle, int player) override;
	void SetPower(EffectHandle handle, float power) override;
	void SetMagnitude(EffectHandle handle, float magnitude) override;
	void SetAlpha(EffectHandle handle, float alpha) override;
	void SetProcessInfo(EffectHandle handle, const particles::ProcessInfo& info) override;
	void SetEnabled(EffectHandle handle, bool enabled) override;
	void AddTarget(EffectHandle handle, entt::entity target) override;
	void AddTargetPosition(EffectHandle handle, glm::vec3 position) override;
	void AddPlasma(EffectHandle handle, const particles::PlasmaCommand& command) override;
	void AddManaPathSpark(EffectHandle handle, const particles::mana_path::Spark& spark) override;
	[[nodiscard]] bool ShowsEffects() const override;
	bool Step(EffectHandle handle, const particles::ProcessInfo& info, float seconds) override;
	void RestoreSink(EffectHandle handle, particles::SpellSink* sink) override;
	void SetHidden(EffectHandle handle, bool hidden) override;
	void SetDrawOffset(EffectHandle handle, glm::vec3 offset) override;
	void SetLiveHand(EffectHandle handle, glm::vec3 hand) override;
	[[nodiscard]] std::shared_ptr<particles::ShieldSphere> FindShield(glm::vec3 point, float margin) const override;
	void AddBeliefSprite(const particles::BeliefSprite& sprite) override;
	void AddHandManaPathSpark(const particles::mana_path::Spark& spark) override;
	void AddStormBolt(glm::vec3 cloudPoint) override;
	void SetFightMarks(std::vector<particles::draw::FightMark> marks) override;
	void AddLightSheet(const std::shared_ptr<particles::LightSheet>& sheet) override;
	void SetPlayerSymbols(std::vector<particles::draw::PlayerSymbol> symbols) override;
	void Reset() override;
	void SetShown(EffectHandle handle, bool shown) override;
	bool Renumber(EffectHandle from, EffectHandle to) override;
	[[nodiscard]] EffectHandle NextHandle() const override { return _next; }

	void EffectStarted(EffectHandle handle) override { _running.insert(handle); }
	void EffectEnded(EffectHandle handle) override { _running.erase(handle); }
	void EffectsCleared() override { _running.clear(); }

	void SaveState(reflection::BinaryWriter& out) const override;
	bool LoadState(reflection::BinaryReader& in) override;

private:
	/// The particles that show the effect, none for no effect or on a computer that shows nothing
	[[nodiscard]] EffectServerInterface* ServerFor(EffectHandle handle) const;

	FindServer _findServer;
	EffectHandle _next {k_FirstEffectHandle};
	/// The effects that run, as the side that steps them told. A save does not keep the record: the load of the
	/// effects tells it again.
	std::unordered_set<EffectHandle> _running;
};

} // namespace openblack::ecs::systems
