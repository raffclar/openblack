#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""A stdio MCP server for openblack's debug inspector.

The game, built with OPENBLACK_INSPECTOR and started with --inspect-port PORT, answers one JSON request per line
on 127.0.0.1:PORT. This adapter exposes its queries to agents as MCP tools. It has no dependencies beyond Python 3.8,
and the game knows nothing of MCP.

Register it with Claude Code (once per machine or project):

    claude mcp add openblack-inspector -- python <repo>/tools/inspector/openblack_inspector_mcp.py --port 47800

then start the game with --inspect-port 47800. The port can also come from OPENBLACK_INSPECT_PORT. The game may be
started and restarted at any time: the adapter connects on each call.

Several games can run at once, each started with --inspect-port 0 (any free port). Each keeps a file
<pid>.json in openblack-inspector under the temporary directory while its inspector runs. inspector_games lists
them. One MCP session is shared by every agent using it, so every tool takes the game it is for (port, pid or
worktree); with several games running a call naming none is refused with the list, rather than sent to a game chosen
earlier by someone else. Every answer names the game it came from (pid, port, worktree), and the adapter checks with a
ping on each new connection that the game answering is the one meant. While a game loads, calls wait for it.
This works with games of older builds too, which don't name themselves: the adapter names them from the ping.

Run it with --call QUERY [JSON] to send a single request from a shell, without MCP:

    python openblack_inspector_mcp.py --port 47800 --call sky.moon
    python openblack_inspector_mcp.py --call objects.find '{"params": {"component": "Tree"}, "near": [0, 0], "radius": 50}'
    python openblack_inspector_mcp.py --games
    python openblack_inspector_mcp.py --worktree ob-wt-inspect --call sky.moon
