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

Run it with --call QUERY [JSON] to send a single request from a shell, without MCP:

    python openblack_inspector_mcp.py --port 47800 --call sky.moon
    python openblack_inspector_mcp.py --call objects.find '{"params": {"component": "Tree"}, "near": [0, 0], "radius": 50}'
"""

import argparse
import json
import os
import socket
import sys
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
TOOLS_BY_NAME = {tool["name"]: tool for tool in TOOLS}
SHAPING_KEYS = set(SHAPING) | set(NEAR)


class InspectorConnection:
    """One line in, one line out, over a socket that is reopened whenever the game has gone"""

    def __init__(self, port, timeout):
        self.port = port
        self.timeout = timeout
        self.socket = None
        self.buffer = b""
        self.next_id = 1

    def close(self):
        if self.socket is not None:
            self.socket.close()
        self.socket = None
        self.buffer = b""

    def request(self, request):
        request = dict(request)
        request["id"] = self.next_id
        self.next_id += 1
        line = (json.dumps(request) + "\n").encode("utf-8")
        for attempt in range(2):
            try:
                if self.socket is None:
                    self.socket = socket.create_connection(("127.0.0.1", self.port), timeout=self.timeout)
                self.socket.sendall(line)
                return self._read_answer(request["id"])
            except OSError as error:
                self.close()
                if attempt == 1:
                    raise ConnectionError(
                        f"no inspector on 127.0.0.1:{self.port} ({error}); start the game with --inspect-port "
                        f"{self.port} in a build with OPENBLACK_INSPECTOR") from error

    def _read_answer(self, wanted):
        # The game answers at its next frame, which may be slow while it loads
        deadline = time.monotonic() + self.timeout
        while True:
            end = self.buffer.find(b"\n")
            if end >= 0:
                line, self.buffer = self.buffer[:end], self.buffer[end + 1:]
                answer = json.loads(line.decode("utf-8", errors="replace"))
                if answer.get("id") == wanted:
                    return answer
                continue
            if time.monotonic() > deadline:
                raise OSError("the game didn't answer in time")
            chunk = self.socket.recv(65536)
            if not chunk:
                raise OSError("the game closed the connection")
            self.buffer += chunk


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


def call_tool(connection, name, arguments):
    tool = TOOLS_BY_NAME.get(name)
    if tool is None:
        return {"ok": False, "error": f"no tool {name}"}
    answer = connection.request(build_request(tool, arguments))
    # A step waits for the game to have run it, polling the state as the game serves a request each frame
    if name == "game_step" and answer.get("ok") and arguments.get("wait", True):
        deadline = time.monotonic() + connection.timeout * 4
        while answer.get("ok") and answer["result"].get("stepping") is not None and time.monotonic() < deadline:
            time.sleep(0.02)
            answer = connection.request({"query": "game.state"})
    return answer


def tool_result(answer):
    if answer.get("ok"):
        return {"content": [{"type": "text", "text": json.dumps(answer["result"], separators=(",", ":"))}]}
    return {"content": [{"type": "text", "text": answer.get("error", "failed")}], "isError": True}


def serve(connection):
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
                    "serverInfo": {"name": "openblack-inspector", "version": "1.0.0"},
                }
            elif method == "tools/list":
                result = {"tools": [{key: tool[key] for key in ("name", "description", "inputSchema")}
                                    for tool in TOOLS]}
            elif method == "tools/call":
                params = message.get("params", {})
                try:
                    answer = call_tool(connection, params.get("name", ""), params.get("arguments") or {})
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
    parser.add_argument("--port", type=int, default=int(os.environ.get("OPENBLACK_INSPECT_PORT", DEFAULT_PORT)))
    parser.add_argument("--timeout", type=float, default=10.0, help="Seconds to wait for the game to answer")
    parser.add_argument("--call", metavar="QUERY", help="Send one query and print the answer, without MCP")
    parser.add_argument("request", nargs="?", default="{}", help="With --call: the rest of the request as JSON")
    args = parser.parse_args()

    connection = InspectorConnection(args.port, args.timeout)
    if args.call:
        request = json.loads(args.request)
        request["query"] = args.call
        try:
            print(json.dumps(connection.request(request), indent=1))
        except ConnectionError as error:
            print(error, file=sys.stderr)
            return 1
        return 0
    serve(connection)
    return 0


if __name__ == "__main__":
    sys.exit(main())
