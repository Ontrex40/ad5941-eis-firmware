/*!
 *****************************************************************************
 @file:    AD5940Main.c
 @brief:   Impedance spectroscopy terminal application for the EVAL-AD5941ELCZ
           board on the EVAL-ADICUP3029 platform (ADuCM3029).

           The frequency sweep, RCAL value, DFT window and electrode wiring
           are set from a UART terminal (230400 baud). RTIA and excitation
           amplitude can be fixed or selected automatically per frequency band
           (see Impedance.c). The measured impedance is printed one line per
           frequency point.

 @author:  Bilal Mohammed
 @note:    Written for a bachelor thesis (Free University of Bozen-Bolzano).
           Uses the Analog Devices AD5940 library and the structure of the
           "AD5940_Impedance_Adjustable_with_frequency" example; see README.md
           and LICENSE_ADI.txt for the licensing of the Analog Devices parts.
 *****************************************************************************/
#include "Impedance.h"
#include "ADuCM3029.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define APPBUFF_SIZE      512
#define MAX_SWEEP_POINTS  200
#define LINE_MAX_LEN      80

#define FREQ_MIN_HZ       0.1f      /* lowest frequency accepted by the terminal */
#define FREQ_MAX_HZ       200000.0f /* highest frequency of the AD5941           */
#define VPP_MIN           1.0f      /* excitation amplitude limits (nominal mV)  */
#define VPP_MAX           800.0f

static uint32_t AppBuff[APPBUFF_SIZE];
static uint32_t PointCnt = 0;

/* ------------------------------------------------------------------ */
/*  User parameters                                                    */
/* ------------------------------------------------------------------ */
typedef struct { const char *name; uint32_t sel; } RtiaEntry;
static const RtiaEntry RtiaTab[] =
{
  {"200",  HSTIARTIA_200},
  {"1k",   HSTIARTIA_1K},
  {"5k",   HSTIARTIA_5K},
  {"10k",  HSTIARTIA_10K},
  {"20k",  HSTIARTIA_20K},
  {"40k",  HSTIARTIA_40K},
  {"80k",  HSTIARTIA_80K},
  {"160k", HSTIARTIA_160K},
};
#define RTIA_COUNT (sizeof(RtiaTab)/sizeof(RtiaTab[0]))

static const char *BandName[IMP_NUM_BANDS] =
{
  "f < 0.51", "0.51 <= f < 5", "5 <= f < 450", "450 <= f < 80k", "f >= 80k"
};

/* Electrode wiring of the AD5941 switch matrix (command "sw"). */
#define WIRING_CONN  0   /* CE0 drive, voltage sensed RE0-SE0, return on DE0 */ 
#define WIRING_AIN   1   /* two-wire path CE0 -> AIN1                        */
#define WIRING_2EL   2   /* two-wire path CE0 -> SE0 on the connector        */

static const char *WiringName[] =
{
  "conn (connector: CE0 drive, RE0/SE0 sense, DE0 return)",
  "ain (CE0 -> AIN1, two-wire)",
  "2el (connector: CE0 -> SE0, two-wire)"
};

typedef struct
{
  float    fStart;     /* Hz */
  float    fStop;      /* Hz */
  uint32_t points;
  int      logSweep;   /* 1 = logarithmic, 0 = linear */
  float    rcal;       /* ohm */
  float    vpp;        /* nominal DAC amplitude, 1..800 (used when vpp is not auto) */
  int      rtiaIdx;    /* index in RtiaTab (used when RTIA is not auto) */
  int      hanWin;     /* 1 = Hanning window on the DFT */
  int      wiring;     /* WIRING_CONN, WIRING_AIN or WIRING_2EL */
} UserCfg_Type;

