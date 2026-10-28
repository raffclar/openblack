/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TattooEditorSystem.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "Commands/PlayerCommands.h"
#include "3D/TempleInteriorInterface.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/CreatureTattooChange.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The skins of the creatures in the registry, painted by the skin system. In a game alone, the changes of the editor go
/// to the creature as commands of the player, which the turn puts on the creature. Until then the skin shows them. In a
/// network game, the changes stay on this computer (see creature_tattoo_change::Apply).
class RegistrySkin final: public TattooEditorSystem::Skin
{
public:
	[[nodiscard]] std::optional<creature_tattoo::Slots> Read(entt::entity creature) const override
	{
		if (!Locator::entitiesRegistry::has_value())
		{
			return std::nullopt;
		}
		const auto* tattoos = Locator::entitiesRegistry::value().TryGet<const ecs::components::CreatureTattoos>(creature);
		if (tattoos == nullptr)
		{
			return std::nullopt;
		}
		// The editor starts from the tattoos that this computer shows: a change that waits for its turn, or a change that
		// stays on this computer
		return ecs::creature_tattoo_change::ShownSlots(*tattoos);
	}

	void Write(entt::entity creature, const creature_tattoo::Slots& slots) override
	{
		player_commands::CreatureTattoo command {.creature = creature, .slots = slots};
		if (_pictures.has_value() && _pictures->first == creature)
		{
			command.hasPictures = true;
			command.pictures = std::move(_pictures->second);
		}
		_pictures.reset();
		if (!Locator::entitiesRegistry::has_value())
		{
			return;
		}
		auto* tattoos = Locator::entitiesRegistry::value().TryGet<ecs::components::CreatureTattoos>(creature);
		if (tattoos == nullptr)
		{
			return;
		}
		ecs::creature_tattoo_change::Submit submit;
		if (Locator::playerCommandSystem::has_value())
		{
			submit = [](player_commands::CreatureTattoo change) {
				Locator::playerCommandSystem::value().Submit(std::move(change));
			};
		}
		const bool networkGame = Locator::time::has_value() && Locator::time::value().IsNetworkGame();
		ecs::creature_tattoo_change::Apply(*tattoos, std::move(command), networkGame, submit);
	}

	void WritePictures(entt::entity creature, std::vector<uint8_t> pictures) override
	{
		// The block goes with the next change of the slots. The masks that the game makes before then use it. In a network
		// game, the block stays on this computer.
		if (Locator::entitiesRegistry::has_value() && Locator::playerCommandSystem::has_value())
		{
			if (auto* tattoos = Locator::entitiesRegistry::value().TryGet<ecs::components::CreatureTattoos>(creature))
			{
				tattoos->picturesShown = true;
				tattoos->shownPictures = pictures;
			}
		}
		_pictures.emplace(creature, std::move(pictures));
	}

	void Show(entt::entity shown, std::optional<creature_tattoo::Slots> slots) override
	{
		if (Locator::creatureSkinSystem::has_value())
		{
			Locator::creatureSkinSystem::value().ShowTattoos(shown, slots);
		}
	}

	void RemakeMasks(entt::entity creature, creature_tattoo::Sheet sheet) override
	{
		if (Locator::creatureSkinSystem::has_value())
		{
			Locator::creatureSkinSystem::value().RemakeTattooMasks(creature, sheet);
		}
	}

	void Paint(entt::entity creature) override
	{
		if (Locator::creatureSkinSystem::has_value())
		{
			Locator::creatureSkinSystem::value().PaintTattoos(creature);
		}
	}

	[[nodiscard]] std::optional<entt::entity> CaveCopy(entt::entity creature) const override
	{
		if (!Locator::creatureCaveSystem::has_value() || !Locator::entitiesRegistry::has_value())
		{
			return std::nullopt;
		}
		const auto copy = Locator::creatureCaveSystem::value().GetCaveCreature();
		if (!copy.has_value())
		{
			return std::nullopt;
		}
		const auto* copies = Copies();
		const auto* of =
		    copies != nullptr && copies->Valid(*copy) ? copies->TryGet<const ecs::components::CreatureCopy>(*copy) : nullptr;
		return of != nullptr && of->creature == creature ? copy : std::nullopt;
	}

