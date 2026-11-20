# =============================================================================
#  mBot2 Line Follower  —  on-screen Start/Stop + live PID tuning
# -----------------------------------------------------------------------------
#  Platform : Makeblock mBot2  (CyberPi controller + mBot2 shield +
#             Quad RGB Line-Follower Sensor)
#  Language : MicroPython via mBlock 5 (Python mode) — upload to the CyberPi.
#
#  Same control principles as the ESP32 build: PID steering from a line-position
#  error, a base cruising speed, and bounded line-loss handling. Here the Quad
#  RGB sensor gives the error directly (get_offset_track), so no 8-channel
#  calibration math is needed — the sensor is "learned" once with its own button.
#
#  ON-SCREEN CONTROLS (CyberPi):
#    Button A ............ START following
#    Button B ............ STOP (back to menu)
#    Joystick LEFT/RIGHT . choose which value to tune (Kp, Ki, Kd, Base)
#    Joystick UP/DOWN .... increase / decrease the chosen value
#    Joystick MIDDLE ..... reset the integral term (handy while tuning)
#
#  NOTE: This has not been run on hardware here. API names follow mBlock 5's
#  mBot2 Python; if your firmware differs slightly, see README.md for the
#  alternative import/method names.
# =============================================================================

import cyberpi
import mbot2
import time
from mbuild import quad_rgb_sensor   # if this errors, see README (use `import mbuild`)

# ----------------------------- tunables --------------------------------------
# Starting PID gains + base speed. Tune them live on the CyberPi screen; copy
# the good values back here afterwards (the robot starts from these each boot).
params = {"Kp": 0.80, "Ki": 0.00, "Kd": 8.00, "Base": 35}

# How much each joystick up/down press changes the selected value.
STEP  = {"Kp": 0.10, "Ki": 0.01, "Kd": 0.50, "Base": 5}
# Allowed range per value (keeps tuning sane).
LIMIT = {"Kp": (0.0, 10.0), "Ki": (0.0, 2.0), "Kd": (0.0, 50.0), "Base": (0, 120)}
ORDER = ["Kp", "Ki", "Kd", "Base"]

MAX_SPEED   = 120     # RPM cap per wheel
I_LIMIT     = 60.0    # anti-windup clamp on the integral term
SENSOR_IDX  = 1       # quad RGB sensor index (1 if you have one sensor)
STEER_SIGN  = +1      # flip to -1 if it steers AWAY from the line
LOST_TIMEOUT = 1.2    # s searching for a lost line before auto-stop

# ----------------------------- state -----------------------------------------
running   = False
sel       = 0          # index into ORDER (which value is selected)
integral  = 0.0
last_err  = 0.0
lost_at   = None       # ticks_ms when the line was first lost
last_draw = 0

# remember previous button/joystick states for edge detection (one action/press)
prev = {"a": False, "b": False, "up": False, "down": False,
        "left": False, "right": False, "middle": False}


def clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


def pressed(key):
    """True only on the rising edge of a button/joystick press."""
    now = cyberpi.controller.is_press(key)
    edge = now and not prev[key]
    prev[key] = now
    return edge


def draw():
    """Render the on-screen menu. Labels reuse fixed indices so they update
    in place. Text is space-padded so shorter values don't leave artifacts."""
    cyberpi.display.show_label("mBot2 LINE PID", 14, 2, 2, 1)
    state = "RUNNING" if running else "STOPPED"
    cyberpi.display.show_label("State: " + state + "   ", 12, 2, 22, 2)
    for i, name in enumerate(ORDER):
        cursor = ">" if i == sel else " "
        val = params[name]
        txt = ("%s %s: %.2f  " % (cursor, name, val)) if name != "Base" \
              else ("%s %s: %d   " % (cursor, name, int(val)))
        cyberpi.display.show_label(txt, 12, 2, 40 + i * 16, 3 + i)
    cyberpi.display.show_label("A=Start B=Stop", 10, 2, 112, 8)


def handle_input():
    """Process buttons + joystick. Returns True if the display should redraw."""
    global running, sel, integral, last_err, lost_at
    changed = False

    if pressed("a") and not running:        # START
        running = True
        integral = 0.0
        last_err = 0.0
        lost_at = None
        try:
            cyberpi.audio.play("switch")
        except Exception:
            pass
        changed = True

    if pressed("b") and running:            # STOP
        running = False
        mbot2.drive_speed(0, 0)
        try:
            cyberpi.audio.play("switch")
        except Exception:
            pass
        changed = True

    if pressed("left"):
        sel = (sel - 1) % len(ORDER)
        changed = True
    if pressed("right"):
        sel = (sel + 1) % len(ORDER)
        changed = True

    if pressed("up"):
        name = ORDER[sel]
        lo, hi = LIMIT[name]
        params[name] = clamp(params[name] + STEP[name], lo, hi)
        changed = True
    if pressed("down"):
        name = ORDER[sel]
        lo, hi = LIMIT[name]
        params[name] = clamp(params[name] - STEP[name], lo, hi)
        changed = True

    if pressed("middle"):                   # reset integral (tuning helper)
        integral = 0.0
        changed = True

    return changed


def follow_step(dt):
    """One PID control step while RUNNING."""
    global integral, last_err, lost_at

    # Which of the 4 probes currently see the line (0 = none -> lost).
    station = quad_rgb_sensor.get_line_station(SENSOR_IDX)

    if station == 0:
        # ---- line lost: search toward the last-known side, then time out ----
        now = time.ticks_ms()
        if lost_at is None:
            lost_at = now
        if time.ticks_diff(now, lost_at) / 1000.0 > LOST_TIMEOUT:
            stop("LINE LOST")
            return
        turn = params["Base"]
        d = 1 if last_err >= 0 else -1          # last side the line was on
        mbot2.drive_speed(int(STEER_SIGN * d * turn),
                          int(-STEER_SIGN * d * turn))
        return

    lost_at = None

    # ---- normal PID tracking --------------------------------------------
    err = quad_rgb_sensor.get_offset_track(SENSOR_IDX)   # ~ -100..+100, 0 = centered

    integral = clamp(integral + err * dt, -I_LIMIT, I_LIMIT)
    deriv = (err - last_err) / dt if dt > 0 else 0.0
    last_err = err

    corr = STEER_SIGN * (params["Kp"] * err +
                         params["Ki"] * integral +
                         params["Kd"] * deriv)

    base = params["Base"]
    left  = clamp(base + corr, -MAX_SPEED, MAX_SPEED)
    right = clamp(base - corr, -MAX_SPEED, MAX_SPEED)
    mbot2.drive_speed(int(left), int(right))


def stop(reason=""):
    global running
    running = False
    mbot2.drive_speed(0, 0)
    if reason:
        cyberpi.display.show_label(reason + "      ", 12, 2, 22, 2)
    try:
        cyberpi.audio.play("exclaim")
    except Exception:
        pass


# ----------------------------- main ------------------------------------------
def main():
    global last_draw
    cyberpi.display.clear()
    mbot2.drive_speed(0, 0)
    draw()

    last = time.ticks_ms()
    while True:
        redraw = handle_input()

        now = time.ticks_ms()
        dt = time.ticks_diff(now, last) / 1000.0
        if dt <= 0:
            dt = 0.01
        last = now

        if running:
            follow_step(dt)
        else:
            mbot2.drive_speed(0, 0)

        # redraw on any input, or ~6x/sec so the loop stays fast while running
        if redraw or time.ticks_diff(now, last_draw) > 160:
            draw()
            last_draw = now

        time.sleep(0.008)   # ~100+ Hz control loop


main()
