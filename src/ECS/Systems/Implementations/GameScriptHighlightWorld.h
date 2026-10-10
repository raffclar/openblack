/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

#include "ECS/ScriptHighlightWorld.h"

namespace openblack::ecs::systems
{

/// The highlights' world in the game: the land and its things, the info table, the models, the particles, the sounds
/// and the scripts
class GameScriptHighlightWorld final: public script_highlights::ScriptHighlightWorldInterface
{
public:
	[[nodiscard]] Registry& Entities() override;
	[[nodiscard]] std::optional<script_highlights::KindInfo> InfoOf(uint32_t kind) const override;
	[[nodiscard]] std::optional<script_highlights::ModelBox> BoxOf(MeshId mesh) const override;
	[[nodiscard]] float LandHeight(glm::vec2 point) const override;
	[[nodiscard]] std::vector<script_highlights::ThingBelow> ThingsBelow(entt::entity highlight,
	                                                                     glm::vec3 point) const override;
	[[nodiscard]] uint32_t Turn() const override;
	[[nodiscard]] float MillisecondsPerTurn() const override;
	[[nodiscard]] bool ScrollsDrawn() const override;
	uint32_t StartEffect(ParticleType type, glm::vec3 at) override;
	void TargetEffect(uint32_t effect, entt::entity target) override;
	void StepEffect(uint32_t effect, float seconds) override;
	void MoveEffect(uint32_t effect, glm::vec3 to) override;
	void DeleteEffect(uint32_t effect) override;
	void PlaySound(uint32_t sample) override;
	[[nodiscard]] std::optional<components::Sprite> GlowLook() const override;
	void HelpEvent(uint32_t event) override;
	void StartHelpScript(std::string_view name) override;
	void ShowTip(entt::entity sign, uint32_t text, uint32_t category) override;
	void HideTip() override;
	void ReplayChallenge(uint32_t challenge) override;
};

} // namespace openblack::ecs::systems
