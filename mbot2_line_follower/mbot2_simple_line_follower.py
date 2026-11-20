# =============================================================================
#  mBot2 SIMPLE Line Follower  —  no PID, just plain threshold steering
# -----------------------------------------------------------------------------
#  Platform : Makeblock mBot2 (CyberPi + Quad RGB Line-Follower Sensor)
#  Controls : Button A = start,  Button B = stop
#
#  How it works: read where the line is (get_offset_track, -100..+100), then
#  pick ONE of five fixed moves — straight, gentle turn, or hard turn — based
#  on simple if/else thresholds. No Kp/Kd, no math. Easy to read and adjust.
#  Uses the API names confirmed on your firmware. Test with wheels up first.
# =============================================================================

import cyberpi
import mbot2
import time
from mbuild import quad_rgb_sensor

# ---- adjust these ----
SPEED = 40     # forward speed (RPM) when going straight / outer wheel in a turn
TURN  = 22     # inner-wheel speed for a GENTLE turn
HARD  = 18     # inner-wheel reverse speed for a HARD turn (pivot)
DEAD  = 15     # |offset| below this = "centered" -> go straight
BIG   = 55     # |offset| above this = sharp turn
SIGN  = 1      # set to -1 if it turns the WRONG way
IDX   = 1      # quad RGB sensor index

running = False
prev_a = False
prev_b = False

cyberpi.console.println("SIMPLE LINE FOLLOW")
cyberpi.console.println("A = start  B = stop")

while True:
    # --- buttons (one press = one action) ---
    a = cyberpi.controller.is_press("a")
    b = cyberpi.controller.is_press("b")
    if a and not prev_a:
        running = True
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

    # --- line lost (no probe sees it) -> stop ---
    if quad_rgb_sensor.get_line_sta(IDX) == 0:
        mbot2.drive_speed(0, 0)
        time.sleep(0.01)
        continue

    # --- where is the line? pick one fixed move ---
    offset = SIGN * quad_rgb_sensor.get_offset_track(IDX)

    if offset > BIG:          # line far to the RIGHT -> hard right
        left, right = SPEED, -HARD
    elif offset > DEAD:       # line a bit RIGHT -> gentle right
        left, right = SPEED, TURN
    elif offset < -BIG:       # line far to the LEFT -> hard left
        left, right = -HARD, SPEED
    elif offset < -DEAD:      # line a bit LEFT -> gentle left
        left, right = TURN, SPEED
    else:                     # centered -> straight
        left, right = SPEED, SPEED

    mbot2.drive_speed(int(left), int(right))
    time.sleep(0.005)
