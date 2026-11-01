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

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/OrientedText.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleSubMeshTexture.h"
#include "Graphics/GraphicsHandle.h"
#include "Help/StoryHistory.h"
#include "Input/KeyBindings.h"
#include "Temple/ScrollFacts.h"
#include "Temple/ScrollPictureLine.h"

namespace openblack
{
namespace gui
{
class GameFont;
class TextDatabase;
} // namespace gui

/// The scrolls on the temple's walls that the rooms write their texts on: the world's statistics in the main room, the
/// creature's four in its room, the challenges, the save games and the library's seven. Each room, as it is set up,
/// makes a texture for each of its LH_Scroll submeshes and writes the scroll's text on it, and the scroll turns up and
/// down as the mouse drags it. Pressing a scroll has the camera look at it from close by, and while the
/// camera is close its text is drawn in front of it instead.
class TempleScrolls
{
public:
	using CreatureFacts = temple_scrolls::CreatureFacts;

	/// A saved game in the save game room: its date and name
	struct SavedGame
	{
		std::u16string date;
		std::u16string name;
	};

	/// What the scrolls tell of the game
	struct Facts
	{
		/// The game's count of the people in the world
		int32_t population {0};
		int32_t believersPercent {0};
		int32_t malePercent {0};
		int32_t deaths {0};
		int32_t births {0};
		int32_t sacrifices {0};
		int32_t buildings {0};
		int32_t wonders {0};
		int32_t disciples {0};
		/// The disciples of each kind the game counts, in the order the scroll lists them: builders, breeders,
		/// fishermen, farmers, foresters, missionaries, craftsmen and traders
		std::array<int32_t, 8> discipleKinds {};
		int32_t challengesDiscovered {0};
		int32_t challengesCompleted {0};
		/// The titles of the challenges discovered, as help text numbers, in the order they started
		std::vector<uint32_t> challenges;
		std::chrono::seconds timePlayed {0};
		int32_t saveCount {0};
		int32_t loadCount {0};
		std::vector<SavedGame> savedGames;
		/// The tips that the player read, as help text numbers, by the library's kinds: controls, the creature, village
		/// life, miracles and "did you know"
		std::array<std::vector<uint32_t>, 5> libraryHelp;
		/// The help texts that the scripts showed, which the library's last scroll shows. The game owns the history.
		const help::StoryHistory* history {nullptr};
		/// The player's creature, without which most of the creature's scrolls aren't written
		std::optional<CreatureFacts> creature;
		/// What each of the game's controls is bound to, which the help that names a control tells
		std::vector<input::KeyBinding> bindings {input::DefaultKeyBindings().begin(), input::DefaultKeyBindings().end()};

		/// Made up facts, for the systems openblack doesn't have yet
		static Facts Mock();
	};

	TempleScrolls(const gui::TextDatabase& texts, const gui::GameFont& font, std::vector<uint8_t> parchment);
	~TempleScrolls();
	TempleScrolls(const TempleScrolls&) = delete;
	TempleScrolls& operator=(const TempleScrolls&) = delete;

	/// What a texture of a scroll was last written with. The same gives the same texels.
	struct Drawn
	{
		std::optional<std::u16string> text;
		int32_t position;
		bool textInFront;
		uint16_t side;
		bool operator==(const Drawn&) const = default;
	};
	/// The textures of the scrolls of an earlier visit and what they show, which the next visit uses again. It destroys
	/// the textures that no visit took back.
	class KeptTextures
	{
	public:
		KeptTextures() = default;
		~KeptTextures();
		KeptTextures(const KeptTextures&) = delete;
		KeptTextures& operator=(const KeptTextures&) = delete;
		KeptTextures(KeptTextures&& other) noexcept;
		KeptTextures& operator=(KeptTextures&& other) noexcept;

	private:
		friend class TempleScrolls;
		struct Entry
		{
			graphics::TextureHandle texture;
			uint16_t side;
			std::optional<Drawn> drawn;
			uint32_t textHeight;
		};
		/// By the scroll's place in the rooms, none for a place with no texture
		std::vector<std::optional<Entry>> _entries;
	};

