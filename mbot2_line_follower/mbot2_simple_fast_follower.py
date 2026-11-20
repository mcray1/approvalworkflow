# =============================================================================
#  mBot2 Line Follower — SIMPLE but FAST
#  (distilled from the advanced version: PD steering + one 2-level speed)
# -----------------------------------------------------------------------------
#  Platform : Makeblock mBot2 (CyberPi + Quad RGB Line-Following Sensor)
#  Controls : Button A = start,  Button B = stop
# =============================================================================

import cyberpi
import mbot2
import mbuild
import time

# ---- SPEED (just two: fast on straights, slower in turns) ----
STRAIGHT_POWER = 75    # power when the line is roughly centered
TURN_POWER     = 45    # power when it's clearly turning
TURN_OFFSET    = 25    # |offset| above this counts as "a turn"
MAX_MOTOR      = 95    # hard cap per motor

# ---- PD STEERING ----
KP = 0.5               # steer toward the line (how far off)
KD = 4                 # damping (how fast the error is changing)
MAX_STEER = 90         # cap on the steering amount

LOOP_DELAY = 0.01      # control-loop period (seconds)


def clamp(value, low, high):
    return max(low, min(high, value))


def drive(left, right):
    left = clamp(left, -MAX_MOTOR, MAX_MOTOR)
    right = clamp(right, -MAX_MOTOR, MAX_MOTOR)
    mbot2.drive_power(left, -right)      # right motor is mirrored -> negate


# ================================  MAIN  ======================================
while True:

    # --- wait on the start screen ---
    mbot2.EM_stop()
    cyberpi.console.println("PRESS A TO START")
    while not cyberpi.controller.is_press("a"):
        time.sleep(0.05)

    previous_offset = 0
    cyberpi.console.println("RUNNING")

    # --- follow until B is pressed ---
    while not cyberpi.controller.is_press("b"):

        # stop if the line is completely lost (all 4 probes off)
        if mbuild.quad_rgb_sensor.get_line_sta(1) == 15:
            mbot2.EM_stop()
            cyberpi.console.println("LINE LOST")
            break

        # where is the line? (-100..+100, 0 = centered)
        offset = mbuild.quad_rgb_sensor.get_offset_track(1)
        change = offset - previous_offset
        previous_offset = offset

        # PD steering
        steer = clamp(KP * offset + KD * change, -MAX_STEER, MAX_STEER)

        # pick one of two speeds
        if abs(offset) > TURN_OFFSET:
            power = TURN_POWER
        else:
            power = STRAIGHT_POWER

        # differential drive
        drive(power - steer, power + steer)

        time.sleep(LOOP_DELAY)

    # --- stopped ---
    mbot2.EM_stop()
    cyberpi.console.println("STOPPED - PRESS A")
    while cyberpi.controller.is_press("b"):
        time.sleep(0.05)