"""

import argparse
import json
import os
import socket
import sys
import tempfile
import time

PROTOCOL_VERSION = "2024-11-05"
DEFAULT_PORT = 47800

# Shaping options every query takes, described once for the tools' schemas
SHAPING = {
    "fields": {"type": "array", "items": {"type": "string"},
               "description": "Only these dotted paths of each item (or of the object), e.g. [\"id\", \"label\"]"},
    "where": {"type": "array", "items": {"type": "object"},
              "description": "Filters: [{\"field\": \"life\", \"op\": \"<\", \"value\": 0.5}]; "
                             "ops ==, !=, <, <=, >, >=, contains, exists"},
    "limit": {"type": "integer", "description": "Most items to return (20 by default, 500 at most)"},
    "cursor": {"type": "integer", "description": "Start from the next_cursor of the last page"},
    "count": {"type": "boolean", "description": "Only how many items match"},
    "summary": {"type": "string", "description": "count/min/max/mean of this numeric field instead of items"},
    "max_bytes": {"type": "integer", "description": "Cap on the result's size (16384 by default)"},
}
NEAR = {
    "near": {"type": "array", "items": {"type": "number"},
             "description": "A point: [x, y, z], or [x, z] to measure across the ground"},
    "radius": {"type": "number", "description": "How far from the point to search"},
}
FIND = {
    "component": {"type": "string", "description": "Only entities with this component, e.g. Tree"},
    "kind": {"type": "string", "description": "Only this kind: Villager, Abode, Tree, Animal, Creature, Feature..."},
    "name": {"type": "string", "description": "Only entities whose label holds this text"},
}


def schema(properties=None, required=None):
    return {"type": "object", "properties": properties or {}, "required": required or []}


CAMERA_POSE = {
    "position": {"type": "array", "items": {"type": "number"}, "description": "[x, y, z] where the camera stands"},
    "focus": {"type": "array", "items": {"type": "number"}, "description": "[x, z] on the land or [x, y, z]"},
    "yaw": {"type": "number"}, "pitch": {"type": "number"}, "distance": {"type": "number"},
}
POINT_SCHEMA = {"type": "object",
                "description": "{\"screen\": [x, y]} pixels, or {\"world\": [x, z] or [x, y, z]}"}

# Each tool: its name, description, input schema, and how its arguments become a request
TOOLS = [
    {
        "name": "inspector_describe",
        "description": "Lists the inspector's providers and queries; with provider or query, their parameters. "
                       "Start here to discover what can be asked.",
        "inputSchema": schema({"provider": {"type": "string"}, "query": {"type": "string"}}),
        "query": "describe",
        "params": ["provider", "query"],
    },
    {
        "name": "inspector_query",
        "description": "Runs any inspector query by name (e.g. ecs.entities, sky.moon, land.weather_at, "
                       "magic.fires, script.tasks) with its params and the shaping options. Every system the game "
                       "has is covered by some provider: ask inspector_describe. Results are small by default: "
                       "lists are paged at 20 items.",
        "inputSchema": schema({"query": {"type": "string"}, "params": {"type": "object"}, **NEAR, **SHAPING},
                              ["query"]),
    },
    {
        "name": "game_moon",
        "description": "The moon and its state only: position (null while down), phase, cycle_day, visible.",
        "inputSchema": schema(),
        "query": "sky.moon",
    },
    {
        "name": "game_find_objects",
        "description": "Objects within a radius of a point, nearest first: id, kind, label, position, distance.",
        "inputSchema": schema({**FIND, **NEAR, **SHAPING}, ["near", "radius"]),
        "query": "objects.find",
        "params": list(FIND),
    },
    {
        "name": "game_splash",
        "description": "Splashes within a radius of a point, nearest first, with their state: rings on the water "
                       "(age, life, alpha) and splash particle effects (age, particles, closing).",
        "inputSchema": schema({**NEAR, **SHAPING}, ["near", "radius"]),
        "query": "particles.splash",
    },
    {
        "name": "game_entities",
        "description": "Entities by component, kind or label: ids, kinds, labels and positions (no components).",
        "inputSchema": schema({**FIND, **NEAR, **SHAPING}),
        "query": "ecs.entities",
        "params": list(FIND),
    },
    {
        "name": "game_entity",
        "description": "One entity: kind, label, position and component names; with components (names or "
                       "\"all\"), their fields.",
        "inputSchema": schema({"id": {"type": "integer"},
                               "components": {"description": "Component names, or \"all\""},
                               **SHAPING}, ["id"]),
        "query": "ecs.entity",
        "params": ["id", "components"],
    },
    {
        "name": "game_state",
        "description": "Whether the game is paused, its turn, frame, speed and any stepping.",
        "inputSchema": schema(),
        "query": "game.state",
    },
    {
        "name": "game_pause",
        "description": "Pauses the game.",
        "inputSchema": schema(),
        "query": "game.pause",
    },
    {
        "name": "game_resume",
        "description": "Lets the game run.",
        "inputSchema": schema(),
        "query": "game.resume",
    },
    {
        "name": "game_step",
        "description": "Runs the game for some frames or turns (ten turns a second), then pauses it. Waits until "
                       "the step is done unless wait is false. With fixed_ms each frame takes that long whatever the "
                       "wall clock says (deterministic stepping for replays).",
        "inputSchema": schema({"frames": {"type": "integer"}, "turns": {"type": "integer"},
                               "fixed_ms": {"type": "integer"}, "wait": {"type": "boolean"}}),
        "query": "game.step",
        "params": ["frames", "turns", "fixed_ms"],
    },
    {
        "name": "game_frame_time",
        "description": "Each frame takes a fixed time in ms from now on (0 for the wall clock again).",
        "inputSchema": schema({"ms": {"type": "integer"}}, ["ms"]),
        "query": "game.frame_time",
        "params": ["ms"],
    },
    {
        "name": "ecs_hash",
        "description": "A hash of where every entity is (and of a component's fields when named), to compare runs.",
        "inputSchema": schema({"component": {"type": "string"}}),
        "query": "ecs.hash",
        "params": ["component"],
    },
    {
        "name": "game_speed",
        "description": "Sets the game's speed: 1 is normal, 2 twice as fast.",
        "inputSchema": schema({"speed": {"type": "number"}}, ["speed"]),
        "query": "game.speed",
        "params": ["speed"],
    },
    {
        "name": "game_scenarios",
        "description": "The testbed scenarios that can be loaded: id, name, facet, description.",
        "inputSchema": schema(SHAPING),
        "query": "game.scenarios",
    },
    {
        "name": "game_load_scenario",
        "description": "Loads a testbed scenario on a fresh testbed by its id (see game_scenarios).",
        "inputSchema": schema({"id": {"type": "string"}}, ["id"]),
        "query": "game.scenario",
        "params": ["id"],
    },
    # Writes: each answers the small state that results, and is logged by the game and listed by inspector_writes
    {
        "name": "edit_kinds",
        "description": "The kinds of thing edit_create makes; with kind, that kind's types by number and name.",
        "inputSchema": schema({"kind": {"type": "string"}, **SHAPING}),
        "query": "edit.kinds",
        "params": ["kind"],
    },
    {
        "name": "edit_create",
        "description": "Makes a thing through the archetype the game makes it with (kinds: creature, villager, "
                       "building, tree, feature, mobile_object, mobile_static, dispenser, miracle_bubble). Answers "
                       "its id, label, components and whether it is in its map cell and the physics.",
        "inputSchema": schema({"kind": {"type": "string"},
                               "type": {"description": "The type's number or name (edit_kinds)"},
                               "position": {"type": "array", "items": {"type": "number"},
                                            "description": "[x, z] on the land, or [x, y, z]"},
                               "yaw": {"type": "number", "description": "Degrees about the up axis"}},
                              ["kind", "type", "position"]),
        "query": "edit.create",
        "params": ["kind", "type", "position", "yaw"],
    },
    {
        "name": "edit_move",
        "description": "Moves a thing as the game's tools do and refiles it in the map; answers its place and cell.",
        "inputSchema": schema({"id": {"type": "integer"},
                               "position": {"type": "array", "items": {"type": "number"},
                                            "description": "[x, z] on the land, or [x, y, z]"}},
                              ["id", "position"]),
        "query": "edit.move",
        "params": ["id", "position"],
    },
    {
        "name": "edit_turn",
        "description": "Turns a thing about the up axis by an angle in degrees.",
        "inputSchema": schema({"id": {"type": "integer"}, "yaw": {"type": "number"}}, ["id", "yaw"]),
        "query": "edit.turn",
        "params": ["id", "yaw"],
    },
    {
        "name": "edit_set",
        "description": "Sets one field of a component (a dotted path into nested values), its type checked; "
                       "answers the field as it now is.",
        "inputSchema": schema({"id": {"type": "integer"}, "component": {"type": "string"},
                               "field": {"type": "string"}, "value": {"description": "Of the field's type"}},
                              ["id", "component", "field", "value"]),
        "query": "edit.set",
        "params": ["id", "component", "field", "value"],
    },
    {
        "name": "edit_add_component",
        "description": "Adds a component as it starts, then sets the given fields; answers the component.",
        "inputSchema": schema({"id": {"type": "integer"}, "component": {"type": "string"},
                               "fields": {"type": "object"}}, ["id", "component"]),
        "query": "edit.add",
        "params": ["id", "component", "fields"],
    },
    {
        "name": "edit_remove_component",
        "description": "Takes a component off an entity; answers the components left.",
        "inputSchema": schema({"id": {"type": "integer"}, "component": {"type": "string"}}, ["id", "component"]),
        "query": "edit.remove_component",
        "params": ["id", "component"],
    },
    {
        "name": "edit_destroy",
        "description": "Takes a thing out through the game's own removal (its physics, sounds, map cells, home and "
                       "town); a creature goes through the creature removal, which lets go of its leash, fight, the "
                       "hand, its player's list (the next creature becomes the primary one), effects and what it "
                       "carries. how=effect destroys it as an effect would (a villager dies, a building burns "
                       "down; never a creature). Answers whether it still exists, is in its map cell or the physics.",
        "inputSchema": schema({"id": {"type": "integer"}, "how": {"type": "string", "enum": ["remove", "effect"]}},
                              ["id"]),
        "query": "edit.destroy",
        "params": ["id", "how"],
    },
    {
        "name": "ecs_references",
        "description": "Every component field that holds an entity id (gone or not): who holds it, the component and "
                       "the field. Use after a destroy to check that nothing still points at it.",
        "inputSchema": schema({"id": {"type": "integer"}, **SHAPING}, ["id"]),
        "query": "ecs.references",
        "params": ["id"],
    },
    {
        "name": "creatures_fight",
        "description": "Starts a fight between two creatures as the scripts and the leash do; answers how it went.",
        "inputSchema": schema({"id": {"type": "integer"}, "opponent": {"type": "integer"}}, ["id", "opponent"]),
        "query": "creatures.fight",
        "params": ["id", "opponent"],
    },
    # Input made at the game's action layer, exactly as the player's mouse and keyboard reach it
    {
        "name": "input_state",
        "description": "The pointer, buttons held, the hand, what the cursor picks, the input lock and the input "
                       "still to come.",
        "inputSchema": schema(),
        "query": "input.state",
    },
    {
        "name": "input_names",
        "description": "The action names (as the options screen names them) and the gesture names.",
        "inputSchema": schema(),
        "query": "input.names",
    },
    {
        "name": "input_pointer",
        "description": "Moves the pointer to a pixel, or to where a point of the world is on the screen; the hand "
                       "follows it this frame.",
        "inputSchema": schema({"screen": {"type": "array", "items": {"type": "number"}},
                               "world": {"type": "array", "items": {"type": "number"}}}),
        "query": "input.pointer",
        "params": ["screen", "world"],
    },
    {
        "name": "input_button",
        "description": "A mouse button (left, middle, right) where the pointer is: press, release or click.",
        "inputSchema": schema({"button": {"type": "string", "enum": ["left", "middle", "right"]},
                               "action": {"type": "string", "enum": ["press", "release", "click"]}}),
        "query": "input.button",
        "params": ["button", "action"],
    },
    {
        "name": "input_key",
        "description": "A key by SDL's name (\"L\", \"Space\", \"Left Shift\"): press, release or tap; or an action "
                       "by name (input_names), pressed for a frame.",
        "inputSchema": schema({"key": {"type": "string"}, "action": {"type": "string"},
                               "how": {"type": "string", "enum": ["press", "release", "tap"]}}),
        "query": "input.key",
        "params": ["key", "action", "how"],
    },
    {
        "name": "input_wheel",
        "description": "Turns the mouse wheel some notches (positive away from the player).",
        "inputSchema": schema({"notches": {"type": "integer"}}, ["notches"]),
        "query": "input.wheel",
        "params": ["notches"],
    },
    {
        "name": "input_drag",
        "description": "Holds a button and moves the pointer evenly to a point over some frames (30 by default), then "
                       "lets go.",
        "inputSchema": schema({"to": POINT_SCHEMA, "from": POINT_SCHEMA, "button": {"type": "string"},
                               "frames": {"type": "integer"}}, ["to"]),
        "query": "input.drag",
        "params": ["to", "from", "button", "frames"],
    },
    {
        "name": "input_path",
        "description": "Moves the pointer through pixels, frames each, with a button held throughout or none.",
        "inputSchema": schema({"points": {"type": "array", "items": {"type": "array"}}, "button": {"type": "string"},
                               "frames": {"type": "integer"}}, ["points"]),
        "query": "input.path",
        "params": ["points", "button", "frames"],
    },
    {
        "name": "input_gesture",
        "description": "Draws a gesture (input_names) through the recogniser as the hand would.",
        "inputSchema": schema({"name": {"type": "string"}, "hold_action": {"type": "boolean"}}, ["name"]),
        "query": "input.gesture",
        "params": ["name", "hold_action"],
    },
    {
        "name": "input_release",
        "description": "Lets go of every key and button and hands the pointer back; forgets input still to come.",
        "inputSchema": schema(),
        "query": "input.release",
    },
    {
        "name": "input_lock",
        "description": "Keeps the player's own mouse and keyboard out of the game: mode locked (always, the default) "
                       "or auto (while an inspector client is connected, as the game starts). Ctrl+Alt+Shift+F12 "
                       "on the keyboard always gives the game back to the human.",
        "inputSchema": schema({"mode": {"type": "string", "enum": ["locked", "auto"]}}),
        "query": "input.lock",
        "params": ["mode"],
    },
    {
        "name": "input_unlock",
        "description": "Lets the player's own mouse and keyboard into the game.",
        "inputSchema": schema(),
        "query": "input.unlock",
    },
    {
        "name": "input_record",
        "description": "start records the input made, by frame; stop answers the recording for input_replay.",
        "inputSchema": schema({"action": {"type": "string", "enum": ["start", "stop"]}}, ["action"]),
        "query": "input.record",
        "params": ["action"],
    },
    {
        "name": "input_replay",
        "description": "Makes a recording (input_record's events) again, frame for frame, from this frame on.",
        "inputSchema": schema({"events": {"type": "array", "items": {"type": "object"}}}, ["events"]),
        "query": "input.replay",
        "params": ["events"],
    },
    {
        "name": "inspector_writes",
        "description": "The last changes made through the inspector, newest first, and whether each was made.",
        "inputSchema": schema(SHAPING),
        "query": "writes",
    },
    # The main systems' named queries; every other system is reached through inspector_query (see describe)
    {
        "name": "physics_body",
        "description": "One object's body in the physics: place, velocity, mass, resting, who threw it.",
        "inputSchema": schema({"id": {"type": "integer"}, **SHAPING}, ["id"]),
        "query": "physics.body",
        "params": ["id"],
    },
    {
        "name": "physics_bodies",
        "description": "The physics' bodies within a radius of a point, nearest first.",
        "inputSchema": schema({**NEAR, **SHAPING}, ["near", "radius"]),
        "query": "physics.bodies",
    },
    {
        "name": "creature_desires",
        "description": "A creature's strongest desires (top 5 by default); id may be left out with one creature.",
        "inputSchema": schema({"id": {"type": "integer"}, "top": {"type": "integer"}}),
        "query": "creature.desires",
        "params": ["id", "top"],
    },
    {
        "name": "creature_plan",
        "description": "A creature's plan (desire, action, object, priority), activity and next agenda steps.",
        "inputSchema": schema({"id": {"type": "integer"}, **SHAPING}),
        "query": "creature.plan",
        "params": ["id"],
    },
    {
        "name": "map_cell",
        "description": "What stands in a map cell (10 by 10), as a search meets it: by a point in it or its cell.",
        "inputSchema": schema({"position": {"type": "array", "items": {"type": "number"}},
                               "cell": {"type": "array", "items": {"type": "integer"}}, **SHAPING}),
        "query": "map.cell",
        "params": ["position", "cell"],
    },
    {
        "name": "town_homes",
        "description": "A town's buildings with who lives in each (town ids from town.list).",
        "inputSchema": schema({"id": {"type": "integer"}, **SHAPING}, ["id"]),
        "query": "town.homes",
        "params": ["id"],
    },
    {
        "name": "town_homeless",
        "description": "A town's people without a home.",
        "inputSchema": schema({"id": {"type": "integer"}, **SHAPING}, ["id"]),
        "query": "town.homeless",
        "params": ["id"],
    },
    {
        "name": "influence_hand",
        "description": "The hand's share of its player's influence: the influence where the hand is, at its own "
                       "place and the player's own, and whether the hand is inside.",
        "inputSchema": schema({"player": {"type": "integer"}}),
        "query": "influence.hand",
        "params": ["player"],
    },
    {
        "name": "particles_emitters",
        "description": "The running particle effects that follow an object (its entity id).",
        "inputSchema": schema({"owner": {"type": "integer"}, **SHAPING}, ["owner"]),
        "query": "particles.emitters",
        "params": ["owner"],
    },
    {
        "name": "camera_state",
        "description": "Where the camera is (origin), what it looks at (focus), its yaw, pitch (degrees below the "
                       "horizon) and distance, field of view, what moves it (world, creature, fight, temple, editor) "
                       "and whether a camera path holds it.",
        "inputSchema": schema(SHAPING),
        "query": "camera.state",
    },
    {
        "name": "camera_set",
        "description": "Puts the camera somewhere at once. Give position and focus, or a focus ([x, z] on the land) "
                       "with any of yaw/pitch/distance, or angles alone to turn about the focus; what isn't given is "
                       "kept. Yaw 0 looks along +z, 90 along +x.",
        "inputSchema": schema(CAMERA_POSE),
        "query": "camera.set",
        "params": list(CAMERA_POSE),
    },
    {
        "name": "camera_fly",
        "description": "Flies the camera somewhere as the bookmarks fly it (same parameters as camera_set).",
        "inputSchema": schema(CAMERA_POSE),
        "query": "camera.fly",
        "params": list(CAMERA_POSE),
    },
    {
        "name": "screenshot",
        "description": "A PNG of the screen at an exact frame (this one by default; in_frames or at_frame for later), "
                       "the camera placed first if camera is given (as camera_set takes it). Answers the path and frame; "
                       "the file is written once that frame is drawn, so step to it (game_step) when paused.",
        "inputSchema": schema({"path": {"type": "string"}, "in_frames": {"type": "integer"},
                               "at_frame": {"type": "integer"},
                               "camera": {"type": "object", "properties": CAMERA_POSE}}),
        "query": "screenshot.take",
        "params": ["path", "in_frames", "at_frame", "camera"],
    },
    {
        "name": "gui_windows",
        "description": "The debug windows and the game's own (menu with its page and buttons, cave): name, open.",
        "inputSchema": schema(SHAPING),
        "query": "gui.windows",
    },
    {
        "name": "gui_open",
        "description": "Opens a window by name (gui_windows); the menu opens with Escape as the player's does.",
        "inputSchema": schema({"window": {"type": "string"}}, ["window"]),
        "query": "gui.open",
        "params": ["window"],
    },
    {
        "name": "gui_close",
        "description": "Closes a window by name.",
        "inputSchema": schema({"window": {"type": "string"}}, ["window"]),
        "query": "gui.close",
        "params": ["window"],
    },
    {
        "name": "gui_press",
        "description": "Presses a button by its label: a debug window's (made at its next frame), or the game menu's "
                       "main page (clicked through the game's input). A debug button inside a table row or other "
                       "scope needs path: the labels and numbers its window pushed before it.",
        "inputSchema": schema({"window": {"type": "string"}, "button": {"type": "string"},
                               "path": {"type": "array"}}, ["window", "button"]),
        "query": "gui.press",
        "params": ["window", "button", "path"],
    },
    {
        "name": "level_list",
        "description": "The story lands and playgrounds the game can load, by name.",
        "inputSchema": schema(SHAPING),
        "query": "level.list",
    },
    {
        "name": "level_load",
        "description": "Loads a land by name: how=fresh (default) as the land menu does, scripts restarted; how=story "
                       "through the story's own change of land, scripts going on. Answers once loaded.",
        "inputSchema": schema({"name": {"type": "string"}, "how": {"type": "string", "enum": ["fresh", "story"]}},
                              ["name"]),
        "query": "level.load",
        "params": ["name", "how"],
    },
    {
        "name": "level_testbed",
        "description": "Loads the empty creature testbed.",
        "inputSchema": schema(),
        "query": "level.testbed",
    },
    {
        "name": "script_scripts",
        "description": "The land's scripts whose names or files hold a text: name, file, type, parameters.",
        "inputSchema": schema({"name": {"type": "string"}, **SHAPING}),
        "query": "script.scripts",
        "params": ["name"],
    },
    {
        "name": "script_run",
        "description": "Starts a script or challenge by name; answers its task number (script.tasks lists tasks).",
        "inputSchema": schema({"name": {"type": "string"}}, ["name"]),
        "query": "script.run",
        "params": ["name"],
    },
    {
        "name": "script_globals",
        "description": "The scripts' global variables whose names hold a text: name, type, value.",
        "inputSchema": schema({"name": {"type": "string"}, **SHAPING}),
        "query": "script.globals",
        "params": ["name"],
    },
    {
        "name": "script_set_global",
        "description": "Sets a global variable by exact name, keeping its type.",
        "inputSchema": schema({"name": {"type": "string"}, "value": {}}, ["name", "value"]),
        "query": "script.set_global",
        "params": ["name", "value"],
    },
    {
        "name": "script_call",
        "description": "Calls a script native by name or number as a script would, with args in order: numbers, "
                       "true/false, [x, y, z], {\"object\": id}, {\"int\": n}. Answers what it gave back.",
        "inputSchema": schema({"native": {}, "args": {"type": "array"}}, ["native"]),
        "query": "script.call",
        "params": ["native", "args"],
    },
    {
        "name": "audio_sounds",
        "description": "The sounds playing within a radius of a point, nearest first.",
        "inputSchema": schema({**NEAR, **SHAPING}, ["near", "radius"]),
        "query": "audio.sounds",
    },
]
# The tools choosing which running game this session talks to; they are answered by the adapter itself
GAME_TOOLS = [
    {
        "name": "inspector_games",
        "description": "The openblack games running on this machine with their inspector on: pid, port, worktree, "
                       "build_type, land, started, whether each answers (responding), whether it is ready (not "
                       "loading), and whether inspector_connect chose it. Games are found by the files they keep in "
                       "the temporary directory; files of games that have gone are removed. Looking doesn't lock any "
                       "game's input.",
        "inputSchema": schema(),
    },
    {
        "name": "inspector_connect",
        "description": "Waits (up to 30 s) until the game named by worktree (its path, a path inside it, or its folder "
                       "name), pid or port answers that it is ready, and answers it, or why not. It becomes the game "
                       "for calls naming none only while it is the one game running: the session is shared by every "
                       "agent, so with several games running every call must name its game (port, pid or worktree).",
        "inputSchema": schema({"worktree": {"type": "string"}, "pid": {"type": "integer"},
                               "port": {"type": "integer"}}),
    },
]
# Every game tool names the game it goes to: one MCP session is shared by every agent using it, so a game chosen
# once for the session (inspector_connect) could be another agent's by the time a call is made. With several games
# running, a call without one of these is refused.
SELECTOR = {
    "port": {"type": "integer", "description": "The game to ask, by its inspector's port"},
    "pid": {"type": "integer", "description": "The game to ask, by its process id"},
    "worktree": {"type": "string",
                 "description": "The game to ask, by its worktree: path, a path inside it, or folder name"},
}
SELECTOR_KEYS = tuple(SELECTOR)
for _tool in TOOLS:
    # A copy: several tools share their properties' dictionaries
    _tool["inputSchema"]["properties"] = {**_tool["inputSchema"]["properties"], **SELECTOR}
TOOLS_BY_NAME = {tool["name"]: tool for tool in TOOLS}
SHAPING_KEYS = set(SHAPING) | set(NEAR)
# A connection nothing has used for this long is closed, so that a game an agent has finished with lets its player's
# input in again
IDLE_SECONDS = 60.0


def discovery_folder():
    """Where running games keep their files: openblack-inspector in the temporary directory, as the game has it"""
    return os.path.join(tempfile.gettempdir(), "openblack-inspector")


def pid_alive(pid):
    """Whether a process of that id runs now"""
    if not isinstance(pid, int) or pid <= 0:
        return False
    if os.name == "nt":
        import ctypes
        from ctypes import wintypes
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.OpenProcess.restype = wintypes.HANDLE
        kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
        process_query_limited_information = 0x1000
        still_active = 259
        handle = kernel32.OpenProcess(process_query_limited_information, False, pid)
        if not handle:
            # A process of another user can't be opened but is there
            return ctypes.get_last_error() == 5
        try:
            code = wintypes.DWORD()
            return bool(kernel32.GetExitCodeProcess(handle, ctypes.byref(code))) and code.value == still_active
        finally:
            kernel32.CloseHandle(handle)
    try:
        os.kill(pid, 0)
    except PermissionError:
        return True
    except OSError:
        return False
    return True


def read_games(folder, alive=pid_alive):
    """The games whose files are in the folder and whose processes run, by pid. Files of games that have gone are
    removed; files that aren't a game's, or can't be read, are left."""
    games = []
    try:
        names = os.listdir(folder)
    except OSError:
        return games
    for name in names:
        stem, extension = os.path.splitext(name)
        if extension != ".json" or not stem.isdigit() or int(stem) == 0:
            continue
        pid = int(stem)
        path = os.path.join(folder, name)
        if not alive(pid):
            try:
                os.remove(path)
            except OSError:
                pass
            continue
        try:
            with open(path, encoding="utf-8") as file:
                record = json.load(file)
        except (OSError, ValueError):
            continue
        if not isinstance(record, dict) or record.get("pid") != pid or not isinstance(record.get("port"), int):
            continue
        games.append(record)
    games.sort(key=lambda game: game["pid"])
    return games


