# Challenge natives: miracles, influence and weather

The challenge scripts' functions for miracles (casting, giving, finding and changing them), rewards, prayer power, influence, the vortex and storms and climates. The game has 464 of these functions in all; the language statement each comes from is shown in italics, and "called" counts are calls in the shipped `challenge.chl`. How the virtual machine runs them is in [../engine/script_vm.md](../engine/script_vm.md); what each challenge is about is in [../story/](../story/).

**Progress: 4/38 done, 2 partial — 13%**

## Used by the shipped scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Changes the inner and outer radius (and calm) of a shield, storm or influence: *set ‹obj› properties inner ‹inner› outer ‹outer› [calm ‹calm›]* (called 43 times in 25 scripts) | todo | `ChangeInnerOuterProperties` logs "not implemented" |
| Puts a ring of influence around an object for a player: *create influence on ‹target› [radius ‹radius›] ‹zero› ‹anti›* (called 19 times in 9 scripts) | todo | `InfluenceObject` logs "not implemented" |
| Puts a ring of influence at a position for a player: *create influence at ‹position› [radius ‹radius›] ‹zero› ‹anti›* (called 29 times in 23 scripts) | todo | `InfluencePosition` logs "not implemented" |
| Gives a player's influence at a position: *influence ‹player› ‹raw› at ‹position›* (called 10 times in 7 scripts) | todo | `GetInfluence` logs "not implemented" |
| Changes a storm's temperature, rain, snow, cloud cover and how fast its rain falls: *set ‹storm› properties degrees ‹temperature› rainfall ‹rainfall› snowfall ‹snowfall› overcast ‹overcast› fallspeed ‹fallspeed›* (called 15 times in 8 scripts) | todo | `ChangeWeatherProperties` logs "not implemented" |
| Changes how often a storm throws sheet and forked lightning: *set ‹storm› properties sheetmin ‹sheetmin› sheetmax ‹sheetmax› forkmin ‹forkmin› forkmax ‹forkmax›* (called 18 times in 8 scripts) | todo | `ChangeLightningProperties` logs "not implemented" |
| Changes how long a storm lasts and how it fades: *set ‹storm› properties time ‹duration› fade ‹fade time›* (called 16 times in 8 scripts) | todo | `ChangeTimeFadeProperties` logs "not implemented" |
| Changes a storm's clouds, their shade and their height: *set ‹storm› properties clouds ‹num clouds› shade ‹blackness› height ‹elevation›* (called 15 times in 8 scripts) | todo | `ChangeCloudProperties` logs "not implemented" |
| Casts a miracle on an object, from a place, with a radius, time and curl: *cast ‹spell› spell on ‹target› from ‹from› radius ‹radius› time ‹duration› curl ‹curl›* (called 4 times in 3 scripts) | done | `SpellAtThing`: core audit: fixed |
| Casts a miracle at a position, from a place, with a radius, time and curl: *cast ‹spell› spell at ‹target› from ‹from› radius ‹radius› time ‹duration› curl ‹curl›* (called 51 times in 16 scripts) | done | `SpellAtPos`: core audit: OK |
| Finds a miracle of a kind already at a position, within a radius: *get spell ‹spell› at ‹position› radius ‹radius›* (called 9 times in 3 scripts) | done | `SpellAtPoint`: core audit: fixed (a query, not a cast) |
| Changes a miracle's radius: *set ‹object› radius ‹radius›* (called once in 1 script) | todo | `SetMagicRadius` logs "not implemented" |
| Gives the player a reward (a miracle, a creature or an object), dropping it from the sky or not: *reward ‹reward› at ‹position› [from sky]* (called 8 times in 5 scripts) | todo | `CreateReward` logs "not implemented" |
| Gives the player a reward in a town: *reward ‹reward› in ‹town› at ‹position› [from sky]* (called 6 times in 3 scripts) | todo | `CreateRewardInTown` logs "not implemented" |
| Whether a player has a miracle: *spell ‹spell› for player ‹player›* (called 41 times in 1 script) | todo | `HasPlayerMagic` logs "not implemented" |
| Gives a player influence everywhere (or takes it back): *enable/disable player ‹player› virtual influence* (called 4 times in 4 scripts) | todo | `SetVirtualInfluence` logs "not implemented" |
| Fades a vortex out: *start ‹vortex› fade out* (called 4 times in 4 scripts) | todo | `VortexFadeOut` logs "not implemented" |
| Sets a vortex's town, the flock it gathers, the flock's position, distance and radius: *set ‹vortex› properties town ‹town› flock position ‹position› distance ‹distance› radius ‹radius› flock ‹flock›* (called 2 times in 2 scripts) | todo | `VortexParameters` logs "not implemented" |
| Sets how much prayer power a store or player has: *set ‹object› mana ‹mana›* (called 9 times in 4 scripts) | todo | `GameSetMana` logs "not implemented" |
| Sets a miracle's properties for a time: *set ‹object› magic properties ‹magic type› [time ‹duration›]* (called once in 1 script) | todo | `SetMagicProperties` logs "not implemented" |
| Whether an object is under a miracle: *‹object› affected by spell ‹spell›* (called 2 times in 2 scripts) | todo | `IsAffectedBySpell` logs "not implemented" |
| Puts a miracle inside an object (a dispenser, a chest) or takes it out: *enable/disable spell ‹magic type› in ‹object›* (called 14 times in 8 scripts) | todo | `SetMagicInObject` logs "not implemented" |
| Pauses or restarts the climates' weather: *enable/disable climate weather* (called 4 times in 2 scripts) | partial | `PauseUnpauseClimateSystem`: `WeatherSystem::SetClimateSystemEnabled`; not audited |
| Gives the prayer power a miracle costs: *get mana for spell ‹spell›* (called 4 times in 3 scripts) | todo | `GetManaForSpell` logs "not implemented" |
| Ends every storm within a radius of a position: *delete all weather at ‹position› radius ‹radius›* (called 2 times in 2 scripts) | done | `KillStormsInArea`: weather audit: fixed |
| Gives a player's prayer power: *get ‹worship site› mana total* (called 2 times in 2 scripts) | todo | `GetMana` logs "not implemented" |
| Stops a player charging a miracle: *clear player ‹player› spell charging* (called 2 times in 2 scripts) | todo | `ClearPlayerSpellCharging` logs "not implemented" |

