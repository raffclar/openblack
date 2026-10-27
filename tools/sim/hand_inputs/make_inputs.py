"""Makes the recorded hand input of server_check.py --input: the moves, the presses and the releases of the hand.

The input is a list of {frame, event} as the inspector's input.record answers it. The frames count from the frame
that input.replay starts at, with frames of 20 ms. The pixels are of a 1280 by 1024 window and of the camera of the
first frame of the land. An input can also be an object with that list as "events" and a "camera" as camera.set takes
it: the game puts its camera there before the first frame of the land, in place of the start view and its flight. The
object can also have an "expect", the least count of each deed of the hand that server_check.py and net_check.py check.

    python tools/sim/hand_inputs/make_inputs.py

writes land1.json, land1_camera.json (the same input with the camera turned), testbed.json, twogods.json,
twogods_totem.json, strokes.json, twogods_scoop_throw.json, gods_ring_slot1_scoop_throw.json, casts.json,
twogods_slot1_tree_press.json, busy_hand.json, scaffold_yard.json and the store inputs of the online maps (<map>_slot<n>_store.json) beside
this script. strokes.json plays on the testbed scenario hand.recorded_strokes
(server_check.py --scenario hand.recorded_strokes), scaffold_yard.json on objects.scaffold_snaps_into_yard and casts.json on miracles.recorded_casts. The twogods inputs play on the
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
    # Up the screen (the pointer above the hand, which the game puts the pointer back on each frame): a share above 0.
    # The frames show the totem's see-through second totem, which the turn made with the totem on every computer.
    script.move((640, 300), 40)
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


def scaffold_take():
    """The testbed scenario objects.scaffold_snaps_into_yard: once the scaffold stands in the first place of the yard, the
    hand takes it with a held press, carries it towards the camera and puts it down with a click. The pixels are of the
    scenario's own camera, which shows the yard in front of the workshop"""
    script = Script()
    script.wait(200)
    script.move((559, 571), 10)
    script.wait(10)
    script.press()
    script.wait(60)
    script.move((559, 760), 30)
    script.release()
    script.wait(20)
    script.press()
    script.release()
    script.wait(60)
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


# The camera of twogods_slot1_tree_press.json: 25 m from a conifer of the second god's town, looking down at 20 degrees;
# the tree's crown is at the pixel of the press (net_check.py --land Playgrounds/TwoGods.txt --input2 ... --camera2)
TWO_GODS_SLOT1_TREE_CAMERA = {"focus": [2928.87, 88.44, 2620.91], "yaw": 0, "pitch": 20, "distance": 25}


def two_gods_slot1_tree_press():
    """A conifer of the second god's town: a short press on it. The pull starts on the frame after the hand has faded
    into its pull pose (0.13 s, 7 frames of 20 ms). The hand is then far from its grip on the trunk, thus the conifer comes
    free on the first frame of the pull, and the hand takes it, also when the button comes up in that frame. The hand of
    the first god cannot take it (out of that god's influence): the press is then a click and the conifer does not
    lean."""
    script = Script()
    script.wait(10)
    script.move((640, 325), 10)
    script.wait(20)
    # Let go 8 frames after the press: on the first frame of the pull
    script.press()
    script.wait(7)
    script.release()
    script.wait(30)
    script.move((600, 600), 20)
    script.wait(20)
    return script.events


def busy_hand(cycles):
    """A hand that keeps busy on any land, from the camera that the land gives: in each cycle it takes what is at a
    point near the middle of the screen and throws it, drags the land, taps a second point and moves about"""
    points = [(640, 600), (560, 640), (720, 560), (600, 520), (700, 680), (520, 560), (760, 620), (640, 700)]
    script = Script()
    script.wait(10)
    for cycle in range(cycles):
        take = points[cycle % len(points)]
        tap = points[(cycle + 3) % len(points)]
        script.move(take, 30)
        script.press()
        script.wait(60)
        script.move((take[0], take[1] - 80), 30)
        script.move((take[0] - 120, take[1] + 60), 4)
        script.release()
        script.wait(40)
        # Drag the land a little
        script.move((640, 800), 30)
        script.press()
        script.move((660, 720), 40)
        script.release()
        script.wait(30)
        script.move(tap, 30)
        script.press()
        script.wait(3)
        script.release()
        script.wait(30)
        script.circle((640, 600), 100, 120, 1.0)
        script.wait(250)
    return script.events


# The cameras of the online maps' store inputs: 35 m from the storage pit of each god slot's first town, looking down at 40
# degrees (net_check.py --cameraN), and from there the pixels of the store's piles that have food or wood in the first
# turns: the food pile first, then the wood pile. Each pixel is the middle of the pixels at which the hand picks that pile
# (the inspector's pick, in steps of 10 pixels, at turns 50 to 820 of a seed 1 game). A pixel from the pile's place on the
# land can fall on the pit's wall, and an empty pile does not let the hand scoop. A third list, when there is one, has the
# pixels of things near the store that the hand of that god can take: the middle of the pixels at which the inspector's
# pick says that the Action button takes the thing (steps of 10 pixels, turns 680 to 1160 of a seed 1 network game).
# The pit, its piles and the town's desire flags are not things that the hand takes: with no third list the take of each
# cycle picks one of these or the land, and takes nothing.
ONLINE_MAP_STORES = {
    "mpm_2p_1": [((2199.33, 2406.03), [(780, 480), (550, 420)], [(1240, 190), (20, 630)]),
                 ((2922.59, 2653.02), [(780, 480), (470, 400)], [(130, 60), (50, 60)])],
    "mpm_3p_1": [((2571.61, 2196.73), [(520, 520), (710, 620)]),
                 ((2217.39, 2671.21), [(750, 440), (460, 410)]),
                 ((2806.55, 2811.45), [(780, 480), (540, 400)])],
    "mpm_4p_1": [((3102.37, 2169.45), [(770, 500)]),
                 ((1987.71, 1958.34), [(730, 650)]),
                 ((1812.98, 3026.09), [(670, 530), (560, 660)]),
                 ((3090.47, 2966.72), [(680, 640)])],
}