def read_line(connection, buffer, wanted, timeout):
    """The answer of that id from a socket, and what is left of the buffer; None when the time is up"""
    deadline = time.monotonic() + timeout
    while True:
        end = buffer.find(b"\n")
        if end >= 0:
            line, buffer = buffer[:end], buffer[end + 1:]
            try:
                answer = json.loads(line.decode("utf-8", errors="replace"))
            except ValueError:
                continue
            if isinstance(answer, dict) and answer.get("id") == wanted:
                return answer, buffer
            continue
        left = deadline - time.monotonic()
        if left <= 0:
            return None, buffer
        connection.settimeout(left)
        try:
            chunk = connection.recv(65536)
        except socket.timeout:
            return None, buffer
        if not chunk:
            raise OSError("the game closed the connection")
        buffer += chunk


def ping(port, timeout):
    """What the game on a port says of itself, None if nothing answers. A ping doesn't lock the game's input."""
    try:
        with socket.create_connection(("127.0.0.1", port), timeout=timeout) as connection:
            connection.sendall(b'{"id": 0, "query": "ping"}\n')
            answer, _ = read_line(connection, b"", 0, timeout)
    except OSError:
        return None
    if answer is None or not answer.get("ok") or not isinstance(answer.get("result"), dict):
        return None
    return answer["result"]


