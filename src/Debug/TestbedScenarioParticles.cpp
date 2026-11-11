/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios for the particle effects.

#include "3D/TempleInteriorInterface.h"
#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddParticles(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "particles.smoke_and_fire",
	    .name = "Smoke, steam and a bonfire",
	    .facet = Facet::Particles,
	    .description = "The spot visuals' smoke, steam, evil smoke and a bonfire, side by side on the plain.",
	    .expected = "Grey smoke and white steam puffs rise, spin and fade; the evil smoke is dark; the bonfire's flames "
	                "flicker in its additive sprites with smoke rising above them.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 0.0f}, {40.0f, 0.0f}}, .distance = 0.3f},
	    .particles = {{.type = ParticleType::Smoke, .offset = {-30.0f, 0.0f}, .magnitude = 8.0f},
	                  {.type = ParticleType::Steam, .offset = {-10.0f, 0.0f}, .magnitude = 8.0f},
	                  {.type = ParticleType::EvilSmoke, .offset = {10.0f, 0.0f}, .magnitude = 8.0f},
	                  {.type = ParticleType::Bonfire, .offset = {30.0f, 0.0f}, .magnitude = 5.0f}},
	});

	all.push_back({
	    .id = "particles.sparkles",
	    .name = "Sparkles and magic",
	    .facet = Facet::Particles,
	    .description = "Short effects started again every few seconds: the failed cast, the hand gripping the land, a "
	                   "magic object made, an object appearing in the second player's colour, and a forest made.",
	    .expected = "Each plays out in sprites from the sprite sheets and starts again: the cross of the failed cast, "
	                "a ring of sparks, a spinning cloud of sparks, sparks fountaining up in green, and falling leaves.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {30.0f, 0.0f}}, .distance = 0.45f},
	    .particles =
	        {{.type = ParticleType::SpellFail, .offset = {-30.0f, 0.0f}, .height = 5.0f, .restartSeconds = 4.0f},
	         {.type = ParticleType::GripLandscape, .offset = {-15.0f, 0.0f}, .restartSeconds = 3.0f},
	         {.type = ParticleType::MagicObjectCreated,
	          .offset = {0.0f, 0.0f},
	          .height = 3.0f,
	          .magnitude = 3.0f,
	          .restartSeconds = 5.0f},
	         {.type = ParticleType::ObjectAppear,
	          .offset = {15.0f, 0.0f},
	          .magnitude = 3.0f,
	          .player = 1,
	          .restartSeconds = 4.0f},
	         {.type = ParticleType::ForestCreated, .offset = {30.0f, 0.0f}, .magnitude = 1.0f, .restartSeconds = 6.0f}},
	});

	all.push_back({
	    .id = "particles.models",
	    .name = "Models among the sparks",
	    .facet = Facet::Particles,
	    .description = "Effects whose particles are models: the vortex rising where a magic object is made, the glowing "
	                   "cone of the beam marking a place, the paper streamers of the ticker tape and the fireball's flash.",
	    .expected = "A spinning additive vortex model rises and fades; a glowing cone turns to face the camera wherever it "
	                "looks from; coloured streamers fall from a disk; the flash's model plays through its texture's frames.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {30.0f, 0.0f}}, .distance = 0.4f},
	    .particles = {{.file = "SF_MagicObjectCreated2", .offset = {-30.0f, 0.0f}, .magnitude = 2.0f, .restartSeconds = 5.0f},
	                  {.file = "SF_SeeThisBeam", .offset = {-10.0f, 0.0f}, .magnitude = 1.0f, .restartSeconds = 6.0f},
	                  {.file = "SF_TickerTape", .offset = {10.0f, 0.0f}, .height = 15.0f, .restartSeconds = 6.0f},
	                  {.file = "SF_Flash", .offset = {30.0f, 0.0f}, .height = 3.0f, .magnitude = 2.0f, .restartSeconds = 3.0f}},
	});

	// The beam that marks a place for the player, as the first land marks where its vortex opens
	all.push_back({
	    .id = "particles.see_this_beam",
	    .name = "The beam that marks a place",
	    .facet = Facet::Particles,
	    .description = "The yellow beam that a script puts on a place to show it to the player, as the first land shows "
	                   "where its vortex will open. It starts again every ten seconds.",
	    .expected = "A yellow cone of light stands on the land and turns to face the camera. Its brightness beats with "
	                "the scrolls' highlights, about once every 1.3 seconds, from a dim glow to full and back.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-6.0f, -6.0f}, {6.0f, 6.0f}}, .distance = 0.5f},
	    .particles = {{.file = "SF_SeeThisBeam", .offset = {0.0f, 0.0f}, .magnitude = 1.0f, .restartSeconds = 10.0f}},
	});

	all.push_back({
	    .id = "particles.tumble",
	    .name = "Tumbling streamers and wood",
	    .facet = Facet::Particles,
	    .description = "Effects whose particles tumble as they move: the paper streamers of the ticker tape and the planks of "
	                   "wood that a store gives out. Each turns about its forward axis when it moves more east to west, else "
	                   "about its east to west axis.",
	    .expected = "Streamers fall from a disk and planks fly out, each turning as it goes, faster the faster it moves.",
	    .framing = {.shot = Shot::Overview, .include = {{-12.0f, 0.0f}, {12.0f, 0.0f}}, .distance = 0.25f},
	    .particles = {{.file = "SF_TickerTape", .offset = {-8.0f, 0.0f}, .height = 15.0f, .restartSeconds = 6.0f},
	                  {.file = "SF_Wood", .offset = {8.0f, 0.0f}, .height = 2.0f, .magnitude = 1.0f, .restartSeconds = 4.0f}},
	});

	all.push_back({
	    .id = "particles.light_maps",
	    .name = "Light on the land",
	    .facet = Facet::Particles,
	    .description = "Effects that light the ground under them, as fire and lightning do, at night so the light shows: "
	                   "the ground effect's ring and the light a landscape vortex leaves, each many cells across.",
	    .expected = "A ring of light spreads over the ground and fades, again and again; a steady pool of coloured light "
	                "lies on the ground beside it, lighting what stands there too.",
	    .environment = {.hour = 23.0f},
	    .framing = {.shot = Shot::Overview, .include = {{-120.0f, 0.0f}, {120.0f, 0.0f}}, .distance = 0.4f},
	    .particles = {{.file = "SF_GroundEffect", .offset = {-70.0f, 0.0f}, .magnitude = 2.0f, .restartSeconds = 4.0f},
	                  {.file = "SF_LandscapeVortexLightMap", .offset = {70.0f, 0.0f}}},
	});

	// The singing stones' heal as the funeral march's raising starts it: the script makes it 13.5 times its size, in the
	// colour of this computer's player
	all.push_back({
	    .id = "particles.singing_stones_heal_rings",
	    .name = "The rings of light of the singing stones' heal",
	    .facet = Facet::Particles,
	    .description = "The singing stones' heal effect at 13.5 times its size, in the first player's colour, at night. "
	                   "Its rule stands three rings of light on the land round the place where the effect starts, each "
	                   "about 15 m across from the centre, under the glow that the effect's light map puts on the land.",
	    .expected = "Three rings of light stand one on the other round the light on the land, 10 high, rising and "
	                "falling in waves that roll round them, their stars sliding up. They are a dim red: the player's red "
	                "times the alpha of 30 of the effect's points. They follow the land and stay where they were made.",
	    .environment = {.hour = 23.0f, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -20.0f}, {20.0f, 20.0f}}, .distance = 0.5f},
	    .particles = {{.file = "SF_SingingStonesHeal", .offset = {0.0f, 0.0f}, .magnitude = 13.5f, .player = 0}},
	});

	all.push_back({
	    .id = "particles.mist_shadows",
	    .name = "Shadows of a storm's mist",
	    .facet = Facet::Particles,
	    .description = "The lightning storm effect over the land at noon. Each puff of its cloud stamps a grey light map on "
	                   "the land that darkens the land's luminosity, as the sky's clouds do, for the frame after it is drawn.",
	    .expected = "Dark patches move on the ground under the storm's clouds and fade with them. The ground goes darker, "
	                "not a different colour, and the things that stand on it go darker with it.",
	    .environment = {.hour = 12.0f},
	    .framing = {.shot = Shot::Overview, .include = {{-120.0f, -120.0f}, {120.0f, 120.0f}}, .distance = 0.5f},
	    .particles = {{.file = "SF_LightningStorm", .offset = {0.0f, 0.0f}, .magnitude = 1.0f, .restartSeconds = 20.0f}},
	});

	all.push_back({
	    .id = "particles.vortex_and_flies_sounds",
	    .name = "The landscape vortex's and the flies' sounds",
	    .facet = Facet::Particles,
	    .description = "The landscape vortex as it opens and as it closes, and a swarm of flies, side by side and close up, "
	                   "every eight seconds. Their sounds are named in the effect files by sound actions the game's header "
	                   "numbers by counting on from the one before.",
	    .expected = "The vortices play the vortex's hum and the flies their buzz as they start; the debug log names an "
	                "emitter for each.",
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 30.0f}, {15.0f, 50.0f}}, .distance = 0.4f},
	    .particles = {{.file = "SF_LandscapeVortexInBefore", .offset = {-8.0f, 40.0f}, .restartSeconds = 8.0f},
	                  {.file = "SF_LandscapeVortexOutAfter", .offset = {8.0f, 40.0f}, .restartSeconds = 8.0f},
	                  {.file = "SF_Flies", .offset = {0.0f, 36.0f}, .height = 2.0f, .restartSeconds = 8.0f}},
	});

	all.push_back({
	    .id = "particles.flies_face_their_way",
	    .name = "Flies turned to the way they fly",
	    .facet = Facet::Particles,
	    .description = "A swarm of flies close up, every eight seconds. Each fly is a sprite that the flock rule turns on "
	                   "the screen.",
	    .expected = "Each fly's picture turns to point the way that the fly moves across the screen, as the game turns "
	                "it. A fly that flies to the right on the screen points to the right.",
	    .framing = {.shot = Shot::Overview, .include = {{-3.0f, 33.0f}, {3.0f, 39.0f}}, .distance = 0.15f},
	    .particles = {{.file = "SF_Flies", .offset = {0.0f, 36.0f}, .height = 2.0f, .restartSeconds = 8.0f}},
	});

	all.push_back({
	    .id = "particles.ribbon_slide",
	    .name = "A ribbon's corners and its texture sliding along it",
	    .facet = Facet::Particles,
	    .description = "The simple beam, three ribbons from a point ten metres up to a creature twenty metres away, "
	                   "started again every six seconds and seen close from the side.",
	    .expected = "Each beam is a ribbon that faces the camera from every side, its corners meeting half way where two "
	                "segments join. Its texture slides along it at twice the speed of its data, as the game adds the "
	                "slide to the texture twice. The slide moves on only by the frames drawn, and stops while the game "
	                "pauses.",
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -5.0f}, {25.0f, 5.0f}}, .distance = 0.35f},
	    .creatures = {{.offset = {20.0f, 0.0f}, .pauseMind = true}},
	    .particles = {{.file = "SF_SimpleBeam",
	                   .targetsCreatures = true,
	                   .offset = {-10.0f, 0.0f},
	                   .height = 10.0f,
	                   .magnitude = 1.0f,
	                   .restartSeconds = 6.0f}},
	});

	all.push_back({
	    .id = "particles.mist",
	    .name = "Mist over the holders",
	    .facet = Facet::Particles,
	    .description = "The water and storm miracles' mists, as they sit over a holder, and the water miracle in the hand.",
	    .expected = "Puffs of mist, shaped wider than tall, turn to face the camera, shrink seen edge on and grow seen from "
	                "above, and play through their frames of the smoke texture.",
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 0.0f}}, .distance = 0.3f},
	    .particles = {{.file = "SF_WaterOnHolder", .offset = {-15.0f, 0.0f}, .height = 4.0f, .magnitude = 3.0f},
	                  {.file = "SF_LightningStormOnHolder", .offset = {0.0f, 0.0f}, .height = 4.0f, .magnitude = 3.0f},
	                  {.file = "SF_WaterInHand", .offset = {15.0f, 0.0f}, .height = 4.0f, .magnitude = 3.0f}},
	});

	all.push_back({
	    .id = "particles.symbols_and_heal",
	    .name = "Player symbols and the heal chakra",
	    .facet = Facet::Particles,
	    .description = "The player icon fountain throwing up the casting player's symbol in each player's colour, and the "
	                   "heal miracle's chakra over two creatures.",
	    .expected = "Symbols between two glows of the player's colour, one turning, arc up out of each fountain and fade; "
	                "a chakra appears over each creature and follows it, a burst of sparks under it rising and fading.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {30.0f, 0.0f}}, .distance = 0.45f},
	    .creatures = {{.offset = {15.0f, 0.0f}}, {.offset = {30.0f, 5.0f}}},
	    .particles = {{.file = "SF_PlayerIconFountain", .offset = {-30.0f, 0.0f}, .magnitude = 2.0f, .player = 0},
	                  {.file = "SF_PlayerIconFountain", .offset = {-15.0f, 0.0f}, .magnitude = 2.0f, .player = 1},
	                  {.file = "SF_HealChakra",
	                   .targetsCreatures = true,
	                   .offset = {22.0f, 0.0f},
	                   .magnitude = 2.0f,
	                   .restartSeconds = 6.0f}},
	});

	all.push_back({
	    .id = "particles.profile_symbol",
	    .name = "The symbol of the player's profile in the world",
	    .facet = Facet::Particles,
	    .description = "The player icon fountain of the first player and of the second player, side by side. The world "
	                   "shows the tenth symbol for the first player, as when the player's profile has that symbol.",
	    .expected = "The fountain on the left throws up the tenth symbol of the players' symbols page in the first "
	                "player's colour. The fountain on the right throws up the second player's symbol as the file of the "
	                "players' symbols has it.",
	    .environment = {.profileSymbol = 9u},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 0.0f}, {15.0f, 0.0f}}, .distance = 0.35f},
	    .particles = {{.file = "SF_PlayerIconFountain", .offset = {-10.0f, 0.0f}, .magnitude = 2.0f, .player = 0},
	                  {.file = "SF_PlayerIconFountain", .offset = {10.0f, 0.0f}, .magnitude = 2.0f, .player = 1}},
	});

	all.push_back({
	    .id = "particles.gods_symbols",
	    .name = "The computer gods' pictures in the world",
	    .facet = Facet::Particles,
	    .description = "As on the second land, a land script switches on the computer minds of the second and third "
	                   "players. The sheet of the players' symbols is then made again, as at the start of a land. The "
	                   "player icon fountains of the first three players stand side by side.",
	    .expected = "The fountain on the left throws up the symbol of the player's profile in red. The fountain in the "
	                "middle throws up the picture of Khazar (a cross) for the second player, in yellow. The fountain "
	                "on the right throws up the picture of Lethys (a swirl) for the third player, in blue. The second "
	                "land gives these colours to these gods.",
	    .environment = {.symbolsAfterLandScript = true, .landNumber = 2},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 0.0f}}, .distance = 0.35f},
	    .particles = {{.file = "SF_PlayerIconFountain", .offset = {-15.0f, 0.0f}, .magnitude = 2.0f, .player = 0},
	                  {.file = "SF_PlayerIconFountain", .offset = {0.0f, 0.0f}, .magnitude = 2.0f, .player = 1},
	                  {.file = "SF_PlayerIconFountain", .offset = {15.0f, 0.0f}, .magnitude = 2.0f, .player = 2}},
	    .landScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