	void ShowCopy(entt::entity copy, std::optional<creature_tattoo::Slots> slots) override
	{
		if (auto* tattoos = CopyTattoos(copy); tattoos != nullptr)
		{
			tattoos->shown = slots;
		}
	}

	void PaintCopy(entt::entity copy) override
	{
		if (auto* tattoos = CopyTattoos(copy); tattoos != nullptr)
		{
			++tattoos->revision;
		}
	}

private:
	/// The block of pictures that waits for the next change of the slots of the creature
	std::optional<std::pair<entt::entity, std::vector<uint8_t>>> _pictures;

	/// The registry of the copy in the cave: the temple's rooms
	[[nodiscard]] static ecs::Registry* Copies()
	{
		return Locator::temple::has_value() ? &Locator::temple::value().GetRooms() : nullptr;
	}

	[[nodiscard]] static ecs::components::CreatureTattoos* CopyTattoos(entt::entity copy)
	{
		auto* copies = Copies();
		return copies != nullptr && copies->Valid(copy) ? copies->TryGet<ecs::components::CreatureTattoos>(copy) : nullptr;
	}
};
} // namespace

TattooEditorSystem::TattooEditorSystem()
    : TattooEditorSystem(std::make_unique<RegistrySkin>())
{
}

TattooEditorSystem::TattooEditorSystem(std::unique_ptr<Skin> skin)
    : _skin(std::move(skin))
{
}

void TattooEditorSystem::Open(entt::entity creature, const creature_tattoo_editor::Orbit& view)
{
	_open = true;
	_creature = creature;
	_shown.reset();
	_caveCopy = _skin->CaveCopy(creature);
	_session = creature_tattoo_editor::Open(_skin->Read(creature).value_or(creature_tattoo::Slots {}));
	_view = view;
	_hovered.reset();
	_hoveredPoint.reset();
	_face.reset();
	// The copy in the cave shows the tattoos being edited. The masks are made again with the player's own pictures.
	if (_caveCopy.has_value())
	{
		_skin->ShowCopy(*_caveCopy, _session.slots);
	}
	_skin->RemakeMasks(creature, creature_tattoo::Sheet::OwnPictures);
	PaintShown();
}

void TattooEditorSystem::OpenFromMenu(std::optional<entt::entity> playerCreature, entt::entity shown,
                                      const creature_tattoo_editor::Orbit& view)
{
	_open = true;
	_creature = playerCreature;
	_shown = shown;
	_caveCopy.reset();
	const auto slots = playerCreature.has_value() ? _skin->Read(*playerCreature) : std::nullopt;
	_session = creature_tattoo_editor::Open(slots.value_or(creature_tattoo::Slots {}));
	_view = view;
	_hovered.reset();
	_hoveredPoint.reset();
	_face.reset();
	// The creature that the menu shows wears the tattoos being edited from the start. Without a creature in the game,
	// the masks stay as they were.
	_skin->Show(shown, _session.slots);
	if (playerCreature.has_value())
	{
		_skin->RemakeMasks(*playerCreature, creature_tattoo::Sheet::OwnPictures);
	}
	_skin->Paint(shown);
}

void TattooEditorSystem::Close()
{
	// The copy in the cave shows the creature's own tattoos again, painted with the masks as they are
	if (_open && _caveCopy.has_value())
	{
		_skin->ShowCopy(*_caveCopy, std::nullopt);
		_skin->PaintCopy(*_caveCopy);
	}
	_open = false;
	_creature.reset();
	_shown.reset();
	_caveCopy.reset();
	_hovered.reset();
	_hoveredPoint.reset();
	_face.reset();
}

