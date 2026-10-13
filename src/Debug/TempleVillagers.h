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
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"
#include "TempleVillagersSurvey.h"
#include "Window.h"

namespace openblack::debug::gui
{

/// The name of the temple villagers' window, which its testbed scenario opens
constexpr std::string_view k_TempleVillagersWindow = "Temple Villagers";

/// The villagers around the player's temple, for testing them: its worship sites and the places at them, the towns that
/// worship there with their share of people at worship, every villager worshipping, on its way or near with what it is
/// doing and where it is going, and those walking about inside the temple. Villagers can be made for a town and sent
/// to worship as the game sends them, the game paused or stepped a turn at a time, any of them picked out in the world
/// and followed by the camera. It is a front end only: everything it does goes through the game's own rules.
class TempleVillagers final: public Window
{
public:
	TempleVillagers() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override {}
	void UpdateAlways() noexcept override;
	void ProcessEventOpen(const SDL_Event&) noexcept override {}
	void ProcessEventAlways(const SDL_Event&) noexcept override {}

private:
	void DrawTemple(const temple_villagers::Survey& survey) noexcept;
	void DrawRun() noexcept;
	void DrawSites(const temple_villagers::Survey& survey) noexcept;
	void DrawTowns(const temple_villagers::Survey& survey) noexcept;
	void DrawSpawn(const temple_villagers::Survey& survey) noexcept;
	void DrawVillagers(const temple_villagers::Survey& survey) noexcept;
	void DrawSelected(const temple_villagers::Survey& survey) noexcept;
	void DrawInside(const temple_villagers::Survey& survey) noexcept;
	/// Marks the villagers, their walks and the sites' places on the screen, over the world
	void DrawMarks(const temple_villagers::Survey& survey) const noexcept;

	void Spawn(const temple_villagers::Town& town) noexcept;
	void ClearSpawned() noexcept;
	/// Puts the camera looking at the point, as it looks now, from no further than the distance
	static void LookAt(const glm::vec3& point, float distance) noexcept;

	/// Villagers further than this from the temple's middle, in metres, are listed only when going to or at worship
	float _radius {60.0f};
	bool _showWorshipping {true};
	bool _showOnTheWay {true};
	bool _showNearby {true};
	bool _marks {true};
	bool _sitePlaces {true};
	bool _walks {true};
	bool _floors {false};
	bool _follow {false};
	std::optional<entt::entity> _selected;

	/// What the next villagers made are like, and how many
	Tribe _tribe {Tribe::CELTIC};
	VillagerNumber _number {VillagerNumber::Housewife};
	int _age {30};
	int _count {4};
	bool _sendOnSpawn {true};
	size_t _town {0};
	/// The villagers this window made, which it can take away again
	std::vector<entt::entity> _spawned;
	/// Made last frame, to be sent to worship once their first state is set
	std::vector<entt::entity> _toSend;

	/// The turn the game is paused again at, when stepping
	std::optional<uint32_t> _stepUntil;
};

} // namespace openblack::debug::gui
