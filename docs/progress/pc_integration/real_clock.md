# The real clock and calendar

Everything in the game that reads the date and time of the player's computer, rather than the game's own clock: voices
that whisper the player's name late at night, the real moon's phase, script functions that read the real date and time,
the smiley footprints of the first of April, the time played, the dates on saved games, and the times the online
features send and compare. The game's own day and night, and its in-game calendar, are owned by
[../sky/day_night_cycle.md](../sky/day_night_cycle.md) and [../engine/game_loop_and_clock.md](../engine/game_loop_and_clock.md).
All of the game's reads of the clock were traced; the dates and hours below are the only ones it acts on.

**Progress: 6/46 done, 3 partial — 16%**

## Night whispers: when

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On real nights a ghostly voice now and then whispers the player's own name | todo | the advisors' summary rows are in ../story/advisors.md ("Spooky voices"); ported on Diego's bw-clean branch, not in openblack |
| A real night is from 20:45 to 20:59, and from 23:00 to 05:59, by the computer's local clock | todo | the hour must be 23 or later, or 5 or earlier, or 20 with the minutes 45 or more; 21:00 to 22:59 is not night (an odd gap, but that is what the game does) |
| The clock is read every 100 game turns, about every 10 seconds of play | todo | the count of turns runs only while the game runs turns, so not while paused |
| Never on the first two lands | todo | the land numbers 1 and 2 are skipped |
| Never when no name could be matched (see below) | todo | |
| There is no option to turn it off | todo | none was found in the game (unconfirmed for the setup program) |
| Nothing stops it in a multiplayer game | todo | no multiplayer check was found (unconfirmed in play) |

## Night whispers: how likely

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each real-night check counts up by one; the count starts at 0 and goes back to 0 after each whisper | todo | the count does not grow outside real nights |
| A check whispers when (1 − r³) × 1000, cut to a whole number, is below the count, with r a local random number from 0 to 1 | todo | so the chance after k night checks is 1 − (1 − k/1000)^⅓ |
| So it is rare at first and grows: about 0.3% a check after 10 checks (under 2 minutes), 2% after 60 (10 minutes), 6.4% after 180 (30 minutes), 14% after 360 (an hour), and certain after 1000 | todo | worked from the rule above |
| A check that does not whisper may instead have the evil advisor say, once per run of the game, "Night's falling. Beware. The foul are abroad." with a chance of 0.02% | todo | the advisor line is one of the game's one-off guidance remarks; see ../story/advisors.md |

