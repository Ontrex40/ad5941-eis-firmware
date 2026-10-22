# AD5941 EIS firmware (EVAL-AD5941ELCZ + EVAL-ADICUP3029)

Terminal-controlled electrical impedance spectroscopy (EIS) firmware for the
**EVAL-AD5941ELCZ** board driven by the **ADuCM3029** microcontroller of the
**EVAL-ADICUP3029** platform. The frequency sweep, the reference resistor, the
electrode wiring and the transimpedance gain (RTIA) and excitation amplitude are
set from a serial terminal; the impedance magnitude and phase are printed one line
per frequency point.

It was developed for a bachelor thesis on EIS measurements of fruit tissue
(Free University of Bozen-Bolzano) and is based on the Analog Devices example
`AD5940_Impedance_Adjustable_with_frequency`.

## Repository layout

```
AD5940Lib/                    Analog Devices AD5940 library (git submodule, see below)
AD5941_EIS_Firmware/
  AD5940Main.c                UART terminal, parameter handling, measurement control
  Impedance.c / Impedance.h   Impedance application (ADI example, modified)
  ADICUP3029/                 Platform port and Keil uVision project for the ADICUP3029
    main.c, ADICUP3029Port.c  ADI platform files (main.c modified)
    AD5940_Impedance.uvprojx  Keil project
    RTE/                      Keil run-time environment configuration
LICENSE_ADI.txt               Analog Devices source code license
```

The Keil project expects the library at `..\..\AD5940Lib` relative to the
`ADICUP3029` folder, i.e. `AD5940Lib/` next to `AD5941_EIS_Firmware/`.

## Build and flash

1. Clone with the library submodule:
   ```
   git clone --recurse-submodules <repository-url>
   ```
   (or, in an existing clone:
   `git submodule add https://github.com/analogdevicesinc/ad5940lib.git AD5940Lib`)
2. Open `AD5941_EIS_Firmware/ADICUP3029/AD5940_Impedance.uvprojx` in Keil uVision 5
   (the ADuCM3029 device family pack must be installed). The project defines
   `CHIPSEL_594X`.
3. Build and flash through the on-board debugger of the EVAL-ADICUP3029.

## Use

Open the ADICUP3029 virtual COM port in a serial terminal at **230400 baud, 8-N-1**.
The firmware prints the command list and the current parameters, then waits for commands.

| Command | Function |
|---|---|
| `fstart <Hz>`, `fstop <Hz>` | Start and stop frequency (0.1 Hz – 200 kHz) |
| `points <n>` | Number of points (2 – 200) |
| `sweep <start> <stop> <n> [lin\|log]` | Set start, stop, points and type together |
| `log <0\|1>` | Linear (0) or logarithmic (1) sweep |
| `rtia <auto\|200\|1k\|5k\|10k\|20k\|40k\|80k\|160k>` | Fixed RTIA, or automatic per band |
| `rtiaband <0..4> <value>` | RTIA of a band (used with `rtia auto`) |
| `vpp <auto\|1..800>` | Excitation amplitude (nominal mV), fixed or automatic per band |
| `vppband <0..4> <1..800>` | Amplitude of a band (used with `vpp auto`) |
| `rcal <ohm>` | Reference resistor (10 kΩ on the EVAL-AD5941ELCZ) |
| `hann <0\|1>` | Hanning window on the DFT |
| `sw <conn\|ain\|2el>` | Electrode wiring (see below) |
| `show` / `help` / `go` | Show parameters / command list / start the measurement |

Numbers accept the suffix `k` (e.g. `200k`). Default parameters: 0.1 Hz – 200 kHz,
100 logarithmic points, RCAL 10 kΩ, RTIA 20 kΩ, amplitude 800, Hanning window on, wiring `conn`.

### Frequency bands

RTIA and amplitude can be selected automatically per frequency band. The band
boundaries come from the ADI example. Default values (change them with `rtiaband`
and `vppband`):

