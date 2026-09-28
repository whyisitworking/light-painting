# Bringing up a board

A new board, or one rebuilt, checked in steps: each one relies on those before it, so a fault shows up where it is and does not hide behind another. Flash a build with `-DPRINT_DIAGNOSTICS=ON -DWAIT_FOR_USB_HOST=ON` and keep a USB serial monitor open: the startup messages and, every 0.5 s, the [diagnostics](../README.md#diagnostics).

## 1. The board alone, on USB

Nothing connected but USB.

- **See:** the startup messages, ending in `LCD init!`. The status screen upright, colours as named (Synthwave's swatch runs indigo, violet, pink, orange, cyan), no noise along an edge. Screen, under System, changes the backlight.
- **If not:** see Troubleshooting in the README: "The LCD stays dark", "upside down or mirrored".

## 2. The switch

Wired to GP0-GP4 and ground.

- **See:** centre opens the menu; up and down move the highlight; right opens a page; left or centre held goes back. Holding left or right on a setting repeats.
- **If not:** swap the `JOYSTICK_*_PIN` numbers in `app/config.h`.

## 3. Diagnostics, without microphones

System › Diagnostics.

- **See:** both microphones read "none". Audio lost 0, and it stays 0. Load well under 80 %. Menu and Lights stacks well under their sizes.
- **If not:**
  - Audio lost counting up means core 0 falls behind: note the Load and report it.
  - A stack near its size needs a bigger one: `UI_STACK_SIZE` for the menu.

## 4. The microphones

Both on the bus, one with L/R to ground, the other to 3.3 V.

- **See:** in a quiet room, both about -85 dBFS. Talking moves both meters, to about -60. A finger over one microphone's hole drops only its own meter.
- **If not:**
  - "none" on one side: that microphone's L/R pin, or its SD wire.
  - The same meter moving for both: both L/R pins are wired alike.
  - Both "none": SCK, WS or SD, or the microphones' power.

## 5. A short strip

A few LEDs, through the level shifter, on the strip's 5 V supply.

- **See:** the first LEDs follow the sound (the defaults light the middle of a 300 LED strip, so play something loud or use Spectrum mode). The colours are those of the palette. Diagnostics: LEDs about 150 fps.
- **If not:** see "The strip stays dark", "Colours are swapped" and "Random colours or glitches" in the README.

## 6. The full strip

All 300 LEDs, on their own supply, grounds joined.

- **See:** brightness from 10 % to 100 % under Look. The beat flash on kicks. No flicker in silence.
- **If not:** voltage drop along the strip shows as colours shifting towards red at the far end: feed power at both ends.

## 7. Saving

- **See:** change a setting, wait for "Saved" on the status screen, cut the power, and the setting is back after power-up.
- **If not:** "Not saved" means the flash refused. See "The settings are back to their defaults" in the README.

## 8. Tuning, with music

- **Quiet floor (Sound):** silence dark, quiet music still lit. In a quiet room the diagnostics read loudness about 0 and no beats.
- **Beat threshold (Sound):** beats on the kicks, none on the rest.
- **Brightness floor:** whether 10 % is a useful lowest brightness, or it should be 20-30 %. An open decision.

## 9. Soak

An hour with music playing.

- **See:** Audio lost still 0, the stacks' peaks where they were, the menu responsive.
