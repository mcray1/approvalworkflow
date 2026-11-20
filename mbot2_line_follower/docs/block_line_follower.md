# mBot2 Line Follower — block version (mBlock 5)

A simple, no-PID line follower built from blocks so it's easy to edit. Follow the
picture (`block_line_follower.png`) and this recipe. ~5 minutes to assemble.

> mBlock can't be handed a ready-made file reliably here, so this is a rebuild
> recipe. In mBlock: add the **mBot2** device, add the **Quad RGB Sensor**
> extension, and switch to **block** mode (not Python).

## One-time setup
1. Connect the mBot2, add it as the device.
2. In **Extensions**, add the **Quad RGB Line-Following Sensor**.
3. Under **Variables**, click *Make a Variable* → name it **`offset`**.

## Build the script (top to bottom)

1. **Events** → `when CyberPi starts up`  *(the hat block on top)*
2. **Control** → `forever`  *(wrap everything below inside it)*
   Inside the forever:
   1. **Control** → `wait until < >` — drop **CyberPi → `button [A] pressed?`** into the slot.
   2. **CyberPi** → `CyberPi print [RUN]`
   3. **Control** → `repeat until < >` — drop **`button [B] pressed?`** into the slot.
      Inside the repeat:
      1. **Variables** → `set [offset] to ( )` — drop **Quad RGB Sensor → `quad rgb sensor [1] line-following offset`** into the ( ) slot.
      2. **Control** → `if < > then / else`
         - Condition: **Operators** → `( ) > ( )` with `offset` on the left and `20` on the right.
         - **then** branch: **mBot2** → `drive left wheel [40] right wheel [12]`  *(turn right)*
         - **else** branch: another **`if < > then / else`**
           - Condition: **Operators** → `( ) < ( )` with `offset` and `-20`.
           - **then**: **mBot2** → `drive left wheel [12] right wheel [40]`  *(turn left)*
           - **else**: **mBot2** → `drive left wheel [40] right wheel [40]`  *(straight)*
   4. **mBot2** → `drive left wheel [0] right wheel [0]`  *(stop after B)*
   5. **CyberPi** → `CyberPi print [STOP]`

That's it — press **A** to follow, **B** to stop.

## Notes on the exact block names
Block wording varies a little by mBlock version. Look for:
- **Drive block (mBot2 palette):** the one that sets **left and right wheel speed**
  together (two number slots). It's the block equivalent of `drive_speed`.
- **Offset block (Quad RGB Sensor palette):** the **“line-following offset / deviation”**
  reporter (a rounded block that returns roughly −100…+100). If you only see a
  **“line station / detected”** block, use the offset/deviation one for this script.
- **Button block (CyberPi palette):** `button [A] pressed?` (hexagon).

## Tuning (just change numbers)
- **Speed:** raise the `40`s (e.g. 50–60) to go faster.
- **Turn sharpness:** the small numbers (`12`) are the inner wheel — lower them
  (or make them negative, e.g. `-15`) for tighter turns.
- **Sensitivity:** `20` / `-20` decide when it starts turning — smaller = reacts
  sooner (twitchier), larger = smoother but lazier.
- **Wrong direction?** swap the two numbers in the *turn right* and *turn left*
  drive blocks.

## Make it smoother later
This "3-move" version wobbles a bit (it's bang-bang control). When you want it
smoother/faster, the Python **PD** version (`mbot2_fast_line_follower.py`) uses
the same idea but scales the turn to how far off the line is.