def is_ready(identity):
    """Whether a ping's answer says the game serves its frames: older games don't say, and answer only when they do"""
    return identity is not None and identity.get("ready", True) is not False


def normalise_path(path):
    return os.path.normcase(os.path.normpath(path.replace("\\", "/")))


def worktree_matches(game_worktree, wanted):
    """A game's worktree is wanted by its path, a path inside it, or its folder's name"""
    if not game_worktree or not wanted:
        return False
    game_path = normalise_path(game_worktree)
    wanted_path = normalise_path(wanted)
    if wanted_path == game_path or wanted_path.startswith(game_path.rstrip(os.sep) + os.sep):
        return True
    return os.path.normcase(os.path.basename(game_path)) == os.path.normcase(wanted.strip("/\\"))


def describe_game(game):
    return f"pid {game.get('pid', '?')} port {game['port']} worktree {game.get('worktree') or '?'} " \
           f"land {game.get('land') or '-'}"


def game_tag(game):
    """How an answer names the game it came from"""
    return {key: game[key] for key in ("pid", "port", "worktree") if game.get(key) is not None}


def select_game(games, port=None, pid=None, worktree=None):
    """The one game a selection names among those running, or why there is none"""
    candidates = games
    if port is not None:
        candidates = [game for game in candidates if game["port"] == port]
    if pid is not None:
        candidates = [game for game in candidates if game["pid"] == pid]
    if worktree is not None:
        candidates = [game for game in candidates if worktree_matches(game.get("worktree", ""), worktree)]
    wanted = ", ".join(f"{key} {value}" for key, value in (("port", port), ("pid", pid), ("worktree", worktree))
                       if value is not None)
    if not candidates:
        running = "; ".join(describe_game(game) for game in games) or "none"
        return None, f"no running game with {wanted}; running: {running}"
    if len(candidates) > 1:
        listed = "; ".join(describe_game(game) for game in candidates)
        return None, f"{len(candidates)} games match {wanted}, choose one by pid: {listed}"
    return candidates[0], None