static UserCfg_Type UserCfg =
{
  .fStart   = FREQ_MIN_HZ,
  .fStop    = FREQ_MAX_HZ,
  .points   = 100,
  .logSweep = 1,
  .rcal     = 10000.0f,   /* RCAL of the EVAL-AD5941ELCZ */
  .vpp      = 800.0f,
  .rtiaIdx  = 4,          /* "20k" */
  .hanWin   = 1,
  .wiring   = WIRING_CONN
};

/* ------------------------------------------------------------------ */
/*  UART (polling)                                                     */
/* ------------------------------------------------------------------ */
static char UartGetCharBlocking(void)
{
  while((pADI_UART0->COMLSR & 0x01) == 0);   /* bit 0 = data ready */
  return (char)pADI_UART0->COMRX;
}

/* Reads one line with echo and backspace. Handles terminals sending CR+LF. */
static int ReadLine(char *buf, int maxlen)
{
  static char lastWasCR = 0;
  int n = 0;
  char c;
  while(1)
  {
    c = UartGetCharBlocking();
    if(c == '\n' && lastWasCR && n == 0)   /* LF right after CR: ignore */
    {
      lastWasCR = 0;
      continue;
    }
    lastWasCR = (c == '\r');
    if(c == '\r' || c == '\n')
    {
      printf("\n");
      buf[n] = 0;
      return n;
    }
    if((c == 0x08 || c == 0x7F) && n > 0)
    {
      n--;
      printf("\b \b");
      fflush(stdout);
      continue;
    }
    if(c >= 32 && c < 127 && n < maxlen - 1)
    {
      buf[n++] = c;
      printf("%c", c);
      fflush(stdout);
    }
  }
}