## Not used by the shipped scripts

The game has these but no shipped script calls them; mods and fan-made challenges can.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gives a player a miracle or takes it away: *enable/disable spell ‹value› for player ‹value›* (not called by the shipped scripts) | todo | `SetPlayerMagic` logs "not implemented" |
| Whether an object was cast by another: *‹spell instance› cast by ‹caster›* (not called by the shipped scripts) | todo | `ObjectCastByObject` logs "not implemented" |
| Whether there is wind magic at a position (unconfirmed): *wind magic at a position (no statement in the language)* (not called by the shipped scripts) | todo | `IsWindMagicAtPos` logs "not implemented" |
| Gives the time since a player last cast a miracle: *get player ‹player› time since last spell cast* (not called by the shipped scripts) | todo | `PlayerSpellCastTime` logs "not implemented" |
| Gives the last miracle a player cast: *get player ‹player› last spell cast* (not called by the shipped scripts) | todo | `PlayerSpellLastCast` logs "not implemented" |
| Gives where a player last cast a miracle: *last player ‹player› spell cast position* (not called by the shipped scripts) | todo | `GetLastSpellCastPos` logs "not implemented" |
| Whether a player is charging a miracle: *player ‹value› spell charging* (not called by the shipped scripts) | todo | `IsSpellCharging` logs "not implemented" |
| Whether a player is charging a given miracle: *player ‹value› spell ‹value› charging* (not called by the shipped scripts) | todo | `IsThatSpellCharging` logs "not implemented" |
| Sets whether a player's miracles resist the wind: *enable/disable player ‹value› wind resistance* (not called by the shipped scripts) | todo | `SetPlayerWindResistance` logs "not implemented" |
| Gives whether a player's miracles resist the wind: *a player's wind resistance (no statement in the language)* (not called by the shipped scripts) | todo | `GetPlayerWindResistance` logs "not implemented" |
| Pauses or restarts the climates making storms: *enable/disable climate create storms* (not called by the shipped scripts) | partial | `PauseUnpauseStormCreationInClimateSystem`: `WeatherSystem::SetStormCreationEnabled`; not audited |
