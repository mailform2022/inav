#pragma once

#include <stdint.h>

extern const uint16_t vtx58frequencyTable[5][8];
extern const char * const vtx58BandNames[];
extern const char * const vtx58ChannelNames[];
extern const char * const vtx58DefaultPowerNames[];
extern const char vtx58BandLetter[];

extern const uint16_t vtx1G3frequencyTable[2][8];
extern const char * const vtx1G3BandNames[];
extern const char * const vtx1G3ChannelNames[];
extern const char * const vtx1G3DefaultPowerNames[];
extern const char vtx51G3BandLetter[];

extern const uint16_t vtx3G3frequencyTable[5][8];
extern const uint16_t vtx3G3frequencyTableTx3339[4][8];
extern const uint16_t vtx3G3frequencyTableNoname1[2][8];
extern const uint16_t vtx3G3frequencyTableFf37[1][20];
extern const char * const vtx3G3BandNames[];
extern const char * const vtx3G3ChannelNames[];
extern const char * const vtx3G3Ff37ChannelNames[];
extern const char * const vtx3G3DefaultPowerNames[];
extern const char * const vtx3G3Tx3339PowerNames[];
extern const char * const vtx3G3Noname1PowerNames[];
extern const char vtx3G3BandLetter[];

// Widest 3.3GHz grid, used to size the band/channel selectors
#define VTX_3G3_MAX_BAND_COUNT     5
#define VTX_3G3_MAX_CHANNEL_COUNT 20
#define VTX_3G3_MAX_POWER_COUNT    4

bool vtx58_Freq2Bandchan(uint16_t freq, uint8_t *pBand, uint8_t *pChannel);
uint16_t vtx58_Bandchan2Freq(uint8_t band, uint8_t channel);
uint16_t vtx1G3_Bandchan2Freq(uint8_t band, uint8_t channel);
uint16_t vtx3G3_Bandchan2Freq(uint8_t band, uint8_t channel);

/* The 3.3GHz group covers two incompatible VTX families, selected by
 * vtx_3g3_grid: the SX33/FF3741 grid and the BeastFPV TX3339-32CH grid.
 * vtx_3g3_grid = AUTO picks between them from what the VTX reports. */
typedef struct {
    uint16_t freqMin;       // raw IRC Tramp 'r' answer, before any override
    uint16_t freqMax;
    uint16_t powerMax;
    uint8_t  grid;          // vtx3g3Grid_e the classifier settled on
    uint8_t  detect;        // vtx3g3Detect_e - how it got there
} vtx3G3DeviceReport_t;

void vtx3G3_ReportTrampCapabilities(uint16_t freqMin, uint16_t freqMax, uint16_t powerMax);
void vtx3G3_ReportSmartAudioDevice(void);
const vtx3G3DeviceReport_t * vtx3G3_DeviceReport(void);
uint8_t vtx3G3_DetectSource(void);

uint8_t vtx3G3_EffectiveGrid(void);
bool vtx3G3_GridIsTx3339(void);
const char * vtx3G3_GridName(void);
bool vtx3G3_TrampPowerIsMilliwatt(void);
uint8_t vtx3G3_BandCount(void);
uint8_t vtx3G3_ChannelCount(void);
uint8_t vtx3G3_PowerCount(void);
const char * const * vtx3G3_BandNames(void);
const char * const * vtx3G3_ChannelNames(void);
const char * const * vtx3G3_PowerNames(void);
const uint16_t * vtx3G3_PowerLevels(void);
uint8_t vtx3G3_PowerDbm(uint8_t index);
uint16_t vtx3G3_FreqMin(void);
uint16_t vtx3G3_FreqMax(void);
uint16_t vtx3G3_MaxPowerMw(void);
