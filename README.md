# AD5941 EIS firmware

Firmware for doing impedance spectroscopy from a serial terminal, on the EVAL-AD5941ELCZ board plugged into the EVAL-ADICUP3029 (ADuCM3029 MCU). You set the sweep, reference resistor, electrode wiring, RTIA and excitation amplitude over serial, and it prints magnitude and phase, one line per frequency point.

I wrote it for my bachelor thesis on EIS of fruit tissue at the Free University of Bozen-Bolzano. It started from the Analog Devices example `AD5940_Impedance_Adjustable_with_frequency` and I changed it quite a bit.

## What's in the repo

```
AD5940Lib/                    ADI's AD5940 library (git submodule)
AD5941_EIS_Firmware/
  AD5940Main.c                UART terminal, parameters, measurement control
  Impedance.c / Impedance.h   Impedance app (ADI example, modified)
  ADICUP3029/                 Platform port + Keil uVision project
    main.c, ADICUP3029Port.c  ADI platform files (main.c is modified)
    AD5940_Impedance.uvprojx  Keil project
    RTE/                      Keil run-time environment config
LICENSE_ADI.txt               Analog Devices source code license
```

The Keil project looks for the library at `..\..\AD5940Lib` from the `ADICUP3029` folder, so `AD5940Lib/` has to sit next to `AD5941_EIS_Firmware/`.

## Building and flashing

1. Clone with submodules:
   ```
   git clone --recurse-submodules <repository-url>
   ```
   If you already cloned without them:
   `git submodule add https://github.com/analogdevicesinc/ad5940lib.git AD5940Lib`
2. Open `AD5941_EIS_Firmware/ADICUP3029/AD5940_Impedance.uvprojx` in Keil uVision 5. You need the ADuCM3029 device family pack installed. The project already defines `CHIPSEL_594X`.
3. Build and flash through the debugger on the EVAL-ADICUP3029 itself.

## Using it

Connect to the ADICUP3029 virtual COM port at 230400 baud, 8-N-1. On startup it prints the command list and the current parameters, then waits.

| Command | What it does |
|---|---|
| `fstart <Hz>`, `fstop <Hz>` | Start and stop frequency (0.1 Hz to 200 kHz) |
| `points <n>` | Number of points (2 to 200) |
| `sweep <start> <stop> <n> [lin\|log]` | Set start, stop, points and type in one go |
| `log <0\|1>` | 0 = linear, 1 = logarithmic |
| `rtia <auto\|200\|1k\|5k\|10k\|20k\|40k\|80k\|160k>` | Fixed RTIA, or automatic per band |
| `rtiaband <0..4> <value>` | RTIA for one band (used with `rtia auto`) |
| `vpp <auto\|1..800>` | Excitation amplitude (nominal mV), fixed or automatic per band |
| `vppband <0..4> <1..800>` | Amplitude for one band (used with `vpp auto`) |
| `rcal <ohm>` | Reference resistor (10 kΩ on the EVAL-AD5941ELCZ) |
| `hann <0\|1>` | Hanning window on the DFT |
| `sw <conn\|ain\|2el>` | Electrode wiring, see below |
| `show` / `help` / `go` | Show parameters / list commands / start measuring |

You can write numbers with a `k` suffix (`200k`). Defaults: 0.1 Hz to 200 kHz, 100 log points, RCAL 10 kΩ, RTIA 20 kΩ, amplitude 800, Hanning on, wiring `conn`.

### Frequency bands

RTIA and amplitude can be picked automatically depending on the frequency. The band edges are the ones from the ADI example. The defaults below can be changed with `rtiaband` and `vppband`.

| Band | Frequency | RTIA | Amplitude (nominal mV) |
|---|---|---|---|
| 0 | f < 0.51 Hz | 160 kΩ | 800 |
| 1 | 0.51 ≤ f < 5 Hz | 80 kΩ | 160 |
| 2 | 5 ≤ f < 450 Hz | 20 kΩ | 200 |
| 3 | 450 Hz ≤ f < 80 kHz | 5 kΩ | 40 |
| 4 | f ≥ 80 kHz | 1 kΩ | 10 |

Automatic selection is off at startup. Turn it on with `rtia auto` and/or `vpp auto`. From 80 kHz the AD5941 switches to high-power mode (32 MHz clock, 1.6 MSPS ADC).

### Electrode wiring

| Mode | Switch matrix (D / P / N / T) | Notes |
|---|---|---|
| `conn` | CE0 / RE0 / SE0 / DE0 | Connector, four electrodes. CE0 drives, current returns through DE0, voltage is sensed between RE0 and SE0 |
| `2el` | CE0 / CE0 / SE0 / SE0LOAD | Connector, two electrodes (CE0 to SE0) |
| `ain` | CE0 / CE0 / AIN1 / AIN1 | Two-wire path CE0 to AIN1 |

With the USB-to-crocodile cable that comes with the EVAL-AD5941ELCZ, the leads are: red = CE0 (counter), blue/white = RE0 (reference), green = SE0 (working/sense), black = DE0 (see the ADI user guide). Jumpers JP9, JP10 and JP11 need to be in position C. Clamp CE0 and RE0 to one side of the sample and SE0 and DE0 to the other.

### Output

```
start
Formato: Freq (Hz), RTIA (ohm), Vpp, RzMag (ohm), RzPhase (gradi)
Freq: <Hz> , RTIA: <ohm> , Vpp: <mV> , RzMag: <ohm> Ohm , RzPhase: <deg>
...
Sweep finito
```

Please don't change `start`, the `Formato` line or `Sweep finito`, because my capture scripts may depend on them. At every point the impedance comes from the ratio between the DFT of the sample and the DFT of RCAL:
`|Z| = RCAL · |DFT_cal| / |DFT_Z|`, `φ = φ_cal − φ_Z`.

## Things to watch out for

- The T6 path used by `conn` doesn't go through the RLOAD_SE0 resistor that the T5 path of `2el` uses. I haven't checked how stable it is at high frequency with long leads.
- The amplitude values are nominal. The real excitation amplitude also depends on the excitation-buffer and HSDAC gains set in `Impedance.c`, so measure it (with a scope, for example) before you quote it in volts.
- The terminal accepts frequencies down to 0.1 Hz, but in my thesis tests anything below that was unreliable.
- `RunMeasurement()` waits for the AD5941 FIFO interrupt flag with no timeout, so it can hang if the flag never comes.

## What I changed from the ADI example

`Impedance.c`, `Impedance.h` and `ADICUP3029/main.c` are modified ADI files, and their headers say so.

- **Impedance.c / Impedance.h:** RTIA and amplitude are now chosen per frequency band (`PickBand`, `PickRtia`, `PickVpp`, `RtiaBandSel`, `VppBandVal`, `RtiaAutoEn`, `VppAutoEn`) instead of using fixed values. The HSDAC and excitation-buffer gains of the two lowest bands follow `AppIMPCfg`. I also changed the default gains, RTIA and sweep values, and the config structures are now cleared in `AppIMPCheckFreq()`.
- **ADICUP3029/main.c:** I removed the UART0 interrupt. The terminal reads the UART by polling.
- **AD5940Main.c:** replaced entirely by the terminal application in this repo.

## License

The ADI files, and my modified versions of them, fall under the Analog Devices Source Code Software License Agreement (`LICENSE_ADI.txt`). If you distribute them, include a copy of that license. `AD5940Main.c` is my own work, but it's built together with the ADI code, so the combined firmware stays under the ADI license and can only be used with processors and products made by or for Analog Devices.