TOGGLE_COMPUTER_PLAYER("PLAYER_THREE", 1)
)",
	});

	all.push_back({
	    .id = "particles.draw_paths",
	    .name = "Sorted and queued sprites",
	    .facet = Facet::Particles,
	    .description = "The same smoke drawn three ways side by side: each puff in its own place among what blends, the "
	                   "whole effect at its origin as some spot visuals are, and as the miracle in the hand is.",
	    .expected = "The sorted smoke's puffs blend far to near from any side; the queued smoke draws newest first as one, "
	                "so seen from above older puffs cover newer ones; the third matches the queued without a hand.",
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 0.0f}}, .distance = 0.3f},
	    .particles = {{.type = ParticleType::Smoke, .offset = {-20.0f, 0.0f}, .magnitude = 8.0f},
	                  {.type = ParticleType::Smoke,
	                   .path = particles::draw::DrawPath::Queued,
	                   .offset = {0.0f, 0.0f},
	                   .magnitude = 8.0f},
	                  {.type = ParticleType::Smoke,
	                   .path = particles::draw::DrawPath::Immediate,
	                   .offset = {20.0f, 0.0f},
	                   .magnitude = 8.0f}},
	});

	all.push_back({
	    .id = "particles.fireball_rows",
	    .name = "The fireball's heat by its power-up",
	    .facet = Facet::Particles,
	    .description = "Three fireballs are thrown at three oaks every twelve seconds: the plain fireball on the left, "
	                   "the first power-up in the middle and the second power-up on the right.",
	    .expected = "The first power-up uses the second row of the fireball table, so it starts hotter and sets its oak "
	                "on fire sooner. The plain fireball and the second power-up use the first row.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 0.0f}, {40.0f, 30.0f}}, .distance = 0.45f},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-30.0f, 25.0f}},
	                {.type = TreeInfo::Oak, .offset = {0.0f, 25.0f}},
	                {.type = TreeInfo::Oak, .offset = {30.0f, 25.0f}}},
	    .miracles = {{.type = MagicType::Fireball,
	                  .point = {-30.0f, 25.0f},
	                  .handOffset = {-30.0f, 10.0f},
	                  .handHeight = 6.0f,
	                  .throwVelocity = {0.0f, 0.0f, 8.0f},
	                  .delaySeconds = 2.0f,
	                  .repeatSeconds = 12.0f},
	                 {.type = MagicType::FireballPowerUpOne,
	                  .point = {0.0f, 25.0f},
	                  .handOffset = {0.0f, 10.0f},
	                  .handHeight = 6.0f,
	                  .throwVelocity = {0.0f, 0.0f, 8.0f},
	                  .delaySeconds = 2.0f,
	                  .repeatSeconds = 12.0f},
	                 {.type = MagicType::FireballPowerUpTwo,
	                  .point = {30.0f, 25.0f},
	                  .handOffset = {30.0f, 10.0f},
	                  .handHeight = 6.0f,
	                  .throwVelocity = {0.0f, 0.0f, 8.0f},
	                  .delaySeconds = 2.0f,
	                  .repeatSeconds = 12.0f}},
	});

	all.push_back({
	    .id = "particles.lightning_forks_arcs",
	    .name = "The lightning bolt's forks and electric arcs",
	    .facet = Facet::Particles,
	    .description = "A lightning bolt held over five villagers and an oak for six seconds, again every ten seconds.",
	    .expected = "The bolt keeps two forks for each target that it finds, and no more. Electric arcs crawl over the "
	                "things that it strikes, from one point of each model to another.",
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 20.0f}}, .distance = 0.35f},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-8.0f, 10.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-3.0f, 14.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {3.0f, 9.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticShepherdMale, .offset = {7.0f, 13.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticTraderMale, .offset = {0.0f, 17.0f}, .inScript = true},
	                {.type = TreeInfo::Oak, .offset = {10.0f, 18.0f}}},
	    .miracles = {{.type = MagicType::LightningBolt,
	                  .point = {0.0f, 12.0f},
	                  .handOffset = {0.0f, 0.0f},
	                  .handHeight = 15.0f,
	                  .delaySeconds = 2.0f,
	                  .holdSeconds = 6.0f,
	                  .repeatSeconds = 10.0f}},
	});

	all.push_back({
	    .id = "particles.globe_alpha",
	    .name = "The one-shot globe's see-through bubble and rings",
	    .facet = Facet::Particles,
	    .description = "Two one-shot globes close up: a plain fireball and the fireball's first power-up, whose miracle "
	                   "shows one ring.",
	    .expected = "Each bubble is added over what is behind it at an alpha of 149, the globe's 150 times the land's "
	                "fully opaque light. The ring round the powered-up miracle shows at an alpha of 34.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, -18.0f}, .look = {0.0f, 4.0f, 8.0f}},
	    .dispensers = {{.type = MagicType::Fireball, .offset = {-5.0f, 8.0f}},
	                   {.type = MagicType::FireballPowerUpOne, .offset = {5.0f, 8.0f}}},
	});
}