class GameError(ConnectionError):
    """A call that couldn't be made to the game it names, or whose answer came from another"""


class InspectorConnection:
    """One line in, one line out, to one game. The socket is reopened whenever the game has gone; each time, a ping on
    it checks that the game answering is the one meant (a port can be reused by a game started later), so that no
    answer comes from another game. Answers name the game they came from, as newer games do themselves."""

    def __init__(self, game, timeout, alive=pid_alive, load_timeout=None, describe=None):
        # The game meant: its port, and its pid and worktree when known
        self.game = dict(game)
        self.port = game["port"]
        self.timeout = timeout
        self.load_timeout = load_timeout if load_timeout is not None else max(60.0, timeout * 6)
        self.alive = alive
        self.describe = describe
        self.socket = None
        self.buffer = b""
        self.next_id = 1
        self.used = time.monotonic()

    def close(self):
        if self.socket is not None:
            self.socket.close()
        self.socket = None
        self.buffer = b""

    def tag(self):
        return game_tag(self.game)

    def _process_alive(self):
        return self.game.get("pid") is not None and self.alive(self.game["pid"])

    def _open(self):
        try:
            self.socket = socket.create_connection(("127.0.0.1", self.port), timeout=self.timeout)
        except OSError as error:
            hint = self.describe() if self.describe is not None else ""
            raise GameError(f"no inspector on 127.0.0.1:{self.port} ({error}); start the game with --inspect-port 0 "
                            f"in a build with OPENBLACK_INSPECTOR{hint}") from error
        # Which game this is: a ping takes no control of it. A game busy loading (an older one, which can't answer
        # meanwhile) is waited for while its process runs.
        self.socket.sendall(b'{"id": 0, "query": "ping"}\n')
        answer, self.buffer = self._wait(0)
        identity = answer.get("result") if answer.get("ok") else None
        pid = identity.get("pid") if isinstance(identity, dict) else None
        if self.game.get("pid") is not None and pid is not None and pid != self.game["pid"]:
            self.close()
            raise GameError(f"port {self.port} is now answered by another game (pid {pid}), not pid "
                            f"{self.game['pid']}; inspector_games lists the running games")
        if pid is not None:
            self.game["pid"] = pid
            if isinstance(identity.get("worktree"), str) and identity["worktree"]:
                self.game["worktree"] = identity["worktree"]

    def _wait(self, wanted):
        """The answer of that id: waits longer while the game's process runs, as it may be loading a land"""
        answer, self.buffer = read_line(self.socket, self.buffer, wanted, self.timeout)
        if answer is None and self._process_alive():
            answer, self.buffer = read_line(self.socket, self.buffer, wanted,
                                            max(0.0, self.load_timeout - self.timeout))
        if answer is None:
            self.close()
            waited = self.load_timeout if self._process_alive() else self.timeout
            raise GameError(f"the game ({describe_game(self.game)}) didn't answer in {waited:.0f} s: busy loading, "
                            f"or stuck (a dialog?)")
        return answer, self.buffer

    def request(self, request):
        self.used = time.monotonic()
        request = dict(request)
        request["id"] = self.next_id
        self.next_id += 1
        line = (json.dumps(request) + "\n").encode("utf-8")
        for attempt in range(2):
            try:
                if self.socket is None:
                    self._open()
                self.socket.sendall(line)
                answer, self.buffer = self._wait(request["id"])
                break
            except GameError:
                raise
            except OSError as error:
                # A socket left from a game that has gone: open it again once
                self.close()
                if attempt == 1:
                    raise GameError(f"lost the game ({describe_game(self.game)}): {error}") from error
        answered_by = answer.get("game")
        if isinstance(answered_by, dict) and self.game.get("pid") is not None and \
                answered_by.get("pid") not in (None, self.game["pid"]):
            raise GameError(f"an answer came from another game (pid {answered_by.get('pid')}) than the one asked "
                            f"({describe_game(self.game)}); nothing was taken from it")
        if not isinstance(answered_by, dict):
            # An older game doesn't name itself: the adapter does, from the ping it checked
            answer["game"] = self.tag()
        return answer


