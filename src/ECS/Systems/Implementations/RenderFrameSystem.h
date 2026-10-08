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

#include <glm/vec3.hpp>

#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "ECS/Systems/RenderFrameSystemInterface.h"
#include "Graphics/Mists.h"
#include "Graphics/ModelLight.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// What the renderer keeps from frame to frame, as the game starts it
class RenderFrameSystem final: public RenderFrameSystemInterface
{
public:
	[[nodiscard]] const land_light::Cells& GetLandCells() const noexcept override { return _cells; }
	void SetLandCells(land_light::Cells cells) override;
	void RestoreLandCells() override;
	void SetLandLuminosity(const std::vector<uint8_t>& luminosity) override;

	[[nodiscard]] const std::vector<land_light::Stamp>& GetLandStamps() const noexcept override { return _stamps; }
	void AddLandStamp(const land_light::Stamp& stamp) override;
	void ApplyLandStamps() override;
	void ClearLandStamps() noexcept override;
	[[nodiscard]] uint32_t GetLandStampFrame() const noexcept override { return _stampFrame; }

	[[nodiscard]] const LandLightTable& GetLandLightTable() const noexcept override { return _landLightTable; }
	void SetLandLightTable(const LandLightTable& table) noexcept override { _landLightTable = table; }

	[[nodiscard]] glm::vec3 GetModelLight() const noexcept override { return _modelLight; }
	void SetModelLight(const glm::vec3& position) noexcept override { _modelLight = position; }
	[[nodiscard]] int GetModelAmbient() const noexcept override { return _modelAmbient; }
	void SetModelAmbient(int ambient) noexcept override { _modelAmbient = ambient; }

	void SubmitMist(const mists::MistDesc& mist) override;
	[[nodiscard]] std::vector<mists::MistDesc> TakeSubmittedMists() override;

private:
	land_light::Cells _cells;
	std::vector<land_light::Stamp> _stamps;
	uint32_t _stampFrame {0};
	LandLightTable _landLightTable {LandLightTable::Unset()};
	glm::vec3 _modelLight {model_light::k_DefaultSun};
	int _modelAmbient {model_light::k_DefaultAmbient};
	std::vector<mists::MistDesc> _submittedMists;
};
} // namespace openblack::ecs::systems
