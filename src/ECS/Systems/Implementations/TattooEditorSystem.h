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
#include <utility>
#include <vector>

#include "ECS/Systems/TattooEditorSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class TattooEditorSystem final: public TattooEditorSystemInterface
{
public:
	/// Where the creature's tattoos are read from and written to, and the skins that show them
	class Skin
	{
	public:
		virtual ~Skin() = default;
		[[nodiscard]] virtual std::optional<creature_tattoo::Slots> Read(entt::entity creature) const = 0;
		/// The creature's slots change, but its skin does not
		virtual void Write(entt::entity creature, const creature_tattoo::Slots& slots) = 0;
		/// The creature keeps the block of pictures that its tattoos use, in place of the block that it had
		virtual void WritePictures(entt::entity creature, std::vector<uint8_t> pictures) = 0;
		/// The tattoos that a skin shows in place of its slots, or none to show its slots again
		virtual void Show(entt::entity shown, std::optional<creature_tattoo::Slots> slots) = 0;
		/// The masks of the creature's player are made again from the creature's tattoos
		virtual void RemakeMasks(entt::entity creature, creature_tattoo::Sheet sheet) = 0;
		/// The skin is painted again
		virtual void Paint(entt::entity creature) = 0;
		/// The copy of the creature that the Creature Cave shows, with a skin of its own
		[[nodiscard]] virtual std::optional<entt::entity> CaveCopy(entt::entity creature) const = 0;
	};

	/// The creatures' skins in the registry
	TattooEditorSystem();
	explicit TattooEditorSystem(std::unique_ptr<Skin> skin);

	void Open(entt::entity creature, const creature_tattoo_editor::Orbit& view) override;
	void OpenFromMenu(std::optional<entt::entity> playerCreature, entt::entity shown,
	                  const creature_tattoo_editor::Orbit& view) override;
	[[nodiscard]] bool IsFromMenu() const override { return _shown.has_value(); }
	[[nodiscard]] std::optional<entt::entity> GetShown() const override { return _shown; }
	void Close() override;
	[[nodiscard]] bool IsOpen() const override { return _open; }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override { return _creature; }
	[[nodiscard]] const creature_tattoo_editor::Orbit& GetView() const override { return _view; }
	[[nodiscard]] const creature_tattoo_editor::Session& GetSession() const override { return _session; }
	void Update(float milliseconds) override;
	void Steer(glm::ivec2 grab, glm::ivec2 pointer, float milliseconds) override;
	void Hover(std::span<const creature_tattoo_editor::SiteOnScreen> sites, glm::ivec2 pointer) override;
	[[nodiscard]] std::optional<uint8_t> GetHoveredSite() const override { return _hovered; }
	[[nodiscard]] std::optional<glm::ivec2> GetHoveredPoint() const override { return _hoveredPoint; }
	bool Drop(uint8_t design, const glm::u8vec3& colour) override;
	std::optional<creature_tattoo::Slot> Lift() override;
	[[nodiscard]] std::optional<creature_face::Face> TakeFace() override { return std::exchange(_face, std::nullopt); }
	creature_tattoo_editor::Accept Ok(bool networkGame) override;
	void Answered() override { _session.changed = false; }
	void Cancel() override;
	void SetPictures(const creature_tattoo::Pictures& pictures) override { _pictures = pictures; }
	[[nodiscard]] const creature_tattoo::Pictures& GetPictures() const override { return _pictures; }

private:
	/// The player's creature gets the tattoos as they are now, and the masks are made again. The creature that the
	/// editor shows is painted again.
	void Remake();
	/// The creature that the editor shows: the copy in the Creature Cave, or the creature of the main menu
	[[nodiscard]] std::optional<entt::entity> Shown() const;

	std::unique_ptr<Skin> _skin;
	bool _open {false};
	std::optional<entt::entity> _creature;
	/// The creature of the main menu
	std::optional<entt::entity> _shown;
	/// The copy of the creature in the Creature Cave
	std::optional<entt::entity> _caveCopy;
	creature_tattoo_editor::Session _session;
	creature_tattoo_editor::Orbit _view;
	std::optional<uint8_t> _hovered;
	std::optional<glm::ivec2> _hoveredPoint;
	/// The face to pull after the last tattoo went on or came off
	std::optional<creature_face::Face> _face;
	creature_tattoo::Pictures _pictures {};
};

} // namespace openblack::ecs::systems
