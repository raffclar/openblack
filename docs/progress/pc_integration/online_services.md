# Online services and PC extras

The game's links to the internet and to the player's PC outside the game world itself: the online account, the creature's
web page and the creature upload, maps fetched from the internet, the GameSpy lobby, and the force-feedback mouse. Lionhead's
servers are long gone, so the online parts are **n/a** in openblack, but are described here as the game did them. Network
play itself is owned by [../multiplayer/network_play.md](../multiplayer/network_play.md); the short summary of these extras
is [../multiplayer/online_services.md](../multiplayer/online_services.md). The real weather and the contact names have their
own files: [real_weather.md](real_weather.md) and [villager_names_from_contacts.md](villager_names_from_contacts.md).

**Progress: 0/26 done, 2 partial — 4%**

## Connecting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At start-up the game checks whether the computer is online, trying newer Internet Explorer's network check, then Internet Explorer 4's connected state, then the dial-up connections list | todo | openblack makes no such check |
| Each online step is logged, prefixed "[INET]", to `inetlog.txt` in the game folder | n/a | a log of the original's online steps |
| A command-line switch forces the game to think it is connected (unconfirmed) | n/a | a switch named FORCEINETCONN is in the game; its effect was not traced (unconfirmed) |
| The online features need an account registered at the game's web site, `http://www.bwgame.com/register/` | n/a | the site is gone |
| The game logs in at `login.bwgame.com` with the account's name and password | n/a | see ../multiplayer/network_play.md |
| The account's login name and password are kept in the online player profile, the password encrypted | n/a | |
| The installer offers to register online | n/a | from the readme |

## The creature's web page

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each time the creature's mind and body are saved, the game also writes a web page about the creature | todo | written alongside the creature's files in `Scripts/CreatureMind`; exactly when the mind is saved is owned by ../creature/saves_and_files.md |
| The page goes in the player's profile folder, in an `html` folder made if missing, named after the creature: `<creature name>.html` | todo | e.g. `Profiles/A/<profile>/html/aaa.html` in the game data |
| The page is made from the template `Scripts/creature_html.tmpl`, line by line, each `[$key$]` replaced by the creature's value for that key | todo | an unknown key is left out |
| The page is titled "Creature Cave" and shows the creature's name, age, alignment, strength, health, energy, villagers killed and battles won | todo | the keys the shipped template uses |
| It shows up to five pictures of the creature, `creatureshot_0.jpg` to `creatureshot_4.jpg` from the profile folder, faded to 40% until the mouse is over one, each opening larger in a pop-up | todo | the page's own script; Internet Explorer filters |
| Its frames, scrolls, banner, logo and good and evil pictures come from `Scripts/html` | todo | banner, backgrounds, candle, frames, scrolls, title, good and evil pictures |
| The game also offers keys the shipped template does not use: fatness, exhaustion, dehydration, itchiness, amount of poo, animals killed, creatures killed, battles fought, poos, mushrooms eaten, how nice the player has been and how much attention the player has given | todo | a modder's template could show them |
| It offers each of the creature's 40 desires, a caption for each of the five pictures, 24 further values and one more chosen from 16 (unconfirmed which) | todo | (unconfirmed what the 24 and the 16 are) |
| The pictures are taken by scripts that snapshot the creature | todo | `CHLApi.cpp` SNAPSHOT, UPDATE_SNAPSHOT and UPDATE_SNAPSHOT_PICTURE are stubs; which scripts take them is not confirmed (unconfirmed) |

## Uploading the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At the same time the creature is packed into one file, `creature.lhp`, in the profile folder | todo | openblack reads and writes creature minds (`components/`) but not this file |
| The file holds a creature block with every value the web page offers, and a pictures block with the JPEG pictures | todo | |
| Its header carries a random number from a generator seeded with the clock | todo | |
| The account's login name is written into it with every character other than letters, digits and "-" changed to "_" | todo | |
| A separate program, the Creature Upload Utility (`CreatureUpload.exe`), sends the file to Lionhead's server | n/a | the server is gone |
| The utility asks for the profile, user name and password, checks the account ("Validating user account, may take a while…"), then sends | n/a | its text is in `creatureupload.cfg` |
| When sent, it says the creature can be seen at `http://www.bwcreatures.com/<name>` | n/a | the site is gone |
| Its errors: could not connect, could not send or get data, could not open the creature file, timed out, user not validated, user not found, wrong password, no profile ("You need to register online"), no password entered | n/a | from `creatureupload.cfg` |
| Sending a creature to a friend by e-mail | n/a | no such feature was found in the game (unconfirmed for the upload utility) |

