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
#include <cstdint>

#include <optional>
#include <string_view>

#include <entt/entity/entity.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/vec2.hpp>

#include "Commands/PlayerCommands.h"
#include "Editor/EditorPaths.h"
#include "Editor/LandBrush.h"

namespace openblack
{
class LandIslandInterface;
}

namespace openblack::ecs
{
class Registry;
}

/// The strokes of the land brush and the paths of the editor as edits that each computer does at the same turn. A single
/// player game does them at once with the same rules.
namespace openblack::editor::land_edits
{

/// The land as the land edits read and change it. The game gives the land of the world, and the tests give a fake.
class EditableLand
{
public:
	virtual ~EditableLand() = default;
	/// The altitude of a corner, none where the map has no land
	[[nodiscard]] virtual std::optional<uint8_t> AltitudeAt(glm::u16vec2 corner) const = 0;
	/// The country of a corner, none where the map has no land
	[[nodiscard]] virtual std::optional<uint8_t> CountryAt(glm::u16vec2 corner) const = 0;
	/// The count of the countries of the land
	[[nodiscard]] virtual int Countries() const = 0;
	/// The height of the land at a point, x and z in metres
	[[nodiscard]] virtual float HeightAt(glm::vec2 point) const = 0;
	virtual void SetAltitude(glm::u16vec2 corner, uint8_t altitude) = 0;
	virtual void SetCountry(glm::u16vec2 corner, uint8_t country) = 0;
};

/// The land of the world as an editable land. The drawing of the land follows at its next commit.
class WorldLand final: public EditableLand
{
public:
	explicit WorldLand(LandIslandInterface& land)
	    : _land(land)
	{
	}
	[[nodiscard]] std::optional<uint8_t> AltitudeAt(glm::u16vec2 corner) const override;
	[[nodiscard]] std::optional<uint8_t> CountryAt(glm::u16vec2 corner) const override;
	[[nodiscard]] int Countries() const override;
	[[nodiscard]] float HeightAt(glm::vec2 point) const override;
	void SetAltitude(glm::u16vec2 corner, uint8_t altitude) override;
	void SetCountry(glm::u16vec2 corner, uint8_t country) override;

private:
	LandIslandInterface& _land;
};

/// The length of each side of the map, in metres. A stroke or a point of a path is on the map from 0 to this length.
constexpr float k_MapSide = static_cast<float>(land_brush::k_MapCorners) * land_brush::k_CornerSpacing;

/// The stroke of a brush at a point on the land, x and z in metres
[[nodiscard]] player_commands::EditorLandStroke StrokeOf(const land_brush::Brush& brush, glm::vec2 centre) noexcept;

/// The brush that a stroke uses
[[nodiscard]] land_brush::Brush BrushOf(const player_commands::EditorLandStroke& stroke) noexcept;

/// Why the world does not do a stroke, or none when it does it. A paint needs a country under the count of the countries
/// of the land, when the count is known. The rules keep no state.
[[nodiscard]] std::optional<std::string_view> LandStrokeRefusal(const player_commands::EditorLandStroke& stroke,
                                                                std::optional<int> countries) noexcept;

/// Does a stroke on the land, and gives the count of the corners that changed. A stroke that raises or lowers the land
/// keeps each thing in reach at its height above the land, in the order of the registry. A thing in a hand or in the
/// physics does not move. The drawing of the land does not change: the caller commits the land after the strokes.
size_t ApplyLandStroke(ecs::Registry& registry, EditableLand& land, const player_commands::EditorLandStroke& stroke);

/// Why the world does not make a path, or none when it makes it. The rules keep no state.
[[nodiscard]] std::optional<std::string_view> PathRefusal(const player_commands::EditorPath& path) noexcept;

/// The path that the world made
struct MadePath
{
	entt::entity entity {entt::null};
	/// The number of a stream, none for a footpath
	std::optional<int> stream;
	/// The buildings that a footpath leads from
	size_t links {0};
};

/// Makes a footpath or a stream from a path of the editor. The land of now gives the height of each point. A footpath
/// leads from the buildings at its ends that can keep a link. The path goes in the drawn paths, when there are drawn
/// paths. None when the world refuses the path.
std::optional<MadePath> ApplyPath(ecs::Registry& registry, const EditableLand& land, const player_commands::EditorPath& path,
                                  paths::DrawnPaths* drawn);

/// Whether a thing can keep the link of a footpath that starts or ends on it: a building
[[nodiscard]] bool CanHoldLink(const ecs::Registry& registry, entt::entity entity);

/// Does a stroke on the land of the world at the turn, as each computer of a network game does it. The drawing of the land
/// follows on the main thread, one time for all the strokes of the turn. Gives the count of the corners that changed, or
/// none when the world refused the stroke.
std::optional<size_t> RunLandStroke(const player_commands::EditorLandStroke& stroke);

/// Makes a path of the editor in the world at the turn, as each computer of a network game does it
std::optional<MadePath> RunPath(const player_commands::EditorPath& path);

/// Sends a stroke as a command of the turns. The stroke comes at the next turn.
void SendLandStroke(const player_commands::EditorLandStroke& stroke);

/// Sends a path as a command of the turns. The path comes at the next turn.
void SendPath(const player_commands::EditorPath& path);

} // namespace openblack::editor::land_edits
