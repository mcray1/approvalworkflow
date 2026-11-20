# =============================================================================
#  mBot2 Line Follower — constant speed + PD steering
#  (adaptive speed and curve prediction removed)
# -----------------------------------------------------------------------------
#  Platform : Makeblock mBot2 (CyberPi + Quad RGB Line-Following Sensor)
#  Controls : Button A = start,  Button B = stop
#
#  Drives at ONE fixed power everywhere; PD steering keeps it on the line.
#  Still has lost-line recovery. Note: without auto-slowdown, set BASE_POWER low
#  enough that it can hold your sharpest corner.
# =============================================================================

import cyberpi
import mbot2
import mbuild
import time

# ---- SPEED ----
BASE_POWER = 55        # constant driving power (raise = faster everywhere)
MAX_MOTOR  = 95        # hard cap per motor

# ---- PD STEERING ----
KP = 0.5               # how hard it steers toward the line (position)
KD = 4                 # damping / anticipation (rate of change)
MAX_STEER = 90         # cap on the steering amount
FILTER_ALPHA = 0.65    # offset smoothing: lower = smoother, higher = snappier

# ---- RECOVERY ----
SEARCH_POWER  = 22     # pivot speed while hunting for a lost line
MAX_LOST_STEPS = 120   # loops lost before it stops (~1.2 s at LOOP_DELAY)
LOOP_DELAY = 0.01      # control-loop period (seconds)


def clamp(value, low, high):
    return max(low, min(high, value))


def drive(left, right):
    left = clamp(left, -MAX_MOTOR, MAX_MOTOR)
    right = clamp(right, -MAX_MOTOR, MAX_MOTOR)
    mbot2.drive_power(left, -right)      # right motor is mirrored -> negate


# ====================================  MAIN  ==================================
while True:

    mbot2.EM_stop()
    cyberpi.console.println("PRESS A TO START")
    while not cyberpi.controller.is_press("a"):
        time.sleep(0.05)

    filtered_offset = 0
    previous_offset = 0
    last_turn = 1
    lost_steps = 0

    cyberpi.console.println("RUNNING")

    while not cyberpi.controller.is_press("b"):

        state = mbuild.quad_rgb_sensor.get_line_sta(1)

        # ---- lost line (all probes off the line) ----
        if state == 15:
            lost_steps += 1
            if lost_steps > MAX_LOST_STEPS:
                mbot2.EM_stop()
                cyberpi.console.println("LINE LOST")
                break
            drive(SEARCH_POWER * last_turn, -SEARCH_POWER * last_turn)
            time.sleep(LOOP_DELAY)
            continue

        lost_steps = 0

        # ---- read + filter line position ----
        raw_offset = mbuild.quad_rgb_sensor.get_offset_track(1)
        filtered_offset = (FILTER_ALPHA * raw_offset +
                           (1 - FILTER_ALPHA) * filtered_offset)
        change = filtered_offset - previous_offset

        # ---- PD steering ----
        steer = KP * filtered_offset + KD * change
        steer = clamp(steer, -MAX_STEER, MAX_STEER)

        # ---- constant-speed differential drive ----
        drive(BASE_POWER - steer, BASE_POWER + steer)

        # ---- remember which side the line was on (for recovery) ----
        if filtered_offset < -7:
            last_turn = 1
        elif filtered_offset > 7:
            last_turn = -1

        previous_offset = filtered_offset
        time.sleep(LOOP_DELAY)

    # ---- stop ----
    mbot2.EM_stop()
    cyberpi.console.println("STOPPED - PRESS A")
    while cyberpi.controller.is_press("b"):
        time.sleep(0.05)
