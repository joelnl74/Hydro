"""The battle stage camera, shared by build_arena.py and preview.py.

Keep in step with BattleScript (Oxygen/src/Scripts/BattleScripts.h, "Stage camera"): the same properties and the
same maths, so the positions the builder bakes into Battle.hyscene are where the script puts things at runtime.

Layout units (L): the battle's 2D layout from before the 3D stage, 18 units from the bottom of the screen to the
top, (0, 0) at the centre. The script keeps its rules in these units and maps them to the world:
  - screen_point(L, lift): on the plane facing the camera through the focus point (HUD sprites, Overlay)
  - ground_point(L): where the camera ray through L meets the floor (actors stand there)
  - layout_of(world point): back from the world to L
"""
from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np

LAYOUT_HEIGHT = 18.0


@dataclass
class StageCamera:
    tilt_degrees: float = 73.0           # from straight down
    field_of_view: float = 30.0          # vertical, degrees
    focus: tuple = (0.0, 0.0, 0.0)       # the world point at the screen's centre
    view_height: float = 10.0            # metres seen vertically at the focus point
    ground_z: float = 0.0                # the arena floor

    def __post_init__(self):
        t = math.radians(self.tilt_degrees)
        self.forward = np.array([0.0, math.sin(t), -math.cos(t)])
        self.up = np.array([0.0, math.cos(t), math.sin(t)])
        self.right = np.array([1.0, 0.0, 0.0])
        self.back = -self.forward
        self.distance = self.view_height / (2.0 * math.tan(math.radians(self.field_of_view) * 0.5))
        self.focus_point = np.array(self.focus, dtype=float)
        self.position = self.focus_point - self.forward * self.distance
        self.k = self.view_height / LAYOUT_HEIGHT

    def screen_point(self, layout, lift: float = 0.0):
        return self.focus_point + (self.right * layout[0] + self.up * layout[1]) * self.k + self.back * lift

    def ground_point(self, layout):
        p = self.screen_point(layout)
        direction = p - self.position
        if abs(direction[2]) < 1e-6:
            return p
        t = (self.ground_z - self.position[2]) / direction[2]
        return self.position + direction * t

    def layout_of(self, world):
        world = np.asarray(world, dtype=float)
        direction = world - self.position
        denominator = float(np.dot(direction, self.forward))
        if abs(denominator) < 1e-6:
            return (0.0, 0.0)
        s = self.distance / denominator
        on_plane = self.position + direction * s
        offset = on_plane - self.focus_point
        return (float(np.dot(offset, self.right) / self.k), float(np.dot(offset, self.up) / self.k))

    def plane_layout_of(self, world):
        """A point on (or lifted off) the view plane back to L: its offset along right and up."""
        offset = np.asarray(world, dtype=float) - self.focus_point
        return (float(np.dot(offset, self.right) / self.k), float(np.dot(offset, self.up) / self.k))

    def camera_rotation(self):
        return (math.radians(self.tilt_degrees), 0.0, 0.0)