	/// As the rooms are set up: finds each scroll's submesh, makes its texture and writes it. The textures of an earlier
	/// visit are used again, and a scroll that shows the same as it did then is not written again.
	void Create(const Facts& facts, KeptTextures kept = {});
	/// Gives up the textures of the scrolls for the next visit. The scrolls then have none.
	[[nodiscard]] KeptTextures TakeTextures();

	/// Where the camera looks at a scroll from, and the point it looks at, in the temple
	struct Focus
	{
		glm::vec3 position;
		glm::vec3 lookAt;
	};
	/// A press on a scroll takes hold of it, and while held the mouse moving up and down turns it, rewriting it, with a
	/// squeak each time it starts to turn. True while a scroll has the press, which the camera then doesn't see. A
	/// press on a scroll gives where the camera is to look at it from in focus.
	bool Hold(bool pressed, float mouseY, const std::optional<TempleCursorHit>& hit, const Facts& facts,
	          std::optional<Focus>& focus);
	/// The camera looks at a room's scroll, by its place among the room's scrolls, as a press on the scroll does. None
	/// when the room has no such scroll.
	std::optional<Focus> LookAt(TempleRoom room, size_t place, const Facts& facts);
	/// How close the camera is to the scroll it looks at, from 0 to 1. Past half way the scroll's text is drawn in
	/// front of it each frame, and once back under half way it is written on its texture again. The save game room
	/// does not use this for its scroll.
	void SetFocus(float zoom, const Facts& facts);
	/// One frame of the scroll of the save game room, with the closeness of the camera from 0 to 1. The closeness of the
	/// camera to a picture also counts. Past half way, the text goes in front of the scroll from the next frame, and the
	/// room writes it again in each frame. The room changes its scroll only in the frames that it draws.
	void StepSaveGameRoom(bool roomDrawn, float closeness, const Facts& facts);
	/// Whether a scroll is being dragged
	[[nodiscard]] bool IsHeld() const { return _held.has_value(); }
	/// Whether a submesh of a room's mesh is one of its scrolls, a control the cursor makes glow
	[[nodiscard]] bool IsControl(TempleRoom room, uint32_t subMesh) const;
	/// The scrolls' textures for a room's mesh, by submesh
	[[nodiscard]] std::vector<TempleSubMeshTexture> GetTextures(TempleRoom room) const;
	/// A room's scroll, as its callbacks see it to choose the tooltip
	struct Control
	{
		uint32_t subMesh;
		/// Whether the camera was last sent to look at it
		bool focused;
	};
	/// The scrolls of a room's mesh, in the order its callbacks are run
	[[nodiscard]] std::vector<Control> GetControls(TempleRoom room) const;
	/// The text of the scroll the camera is close to, drawn in front of it in the temple
	void AppendFocusedText(std::vector<OrientedTextVertex>& vertices, const Facts& facts) const;
	/// The lines of the scroll the camera is close to that hold a gesture's or a control's picture, in the world
	void AppendFocusedPictures(std::vector<temple::scroll_pictures::WorldPicture>& pictures, const Facts& facts) const;

	using Content = temple_scrolls::Content;
	/// Writes the scrolls of a content again, as the save game room writes its scroll after a save, a load or a delete
	void Rewrite(Content content, const Facts& facts);
	/// The text for a scroll, or none when the room leaves the scroll unwritten. The story history starts at the line
	/// historyStart lines back from the newest line.
	[[nodiscard]] static std::optional<std::u16string> Write(Content content, const gui::TextDatabase& texts,
	                                                         const gui::GameFont& font, const Facts& facts,
	                                                         int32_t historyStart = 0);
	/// The story history scroll shows five lines from its first line
	static constexpr int32_t k_HistoryLinesShown = 5;

