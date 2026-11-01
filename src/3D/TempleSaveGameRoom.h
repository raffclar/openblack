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

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/TempleInteriorInterface.h"
#include "3D/TempleSubMeshTexture.h"
#include "Graphics/GraphicsHandle.h"
#include "Temple/SaveGameRoomRules.h"

namespace openblack
{
namespace graphics
{
class L3DMesh;
}

/// What the tooltips see of one of the save game room's pictures or buttons
struct TempleSaveGameControl
{
	uint32_t subMesh;
	temple::save_game_room::Control control;
	/// The name of the slot's save, if it has one
	std::optional<std::u16string> saveName;
};

/// The temple's save game room: a picture for each slot of the saved games, with a save, a load and a delete button by
/// each. A slot with a save shows its small picture, and the picture clicked to be looked at closer shows its large
/// picture. A slot with no save shows half of the empty picture, by the player's alignment. Once the camera is close to
/// a picture, a click on it, or on a button, asks the player what to do with the slot.
class TempleSaveGameRoom
{
public:
	/// A saved game that the room lists, by its slot
	struct Save
	{
		uint32_t slot;
		std::u16string name;
		/// The date and the time of the save
		std::u16string date;
	};
	/// Reads a file of the saved games' pictures by its name, nothing when there is none
	using ReadPicture = std::function<std::optional<std::vector<uint8_t>>(std::string_view fileName)>;

	TempleSaveGameRoom() = default;
	~TempleSaveGameRoom();
	TempleSaveGameRoom(const TempleSaveGameRoom&) = delete;
	TempleSaveGameRoom& operator=(const TempleSaveGameRoom&) = delete;

	/// Finds the room's pictures and buttons among its mesh's submeshes
	void Find(const graphics::L3DMesh& mesh);
	/// The empty picture, 256 texels a side in RGBA with its top row first, which the room cuts in its two halves
	void SetEmptyPicture(std::span<const uint8_t> rgba);
	/// Reads the small pictures of the slots again, as the room does once it is set up and after each save
	void ReadSmallPictures(const ReadPicture& read);
	/// Reads the pictures again after a save into the slot. The large picture changes too when the slot shows in high
	/// detail.
	void ReadPicturesAfterSave(uint32_t slot, const ReadPicture& read);
	/// Coming into the room, no picture is shown in high detail, and no picture is looked at
	void Enter();
	/// The player leaves the temple: the room keeps its pictures, its empty picture and its list for the next visit, and
	/// forgets the rest, as a new room
	void Leave();
	/// The saved games that the room lists, in its order
	void SetSaves(std::vector<Save> saves);
	/// The player's alignment, from -1 to 1, which chooses the empty picture's half
	void SetAlignment(float alignment) { _alignment = alignment; }
	/// The camera was sent to the room's scroll
	void LookedAtScroll() { _pictureLookedAt = false; }

	[[nodiscard]] bool HasSave(uint32_t slot) const;
	/// The name of the slot's save, if it has one
	[[nodiscard]] std::optional<std::u16string_view> SaveName(uint32_t slot) const;
	/// The save of a slot that the close look shows: the last save in the list with the slot, if any
	[[nodiscard]] const Save* LastSaveOf(uint32_t slot) const;

	/// The pictures' textures, by submesh
	[[nodiscard]] std::vector<TempleSubMeshTexture> GetTextures() const;
	/// The buttons not drawn
	[[nodiscard]] std::vector<uint32_t> GetHidden(const temple::save_game_room::View& view) const;
	/// The button held, moved into the wall
	[[nodiscard]] std::vector<TempleSubMeshOffset> GetOffsets() const;
	/// Whether a submesh is a control drawn, which the cursor makes glow
	[[nodiscard]] bool IsControl(uint32_t subMesh, const temple::save_game_room::View& view) const;
	/// The pictures and buttons drawn, in the mesh's order, as the tooltips see them
	[[nodiscard]] std::vector<TempleSaveGameControl> GetToolTipControls(const temple::save_game_room::View& view) const;
	/// The slot of the picture last looked at in high detail, kept until the player comes into the room again
	[[nodiscard]] std::optional<uint32_t> GetHighDetailSlot() const { return _highDetail; }
	/// Whether the camera was last sent to a picture rather than the scroll
	[[nodiscard]] bool IsPictureLookedAt() const { return _pictureLookedAt; }

	/// Where the camera is to look at a picture from, and the point it looks at, in the temple
	struct Focus
	{
		glm::vec3 position;
		glm::vec3 lookAt;
	};
	/// What letting go of a click on a picture or a button did
	struct Outcome
	{
		std::optional<Focus> lookAt;
		/// The question to ask about a slot
		temple::save_game_room::Click click {temple::save_game_room::Click::Nothing};
		uint32_t slot {0};
	};
	/// A press on a control drawn takes the mouse, and letting go over it clicks it. The large picture of a picture
	/// looked at closer is read. True while a control has the press.
	bool Hold(bool pressed, std::optional<uint32_t> hoveredSubMesh, const temple::save_game_room::View& view,
	          const ReadPicture& read, Outcome& outcome);
	[[nodiscard]] bool IsHeld() const { return _held.has_value(); }
	/// The camera looks at the picture of a slot, as a click on the picture does. None when the room has no such picture.
	std::optional<Focus> LookAtPicture(uint32_t slot, const ReadPicture& read);

private:
	struct Part
	{
		uint32_t subMesh;
		temple::save_game_room::Control control;
		/// The part's frame and box, from the mesh
		glm::mat4 frame {1.0f};
		glm::vec3 min {0.0f};
		glm::vec3 max {0.0f};
	};
	[[nodiscard]] const Part* FindPart(uint32_t subMesh) const;
	/// Shows the picture of a part in high detail and gives where the camera looks at it from
	Focus LookCloser(const Part& part, const ReadPicture& read);
	void ReadLargePicture(uint32_t slot, const ReadPicture& read);
	static void Destroy(std::optional<graphics::TextureHandle>& texture);

	std::vector<Part> _parts;
	std::vector<Save> _saves;
	float _alignment {0.0f};
	/// The empty picture's evil and good halves
	std::optional<graphics::TextureHandle> _emptyEvil;
	std::optional<graphics::TextureHandle> _emptyGood;
	/// Each slot's small picture, from the files of small pictures
	std::vector<std::optional<graphics::TextureHandle>> _small;
	std::optional<graphics::TextureHandle> _large;
	std::optional<uint32_t> _highDetail;
	bool _pictureLookedAt {false};
	std::optional<uint32_t> _held;
	bool _wasPressed {false};
};

} // namespace openblack
