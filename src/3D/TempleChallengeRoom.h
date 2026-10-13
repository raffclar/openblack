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

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/TempleInteriorInterface.h"
#include "Graphics/GraphicsHandle.h"
#include "Temple/ChallengeRoomRules.h"

namespace openblack
{
namespace graphics
{
class L3DMesh;
}
namespace ecs
{
class Registry;
}

/// What the tooltips see of one of the challenge room's pictures or replay buttons
struct TempleChallengeControl
{
	uint32_t subMesh;
	bool button;
	/// The challenge's place on the walls, and whether it is done
	uint32_t place;
	bool done;
};

/// The temple's challenge room: a picture for each challenge the player has started, framed in silver, or gold for a
/// quest, with a button under it to replay its reminder until it is done. The pictures are drawn in low detail; the
/// one clicked to be looked at closer is drawn in high detail, and once the camera is close a click on it (or on a
/// button) replays its challenge.
class TempleChallengeRoom
{
public:
	TempleChallengeRoom() = default;
	~TempleChallengeRoom();
	TempleChallengeRoom(const TempleChallengeRoom&) = delete;
	TempleChallengeRoom& operator=(const TempleChallengeRoom&) = delete;

	/// Finds the room's pictures, frames and buttons among its mesh's submeshes
	void Find(const graphics::L3DMesh& mesh);
	/// Coming into the room, no picture is looked at in high detail
	void Enter();
	/// Brings the pictures and the parts shown up to date with the player's challenge records, in the order they started
	void Update(const ecs::Registry& registry);

	/// The pictures' textures, by submesh
	[[nodiscard]] std::vector<TempleSubMeshTexture> GetTextures() const;
	/// The parts not drawn: those of places without a challenge, the frames of the other kind and done challenges'
	/// buttons
	[[nodiscard]] std::vector<uint32_t> GetHidden() const;
	/// Whether a submesh is a part drawn, a control the cursor makes glow
	[[nodiscard]] bool IsControl(uint32_t subMesh) const;
	/// The pictures and buttons drawn, in the mesh's order, as the tooltips see them
	[[nodiscard]] std::vector<TempleChallengeControl> GetToolTipControls() const;
	/// The place of the picture last looked at in high detail, kept until the player comes into the room again
	[[nodiscard]] std::optional<uint32_t> GetHighDetailPlace() const { return _highDetail; }
	/// Whether the camera was last sent to a picture rather than the scroll
	[[nodiscard]] bool IsPictureLookedAt() const { return _pictureLookedAt; }
	/// The record of the picture the camera was last sent to, while it has one
	[[nodiscard]] std::optional<entt::entity> GetLookedAtRecord() const;
	/// The camera was sent to the room's scroll
	void LookedAtScroll() { _pictureLookedAt = false; }

	/// Where the camera is to look at a picture from, and the point it looks at, in the temple
	struct Focus
	{
		glm::vec3 position;
		glm::vec3 lookAt;
	};
	/// What letting go of a click on a picture or button did
	struct Outcome
	{
		std::optional<Focus> lookAt;
		/// The challenge whose reminder is to be replayed
		std::optional<entt::entity> replay;
	};
	/// A press on a picture or button drawn takes the mouse, and letting go over it clicks it; closeUp is whether the
	/// camera is wholly close to a picture. True while a part has the press.
	bool Hold(bool pressed, std::optional<uint32_t> hoveredSubMesh, bool closeUp, Outcome& outcome);
	[[nodiscard]] bool IsHeld() const { return _held.has_value(); }

private:
	struct Part
	{
		uint32_t subMesh;
		temple::challenge_room::Control control;
		/// The part's frame and box, from the mesh
		glm::mat4 frame {1.0f};
		glm::vec3 min {0.0f};
		glm::vec3 max {0.0f};
	};
	/// A challenge on the walls, and its picture's textures as last uploaded
	struct Place
	{
		entt::entity record;
		temple::challenge_room::Challenge shown;
		std::optional<graphics::TextureHandle> small;
		std::optional<graphics::TextureHandle> full;
		/// The record's count of pictures taken when each texture was last written
		std::optional<uint32_t> smallTaken;
		std::optional<uint32_t> fullTaken;
	};
	[[nodiscard]] std::vector<temple::challenge_room::Challenge> Challenges() const;
	[[nodiscard]] bool IsShown(const Part& part) const;
	[[nodiscard]] const Part* FindPart(uint32_t subMesh) const;
	static void Destroy(Place& place);

	std::vector<Part> _parts;
	std::vector<Place> _places;
	std::optional<uint32_t> _highDetail;
	bool _pictureLookedAt {false};
	std::optional<uint32_t> _held;
	bool _wasPressed {false};
};

} // namespace openblack