/* ------------------------------------------------------------------ */
/*  Parsing                                                            */
/* ------------------------------------------------------------------ */
static void ToLower(char *s)
{
  for(; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* Number with optional 'k' suffix (x1000), e.g. 200k, 10k, 0.5. Returns 1 on success. */
static int ParseNum(const char *s, float *out)
{
  char *end;
  float v;
  if(!s) return 0;
  v = strtof(s, &end);
  if(end == s) return 0;
  if(*end == 'k' && end[1] == 0) { v *= 1000.0f; end++; }
  if(*end != 0) return 0;
  *out = v;
  return 1;
}

/* Band index 0..IMP_NUM_BANDS-1. Returns 1 on success. */
static int ParseBand(const char *s, int *band)
{
  char *end;
  long b;
  if(!s) return 0;
  b = strtol(s, &end, 10);
  if(end == s || *end != 0 || b < 0 || b >= IMP_NUM_BANDS) return 0;
  *band = (int)b;
  return 1;
}

static int FindRtia(const char *s)
{
  for(unsigned i = 0; i < RTIA_COUNT; i++)
    if(strcmp(s, RtiaTab[i].name) == 0) return (int)i;
  return -1;
}

static const char *RtiaNameFromSel(uint32_t sel)
{
  for(unsigned i = 0; i < RTIA_COUNT; i++)
    if(RtiaTab[i].sel == sel) return RtiaTab[i].name;
  return "?";
}

static void PrintCfg(void)
{
  printf("\n--- Current parameters ---\n");
  printf("fstart : %.3f Hz\n", UserCfg.fStart);
  printf("fstop  : %.3f Hz\n", UserCfg.fStop);
  printf("points : %lu\n", (unsigned long)UserCfg.points);
  printf("sweep  : %s\n", UserCfg.logSweep ? "log" : "lin");
  printf("rtia   : %s\n", RtiaAutoEn ? "auto (per band)" : RtiaTab[UserCfg.rtiaIdx].name);
  if(VppAutoEn) printf("vpp    : auto (per band)\n");
  else          printf("vpp    : %.1f (%.0f..%.0f)\n", UserCfg.vpp, VPP_MIN, VPP_MAX);
  if(RtiaAutoEn || VppAutoEn)
  {
    for(int b = 0; b < IMP_NUM_BANDS; b++)
      printf("  band %d (%-15s): rtia %s ohm, vpp %.0f\n", b, BandName[b],
             RtiaNameFromSel(RtiaBandSel[b]), VppBandVal[b]);
  }
  printf("rcal   : %.1f ohm\n", UserCfg.rcal);
  printf("hann   : %d\n", UserCfg.hanWin);
  printf("sw     : %s\n", WiringName[UserCfg.wiring]);
  printf("--------------------------\n");
}

static void PrintHelp(void)
{
  printf("\nCommands (numbers accept the k suffix, e.g. 200k):\n");
  printf("  fstart <Hz>                 start frequency (%.1f .. %.0f)\n", FREQ_MIN_HZ, FREQ_MAX_HZ);
  printf("  fstop <Hz>                  stop frequency\n");
  printf("  points <n>                  number of points (2..%d)\n", MAX_SWEEP_POINTS);
  printf("  sweep <start> <stop> <n> [lin|log]   set start, stop, points and type together\n");
  printf("  log <0|1>                   0 = linear, 1 = logarithmic\n");
  printf("  rtia <auto|200|1k|5k|10k|20k|40k|80k|160k>\n");
  printf("  rtiaband <0..4> <value>     RTIA of a band (used with rtia auto)\n");
  printf("  vpp <auto|%.0f..%.0f>          excitation amplitude, nominal mV (auto = per band)\n", VPP_MIN, VPP_MAX);
  printf("  vppband <0..4> <%.0f..%.0f>    amplitude of a band (used with vpp auto)\n", VPP_MIN, VPP_MAX);
  printf("                              bands 0:<0.51  1:<5  2:<450  3:<80k  4:>=80k Hz\n");
  printf("  rcal <ohm>                  reference resistor (10k on the EVAL board)\n");
  printf("  hann <0|1>                  Hanning window on the DFT\n");
  printf("  sw <conn|ain|2el>           electrode wiring\n");
  printf("  show                        show the parameters\n");
  printf("  go                          start the measurement\n");
  printf("  help                        this list\n");
  printf("Rule: large Z -> large RTIA; small Z -> small RTIA and lower amplitude.\n");
}

/* Returns 1 if the user asked to start the measurement. */
static int HandleCommand(char *line)
{
  char *cmd, *a1, *a2, *a3, *a4;
  float v, v2, v3;
  int b;

  ToLower(line);
  cmd = strtok(line, " \t");
  if(!cmd) return 0;
  a1 = strtok(NULL, " \t");
  a2 = strtok(NULL, " \t");
  a3 = strtok(NULL, " \t");
  a4 = strtok(NULL, " \t");

  if(!strcmp(cmd, "go") || !strcmp(cmd, "start"))
  {
    if(UserCfg.fStart >= UserCfg.fStop)
      printf("Error: fstart must be lower than fstop.\n");
    else
      return 1;
  }
  else if(!strcmp(cmd, "show"))   PrintCfg();
  else if(!strcmp(cmd, "help") || !strcmp(cmd, "?")) PrintHelp();
  else if(!strcmp(cmd, "fstart"))
  {
    if(ParseNum(a1, &v) && v >= FREQ_MIN_HZ && v <= FREQ_MAX_HZ) UserCfg.fStart = v;
    else printf("Invalid value (%.1f .. %.0f Hz)\n", FREQ_MIN_HZ, FREQ_MAX_HZ);
  }
  else if(!strcmp(cmd, "fstop"))
  {
    if(ParseNum(a1, &v) && v >= FREQ_MIN_HZ && v <= FREQ_MAX_HZ) UserCfg.fStop = v;
    else printf("Invalid value (%.1f .. %.0f Hz)\n", FREQ_MIN_HZ, FREQ_MAX_HZ);
  }
  else if(!strcmp(cmd, "points"))
  {
    if(ParseNum(a1, &v) && v >= 2 && v <= MAX_SWEEP_POINTS) UserCfg.points = (uint32_t)v;
    else printf("Invalid value (2 .. %d)\n", MAX_SWEEP_POINTS);
  }
  else if(!strcmp(cmd, "sweep"))
  {
    if(ParseNum(a1, &v) && ParseNum(a2, &v2) && ParseNum(a3, &v3) &&
       v  >= FREQ_MIN_HZ && v  <= FREQ_MAX_HZ &&
       v2 >= FREQ_MIN_HZ && v2 <= FREQ_MAX_HZ &&
       v3 >= 2 && v3 <= MAX_SWEEP_POINTS)
    {
      UserCfg.fStart = v;
      UserCfg.fStop = v2;
      UserCfg.points = (uint32_t)v3;
      if(a4)
      {
        if(!strcmp(a4, "lin")) UserCfg.logSweep = 0;
        else if(!strcmp(a4, "log")) UserCfg.logSweep = 1;
        else printf("Invalid sweep type, keeping the previous one.\n");
      }
    }
    else printf("Usage: sweep <start> <stop> <n> [lin|log]\n");
  }
  else if(!strcmp(cmd, "log"))
  {
    if(a1 && (!strcmp(a1, "0") || !strcmp(a1, "1"))) UserCfg.logSweep = (a1[0] == '1');
    else printf("Usage: log <0|1>\n");
  }
  else if(!strcmp(cmd, "rtia"))
  {
    if(a1 && !strcmp(a1, "auto")) RtiaAutoEn = 1;
    else
    {
      int idx = a1 ? FindRtia(a1) : -1;
      if(idx >= 0) { UserCfg.rtiaIdx = idx; RtiaAutoEn = 0; }
      else printf("Invalid RTIA. Values: auto 200 1k 5k 10k 20k 40k 80k 160k\n");
    }
  }
  else if(!strcmp(cmd, "rtiaband"))
  {
    int idx = a2 ? FindRtia(a2) : -1;
    if(ParseBand(a1, &b) && idx >= 0) RtiaBandSel[b] = RtiaTab[idx].sel;
    else printf("Usage: rtiaband <0..4> <200|1k|5k|10k|20k|40k|80k|160k>\n");
  }
  else if(!strcmp(cmd, "vpp"))
  {
    if(a1 && !strcmp(a1, "auto")) VppAutoEn = 1;
    else if(ParseNum(a1, &v) && v >= VPP_MIN && v <= VPP_MAX) { UserCfg.vpp = v; VppAutoEn = 0; }
    else printf("Invalid value (auto or %.0f .. %.0f)\n", VPP_MIN, VPP_MAX);
  }
  else if(!strcmp(cmd, "vppband"))
  {
    if(ParseBand(a1, &b) && ParseNum(a2, &v) && v >= VPP_MIN && v <= VPP_MAX) VppBandVal[b] = v;
    else printf("Usage: vppband <0..4> <%.0f..%.0f>\n", VPP_MIN, VPP_MAX);
  }
  else if(!strcmp(cmd, "rcal"))
  {
    if(ParseNum(a1, &v) && v > 0.0f) UserCfg.rcal = v;
    else printf("Invalid value\n");
  }
  else if(!strcmp(cmd, "hann"))
  {
    if(a1 && (!strcmp(a1, "0") || !strcmp(a1, "1"))) UserCfg.hanWin = (a1[0] == '1');
    else printf("Usage: hann <0|1>\n");
  }
  else if(!strcmp(cmd, "sw"))
  {
    if(a1 && !strcmp(a1, "conn")) UserCfg.wiring = WIRING_CONN;
    else if(a1 && !strcmp(a1, "ain")) UserCfg.wiring = WIRING_AIN;
    else if(a1 && !strcmp(a1, "2el")) UserCfg.wiring = WIRING_2EL;
    else printf("Usage: sw <conn|ain|2el>\n");
  }
  else printf("Unknown command. Type help.\n");
  return 0;
}

/* ------------------------------------------------------------------ */
/*  Results: frequency, RTIA, amplitude, magnitude, phase              */
/* ------------------------------------------------------------------ */
static int32_t ImpedanceShowResult(uint32_t *pData, uint32_t DataCount)
{
  float freq;
  fImpPol_Type *pImp = (fImpPol_Type*)pData;
  AppIMPCtrl(IMPCTRL_GETFREQ, &freq);

  const char *rtiaName = RtiaNameFromSel(PickRtia(freq));
  float vppUsed = PickVpp(freq);

  for(uint32_t i = 0; i < DataCount; i++)
  {
    printf("Freq: %f , RTIA: %s , Vpp: %.0f , RzMag: %f Ohm , RzPhase: %f \n",
           freq, rtiaName, vppUsed, pImp[i].Magnitude,
           pImp[i].Phase * 180.0f / (float)MATH_PI);
  }
  return 0;
}

/* ------------------------------------------------------------------ */
/*  Platform and application configuration                            */
/* ------------------------------------------------------------------ */
static int32_t AD5940PlatformCfg(void)
{
  CLKCfg_Type clk_cfg;
  FIFOCfg_Type fifo_cfg;
  AGPIOCfg_Type gpio_cfg;

  AD5940_HWReset();
  AD5940_Initialize();

  clk_cfg.ADCClkDiv = ADCCLKDIV_1;
  clk_cfg.ADCCLkSrc = ADCCLKSRC_HFOSC;
  clk_cfg.SysClkDiv = SYSCLKDIV_1;
  clk_cfg.SysClkSrc = SYSCLKSRC_HFOSC;
  clk_cfg.HfOSC32MHzMode = bFALSE;
  clk_cfg.HFOSCEn = bTRUE;
  clk_cfg.HFXTALEn = bFALSE;
  clk_cfg.LFOSCEn = bTRUE;
  AD5940_CLKCfg(&clk_cfg);

  fifo_cfg.FIFOEn = bFALSE;
  fifo_cfg.FIFOMode = FIFOMODE_FIFO;
  fifo_cfg.FIFOSize = FIFOSIZE_4KB;
  fifo_cfg.FIFOSrc = FIFOSRC_DFT;
  fifo_cfg.FIFOThresh = 4;
  AD5940_FIFOCfg(&fifo_cfg);
  fifo_cfg.FIFOEn = bTRUE;
  AD5940_FIFOCfg(&fifo_cfg);

  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE);
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  AD5940_INTCCfg(AFEINTC_0, AFEINTSRC_DATAFIFOTHRESH, bTRUE);
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);

  gpio_cfg.FuncSet = GP0_INT|GP1_SLEEP|GP2_SYNC;
  gpio_cfg.InputEnSet = 0;
  gpio_cfg.OutputEnSet = AGPIO_Pin0|AGPIO_Pin1|AGPIO_Pin2;
  gpio_cfg.OutVal = 0;
  gpio_cfg.PullEnSet = 0;
  AD5940_AGPIOCfg(&gpio_cfg);
  AD5940_SleepKeyCtrlS(SLPKEY_UNLOCK);
  return 0;
}

