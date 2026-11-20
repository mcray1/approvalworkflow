# mBot2 / CyberPi API probe — tells us which function names your firmware has.
# Paste into mBlock (Python mode, mBot2 device) and Run.
#
# Output goes to BOTH:
#   - the CyberPi screen (cyberpi.console), and
#   - the serial monitor / mBlock terminal (plain print, 115200 baud).
# Send me the Y/- lines. Nothing moves.

import cyberpi
import mbot2


def log(s):
    print(s)                      # -> serial monitor / mBlock terminal
    try:
        cyberpi.console.println(s)  # -> CyberPi screen (may fail on some firmware)
    except Exception:
        pass


try:
    cyberpi.console.clear()
except Exception:
    pass

log("PROBE START")

# --- how to drive the motors? ---
for n in ["drive_speed", "drive_power", "EM_set_speed", "EM_control_speed",
          "motor_set", "forward"]:
    log("mbot2." + n + ": " + ("Y" if hasattr(mbot2, n) else "-"))

# --- how to stop? ---
for n in ["EM_stop", "stop"]:
    log("mbot2." + n + ": " + ("Y" if hasattr(mbot2, n) else "-"))

# --- the quad RGB line sensor ---
try:
    from mbuild import quad_rgb_sensor as q
    log("quad import: Y")
    for n in ["get_offset_track", "get_offset", "get_line_station",
              "get_line_sta", "get_line", "is_line"]:
        log("q." + n + ": " + ("Y" if hasattr(q, n) else "-"))
except Exception as e:
    log("quad import FAIL: " + str(e))

# --- buttons / joystick ---
log("ctrl.is_press: " + ("Y" if hasattr(cyberpi.controller, "is_press") else "-"))

log("PROBE DONE")