void TattooEditorSystem::Update(float milliseconds)
{
	if (IsOpen())
	{
		creature_tattoo_editor::Turn(_view, milliseconds);
	}
}

void TattooEditorSystem::Steer(glm::ivec2 grab, glm::ivec2 pointer, float milliseconds)
{
	creature_tattoo_editor::Steer(_view, grab, pointer, milliseconds);
}

void TattooEditorSystem::Hover(std::span<const creature_tattoo_editor::SiteOnScreen> sites, glm::ivec2 pointer)
{
	_hovered = IsOpen() ? creature_tattoo_editor::SiteUnderPointer(sites, pointer) : std::nullopt;
	_hoveredPoint.reset();
	if (_hovered.has_value())
	{
		const auto site = std::ranges::find(sites, *_hovered, &creature_tattoo_editor::SiteOnScreen::site);
		_hoveredPoint = site->screen;
	}
}

bool TattooEditorSystem::Drop(uint8_t design, const glm::u8vec3& colour)
{
	if (!IsOpen())
	{
		return false;
	}
	const auto putOn = creature_tattoo_editor::Drop(_session, design, colour, _hovered);
	// Each end of a drag makes the creature's block of pictures again from its tattoos
	if (_creature.has_value())
	{
		_skin->WritePictures(*_creature, creature_tattoo::PicturesBlock(_session.slots, _pictures));
	}
	Remake();
	// The player's creature in the world is painted again too
	if (_creature.has_value() && !ShowsCreature())
	{
		_skin->Paint(*_creature);
	}
	if (putOn)
	{
		_face = creature_tattoo_editor::k_PutOnFace;
	}
	return putOn;
}

std::optional<creature_tattoo::Slot> TattooEditorSystem::Lift()
{
	if (!IsOpen() || !_hovered.has_value())
	{
		return std::nullopt;
	}
	auto lifted = creature_tattoo_editor::Lift(_session, *_hovered);
	// A press on a place without a tattoo also changes the tattoos again and makes the creature sad. This occurs again
	// each frame while the button stays down.
	_session.changed = true;
	Remake();
	_face = creature_tattoo_editor::k_LiftFace;
	return lifted;
}

creature_tattoo_editor::Accept TattooEditorSystem::Ok(bool networkGame)
{
	const auto accept = creature_tattoo_editor::Ok(_session, networkGame);
	if (accept == creature_tattoo_editor::Accept::Close)
	{
		Close();
	}
	return accept;
}

void TattooEditorSystem::Cancel()
{
	if (IsOpen() && _creature.has_value())
	{
		// The creature gets back its tattoos, but no skin is painted and the masks stay as they are. The game does not
		// make the block of pictures again here (vanilla-bugs 379), and openblack fixes this.
		_skin->WritePictures(*_creature, creature_tattoo::PicturesBlock(_session.opened, _pictures));
		_skin->Write(*_creature, _session.opened);
	}
	Close();
}

void TattooEditorSystem::Remake()
{
	if (_creature.has_value())
	{
		_skin->Write(*_creature, _session.slots);
		_skin->RemakeMasks(*_creature, creature_tattoo::Sheet::OwnPictures);
	}
	// Without a copy in the cave or a creature of the menu, the player's creature shows its own tattoos
	if (_shown.has_value())
	{
		_skin->Show(*_shown, _session.slots);
	}
	else if (_caveCopy.has_value())
	{
		_skin->ShowCopy(*_caveCopy, _session.slots);
	}
	PaintShown();
}

void TattooEditorSystem::PaintShown()
{
	if (_shown.has_value())
	{
		_skin->Paint(*_shown);
	}
	else if (_caveCopy.has_value())
	{
		_skin->PaintCopy(*_caveCopy);
	}
	else if (_creature.has_value())
	{
		_skin->Paint(*_creature);
	}
}

bool TattooEditorSystem::ShowsCreature() const
{
	if (_shown.has_value())
	{
		return _shown == _creature;
	}
	return !_caveCopy.has_value();
}
