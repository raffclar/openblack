"""Makes the recorded hand input of server_check.py --input: the moves, the presses and the releases of the hand.

The input is a list of {frame, event} as the inspector's input.record answers it. The frames count from the frame
that input.replay starts at, with frames of 20 ms. The pixels are of a 1280 by 1024 window and of the camera of the
first frame of the land.

    python tools/sim/hand_inputs/make_inputs.py

writes land1.json and testbed.json beside this script.
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


def main():
    for name, events in (("land1", land1()), ("testbed", testbed())):
        # One event a line
        with open(os.path.join(HERE, name + ".json"), "w", encoding="utf-8", newline="\n") as file:
            file.write("[\n" + ",\n".join(json.dumps(event) for event in events) + "\n]\n")
        print(name, len(events), "events,", events[-1]["frame"] + 1, "frames")


if __name__ == "__main__":
    main()
