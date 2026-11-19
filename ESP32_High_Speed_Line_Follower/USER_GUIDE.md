# ESP32 High-Speed Line-Follower — Complete User Guide

Everything to **install**, **flash**, **calibrate**, **test**, and **use** the
firmware, plus troubleshooting. This is the one-stop guide; deeper references
(`WIRING.md`, `CALIBRATION.md`, `PID_TUNING.md`, `TEST_PLAN.md`, and the diagrams
in `docs/`) are linked where useful.

> **Build status:** the firmware compiles against the toolchain below but has not
> been hardware-verified by the author — you are the first to run it. Follow the
> test plan; nothing drives the motors until you explicitly start it.

---

## 0. What you have

| Part | Value |
|------|-------|
| MCU | ESP32-DevKit (ESP-WROOM-32, 38-pin), CP2102 USB |
| Sensor | Pololu QTR-8RC (8-channel) |
| Driver | TB6612FNG breakout |
| Motors | 2× DC gear motor (6–12 V) |
| Battery | 3S LiPo 11.1 V (12.6 V full), 1400 mAh 30C |
| Logic power | 11.1 V → 5 V buck → ESP32 5V/VIN → on-board 3.3 V |

Pin map, wiring and power path are in [`WIRING.md`](WIRING.md) and
[`docs/`](docs/) (start with `docs/connection_map.svg`).

---

## 1. Install the software (one time)

### 1.1 Arduino IDE
Install **Arduino IDE 2.x** from arduino.cc.

### 1.2 ESP32 board support — **core 3.x (required)**
1. **File → Preferences → Additional boards manager URLs**, add:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
2. **Tools → Board → Boards Manager → search "esp32" (Espressif) → install a 3.x version.**
   - This firmware uses the core-3.x `ledcAttach()` PWM API; it will **not** compile on 2.x.

### 1.3 QTRSensors library
**Tools → Manage Libraries → search "QTRSensors" (Pololu) → Install.**

