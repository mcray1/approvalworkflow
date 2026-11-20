# =============================================================================
#  mBot2 Line Follower — SIMPLE but FAST
#  PD steering + 2-level speed + gradual acceleration + lost-line recovery.
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

# ---- ACCELERATION ----
ACCEL_STEP = 4         # max power INCREASE per loop (lower = smoother/slower
                       # speed-up). Slowing down is always instant.

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
        current_power = 0      # ramps up gradually from a standstill
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
                current_power = 0        # so it ramps up again after recovering
                time.sleep(LOOP_DELAY)
                continue

            lost_steps = 0

            # ---- read line position + PD steering ----
            offset = mbuild.quad_rgb_sensor.get_offset_track(1)   # -100..+100
            change = offset - previous_offset
            previous_offset = offset

            steer = clamp(KP * offset + KD * change, -MAX_STEER, MAX_STEER)

            # ---- choose the target speed ----
            if abs(offset) > TURN_OFFSET:
                target_power = TURN_POWER
            else:
                target_power = STRAIGHT_POWER

            # ---- ramp toward it: speed UP gradually, slow DOWN instantly ----
            if target_power > current_power:
                current_power = min(target_power, current_power + ACCEL_STEP)
            else:
                current_power = target_power

            drive(current_power - steer, current_power + steer)

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
