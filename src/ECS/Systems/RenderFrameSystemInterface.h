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

namespace openblack
{
class LandLightTable;
} // namespace openblack

namespace openblack::land_light
{
struct Cells;
struct Stamp;
} // namespace openblack::land_light

namespace openblack::mists
{
struct MistDesc;
} // namespace openblack::mists

namespace openblack::ecs::systems
{
/// What the renderer keeps from one frame to the next for the rest of the game: the land light's cells and stamps, the
/// land light table last built, the model light and the mists submitted this frame (Locator::renderFrameSystem)
class RenderFrameSystemInterface
{
public:
	virtual ~RenderFrameSystemInterface() = default;

	/// The land light's cells, which the land light is sampled from and the shaders are given
	[[nodiscard]] virtual const land_light::Cells& GetLandCells() const noexcept = 0;
	virtual void SetLandCells(land_light::Cells cells) = 0;
	/// This frame's cells start again from the loaded ones
	virtual void RestoreLandCells() = 0;
	/// Writes the luminosity bytes of this frame's cells back, as many as there are of both
	virtual void SetLandLuminosity(const std::vector<uint8_t>& luminosity) = 0;

	/// The light and shadow stamps of this frame, in the order they were added
	[[nodiscard]] virtual const std::vector<land_light::Stamp>& GetLandStamps() const noexcept = 0;
	virtual void AddLandStamp(const land_light::Stamp& stamp) = 0;
	/// Every stamp into this frame's cells, in the order they were added
	virtual void ApplyLandStamps() = 0;
	/// Empties the stamps and counts one more frame of them
	virtual void ClearLandStamps() noexcept = 0;
	/// How many times the stamps were cleared
	[[nodiscard]] virtual uint32_t GetLandStampFrame() const noexcept = 0;

	/// The land light table the renderer built last, which the game reads when it colours something by the land's
	/// light; LandLightTable::Unset() before the first one
	[[nodiscard]] virtual const LandLightTable& GetLandLightTable() const noexcept = 0;
	virtual void SetLandLightTable(const LandLightTable& table) noexcept = 0;

	/// The engine's one point light for the models: the default sun until it is moved
	[[nodiscard]] virtual glm::vec3 GetModelLight() const noexcept = 0;
	virtual void SetModelLight(const glm::vec3& position) noexcept = 0;
	[[nodiscard]] virtual int GetModelAmbient() const noexcept = 0;
	virtual void SetModelAmbient(int ambient) noexcept = 0;

	/// A mist to draw this frame besides the mist entities
	virtual void SubmitMist(const mists::MistDesc& mist) = 0;
	/// The mists submitted since the last call, which leaves none
	[[nodiscard]] virtual std::vector<mists::MistDesc> TakeSubmittedMists() = 0;
};
} // namespace openblack::ecs::systems
