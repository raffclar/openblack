"""Makes the recorded hand input of server_check.py --input: the moves, the presses and the releases of the hand.

The input is a list of {frame, event} as the inspector's input.record answers it. The frames count from the frame
that input.replay starts at, with frames of 20 ms. The pixels are of a 1280 by 1024 window and of the camera of the
first frame of the land. An input can also be an object with that list as "events" and a "camera" as camera.set takes
it: the game puts its camera there before the first frame of the land.

    python tools/sim/hand_inputs/make_inputs.py

writes land1.json, land1_camera.json (the same input with the camera turned), testbed.json, twogods.json, twogods_totem.json, strokes.json, twogods_scoop_throw.json, gods_ring_slot1_scoop_throw.json and casts.json and twogods_slot1_tree_press.json beside this script. strokes.json
plays on the testbed scenario hand.recorded_strokes (server_check.py --scenario hand.recorded_strokes), and casts.json on
miracles.recorded_casts. The twogods inputs play on the
playground Playgrounds/TwoGods.txt (server_check.py --land Playgrounds/TwoGods.txt), whose first town is the player's.

fight.json is not made here: it is the recording of the fight autopilot on the testbed scenario combat.recorded_presses,
which the server plays too. To record it again:

    python tools/sim/server_check.py --scenario combat.recorded_presses --turns 400 --frame-ms 20 --server-fast
        --hash-parts --input <a file with one pointer event, [640, 900] at frame 0> --game-args "--fight-autopilot
        sequence --fight-autopilot-sequence 1:high,2.5:block,4:step_forward,5.5:middle,7:step_back,8.5:low,10:step_left,
        11.5:step_right,13:high --sim-record-input fight.json"
"""

import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ACTION = 3


class Script:
    def __init__(self):
        self.events = []
        self.frame = 0
        self.pointer = (640, 512)

    def add(self, event):
        self.events.append({"frame": self.frame, "event": event})

    def wait(self, frames):
        self.frame += frames

    def move(self, to, frames):
        """Moves the pointer in a straight line, one step a frame"""
        start = self.pointer
        for step in range(1, frames + 1):
            share = step / frames
            point = [round(start[0] + (to[0] - start[0]) * share), round(start[1] + (to[1] - start[1]) * share)]
            self.add({"kind": "pointer", "timing": "mouse", "position": point})
            self.wait(1)
        self.pointer = tuple(to)

    def circle(self, centre, radius, frames, turns=1.0):
        """Moves the pointer around a circle, one step a frame"""
        for step in range(frames + 1):
            angle = 2.0 * math.pi * turns * step / frames
            point = [round(centre[0] + radius * math.cos(angle)), round(centre[1] + radius * math.sin(angle))]
            self.add({"kind": "pointer", "timing": "mouse", "position": point})
            self.wait(1)
            self.pointer = tuple(point)

    def press(self):
        self.add({"kind": "button_down", "timing": "mouse", "button": ACTION})
        self.wait(1)

    def release(self):
        self.add({"kind": "button_up", "timing": "mouse", "button": ACTION})
        self.wait(1)


def land1():
    """Land 1 from the sea: taps a rock, takes a second rock and puts it down, takes a third rock and throws it, taps a bush, and moves
    over the land"""
    script = Script()
    script.wait(10)
    script.move((736, 447), 40)
    # Take the first rock and throw it
    script.press()
    script.wait(60)
    script.move((700, 380), 40)
    script.move((500, 520), 4)
    script.release()
    script.wait(20)
    # Take the second rock and put it down
    script.move((928, 450), 30)
    script.press()
    script.wait(60)
    script.move((809, 497), 50)
    script.wait(15)
    script.release()
    script.wait(20)
    # Take a third rock and throw it
    script.move((973, 451), 25)
    script.press()
    script.wait(60)
    script.move((900, 360), 30)
    script.move((600, 520), 4)
    script.release()
    script.wait(20)
    # Tap the bush
    script.move((976, 463), 25)
    script.press()
    script.wait(3)
    script.release()
    script.wait(20)
    # Move about over the land
    script.circle((700, 520), 120, 160, 2.0)
    script.move((635, 561), 30)
    script.wait(40)
    script.circle((640, 540), 60, 120, 1.0)
    return script.events


