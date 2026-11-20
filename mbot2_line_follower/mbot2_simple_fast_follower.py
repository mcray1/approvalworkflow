# =============================================================================
#  mBot2 Line Follower — SIMPLE but FAST
#  (PD steering + one 2-level speed; wrapped to print the real error if any)
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

LOOP_DELAY = 0.01


def clamp(value, low, high):
    return max(low, min(high, value))


def stop_motors():
    mbot2.drive_power(0, 0)      # reliable stop (no EM_stop argument needed)


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
        cyberpi.console.println("RUNNING")

        # --- follow until B ---
        while not cyberpi.controller.is_press("b"):

            # stop if the line is completely lost (all 4 probes off)
            if mbuild.quad_rgb_sensor.get_line_sta(1) == 15:
                stop_motors()
                cyberpi.console.println("LINE LOST")
                break

            offset = mbuild.quad_rgb_sensor.get_offset_track(1)   # -100..+100
            change = offset - previous_offset
            previous_offset = offset

            steer = clamp(KP * offset + KD * change, -MAX_STEER, MAX_STEER)

            if abs(offset) > TURN_OFFSET:
                power = TURN_POWER
            else:
                power = STRAIGHT_POWER

            drive(power - steer, power + steer)
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
        sys.print_exception(e)     # full traceback to the serial monitor
    except Exception:
        pass