## Night whispers: whose name

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The name is looked up as the game starts up, and again as each game starts | todo | |
| The game first tries the player's profile name | todo | openblack's profile is only the computer's login name (see ../interface/profiles.md) |
| If that does not match, it tries a second name the game holds for the player (unconfirmed which: most likely the online player's name) | todo | (unconfirmed) |
| If that does not match either, it tries the name Windows was registered to, from the system's setup information | n/a | read from the Windows registry; Windows-only |
| Each word of the name is tried in turn | todo | words are split at spaces |
| A word matches a recorded name when their first letters are the same exactly (case counts) and their next three sound codes match | todo | a Soundex-style code: b f p v; c g j k q s x z; d t; l; m n; r; vowels and other letters give none; a letter with the same code as the one before is skipped |
| There are 100 recorded names: James, John, Peter, Pete, Matthew, Mark, Simon, Paul, Timothy, Tim, Jon, Andrew, Andy, Jamie, Will, William, Oliver, Ollie, Richard, Rich, Dan, Ben, Neil, Rob, Robert, Russell, Jeremy, Jez, Gary, Mike, Chris, Christopher, Dave, David, Aaron, Wayne, Ian, Phil, Julian, Ed, Tom, Charlie, Jack, Steven, Steve, Liam, Jason, Nick, Nicholas, Thomas, Darren, Kevin, Lee, Terry, Adrian, Dominic, Alex, Matt, Nathan, Adam, Alan, Graham, Angus, Abdul, Achmed, Mohammed, Habib, Gareth, Jo, Joe, Janice, Claire, Sophie, Vicky, Mel, Rachel, Emma, Gemma, Alison, Caroline, Jade, Kylie, Lucy, Sam, Debbie, Jasmine, Lisa, Louise, Emily, Amanda, Lizzie, Daisy, Katie, Sarah, Cat, Amy, Naomi, Helen, Karen, Rebecca | todo | the game's text table; the first recorded name that matches wins |
| The whisper is that name's recording, from the advisors' sound bank | todo | `Audio/Dialogue/Guidance.sad` |
| The game's info table also lists five named voices: Tblamb, Revans, Aevans, Bill and Pmolyneux | partial | openblack reads them (`InfoConstants::spookyVoice`, `src/InfoConstants.h`); what the game does with them was not found (unconfirmed) |

## Night whispers: how it sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The volume is the normal one times or divided by 1 + a³, a a local random number up to 0.65, so 78% to 127% | todo | multiply or divide is a coin toss |
| The pitch is the sample's own times or divided by 1 + b³, b up to 0.8, so 0.66 to 1.51 times | todo | as above |
| A third random number from 0 to 179 is set on the sound (unconfirmed: most likely where it is heard from) | todo | (unconfirmed) |
| It is played as a plain sound, not placed in the world (unconfirmed) | todo | (unconfirmed) |

## The real moon

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The moon in the sky shows the real moon's phase, from the computer's clock | done | see ../sky/moon.md: `moon::Phase`; test `Moon.PhaseFollowsTheRealMoon` |
| The clock is read at most once every 2 seconds for it | done | openblack reads it every frame, which gives the same phase (../sky/moon.md) |
| Scripts can ask how full the moon is | todo | `CHLApi.cpp` GET_MOON_PERCENTAGE is a stub; see ../sky/moon.md |
| The advisors remark on the moon's phase | todo | see ../interface/help_system.md; whether the remark uses the real phase is not confirmed (unconfirmed) |

## Script functions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "get real time" gives the local time as an hour with a fraction: hours + minutes / 60 + seconds / 3600 | todo | `CHLApi.cpp` GET_REAL_TIME is a stub that gives 0 |
| "get real day" gives the day of the month, 1 to 31 | todo | GET_REAL_DAY is a stub |
| "get real weekday" gives the day of the week, 1 to 7 with Sunday as 1 | todo | openblack binds both day functions to the same stub; the second one of the pair is the weekday in the game |
| "get real month" gives the month, 1 to 12 | todo | GET_REAL_MONTH is a stub |
| "get real year" gives the full year, e.g. 2026 | todo | GET_REAL_YEAR is a stub |
| In a multiplayer game each of them reports "This is not multiplayer friendly yet!" as a script error, but still gives the value | todo | |
| None of the shipped challenge scripts use them | n/a | searched all the game's script sources; only mods would see the difference |

## Special days

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the first of April every creature leaves a smiley face instead of its own footprint | done | `FootprintSystem::IsAprilFools` (`src/ECS/Systems/Implementations/FootprintSystem.cpp`), `creature_footprints::IsAprilFools`; test `CreatureFootprints.AprilFoolsSmileyKeepsTheSpeciesSize`; see ../terrain/land_marks.md |
| The day is the computer's local date, from midnight to midnight, read again for every print, so prints change mid-walk at midnight | done | openblack reads the local date for each print too |
| The smiley keeps the size the species' own print would have | done | test `CreatureFootprints.AprilFoolsSmileyKeepsTheSpeciesSize` |
| The smiley is the eighth picture in the row of footprint pictures | done | `creature_footprints::k_SmileyCell` |
| It applies to every creature on the land, the player's, the enemies' and in multiplayer games, with no option to turn it off | partial | as the game; openblack adds a debug override in the creature spawner (`src/Debug/CreatureSpawnerFootprints.cpp`) and a testbed scenario |
| No other date changes anything: no Christmas, Halloween, New Year, Easter, birthday or release-date surprise | n/a | every read of the date in the game was traced; only the first of April and the real-night hours are acted on |
| The Christmas music and the "White Christmas" singing-stones song play from a script on any date | n/a | not date-based; see ../audio/music.md and ../nature/one_shot_features.md |
| The in-game calendar (days, months, years and seasons) is the land's own, set by its script, not the real date | n/a | see ../weather/climates.md |

## Time played and saved games

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The real time is noted when a game starts | partial | openblack counts from when it was started (`src/3D/Implementations/TempleInterior.cpp`) |
| When the game is closed, the time since then is added to the total time played, which is kept in the player profile | todo | openblack keeps no total |
| The save game room's scroll shows the total time played, including the current session, as hours, minutes and seconds | todo | `src/3D/TempleScrolls.cpp` shows only this session's time |
| Each saved game is labelled with the time and date it was saved, in the user's own Windows format: the time, a space, then the long date (the short date if the long one is not available) | todo | `TempleScrolls` shows a date field, but openblack's saved games are made-up placeholders |
| If the time and date together are 62 characters or more, the label is left blank | todo | |
| The real time is noted as each land's statistics start (unconfirmed what for) | todo | it is saved with the statistics and never shown; perhaps the online record's time played (unconfirmed) ([../interface/statistics_counted.md](../interface/statistics_counted.md)) |
| The music notes the real time each mood was last chosen (unconfirmed what for) | todo | see ../audio/music.md (unconfirmed) |

## Other uses of the clock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The e-mail feature notes the start-up time and ignores older mail | n/a | see villager_names_from_contacts.md |
| The real weather queries send the computer's date as day.month.year | n/a | see real_weather.md |
| The creature upload file is given a random number from a generator seeded with the clock | todo | see online_services.md |
| A config text file named after the detail level is given a line with the date, time and detail level (unconfirmed when) | n/a | a developer log, not a player feature |
