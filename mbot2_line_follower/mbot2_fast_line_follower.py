# =============================================================================
#  mBot2 FAST Line Follower  —  minimal, high-speed PD loop
# -----------------------------------------------------------------------------
#  Platform : Makeblock mBot2 (CyberPi + Quad RGB Line-Follower Sensor)
#  Controls : Button A = start,  Button B = stop
#
#  "Simple but fast": no on-screen menu/tuning to slow the loop — just read the
#  line offset, apply a PD correction, and drive. Tune the 3 numbers below.
#  Uses the API names confirmed on your firmware (get_offset_track, get_line_sta,
#  drive_speed).  Test with the wheels off the ground first.
# =============================================================================

import cyberpi
import mbot2
import time
from mbuild import quad_rgb_sensor

# ---- tune these three for speed vs. stability ----
KP   = 0.9      # steering strength (turn harder for bigger error)
KD   = 11.0     # damping (kills wobble; raise if it oscillates)
BASE = 60       # cruising speed in RPM — raise to go faster

MAX     = 150   # per-wheel speed cap (RPM)
SIGN    = 1     # set to -1 if it steers AWAY from the line
LOST_MAX = 120  # loop cycles with no line before auto-stop (~ a second)
IDX     = 1     # quad RGB sensor index

running  = False
last_err = 0.0
lost     = 0
prev_a   = False
prev_b   = False

cyberpi.console.println("FAST LINE FOLLOW")
cyberpi.console.println("A = start  B = stop")

while True:
    # --- buttons (edge-detected so one press = one action) ---
    a = cyberpi.controller.is_press("a")
    b = cyberpi.controller.is_press("b")
    if a and not prev_a:
        running = True
        last_err = 0.0
        lost = 0
        cyberpi.console.println("RUN")
    if b and not prev_b:
        running = False
        mbot2.drive_speed(0, 0)
        cyberpi.console.println("STOP")
    prev_a, prev_b = a, b

    if not running:
        mbot2.drive_speed(0, 0)
        time.sleep(0.01)
        continue

    # --- line lost? steer toward the last-seen side, then time out ---
    if quad_rgb_sensor.get_line_sta(IDX) == 0:
        lost += 1
        if lost > LOST_MAX:
            running = False
            mbot2.drive_speed(0, 0)
            cyberpi.console.println("LINE LOST")
            continue
        d = 1 if last_err >= 0 else -1
        mbot2.drive_speed(int(SIGN * d * BASE), int(-SIGN * d * BASE))
        continue
    lost = 0

    # --- PD steering (discrete derivative = change in error; fast & simple) ---
    err = quad_rgb_sensor.get_offset_track(IDX)      # ~ -100..+100, 0 = centered
    corr = SIGN * (KP * err + KD * (err - last_err))
    last_err = err

    left = BASE + corr
    right = BASE - corr
    if left > MAX:   left = MAX
    if left < -MAX:  left = -MAX
    if right > MAX:  right = MAX
    if right < -MAX: right = -MAX

    mbot2.drive_speed(int(left), int(right))
    time.sleep(0.003)    # tiny pause; remove for absolute max rate