/* Applies UserCfg to the application configuration. */
static void AD5940ImpedanceStructInit(void)
{
  AppIMPCfg_Type *pImpedanceCfg;

  AppIMPGetCfg(&pImpedanceCfg);

  /* Required: RTIA, amplitude and switches are written in the init sequence,
     which is regenerated only if bParaChanged is set (or at the first init). */
  pImpedanceCfg->bParaChanged = bTRUE;

  pImpedanceCfg->SeqStartAddr = 0;
  pImpedanceCfg->MaxSeqLen = 512;
  pImpedanceCfg->FifoThresh = 4;

  pImpedanceCfg->RcalVal = UserCfg.rcal;
  pImpedanceCfg->SinFreq = UserCfg.fStart;
  pImpedanceCfg->DacVoltPP = UserCfg.vpp;      /* with vpp auto it is overridden per point */
  pImpedanceCfg->HanWinEn = UserCfg.hanWin ? bTRUE : bFALSE;
  pImpedanceCfg->HstiaRtiaSel = RtiaTab[UserCfg.rtiaIdx].sel;  /* with rtia auto: per point */

  switch(UserCfg.wiring)
  {
  case WIRING_CONN:    /* CE0 drive, voltage sensed RE0-SE0, return on DE0 */ 
    pImpedanceCfg->DswitchSel = SWD_CE0;
    pImpedanceCfg->PswitchSel = SWP_RE0;
    pImpedanceCfg->NswitchSel = SWN_SE0;
    pImpedanceCfg->TswitchSel = SWT_DE0;
    break;
  case WIRING_2EL:    /* connector, two electrodes: CE0 -> SE0, sense on CE0 */
    pImpedanceCfg->DswitchSel = SWD_CE0;
    pImpedanceCfg->PswitchSel = SWP_CE0;
    pImpedanceCfg->NswitchSel = SWN_SE0;
    pImpedanceCfg->TswitchSel = SWT_SE0LOAD;
    break;
  case WIRING_AIN:
  default:            /* two electrodes: CE0 -> AIN1 */
    pImpedanceCfg->DswitchSel = SWD_CE0;
    pImpedanceCfg->PswitchSel = SWP_CE0;
    pImpedanceCfg->NswitchSel = SWN_AIN1;
    pImpedanceCfg->TswitchSel = SWT_AIN1;
    break;
  }

  pImpedanceCfg->SweepCfg.SweepEn = bTRUE;
  pImpedanceCfg->SweepCfg.SweepStart = UserCfg.fStart;
  pImpedanceCfg->SweepCfg.SweepStop = UserCfg.fStop;
  pImpedanceCfg->SweepCfg.SweepPoints = UserCfg.points;
  pImpedanceCfg->SweepCfg.SweepLog = UserCfg.logSweep ? bTRUE : bFALSE;
  pImpedanceCfg->SweepCfg.SweepIndex = 0;

  pImpedanceCfg->PwrMod = AFEPWR_LP;
  pImpedanceCfg->ADCSinc3Osr = ADCSINC3OSR_2;
  pImpedanceCfg->DftNum = DFTNUM_16384;
  pImpedanceCfg->DftSrc = DFTSRC_SINC3;
}

