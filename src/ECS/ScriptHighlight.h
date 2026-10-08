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

#include "3D/Billboard.h"
#include "Enums.h"

/// The script highlights (state in ECS/Components/ScriptHighlight.h): the challenge scrolls and the did-you-know
/// signs. A script makes one with CREATE_HIGHLIGHT 272, names it with HIGHLIGHT_PROPERTIES 334, lifts it with
/// SET_PROPERTY YPOS, lights it with SET_ACTIVE 255 and waits for GAME_THING_CLICKED 016 on it; SET_DRAW_HIGHLIGHT 306
/// hides the scrolls. The finders see it as SCRIPT_OBJECT_TYPE 37 with its info row as the sub-type, so FollowUs'
/// CALL_NEAR(HIGHLIGHT, 1, ...) at CHL L53068 / L53085 finds DidYouKnow's sign. See docs/bw1-notes/intro.md.
namespace openblack::ecs::script_highlight
{

/// The four script highlight info rows (info.dat order).
/// The CHL HIGHLIGHT_INFO enum of the later games (bronze, silver, gold, scoreboard...) does not match them
enum class Info : uint32_t
{
	Bronze = 0,     ///< "Script Hightlight": MSH_I_MINISCROLL / _ACTIVE, no particles
	DidYouKnow = 1, ///< "Script Did You Know Sign": MSH_O_INFO_SIGN (both), no particles
	Silver = 2,     ///< "Script Hightlight Silver inactive": MINISCROLL_SILVER / _ACTIVE, glints 100, active 103
	Gold = 3,       ///< "Script Hightlight gold inactive": MINISCROLL_GOLD / _ACTIVE, glints 99, active 102
};
constexpr uint32_t k_InfoCount = 4;
/// The save type
constexpr uint32_t k_SaveType = 0x3F;
/// The glow object's mesh (MSH_S_BLAST_CENTRE)
constexpr uint32_t k_GlowMesh = 0x20F;
/// The sprite's texture (".\data\textures\S_SpriteSheet3.raw", render mode 13), cell 0 (one sprite)
constexpr const char* k_SpriteTexture = "S_SpriteSheet3";
/// The sprite's render mode
constexpr uint32_t k_SpriteRenderMode = 13;

// ---- Pure rules (test_script_highlight) -----------------------------------------------------------------------------

/// The row index == 1
[[nodiscard]] constexpr bool IsDidYouKnowInfo(uint32_t index)
{
	return index == static_cast<uint32_t>(Info::DidYouKnow);
}
/// The sprite's alpha by row: 1 -> 0x32, 2 -> 0x96, 3 -> 0x64, else 0
[[nodiscard]] uint8_t SpriteAlpha(uint32_t index);
/// d = the truncated distance from the camera to the point, bound to [10, 30] (unsigned compares); a scroll's scale
/// is scale x d x (1 / 30), a did-you-know keeps its scale
[[nodiscard]] float DistanceScale(float scale, float distanceToCamera, bool didYouKnow);
/// For a turning highlight: the 3D object's Y angle + the frame's game ms x 0.00314159, then minus
/// trunc(a x (1 / 2 pi)) x 2 pi
[[nodiscard]] float SpinAngle(float angle, uint32_t frameMs);
/// The challenge ids whose scroll saves the game when it is clicked (GAME_THING_CLICKED): 0x38, 0x3B, 0x3C, 0x3D
[[nodiscard]] bool SavesGameWhenClicked(uint32_t scriptId);
/// A did-you-know with a script id; a scroll only while active and with a script id
[[nodiscard]] bool ValidToTap(bool didYouKnow, bool active, uint32_t scriptId);
/// The tap tooltip override: 0xEF2 when the info's OBJECT_TYPE is 1, else 0 (every row is 35: 0)
[[nodiscard]] uint32_t OverwriteTapToolTip(ObjectType infoType);

/// The shared pulse of ProcessHighlights (phase, value, last value; all 0 from OnClearMap)
struct Pulse
{
	float phase {0.0f};
	float value {0.0f};
	float previous {0.0f};
};
/// phase += msPerTurn x 5 x 0.001, less 2 pi once when above; previous = value; value = (1 - cos phase) x 0.5
void StepPulse(Pulse& pulse, uint32_t msPerTurn);
/// lerp(previous, value, the turn fraction), bound to [0, 1], x 0.6 + 0.4
[[nodiscard]] float ActivePulse(const Pulse& pulse, float turnFraction);
/// The glow's ARGB, 0x14B4DCFF for Silver (row 2), else ((byte)trunc(ActivePulse) x 0x50) << 24
/// | 0xFFFF00 (literal: the pulse is 0.4..1, so the alpha is 0x50 only at 1.0, 0 otherwise)
[[nodiscard]] uint32_t ActiveGlowArgb(uint32_t index, float activePulse);

/// The did-you-know texts already read (the help bubble asks and adds): one list of at most 48 per DYK_CATEGORY 0..4.
/// (pending) where the original saves them (the help profile)
namespace did_you_know_read
{
[[nodiscard]] bool IsRead(uint32_t text, DykCategory category);
/// (inferred) an append when the list has room
void MarkRead(uint32_t text, DykCategory category);
/// The sum of the five counts (0 -> the help bubble starts "FirstDYKExplained")
[[nodiscard]] uint32_t Total();
void Clear();
} // namespace did_you_know_read

// ---- The highlights ------------------------------------------------------------------------------------------------

/// A new highlight at the head of the list, with its script id, then its mesh, map cell and effects. entt::null for
/// an info index past the 4 rows (openblack: the original reads past the array)
entt::entity Create(const glm::vec3& position, uint32_t infoIndex, uint32_t scriptId, float yAngle, float scale);

[[nodiscard]] bool IsHighlight(entt::entity thing);
/// The info row (the script sub-type); script_type::k_NoSubtype for a thing that is not a highlight
[[nodiscard]] uint32_t InfoIndexOf(entt::entity thing);
[[nodiscard]] bool IsDidYouKnow(entt::entity thing);
[[nodiscard]] bool IsActive(entt::entity thing);
/// The script id (0 for a thing that is not a highlight)
[[nodiscard]] uint32_t ScriptIdOf(entt::entity thing);
/// The script id and the did-you-know category
void SetScriptId(entt::entity thing, uint32_t scriptId, DykCategory category);
/// SET_PROPERTY YPOS on a highlight: the draw height h is set, then the altitude = h and the 3D object moved there
void SetYPos(entt::entity thing, float height);
/// GET_PROPERTY YPOS: the altitude
[[nodiscard]] float GetYPos(entt::entity thing);
/// Lights or dims it: its mesh and its active effect (a scroll's sound is pending)
void SetActivated(entt::entity thing, bool on);

/// Each game turn (after the bookmarks and before the climate): the pulse, then each available highlight of the
/// list, from its head
void ProcessHighlights();
/// The per-frame half of the draw that moves the 3D object (its angle, scale and position) and the effects
void UpdateFrame(uint32_t frameMs, const glm::vec3& eye);

/// What Draw adds around the mesh this frame, for the renderer and the interface (pending: nothing draws these yet)
struct DrawExtras
{
	bool drawn {false};                                ///< false: Draw returned before the mesh is drawn
	glm::vec3 centre {0.0f};                           ///< the mesh's centre in the world
	float radius {0.0f};                               ///< |the 3D object's matrix x the mesh's half extents|
	std::optional<graphics::billboard::Sprite> sprite; ///< Screen mode
	std::optional<glm::mat4> glow;                     ///< Mesh k_GlowMesh, an active scroll only
	uint32_t glowArgb {0};                             ///< its colour (ActiveGlowArgb)
	std::optional<float> clickRadius;                  ///< The invisible click sphere at the centre, r + 0.5
};
[[nodiscard]] DrawExtras ExtrasOf(entt::entity thing, const glm::vec3& eye);

/// A hand tap on it (through the tap packet); `byLocalPlayer`: the tapping interface is the local one and its player
/// the local player. Always true
bool InterfaceTap(entt::entity thing, bool byLocalPlayer);
/// ValidToTap of a thing's did-you-know info, active flag and script id; false for a thing that is not a highlight
[[nodiscard]] bool InterfaceValidToTap(entt::entity thing);

/// When it is deleted: out of the list, the two effects closed (and its render particle)
void OnToBeDeleted(entt::entity thing);
/// When the map is cleared (after the bookmarks): the pulse back to 0; openblack also empties its list
void OnClearMap();
/// The highlights' list (head first)
[[nodiscard]] const std::vector<entt::entity>& All();

} // namespace openblack::ecs::script_highlight
