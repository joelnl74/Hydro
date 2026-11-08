"""The game's look per time of day (Nov 3, 2026): the vignette and the colour grade (#137) that the Japanese scenes'
render profiles share. The City profiles (the trade quarter, the countryside, and through them the temple) and the
Shrine Glade's get them from here (build_city_slice.city_profiles, build_temple_district.temple_profiles,
terrain/shrine_glade_jp_pass.py), so the scenes stay graded alike.

    apply(text, time)   the .hyrender text with these values set in its PostProcess map

The grade is gentle on purpose (the palette is authored): a little contrast, warm lights and cool lifted shadows
(the HD-2D look), less colour at night. The miko stays the most saturated thing in a frame: saturation stays near 1.
"""

import re

VIGNETTE = {"Day": (0.3, 0.45, 0.6), "Dusk": (0.5, 0.42, 0.6), "Night": (0.6, 0.38, 0.6)}   # intensity, radius, softness

GRADE = {   # (lift is linear and added after the roll-off: 0.003 lifts black to about 10 / 255 in sRGB, so it stays tiny)
    "Day": dict(Grading="true", Temperature=0.03, Tint=0.0, Contrast=1.05, Saturation=1.04,
                Lift=(0.0, 0.0, 0.002), Gamma=(1.0, 1.0, 1.0), Gain=(1.02, 1.0, 0.98)),
    "Dusk": dict(Grading="true", Temperature=0.06, Tint=0.01, Contrast=1.06, Saturation=1.04,
                 Lift=(0.0, 0.0, 0.003), Gamma=(1.0, 1.0, 1.0), Gain=(1.02, 1.0, 0.98)),
    "Night": dict(Grading="true", Temperature=-0.08, Tint=0.0, Contrast=1.08, Saturation=0.92,
                  Lift=(0.0, 0.001, 0.004), Gamma=(1.0, 1.0, 1.0), Gain=(0.98, 1.0, 1.03)),
}


def values(time):
    """The PostProcess keys for a time of day (Day, Dusk or Night), as YAML value text."""
    out = dict(zip(("VignetteIntensity", "VignetteRadius", "VignetteSoftness"), ("%g" % v for v in VIGNETTE[time])))
    for key, value in GRADE[time].items():
        out[key] = ("[%s]" % ", ".join("%g" % v for v in value)) if isinstance(value, tuple) else (
            value if isinstance(value, str) else "%g" % value)
    return out


def apply(text, time):
    """Sets the look's keys in a .hyrender's PostProcess map (the file's last map): replaced where the key is there,
    appended where it isn't. Line endings: whatever `text` uses."""
    nl = "\r\n" if "\r\n" in text else "\n"
    t = text.replace("\r\n", "\n")
    for key, value in values(time).items():
        line = "    %s: %s" % (key, value)
        if re.search(r"^    %s: " % key, t, re.M):
            t = re.sub(r"^    %s: [^\n]*" % key, lambda m: line, t, count=1, flags=re.M)
        else:
            t = t.rstrip("\n") + "\n" + line + "\n"
    return t.replace("\n", nl)
