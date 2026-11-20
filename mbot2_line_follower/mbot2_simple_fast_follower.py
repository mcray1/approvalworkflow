# =============================================================================
#  mBot2 Line Follower — SIMPLE but FAST  (with lost-line recovery)
#  PD steering + one 2-level speed. If the line is lost, it keeps turning the
#  way it was already turning until it finds the line again.
# -----------------------------------------------------------------------------
#  Platform : Makeblock mBot2 (CyberPi + Quad RGB Line-Following Sensor)
#  Controls : Button A = start,  Button B = stop
# =============================================================================

import cyberpi
import mbot2
import mbuild
import time
import sys

# ---- SPEED (just two: fast on straights, slower in turns) ----
STRAIGHT_POWER = 75    # power when the line is roughly centered
TURN_POWER     = 45    # power when it's clearly turning
TURN_OFFSET    = 25    # |offset| above this counts as "a turn"
MAX_MOTOR      = 95    # hard cap per motor

# ---- PD STEERING ----
KP = 0.5
KD = 4
MAX_STEER = 90

# ---- LOST-LINE RECOVERY ----
SEARCH_POWER   = 35    # pivot power while hunting for the line
MAX_LOST_STEPS = 200   # safety: stop if still lost after this many loops
                       #         (set to 0 to search forever until B)

LOOP_DELAY = 0.01


def clamp(value, low, high):
    return max(low, min(high, value))


def stop_motors():
    mbot2.drive_power(0, 0)


def drive(left, right):
    left = clamp(left, -MAX_MOTOR, MAX_MOTOR)
    right = clamp(right, -MAX_MOTOR, MAX_MOTOR)
    mbot2.drive_power(left, -right)      # right motor is mirrored -> negate


def run():
    while True:
        # --- start screen ---
        stop_motors()
        cyberpi.console.println("PRESS A TO START")
        while not cyberpi.controller.is_press("a"):
            time.sleep(0.05)

        previous_offset = 0
        last_turn = 1          # +1 or -1: the way it was last turning
        lost_steps = 0
        cyberpi.console.println("RUNNING")

        # --- follow until B ---
        while not cyberpi.controller.is_press("b"):

            state = mbuild.quad_rgb_sensor.get_line_sta(1)

            # ---- line completely lost (all 4 probes off) ----
            if state == 15:
                lost_steps += 1
                if MAX_LOST_STEPS and lost_steps > MAX_LOST_STEPS:
                    stop_motors()
                    cyberpi.console.println("LINE LOST")
                    break
                # keep turning the SAME way it was turning before it lost the line
                drive(SEARCH_POWER * last_turn, -SEARCH_POWER * last_turn)
                time.sleep(LOOP_DELAY)
                continue

            lost_steps = 0

            # ---- normal following ----
            offset = mbuild.quad_rgb_sensor.get_offset_track(1)   # -100..+100
            change = offset - previous_offset
            previous_offset = offset

            steer = clamp(KP * offset + KD * change, -MAX_STEER, MAX_STEER)

            if abs(offset) > TURN_OFFSET:
                power = TURN_POWER
            else:
                power = STRAIGHT_POWER

            drive(power - steer, power + steer)

            # remember which way it's turning, for the next time the line is lost
            if offset < -7:
                last_turn = 1
            elif offset > 7:
                last_turn = -1

            time.sleep(LOOP_DELAY)

        # --- stopped ---
        stop_motors()
        cyberpi.console.println("STOPPED - PRESS A")
        while cyberpi.controller.is_press("b"):
            time.sleep(0.05)


# Run, and if anything errors, show exactly what and where (screen + serial).
try:
    run()
except Exception as e:
    stop_motors()
    print("ERROR:", e)
    try:
        cyberpi.console.println("ERROR:")
        cyberpi.console.println(str(e))
    except Exception:
        pass
    try:
        sys.print_exception(e)
    except Exception:
        pass