## Creatures in online games

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature's mind is packed for sending when an online game starts | n/a | see ../multiplayer/network_play.md |
| In a clan game (from patch 1.2), the clan's creature is downloaded from the game's servers at the start and uploaded at the end, instead of the team leader's | n/a | from the patch 1.2 readme; see ../multiplayer/multiplayer_rules.md |
| If the upload at the end fails, the end-of-game box says so, and the creature is uploaded next time | n/a | |
| Creatures keep what they learn online | todo | see ../multiplayer/multiplayer_rules.md |

## Maps from the internet

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Multiplayer maps live in the `Online Maps` folder, each a `.map` file with a `.thm` thumbnail | todo | see ../terrain/land_loading.md and ../multiplayer/skirmish.md |
| The local list, `localmaps.txt`, gives each map's title, file and number of players: "Bombardment - 2 players", "King of the hill - 3 players" and "The four corners of Eden - 4 players" | todo | |
| A global list of maps, `GlobalList.txt`, is fetched from the game's servers | n/a | the servers are gone |
| Maps and their thumbnails missing on the player's PC are downloaded into `Online Maps` and `Online Maps/Thumbs` | n/a | from the game's storage server (`storage.bwgame.com` or `www.bwgame.com`, unconfirmed which for which) |
| If a map cannot be downloaded, the player is told | n/a | |
| Each map's winning conditions, with default, least and most values and the steps for the left and right mouse buttons, come from `ConditionTemplate.txt` | todo | see ../multiplayer/multiplayer_rules.md |
| Newly made maps keep their thumbnails in `Online Maps/Creation` | n/a | the map-making side is not part of the game's player features (unconfirmed) |

## The GameSpy lobby

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Internet games are found in a GameSpy lobby, through GameSpy's master server | n/a | GameSpy closed in 2014; see ../multiplayer/network_play.md |
| The player's GameSpy registration is read from GameSpy's own settings on the PC | n/a | |
| The lobby has a main room and game channels: create or join a room, list the players in it, see players join and leave | n/a | |
| The host can ban a player from the channel, lock it, and allow invitations | n/a | |
| The game's own lobby and message servers, and a ping server, were also used | n/a | the ping server's side is done in bwgame-service: see [../multiplayer/gathering_box_and_ping_server.md](../multiplayer/gathering_box_and_ping_server.md) |
| Internet games earn ranking points for players and clans, shown on the web site | n/a | see ../multiplayer/multiplayer_rules.md |

## Force-feedback mouse

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game supports Immersion's force-feedback ("TouchSense") mice through Immersion's library, `IFC22.dll` | todo | openblack has no force feedback; a gamepad rumble or haptics could stand in |
| The effects are loaded from `Data/Immersion/FullFeedback.ifr`, or `Data/Immersion/Tactile.ifr` for plain vibrating mice (unconfirmed which mouse uses which) | todo | (unconfirmed) |
| A dialog sets the strength: Minimum, Normal or Maximum | todo | |
| There are 49 effects: fur, slap, hit, gesture success, grip, uproot, the hand hitting the influence edge, leash pull, gesture trail, spell crackle, heartbeat, sprinkling food, wood and water, burning, a miracle's pop, heavy, the trademark, the mushroom challenge, three fireball strengths, lightning bolt, something moving in the hand, casting, shield, flocks flying and on the ground, creature spells, a hit that is not allowed, a moving fish, a command obeyed, the Hanoi puzzle, planting, and the hand passing over a miracle, the land, straw, smooth things, wood, canvas, a challenge scroll, a story scroll, foliage, the creed, the singing stones, a vortex, the temple, a teleport and special crops | partial | openblack lists them (`ImmersionEffectType` in `src/Enums.h`), and objects' info carries their in-hand effect (`src/InfoConstants.h`), but nothing plays them |
| Each turn the hand picks one effect: what it holds if anything, else what it is over; when that changes, the old effect stops and the new one starts | todo | none while the hand is in one particular state or a help message is showing |
| Clicking the game's menus gives a click effect | todo | |
| Putting a tattoo on the creature or taking it off gives a pulse | todo | see ../creature/creature_tattoos.md |
| Scripts can start and stop effects, stop them all, and ask whether a force-feedback mouse is present | partial | `CHLApi.cpp` START_IMMERSION, STOP_IMMERSION and STOP_ALL_IMMERSION are stubs; IMMERSION_EXISTS always answers no, which is the game's answer without such a mouse |
| A debug line shows the current effect's name | n/a | a developer display |
