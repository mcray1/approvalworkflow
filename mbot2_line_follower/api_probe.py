# mBot2 / CyberPi API probe — tells us which function names your firmware has.
# Paste into mBlock (Python mode, mBot2 device) and Run. Read the CyberPi screen
# (or mBlock console) and send me the Y/- lines. Nothing moves.

import cyberpi
import mbot2

cyberpi.console.clear()
cyberpi.console.println("PROBE START")

# --- how to drive the motors? ---
for n in ["drive_speed", "drive_power", "EM_set_speed", "EM_control_speed",
          "motor_set", "forward"]:
    cyberpi.console.println("mbot2." + n + ": " + ("Y" if hasattr(mbot2, n) else "-"))

# --- how to stop? ---
for n in ["EM_stop", "stop"]:
    cyberpi.console.println("mbot2." + n + ": " + ("Y" if hasattr(mbot2, n) else "-"))

# --- the quad RGB line sensor ---
try:
    from mbuild import quad_rgb_sensor as q
    cyberpi.console.println("quad import: Y")
    for n in ["get_offset_track", "get_offset", "get_line_station",
              "get_line_sta", "get_line", "is_line"]:
        cyberpi.console.println("q." + n + ": " + ("Y" if hasattr(q, n) else "-"))
except Exception as e:
    cyberpi.console.println("quad import FAIL: " + str(e))

# --- buttons / joystick ---
cyberpi.console.println("ctrl.is_press: " +
    ("Y" if hasattr(cyberpi.controller, "is_press") else "-"))

cyberpi.console.println("PROBE DONE")