class Session:
    """The games this adapter talks to. Every call finds its game anew from what it names, so that agents sharing the
    session never get each other's game: with several games running, a call naming none is refused."""

    def __init__(self, default_port, timeout, folder=None, alive=pid_alive, ping_timeout=2.0, ready_timeout=30.0,
                 load_timeout=None):
        self.default_port = default_port
        self.timeout = timeout
        self.folder = folder or discovery_folder()
        self.alive = alive
        self.ping_timeout = ping_timeout
        self.ready_timeout = ready_timeout
        self.load_timeout = load_timeout if load_timeout is not None else max(60.0, timeout * 6)
        # What inspector_connect chose ({"port"|"pid"|"worktree": value}), used only while it can't be mistaken
        self.selection = None
        # Chosen by whoever started the adapter (its command line): it is that caller's own, so always followed
        self.pinned = None
        # Open connections by game, kept so that a game driven call after call stays locked to its agent
        self.connections = {}

    def close(self):
        for connection in self.connections.values():
            connection.close()
        self.connections = {}

    def _running_hint(self):
        games = read_games(self.folder, self.alive)
        if not games:
            return ""
        return "; running games: " + "; ".join(describe_game(game) for game in games)

    def _connection(self, game):
        """The open connection to a game, made if there is none; a game restarted on another port gets a new one"""
        key = game.get("pid") or ("port", game["port"])
        connection = self.connections.get(key)
        if connection is not None and connection.port != game["port"]:
            connection.close()
            connection = None
        if connection is None:
            connection = InspectorConnection(game, self.timeout, alive=self.alive, load_timeout=self.load_timeout,
                                             describe=self._running_hint)
            self.connections[key] = connection
        return connection

    def _close_idle(self):
        now = time.monotonic()
        for key, connection in list(self.connections.items()):
            if now - connection.used > IDLE_SECONDS:
                connection.close()
                del self.connections[key]

    def find(self, selection):
        """The game a selection names, or why there is none. A port that no file names is still a game when something
        answers a ping on it (a game too old to keep a file)."""
        games = read_games(self.folder, self.alive)
        game, error = select_game(games, **selection)
        if game is None and set(selection) == {"port"}:
            identity = ping(selection["port"], self.ping_timeout)
            if identity is None:
                return None, error
            game = {"port": selection["port"], "pid": identity.get("pid"), "worktree": identity.get("worktree")}
        return game, error

    def target(self, arguments):
        """The game a call goes to: the one it names; else the one the adapter was started for; else the only one
        running; else the connected or default port when no game keeps a file. Several running and none named: why
        it was refused."""
        selection = {key: arguments[key] for key in SELECTOR_KEYS if arguments.get(key) is not None}
        if not selection and self.pinned is not None:
            selection = self.pinned
        if selection:
            return self.find(selection)
        games = read_games(self.folder, self.alive)
        if len(games) > 1:
            listed = "; ".join(describe_game(game) for game in games)
            return None, (f"{len(games)} games are running: name the one meant with port, pid or worktree in every "
                          f"call (the session is shared by every agent, so inspector_connect doesn't choose for "
                          f"you). Running: {listed}")
        if len(games) == 1:
            return games[0], None
        if self.selection is not None:
            return self.find(self.selection)
        return {"port": self.default_port}, None

    def games(self):
        """The running games, each with whether it answers, whether it is ready, and whether it is the connected one"""
        games = read_games(self.folder, self.alive)
        connected = self.selection or {}
        for game in games:
            identity = ping(game["port"], self.ping_timeout)
            # A port answering for another process is a reused port, not this game
            game["responding"] = identity is not None and identity.get("pid", game["pid"]) == game["pid"]
            game["ready"] = game["responding"] and is_ready(identity)
            if identity is not None and identity.get("loading"):
                game["loading"] = identity["loading"]
            game["connected"] = bool(connected) and select_game([game], **connected)[0] is not None
        return games

    def wait_until_ready(self, selection):
        """The game a selection names once it answers that it is ready, or why not: a game just started may have no
        file yet, not listen yet, or be loading"""
        deadline = time.monotonic() + self.ready_timeout
        while True:
            game, error = self.find(selection)
            identity = ping(game["port"], self.ping_timeout) if game is not None else None
            if identity is not None and game.get("pid") is not None and identity.get("pid") not in (None, game["pid"]):
                error = f"port {game['port']} is answered by pid {identity.get('pid')}, not {game['pid']}"
            elif is_ready(identity):
                return game, None
            elif game is not None:
                error = f"the game ({describe_game(game)}) " + \
                        (f"is loading {identity['loading']}" if identity is not None and identity.get("loading")
                         else "isn't answering yet")
            if time.monotonic() > deadline:
                return None, f"{error}; waited {self.ready_timeout:.0f} s, try again"
            time.sleep(0.25)

    def connect(self, port=None, pid=None, worktree=None):
        """Chooses the game calls go to while it is the only one running, once it is ready; answers what was chosen,
        or why not"""
        selection = {key: value for key, value in (("port", port), ("pid", pid), ("worktree", worktree))
                     if value is not None}
        if not selection:
            self.selection = None
            return {"ok": True, "result": {"connected": {"port": self.default_port}, "default": True}}
        game, error = self.wait_until_ready(selection)
        if game is None:
            return {"ok": False, "error": error}
        self.selection = selection
        result = {"connected": game}
        running = read_games(self.folder, self.alive)
        if len(running) > 1:
            result["note"] = (f"{len(running)} games are running: every call must still name its game (port, pid or "
                              f"worktree), because this session is shared by every agent")
        return {"ok": True, "result": result, "game": game_tag(game)}

    def call(self, name, arguments):
        if name == "inspector_games":
            return {"ok": True, "result": {"games": self.games(), "selection": self.selection}}
        if name == "inspector_connect":
            return self.connect(arguments.get("port"), arguments.get("pid"), arguments.get("worktree"))
        tool = TOOLS_BY_NAME.get(name)
        if tool is None:
            return {"ok": False, "error": f"no tool {name}"}
        return self.send(build_request(tool, arguments), wait_step=name == "game_step" and arguments.get("wait", True),
                         target=arguments)

    def send(self, request, wait_step=False, target=None):
        """A request to the game a call names, waiting while the game loads; the answer names the game"""
        self._close_idle()
        game, error = self.target(target or {})
        if game is None:
            return {"ok": False, "error": error}
        connection = self._connection(game)
        answer = request_until_loaded(connection, request, self.load_timeout)
        # A step waits for the game to have run it, polling the state as the game serves a request each frame
        if wait_step and answer.get("ok"):
            deadline = time.monotonic() + self.timeout * 4
            while answer.get("ok") and isinstance(answer.get("result"), dict) and \
                    answer["result"].get("stepping") is not None and time.monotonic() < deadline:
                time.sleep(0.02)
                answer = request_until_loaded(connection, {"query": "game.state"}, self.load_timeout)
        return answer