### 1.4 USB driver
This board uses a **CP2102** → install the **Silicon Labs CP210x VCP** driver, then reboot.
(If your board's chip is a CH340, install the WCH CH340 driver instead.)

---

## 2. Get the firmware

Clone the repo and check out the working branch, or download the folder as a ZIP
from GitHub:
```
git clone https://github.com/mcray1/approvalworkflow.git
cd approvalworkflow
git checkout ccr-8979dddd-45x0gj
```
Open **`ESP32_High_Speed_Line_Follower/ESP32_High_Speed_Line_Follower.ino`**.
All `.h/.cpp` must stay in that one folder (they will if you open the folder).

---

## 3. Flash the firmware

Pick **Method A** if the board's onboard USB works (shows a COM port). Use
**Method B** if the onboard USB is dead (e.g. "Unknown USB Device / Device
Descriptor Request Failed" that a known-good cable on another board disproves).

### Method A — onboard USB (normal)
1. Connect the ESP32 by USB **only** (no battery, no buck, wheels irrelevant yet).
2. **Tools → Board → "ESP32 Dev Module".**
3. **Tools → Port →** the new COM port ("Silicon Labs CP210x … (COMx)").
4. Recommended: **Tools → Erase All Flash Before Sketch Upload → Enabled** for the first flash.
5. Click **Upload**. If it sticks at `Connecting…….____`, **hold BOOT** until it starts, then release.
6. Open **Serial Monitor @ 115200**.

### Method B — external USB-to-TTL adapter (dead onboard USB)
A faulty onboard CP2102 does **not** mean the ESP32 is dead — flash it through a
separate **3.3 V** USB-to-serial adapter (FTDI / CP2102 / CH340 module).

**Set the adapter to 3.3 V logic** (jumper), then wire:

| Adapter | ESP32 DevKit |
|---------|--------------|
| GND | GND |
| TX | **RX** (RXD / GPIO3) |
| RX | **TX** (TXD / GPIO1) |
| 3V3 | 3V3  *(powers the board; do **not** also plug the board's micro-USB)* |

(TX→RX and RX→TX is crossed over — that's correct.)

**Enter download mode manually** (no auto-reset wired):
1. Press and **hold the BOOT (IO0)** button on the ESP32.
2. **Tap EN (reset)**, then **release EN**.
3. **Release BOOT.** The chip is now in the bootloader.

Then in Arduino IDE: select the **adapter's** COM port, set **Upload Speed 115200**,
and click **Upload**. (Repeat the BOOT/EN sequence just before each upload if it
won't connect.) After upload, tap **EN** to run the sketch, and open Serial Monitor
on the adapter's port @ 115200.

> Common ground between adapter and ESP32 is mandatory. Never put 5 V on RX/TX —
> keep the adapter at 3.3 V.

---

## 4. First boot

With Serial Monitor open @ **115200**, reset the board. You should see:
```
=== ESP32 High-Speed Line Follower ===
BUILD: unverified until compiled on your toolchain.
==== Serial commands ====
  s  start race (only from READY)
  ...
[CAL] Sweep the array over BLACK and WHITE now (~4.5 s).
```
Motors stay **off** — it auto-enters calibration. No motion ever happens at boot.

---

## 5. Calibrate the sensors (USB power only is fine)

1. When you see `[CAL] Sweep…`, **slowly sweep the 8-sensor bar across the black
   line and the white surface for ~4.5 s** — every sensor must cross both, 3–4 passes.
2. Keep the array at race height (~2–4 mm off the surface).
3. Result:
   - `[CAL] OK` → `[READY]` — success.
   - `[CAL] FAILED. Weak channels mask=0x..` → send **`c`** to redo (sweep wider/slower,
     lower the array, confirm QTR **LEDON tied to 3.3 V**).
4. Verify with **`d`**: over white all channels low; on the line the covered channel
   ~850–1000; `conf` high on-line and ~0 off-line; `line` = 1/0.

Details: [`CALIBRATION.md`](CALIBRATION.md).

---

## 6. Test (wheels up until Test 4 passes)

Run in order; full procedures/pass-criteria in [`TEST_PLAN.md`](TEST_PLAN.md).

| # | Test | Pass when |
|---|------|-----------|
| 1 | Startup safety | boots, motors idle |
| 2 | Sensor detection | all 8 respond (via `d`) |
| 3 | Calibration | `[CAL] OK`, good contrast |
| 4 | **Motor direction** (wheels up, motor power on) | forward = forward; steers **toward** the line |
| 5 | PID tracking (low speed) | follows line, doesn't diverge |
| 6 | Straight-line speed | accelerates smoothly toward PWM 200 |
| 7 | Curves | handles wide→tight + S-curves |
| 8 | Line loss | enters recovery, reacquires |
| 9 | Recovery timeout | stops safely after 1.2 s if not found |
| 10 | Timing | ~5 ms loop, few/no missed deadlines (`d`) |
| 11 | Endurance | stable over repeated laps |
| 12 | Speed optimization | fastest config that still finishes every run |

**Test 4 fixes (edit `config.h`, re-flash — never rewire):**
- Wheel spins backward → `MOTOR_L_INVERT` / `MOTOR_R_INVERT = true`.
- Steers **away** from the line → `STEERING_SIGN = -1.0f`.

Power for motor tests: elevate the robot, connect the **LiPo to VM**, keep the
ESP32 on USB for Serial, and **don't feed the buck's 5 V to VIN while USB is
connected** (two 5 V sources).

---

## 7. Use it

### Serial commands (115200)
| Cmd | Action |
|-----|--------|
| `s` | Start race (only from READY) |
| `x` | Stop immediately |
| `c` | Recalibrate |
| `p` | Print PID parameters |
| `d` | Diagnostics (sensors, motors, timing) |
| `h` | Help |
| `P<v>` / `I<v>` / `D<v>` | Set a PID gain live, e.g. `P28.5` + Enter (validated) |

### Physical button (GPIO34)
- **READY** → press = start. **RUNNING/RECOVERING** → press = stop.
- **STOPPED/ERROR** → press = recalibrate (never leaps back into motion).
- Needs an external 10 kΩ pull-up (GPIO34 is input-only). You can run entirely on
  Serial `s`/`x` without the button.

### Competition mode
Define `COMPETITION_MODE_ON` in `config.h` to silence telemetry in the hot path.

---

## 8. Tuning (the short version)

Start gentle, make it **finish reliably before fast**. Live-tune over Serial,
then copy good values into `config.h`.

1. Low speed, `Ki=0`, `Kd=0`. Raise **`Kp`** until it tracks with slight wobble.
2. Back `Kp` off ~15 %. Add **`Kd`** (start 0.2) to damp the wobble.
3. If jittery on noise, raise `PID_D_FILTER` (0.70→0.80).
4. Raise `PWM_MAX` toward 200 for straights; tune `PWM_CURVE`/`ERR_SHARP_MIN` for turns.
5. Leave `Ki = 0` unless it rides consistently to one side.

Key knobs in `config.h`: `PWM_START` 100, `PWM_MAX` 200, `PWM_CURVE` 100,
`PID_KP/KI/KD` 30/0/0.30, `STEERING_SIGN`, `ENABLE_PREDICTIVE`.
Full method + symptom table: [`PID_TUNING.md`](PID_TUNING.md).

---

## 9. Troubleshooting

### USB / upload
| Symptom | Fix |
|---------|-----|
| "Unknown USB Device (Device Descriptor Request Failed)" | Charge-only cable (use a **data** cable); bad port (rear USB 2.0, no hub); cracked micro-USB jack; if a known-good cable works on another board, **this board's USB is faulty** → replace it or flash via **Method B**. |
| Powered (LED on) but no COM port | Data lines not reaching CP2102 → cable or jack; DDRF is **not** a driver or firmware problem. |
| `ledcAttach not declared` | Wrong core — install **esp32 3.x**. |
| `QTRSensors.h: No such file` | Install the QTRSensors library. |
| Upload "Failed to connect" | Hold **BOOT** during upload; drop upload speed to 115200. |
| Red LED **blinking** / board resets | Brownout loop — weak cable/port, or a short (bridge). With nothing attached and a good cable, suspect the board. |

### Running
| Symptom | Fix |
|---------|-----|
| Garbled Serial | Set monitor to **115200**. |
| `[CAL] FAILED` | Sweep wider/slower; lower array; confirm LEDON→3V3. |
| `conf` high when lifted | Raise `LINE_CONFIDENCE_MIN`; shield from sunlight. |
| Steers away from line | `STEERING_SIGN = -1.0f`. |
| A wheel runs backward | Flip that `MOTOR_*_INVERT`. |
| Oscillates side to side | ↓`Kp`, ↑`Kd` (see PID_TUNING). |
| ESP32 resets when LiPo connected | Brownout — common ground, VM bulk cap, don't run USB + buck together. |
| Many missed deadlines (`d`) | Lower `SENSOR_RC_TIMEOUT_US`; consider QTR-8A; or relax `CONTROL_PERIOD_US`. |

---

## 10. Safety

- **Wheels off the ground** until Test 4 passes.
- A **physical kill switch** on the 12 V/VM line — firmware stop is not a safety guarantee.
- **LiPo:** balance-charge; don't discharge below ~9.9 V (3.3 V/cell); never short + to −; never solder with it connected.
- Keep **VM (motor)** and **logic** supplies separate; all grounds common.
- Never put 5 V on a 3.3 V-only ESP32 pin; never 12 V on ESP32 VIN.

---

## 11. Where everything is

- Firmware: `ESP32_High_Speed_Line_Follower.ino` + `config.h` + modules
- Wiring & power: `WIRING.md`, `docs/connection_map.svg`, `docs/wiring_diagram.svg`,
  `docs/wiring_breadboard.svg`, `docs/power_path.svg`, `docs/soldering_guide.svg`
- Build/mechanical: `docs/robot_layout.svg`, `docs/build_spec.svg`, interactive 3D model
- Procedures: `CALIBRATION.md`, `TEST_PLAN.md`, `PID_TUNING.md`
- History: `CHANGELOG.md`

Tip: tune and race only after Tests 1–5 pass. Reliable completion first, speed second.