def testbed():
    """The testbed: the hand moves over the land, takes a globe from its dispenser and throws it, which casts its
    miracle, and takes a second globe and puts it down on the land"""
    script = Script()
    script.wait(10)
    script.circle((640, 470), 120, 80, 1.0)
    # Take a globe and throw it far over the land
    script.move((656, 551), 30)
    script.press()
    script.wait(60)
    script.move((650, 480), 30)
    script.move((640, 380), 4)
    script.release()
    script.wait(60)
    # Take a second globe and put it down
    script.move((560, 551), 30)
    script.press()
    script.wait(60)
    script.move((479, 440), 60)
    script.wait(20)
    script.release()
    script.wait(40)
    script.circle((560, 470), 80, 80, 1.0)
    return script.events


# The camera of land1_camera.json: turned from where Land 1 starts it and further back, so that the story's scripts read
# a camera that only the player's reports give the server
LAND1_TURNED_CAMERA = {"yaw": 120, "pitch": 40, "distance": 90}


# The camera of twogods.json: 35 m from the store of the player's first town, looking down at 40 degrees
TWO_GODS_STORE_CAMERA = {"focus": [2199.33, 88.44, 2406.03], "yaw": 30, "pitch": 40, "distance": 35}


# The camera of gods_ring_slot1_scoop_throw.json: 35 m from the store of slot 1 on the map for 32 gods, looking down at
# 40 degrees; the food pot to the east of the store is at the pixel of the scoop
GODS_RING_SLOT1_STORE_CAMERA = {"focus": [3028.14, 21.44, 827.47], "yaw": 4, "pitch": 40, "distance": 35}


def two_gods():
    """The player's first town of the playground: the hand scoops from the food pile of the store, lets go of the
    button, and puts the handful into the store"""
    script = Script()
    script.wait(10)
    # Scoop from the food pile for 60 frames
    script.move((712, 437), 20)
    script.wait(10)
    script.press()
    script.wait(60)
    script.release()
    script.wait(10)
    # Put the handful into the store
    script.move((640, 512), 30)
    script.wait(10)
    script.press()
    script.wait(3)
    script.release()
    script.wait(20)
    script.move((600, 600), 20)
    script.wait(20)
    return script.events


# The camera of twogods_totem.json: 40 m from the totem of the player's first town, looking down at 30 degrees
TWO_GODS_TOTEM_CAMERA = {"focus": [2188.23, 93.05, 2317.47], "yaw": 30, "pitch": 30, "distance": 40}


def two_gods_totem():
    """The totem of the player's first town of the playground: the hand takes hold of it, slides it and lets go"""
    script = Script()
    script.wait(10)
    script.move((640, 512), 10)
    script.wait(20)
    script.press()
    script.wait(10)
    # Down the screen: the share stays 0. A slide up (the pointer above the hand, which the game puts the pointer back on
    # each frame) gives a share above 0, but then the frames make the totem's ghost as entities of the world, which the
    # server does not make (tracker row)
    script.move((640, 700), 40)
    script.wait(10)
    script.release()
    script.wait(30)
    script.move((600, 600), 20)
    script.wait(40)
    return script.events


def strokes():
    """The testbed scenario hand.recorded_strokes: the hand holds the player's tiger by its middle, strokes it with slow
    moves up and down, slaps it with fast sweeps across the screen, and lets go"""
    script = Script()
    script.wait(10)
    script.move((640, 512), 10)
    script.wait(40)
    script.press()
    script.wait(30)
    # Slow strokes up and down the body
    for _ in range(3):
        script.move((640, 470), 20)
        script.move((640, 540), 20)
    script.move((640, 512), 10)
    script.wait(20)
    # Fast sweeps across the middle, each a slap
    for across in (900, 380, 900, 380):
        script.move((across, 512), 3)
        script.wait(40)
    script.move((640, 512), 10)
    script.wait(20)
    script.release()
    script.wait(60)
    script.move((640, 900), 20)
    script.wait(40)
    return script.events