static void RunMeasurement(void)
{
  uint32_t temp;

  PointCnt = 0;
  AD5940PlatformCfg();                    /* bring the AFE back to a clean state */
  AD5940ImpedanceStructInit();
  AppIMPInit(AppBuff, APPBUFF_SIZE);
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  AppIMPCtrl(IMPCTRL_START, 0);

  /* These three output lines are kept unchanged: capture scripts may rely on them. */
  printf("start\n");
  printf("Formato: Freq (Hz), RTIA (ohm), Vpp, RzMag (ohm), RzPhase (gradi)\n");

  while(1)
  {
    if(AD5940_INTCTestFlag(AFEINTC_0, AFEINTSRC_DATAFIFOTHRESH))
    {
      temp = APPBUFF_SIZE;
      AppIMPISR(AppBuff, &temp);
      ImpedanceShowResult(AppBuff, temp);
      PointCnt += temp;

      if(PointCnt >= UserCfg.points)
      {
        AppIMPCtrl(IMPCTRL_STOPNOW, 0);
        printf("Sweep finito\n");
        break;
      }
    }
  }
}

void AD5940_Main(void)
{
  char line[LINE_MAX_LEN];

  printf("\nAD5941 impedance - parameter terminal\n");
  PrintHelp();
  PrintCfg();

  while(1)
  {
    printf("> ");
    fflush(stdout);
    ReadLine(line, LINE_MAX_LEN);
    if(HandleCommand(line))
      RunMeasurement();
  }
}