| Band | Frequency | RTIA | Amplitude (nominal mV) |
|---|---|---|---|
| 0 | f < 0.51 Hz | 160 kΩ | 800 |
| 1 | 0.51 ≤ f < 5 Hz | 80 kΩ | 160 |
| 2 | 5 ≤ f < 450 Hz | 20 kΩ | 200 |
| 3 | 450 Hz ≤ f < 80 kHz | 5 kΩ | 40 |
| 4 | f ≥ 80 kHz | 1 kΩ | 10 |

Automatic selection is **off** at start-up (`rtia auto` / `vpp auto` enable it).
From 80 kHz the AD5941 runs in high-power mode (32 MHz clock, 1.6 MSPS ADC).

### Electrode wiring

| Mode | Switch matrix (D / P / N / T) | Description |
|---|---|---|
| `conn` | CE0 / RE0 / SE0 / DE0 | Connector, four electrodes: CE0 drive, current returned through DE0, voltage sensed between RE0 and SE0 |
| `2el` | CE0 / CE0 / SE0 / SE0LOAD | Connector, two electrodes (CE0 → SE0) |
| `ain` | CE0 / CE0 / AIN1 / AIN1 | Two-wire path CE0 → AIN1 |

With the USB-to-crocodile cable of the EVAL-AD5941ELCZ the leads are red = CE0
(counter), blue/white = RE0 (reference), green = SE0 (working/sense) and black = DE0
(see the Analog Devices user guide). Jumpers JP9, JP10 and JP11 must be in
position C. Clamp CE0 and RE0 to one terminal of the DUT and SE0 and DE0 to the
other.

### Output

```
start
Formato: Freq (Hz), RTIA (ohm), Vpp, RzMag (ohm), RzPhase (gradi)
Freq: <Hz> , RTIA: <ohm> , Vpp: <mV> , RzMag: <ohm> Ohm , RzPhase: <deg>
...
Sweep finito
```

`start`, the `Formato` line and `Sweep finito` are kept in this form because
capture scripts may rely on them. The impedance is computed from the ratio of the
DFT of the sample and of RCAL measured at every point:
`|Z| = RCAL · |DFT_cal| / |DFT_Z|`, `φ = φ_cal − φ_Z`.

## Known limitations

- The T6 path used by `conn` does not pass through the RLOAD_SE0 resistor used by
  the T5 path of `2el`. Stability at high frequency with long leads has not been
  assessed.
- The amplitude values are nominal. The physical excitation amplitude also depends on
  the excitation-buffer and HSDAC gains set in `Impedance.c`; check it (for example
  with an oscilloscope) before quoting it in volts.
- The terminal accepts frequencies from 0.1 Hz. Measurements below 0.1 Hz were found
  unreliable in the thesis tests.
- `RunMeasurement()` waits for the AD5941 FIFO interrupt flag without a timeout.

## Changes with respect to the Analog Devices example

`Impedance.c`, `Impedance.h` and `ADICUP3029/main.c` are modified versions of the
Analog Devices files and are marked as such in their headers.

- **Impedance.c / Impedance.h:** RTIA and excitation amplitude are selected per
  frequency band (`PickBand`, `PickRtia`, `PickVpp`, `RtiaBandSel`, `VppBandVal`,
  `RtiaAutoEn`, `VppAutoEn`) instead of the fixed values of the example; HSDAC and
  excitation-buffer gains of the two lowest bands follow `AppIMPCfg`; default gains,
  RTIA and sweep values changed; the configuration structures are cleared in
  `AppIMPCheckFreq()`.
- **ADICUP3029/main.c:** UART0 interrupt removed; the terminal reads the UART in polling mode.
- **AD5940Main.c:** replaced by the terminal application of this repository.

## License

The Analog Devices files and the modified versions of them are subject to the
Analog Devices Source Code Software License Agreement (`LICENSE_ADI.txt`); any
distribution must include a copy of that license. `AD5940Main.c` is the work of the
author; since it is built together with the Analog Devices code, the combined firmware
remains subject to the Analog Devices license, and it may be used only with
processors and products manufactured by or for Analog Devices.
