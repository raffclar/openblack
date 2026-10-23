"""Makes the recorded hand input of server_check.py --input: the moves, the presses and the releases of the hand.

The input is a list of {frame, event} as the inspector's input.record answers it. The frames count from the frame
that input.replay starts at, with frames of 20 ms. The pixels are of a 1280 by 1024 window and of the camera of the
first frame of the land. An input can also be an object with that list as "events" and a "camera" as camera.set takes
it: the game puts its camera there before the first frame of the land.

    python tools/sim/hand_inputs/make_inputs.py

writes land1.json, testbed.json, twogods.json and twogods_totem.json beside this script. The twogods inputs play on the
playground Playgrounds/TwoGods.txt (server_check.py --land Playgrounds/TwoGods.txt), whose first town is the player's.
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


# The camera of twogods.json: 35 m from the store of the player's first town, looking down at 40 degrees
TWO_GODS_STORE_CAMERA = {"focus": [2199.33, 88.44, 2406.03], "yaw": 30, "pitch": 40, "distance": 35}


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
    script.move((640, 700), 40)
    script.wait(10)
    script.release()
    script.wait(30)
    script.move((600, 600), 20)
    script.wait(40)
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
    write("testbed", testbed())
    write("twogods", two_gods(), TWO_GODS_STORE_CAMERA)
    write("twogods_totem", two_gods_totem(), TWO_GODS_TOTEM_CAMERA)


if __name__ == "__main__":
    main()