	/// The story history shows its newest line first again, as the player comes into the library
	void ShowLatestHistory() noexcept { _historyStart = 0; }
	/// A button of the story history turns its first line, kept between the newest and the oldest line
	void TurnHistory(help::StoryTurn turn, const Facts& facts);
	/// The line of the story history that the scroll starts at, 0 for the newest line
	[[nodiscard]] int32_t GetHistoryStart() const noexcept { return _historyStart; }
	/// Whether the camera is close to the story history scroll, which then shows its buttons
	[[nodiscard]] bool IsHistoryOpen() const { return IsWrittenInFront(Content::LibraryHistory); }
	/// Whether the scroll of some content is the one the camera is close to, its text written in front of it each frame
	[[nodiscard]] bool IsWrittenInFront(Content content) const;
	/// What the scroll the camera was last sent to look at has written on it, none before one
	[[nodiscard]] std::optional<Content> GetFocusedContent() const;

private:
	struct Scroll
	{
		TempleRoom room;
		Content content;
		std::optional<uint32_t> subMesh;
		std::optional<graphics::TextureHandle> texture;
		/// What the texture shows now
		std::optional<Drawn> drawn;
		int32_t position {0};
		uint32_t textHeight {0};
		/// The scroll's frame and box, from its mesh
		glm::mat4 frame {1.0f};
		glm::vec3 min {0.0f};
		glm::vec3 max {0.0f};
	};
	/// Where the text of a scroll goes in the world while the camera is close to it: the frame at the middle of the
	/// scroll, and the world's length of one texel across and down the texture
	struct Layout
	{
		TextFrame frame;
		float across;
		float down;
	};
	/// The layout of a scroll, none when its box has no height
	[[nodiscard]] static std::optional<Layout> FocusedLayout(const Scroll& scroll);
	/// The camera looks at a scroll, by its place among all the scrolls
	Focus LookAt(size_t index, const Facts& facts);
	/// Writes the scroll's texture, its text left off while the camera is close to it
	void Redraw(Scroll& scroll, const Facts& facts);
	/// Whether the camera looks at the scroll and is close enough to put its text in front of it
	[[nodiscard]] bool IsFocused(const Scroll& scroll) const;
	/// Whether the texture of the scroll has no text, because the text is in front of the scroll
	[[nodiscard]] bool IsTextInFront(const Scroll& scroll) const;
	/// Whether the text of the scroll is drawn in front of it in this frame
	[[nodiscard]] bool IsDrawnInFront(const Scroll& scroll) const;
	/// Adds the text of a scroll in front of it
	void AppendTextOf(const Scroll& scroll, std::vector<OrientedTextVertex>& vertices, const Facts& facts) const;
	/// Adds the pictures of the lines of a scroll in front of it
	void AppendPicturesOf(const Scroll& scroll, std::vector<temple::scroll_pictures::WorldPicture>& pictures,
	                      const Facts& facts) const;

	const gui::TextDatabase& _texts;
	const gui::GameFont& _font;
	/// ChallengeScroll.raw, 256 by 256 RGB, which the rooms of pictures read for every scroll
	std::vector<uint8_t> _parchment;
	std::vector<Scroll> _scrolls;
	/// At the lowest detail levels, each scroll's texture has half the texels along each side. The text is written at
	/// the full size and then made smaller.
	bool _halfSize {false};
	/// The scroll being dragged and where the mouse was
	std::optional<size_t> _held;
	float _heldMouseY {0.0f};
	bool _wasPressed {false};
	/// Whether the scroll has squeaked since it started turning
	bool _squeaked {false};
	/// The scroll the camera looks at, and whether it is close enough for its text to be drawn in front of it
	std::optional<size_t> _focused;
	bool _focusedText {false};
	/// Whether the text of the save game room's scroll is in front of it, and whether the room draws it there in this
	/// frame. The room uses the closeness of the camera, also to a picture, and not the scroll that the camera looks at.
	bool _saveGameInFront {false};
	bool _saveGameDrawnInFront {false};
	/// The line of the story history that its scroll starts at
	int32_t _historyStart {0};
};

} // namespace openblack
