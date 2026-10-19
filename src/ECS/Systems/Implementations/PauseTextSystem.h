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

#include <optional>
#include <vector>

#include "ECS/Systems/PauseTextSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class PauseTextSystem final: public PauseTextSystemInterface
{
public:
	void SetInterface(const gui::GameInterface* interface) override { _interface = interface; }
	void Update(const gui::pause_text::Situation& situation, uint32_t milliseconds) override;
	[[nodiscard]] std::optional<gui::pause_text::Shown> GetShown() const override { return _shown; }
	[[nodiscard]] std::vector<OrientedTextVertex> GetText(const Camera& camera, glm::u16vec2 screen) const override;
	[[nodiscard]] const graphics::Texture2D* GetTextTexture() const override;

private:
	const gui::GameInterface* _interface {nullptr};
	gui::pause_text::Fader _fader;
	std::optional<gui::pause_text::Shown> _shown;
};

} // namespace openblack::ecs::systems