def request_until_loaded(connection, request, load_timeout):
    """A request asked again while the game answers that it is loading, until it is done or the time is up"""
    deadline = time.monotonic() + load_timeout
    while True:
        answer = connection.request(request)
        if answer.get("ok") or not answer.get("loading"):
            return answer
        if time.monotonic() > deadline:
            answer["error"] = (f"the game is still loading {answer['loading']} after {load_timeout:.0f} s; "
                               f"ask again later")
            return answer
        time.sleep(0.25)


def build_request(tool, arguments):
    if tool["name"] == "inspector_query":
        request = {"query": arguments.get("query", "")}
        if "params" in arguments:
            request["params"] = arguments["params"]
    else:
        request = {"query": tool["query"]}
        params = {key: arguments[key] for key in tool.get("params", []) if key in arguments}
        if params:
            request["params"] = params
    for key in SHAPING_KEYS:
        if key in arguments:
            request[key] = arguments[key]
    return request


def tool_result(answer):
    """The MCP content of an answer: the result, or the error, with the game it came from"""
    game = answer.get("game")
    if answer.get("ok"):
        content = {"game": game, "result": answer["result"]} if game else answer["result"]
        return {"content": [{"type": "text", "text": json.dumps(content, separators=(",", ":"))}]}
    error = answer.get("error", "failed")
    if game:
        error += f" (game: {json.dumps(game, separators=(',', ':'))})"
    return {"content": [{"type": "text", "text": error}], "isError": True}