def scoop_throw(pile):
    """The pile at a pixel: the hand scoops from it, lets go of the button, then takes a hold over the land below and
    throws the handful far"""
    script = Script()
    script.wait(10)
    script.move(pile, 20)
    script.wait(10)
    # Scoop for 60 frames
    script.press()
    script.wait(60)
    script.release()
    script.wait(20)
    # Over the land below the pile, press, drag and throw
    script.move((300, 900), 30)
    script.wait(10)
    script.press()
    script.wait(40)
    script.move((300, 820), 20)
    script.move((300, 560), 3)
    script.release()
    script.wait(60)
    script.move((600, 600), 20)
    script.wait(20)
    return script.events


# The camera of twogods_slot1_tree_press.json: 25 m from a conifer of the second god's town, looking down at 20 degrees;
# the tree's crown is at the pixel of the press (net_check.py --land Playgrounds/TwoGods.txt --input2 ... --camera2)
TWO_GODS_SLOT1_TREE_CAMERA = {"focus": [2928.87, 88.44, 2620.91], "yaw": 0, "pitch": 20, "distance": 25}


def two_gods_slot1_tree_press():
    """A conifer of the second god's town: a short press on it, let go after the pull started but before the conifer
    comes free, so that it stays leaning (in single player)"""
    script = Script()
    script.wait(10)
    script.move((640, 325), 10)
    script.wait(20)
    # The pull starts after the hand has faded into its pull pose, and a conifer comes free a few frames later: a
    # release 8 frames after the press lets go of it in between, leaning
    script.press()
    script.wait(7)
    script.release()
    script.wait(30)
    script.move((600, 600), 20)
    script.wait(20)
    return script.events


# The gesture that powers up the water miracle of miracles.recorded_casts to its first level
CASTS_POWER_UP = "spiral"


def casts():
    """The seeds of the testbed scenario miracles.recorded_casts: a power-up, a scribble that stops it and a scribble
    that drops the first water miracle; the fireball thrown from the moving hand; the second water miracle held on the land
    while the hand moves, then let go"""
    script = Script()
    script.wait(10)
    script.move((640, 600), 20)
    # The first water miracle comes into the hand at 4.5 s
    script.frame = 350
    script.add({"kind": "gesture", "name": CASTS_POWER_UP, "hold_action": False})
    script.frame = 480
    script.add({"kind": "gesture", "name": "scribble", "hold_action": False})
    script.frame = 610
    script.add({"kind": "gesture", "name": "scribble", "hold_action": False})
    # The fireball miracle comes into the hand at 15 s: the hand arms it, moves and throws it as the button comes up
    script.frame = 760
    script.move((660, 600), 40)
    script.wait(50)
    script.press()
    script.wait(10)
    script.move((640, 450), 6)
    script.release()
    # The second water miracle comes into the hand at 20 s: the hand holds it on the land and moves
    script.frame = 1100
    script.move((660, 600), 20)
    script.press()
    script.move((720, 640), 60)
    script.move((600, 620), 60)
    script.release()
    script.wait(100)
    return script.events


def write(name, events, camera=None):
    # One event a line
    lines = ",\n".join(json.dumps(event) for event in events)
    with open(os.path.join(HERE, name + ".json"), "w", encoding="utf-8", newline="\n") as file:
        if camera is None:
            file.write("[\n" + lines + "\n]\n")
        else:
            file.write('{"camera": ' + json.dumps(camera) + ',\n"events": [\n' + lines + "\n]}\n")
    print(name, len(events), "events,", events[-1]["frame"] + 1, "frames")


def main():
    write("land1", land1())
    write("land1_camera", land1(), LAND1_TURNED_CAMERA)
    write("testbed", testbed())
    write("twogods", two_gods(), TWO_GODS_STORE_CAMERA)
    write("twogods_totem", two_gods_totem(), TWO_GODS_TOTEM_CAMERA)
    write("strokes", strokes())
    write("twogods_scoop_throw", scoop_throw((712, 437)))
    # Slot 1's store on the map for 32 gods (net_check.py --gods-ring-map --camera2 with GODS_RING_SLOT1_STORE_CAMERA)
    write("gods_ring_slot1_scoop_throw", scoop_throw((769, 471)))
    write("casts", casts())
    # The second god's conifer on TwoGods (net_check.py --input2 with --camera2 TWO_GODS_SLOT1_TREE_CAMERA)
    write("twogods_slot1_tree_press", two_gods_slot1_tree_press())


if __name__ == "__main__":
    main()
