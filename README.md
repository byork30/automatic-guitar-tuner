# Automatic Guitar Tuner

A handheld, standalone guitar tuner that listens to a plucked string, identifies
which string it is, and physically turns the tuning peg for you using a stepper
motor; no app, no screen, just play and let it tune itself.

## How it works

**1. Listening**
An electret microphone feeds into an analog input, which is sampled at 9 kHz.
Each read cycle skips the first ~1800 samples to let the initial pluck transient
decay before capturing a clean steady-state buffer of 512 samples.

**2. Pitch detection**
Pitch is estimated using autocorrelation: the signal is checked against itself
at a range of time lags, and the lag with the strongest self-similarity gives
the fundamental period (and therefore frequency). To avoid the classic
autocorrelation failure mode of locking onto an octave harmonic, the algorithm
also checks the half-lag candidate and switches to it if it's nearly as strong
as the best match. A simple RMS noise gate ignores buffers that are just
ambient noise with no real signal.

**3. String identification & smoothing**
The detected frequency is matched against a reference table of the six
standard guitar string pitches (E2, A2, D3, G3, B3, E4) to figure out which
string was just played. Once a string is identified, a rolling average of the
last several readings smooths out sample-to-sample jitter before any tuning
decision is made, and the average resets whenever a different string is
detected.

**4. Feedback and correction**
An RGB LED gives at-a-glance status:
- **Green** — in tune (within tolerance)
- **Yellow** — flat
- **Orange** — sharp

If the string is out of tune, a 4-pin stepper motor is driven a fixed number
of steps in the appropriate direction to turn the tuning peg, nudging the
string toward pitch. The process repeats continuously, so the tuner keeps
correcting as the string approaches the target frequency.

## Hardware

- Microcontroller: ATmega-based board (e.g. Arduino)
- Electret microphone (analog input)
- 4-pin stepper motor for peg rotation
- RGB LED for tuning status
- Custom 3D-printed mounting parts (see `/cad`) to hold the tuner against the
  headstock and couple the motor to the tuning peg

## Files

- `Tuner.ino` — firmware: audio sampling, pitch detection, string ID, LED and
  motor control
- `cad/*.stl` — 3D-printable parts for the housing and peg coupler

## Notes / limitations

- Tuned for standard 6-string guitar tuning; other tunings or instruments
  would need a new reference table.
- Detection assumes a clean, individually plucked string — heavy background
  noise or multiple simultaneous notes will be rejected by the noise gate.

## Possible improvements

- Replace the fixed step-count correction with a proportional step count
  based on how far off the frequency is
- Support alternate tunings via a mode switch
- Swap Serial debug output for an on-device display
