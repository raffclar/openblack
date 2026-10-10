/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <vector>

#include <entt/entity/registry.hpp>
#include <entt/signal/sigh.hpp>

#include "ECS/Systems/ScriptHighlightSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

#include "ECS/ScriptHighlightWorld.h"

namespace openblack::ecs::components
{
struct ScriptHighlight;
}

namespace openblack::ecs::systems
{

class ScriptHighlightSystem final: public ScriptHighlightSystemInterface
{
public:
	/// The highlights of the game's land
	ScriptHighlightSystem();
	/// The highlights of a world of their own, as tests give it
	explicit ScriptHighlightSystem(std::unique_ptr<script_highlights::ScriptHighlightWorldInterface> world);
	~ScriptHighlightSystem() override;

	entt::entity Create(uint32_t kind, glm::vec3 at, uint32_t challenge) override;
	void SetProperties(entt::entity highlight, uint32_t text, uint32_t category) override;
	void SetActive(entt::entity highlight, bool active) override;
	void SetDrawHeight(entt::entity highlight, float height) override;
	bool Tap(entt::entity highlight, bool byThisPlayer) override;
	void ProcessTurn() override;
	void UpdateFrame(float frameMilliseconds, float turnFraction, glm::vec3 camera) override;
	void Reset() override;
	[[nodiscard]] const script_highlights::Pulse& GetPulse() const override { return _pulse; }
	[[nodiscard]] const script_highlights::TipsRead& GetTipsRead() const override { return _tipsRead; }

private:
	/// The model it shows: its kind's active one once started, else its normal one
	void ShowModel(entt::entity entity, const components::ScriptHighlight& highlight);
	/// Where it stands: its place across the land, at its height above it
	[[nodiscard]] glm::vec3 StandingPoint(entt::entity entity, const components::ScriptHighlight& highlight) const;
	/// How far its model reaches across, as it is made
	[[nodiscard]] float ReachAcross(entt::entity entity) const;
	/// Whether it is drawn now: a sign always, a scroll while the scripts show them
	[[nodiscard]] bool Drawn(const components::ScriptHighlight& highlight) const;
	/// A frame of one highlight
	void UpdateHighlight(entt::entity entity, components::ScriptHighlight& highlight, float frameMilliseconds,
	                     glm::vec3 camera);
	/// Its glow, made the first time it shows, hidden while it doesn't
	void UpdateGlow(entt::entity entity, const components::ScriptHighlight& highlight, bool shown, glm::vec3 camera);
	/// A gold scroll's sparks go on while it is drawn
	void UpdateSparks(entt::entity entity, bool drawn, float frameMilliseconds);
	/// The glows whose highlight has gone go too
	void RemoveLoneGlows();

	void OnHighlightGone(entt::registry& registry, entt::entity entity);

	std::unique_ptr<script_highlights::ScriptHighlightWorldInterface> _world;
	script_highlights::Pulse _pulse;
	/// The tips the player has read, which the temple keeps from land to land
	script_highlights::TipsRead _tipsRead;
	/// The sign whose tip the help system's bubble shows
	entt::entity _tipShownBy {entt::null};
	std::vector<entt::scoped_connection> _connections;
};

} // namespace openblack::ecs::systems