def serve(session):
    """The MCP side: JSON-RPC 2.0 messages, one per line on stdin and stdout"""
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            message = json.loads(line)
        except json.JSONDecodeError:
            continue
        method = message.get("method")
        message_id = message.get("id")
        if message_id is None:
            # Notifications, such as notifications/initialized, are not answered
            continue
        try:
            if method == "initialize":
                version = message.get("params", {}).get("protocolVersion", PROTOCOL_VERSION)
                result = {
                    "protocolVersion": version,
                    "capabilities": {"tools": {}},
                    "serverInfo": {"name": "openblack-inspector", "version": "1.2.0"},
                }
            elif method == "tools/list":
                result = {"tools": [{key: tool[key] for key in ("name", "description", "inputSchema")}
                                    for tool in GAME_TOOLS + TOOLS]}
            elif method == "tools/call":
                params = message.get("params", {})
                try:
                    answer = session.call(params.get("name", ""), params.get("arguments") or {})
                except ConnectionError as error:
                    answer = {"ok": False, "error": str(error)}
                result = tool_result(answer)
            elif method == "ping":
                result = {}
            else:
                reply = {"jsonrpc": "2.0", "id": message_id,
                         "error": {"code": -32601, "message": f"no method {method}"}}
                print(json.dumps(reply), flush=True)
                continue
            print(json.dumps({"jsonrpc": "2.0", "id": message_id, "result": result}), flush=True)
        except Exception as error:  # noqa: BLE001 - the server must answer whatever went wrong
            reply = {"jsonrpc": "2.0", "id": message_id, "error": {"code": -32603, "message": str(error)}}
            print(json.dumps(reply), flush=True)


def main():
    parser = argparse.ArgumentParser(description="MCP server for openblack's debug inspector")
    parser.add_argument("--port", type=int, default=None,
                        help="The game talked to when no game keeps a discovery file (47800 by default, or "
                             "OPENBLACK_INSPECT_PORT); with --call, the game asked")
    parser.add_argument("--worktree", help="Talk to the game running from this worktree (path or folder name)")
    parser.add_argument("--pid", type=int, help="Talk to the game of this process")
    parser.add_argument("--timeout", type=float, default=10.0, help="Seconds to wait for the game to answer")
    parser.add_argument("--games", action="store_true", help="List the running games and exit")
    parser.add_argument("--call", metavar="QUERY", help="Send one query and print the answer, without MCP")
    parser.add_argument("request", nargs="?", default="{}", help="With --call: the rest of the request as JSON")
    args = parser.parse_args()

    default_port = args.port if args.port is not None else int(os.environ.get("OPENBLACK_INSPECT_PORT", DEFAULT_PORT))
    session = Session(default_port, args.timeout)
    # Chosen on the command line, the game is this caller's own: every call goes to it. As an MCP server shared by
    # agents, --port only names the game of builds too old to keep a discovery file.
    pinned = {key: value for key, value in (("worktree", args.worktree), ("pid", args.pid)) if value is not None}
    if args.call and args.port is not None:
        pinned["port"] = args.port
    if pinned:
        session.pinned = pinned
    if args.games:
        print(json.dumps(session.games(), indent=1))
        return 0
    if args.call:
        request = json.loads(args.request)
        request["query"] = args.call
        try:
            answer = session.send(request)
        except ConnectionError as error:
            print(error, file=sys.stderr)
            return 1
        print(json.dumps(answer, indent=1))
        return 0 if answer.get("ok") else 1
    try:
        serve(session)
    finally:
        session.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