def store_camera(store):
    """The camera of a store input, as the inspector's camera_set takes it"""
    return {"focus": [store[0], store[1]], "yaw": 30, "pitch": 40, "distance": 35}


def store_hand(piles, cycles, takes=None):
    """A god's own store, from store_camera: in each cycle the hand scoops from a pile and throws the handful far, takes
    what is at a point near the store and throws it, taps a point and moves about. It does not drag the land, so the
    camera stays at the store. With takes, the point of the take is one of those pixels, and the throw goes towards the
    middle of the screen, so that the pointer stays on the screen."""
    points = [(640, 600), (560, 640), (720, 560), (600, 520), (700, 680), (520, 560), (760, 620), (640, 700)]
    script = Script()
    script.wait(10)
    for cycle in range(cycles):
        pile = piles[cycle % len(piles)]
        take = points[cycle % len(points)]
        tap = points[(cycle + 3) % len(points)]
        # Scoop from the pile for 60 frames, then throw the handful from the land below
        script.move(pile, 20)
        script.wait(10)
        script.press()
        script.wait(60)
        script.release()
        script.wait(20)
        script.move((300, 900), 30)
        script.wait(10)
        script.press()
        script.wait(40)
        script.move((300, 820), 20)
        script.move((300, 560), 3)
        script.release()
        script.wait(60)
        # Take what is at a point near the store and throw it
        if takes:
            take = takes[cycle % len(takes)]
        script.move(take, 30)
        script.press()
        script.wait(60)
        if takes:
            towards = (640 - take[0], 600 - take[1])
            length = math.hypot(towards[0], towards[1])
            step = (towards[0] / length, towards[1] / length)
            script.move((round(take[0] + step[0] * 80), round(take[1] + step[1] * 80)), 30)
            script.move((round(take[0] + step[0] * 280), round(take[1] + step[1] * 280)), 4)
        else:
            script.move((take[0], take[1] - 80), 30)
            script.move((take[0] - 120, take[1] - 200), 4)
        script.release()
        script.wait(40)
        script.move(tap, 30)
        script.press()
        script.wait(3)
        script.release()
        script.wait(30)
        script.circle((640, 600), 100, 120, 1.0)
        script.wait(100)
    return script.events


def write(name, events, camera=None, expect=None):
    """Writes an input: a list of events, or an object with the camera and the deeds that the hand must do (the least
    count of each, which server_check.py and net_check.py check) when it has either"""
    # One event a line
    lines = ",\n".join(json.dumps(event) for event in events)
    with open(os.path.join(HERE, name + ".json"), "w", encoding="utf-8", newline="\n") as file:
        if camera is None and expect is None:
            file.write("[\n" + lines + "\n]\n")
        else:
            head = ([f'"camera": {json.dumps(camera)}'] if camera is not None else []) + (
                [f'"expect": {json.dumps(expect)}'] if expect is not None else [])
            file.write("{" + ", ".join(head) + ',\n"events": [\n' + lines + "\n]}\n")
    print(name, len(events), "events,", events[-1]["frame"] + 1, "frames")


def main():
    write("land1", land1())
    write("land1_camera", land1(), LAND1_TURNED_CAMERA)
    write("testbed", testbed())
    write("twogods", two_gods(), TWO_GODS_STORE_CAMERA, {"scoops": 1, "uses": 1})
    write("twogods_totem", two_gods_totem(), TWO_GODS_TOTEM_CAMERA)
    write("strokes", strokes())
    write("scaffold_yard", scaffold_take(), expect={"takes": 1, "puts_down": 1})
    # The food pile of the first god's store on TwoGods (net_check.py --input, server_check.py --land
    # Playgrounds/TwoGods.txt --input)
    write("twogods_scoop_throw", scoop_throw((712, 437)), TWO_GODS_STORE_CAMERA, {"scoops": 1, "throws": 1})
    # Slot 1's store on the map for 32 gods (net_check.py --gods-ring-map --input2)
    write("gods_ring_slot1_scoop_throw", scoop_throw((769, 471)), GODS_RING_SLOT1_STORE_CAMERA,
          {"scoops": 1, "throws": 1})
    write("casts", casts())
    # The second god's conifer on TwoGods (net_check.py --input2 with --camera2 TWO_GODS_SLOT1_TREE_CAMERA)
    write("twogods_slot1_tree_press", two_gods_slot1_tree_press(), TWO_GODS_SLOT1_TREE_CAMERA, {"takes": 1})
    # Any land, every god slot (net_check.py --input ... --input4 on the online maps)
    write("busy_hand", busy_hand(18))
    # Each god slot's own store on the online maps (net_check.py --inputN), from the camera of store_camera
    for land, stores in ONLINE_MAP_STORES.items():
        for slot, (store, piles, *takes) in enumerate(stores):
            take_pixels = takes[0] if takes else None
            expect = {"scoops": 1, "throws": 1} | ({"takes": 1} if take_pixels else {})
            write(f"{land}_slot{slot}_store", store_hand(piles, 30, take_pixels), store_camera(store), expect)


if __name__ == "__main__":
    main()
