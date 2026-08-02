var VTX = (function() {
    var self = {};

    self.DEV_SMARTAUDIO = 3;
    self.DEV_TRAMP = 4;
    self.DEV_UNKNOWN = 0xFF;

    self.BANDS = [
        {code: 1, name: 'Boscam A'},
        {code: 2, name: 'Boscam B'},
        {code: 3, name: 'Boscam E'},
        {code: 4, name: 'Fatshark'},
        {code: 5, name: 'Raceband'},
    ];

    self.BAND_MIN = 1;
    self.BAND_MAX = 5;

    self.CHANNEL_MIN = 1;
    self.CHANNEL_MAX = 8;

    self.getMinPower = function(vtxDev) {
        return 1;
    }

    self.getMaxPower = function(vtxDev) {
        if ((vtxDev == self.DEV_SMARTAUDIO) || (vtxDev == self.DEV_TRAMP)) {
            return 5;
        }
        return 3;
    }

    self.LOW_POWER_DISARM_MIN = 0;
    self.LOW_POWER_DISARM_MAX = 2;

    self.MAX_FREQUENCY_MHZ = 5999;

    /*
     * Known VTX families the FC can drive, kept in step with the firmware.
     * The codes are vtx3g3Grid_e; the tables are the same ones the firmware
     * tunes from, so what is shown here is what the VTX will actually be set to.
     */
    self.FREQGROUP_3G3 = 3;

    self.GRID_SX33 = 0;
    self.GRID_TX3339 = 1;
    self.GRID_AUTO = 2;

    self.DETECT_NAMES = ['none', 'forced', 'range', 'power', 'protocol', 'fallback'];

    self.BUILTIN_3G3_GRIDS = {};
    self.BUILTIN_3G3_GRIDS[self.GRID_SX33] = {
        name: 'SX33 / FF3741 3.3GHz',
        bands_list: [
            { name: 'A 3200-3340', frequencies: [3200, 3220, 3240, 3260, 3280, 3300, 3320, 3340] },
            { name: 'B 3360-3500', frequencies: [3360, 3380, 3400, 3420, 3440, 3460, 3480, 3500] },
            { name: 'C 3520-3680', frequencies: [3520, 3540, 3560, 3580, 3600, 3620, 3640, 3680] },
            { name: 'D 3210-3490', frequencies: [3210, 3250, 3290, 3330, 3370, 3410, 3450, 3490] },
            { name: 'E 3230-3700', frequencies: [3230, 3290, 3350, 3410, 3470, 3530, 3590, 3700] },
        ],
        powerLevels: [25, 2000, 5000],
        powerLabels: ['25 mW', '2 W', '5 W'],
    };
    self.BUILTIN_3G3_GRIDS[self.GRID_TX3339] = {
        name: 'BeastFPV TX3339-32CH 3.3GHz 10W',
        bands_list: [
            { name: 'A (FR1) 3330-3470', frequencies: [3330, 3350, 3370, 3390, 3410, 3430, 3450, 3470] },
            { name: 'B (FR2) 3340-3480', frequencies: [3340, 3360, 3380, 3400, 3420, 3440, 3460, 3480] },
            { name: 'C (FR3) 3170-3310', frequencies: [3170, 3190, 3210, 3230, 3250, 3270, 3290, 3310] },
            { name: 'D (FR4) 3060-3200', frequencies: [3060, 3080, 3100, 3120, 3140, 3160, 3180, 3200] },
        ],
        powerLevels: [25, 3000, 10000],
        powerLabels: ['25 mW', '3 W', '10 W'],
    };

    /* What the FC last reported over MSP_VTX_CONFIG: which grid it is really using
     * and how it decided. Filled by MSPHelper, so the dropdown shows the FC's own
     * state rather than whatever this configurator was last told. */
    self.fcGrid = {
        valid: false,
        group: 0,
        configured: self.GRID_SX33,   // the vtx_3g3_grid setting (may be AUTO)
        effective: self.GRID_SX33,    // the grid actually in use
        detect: 0,
        freqMin: 0,
        freqMax: 0,
        powerMax: 0,
    };

    self.fcIs3G3 = function () {
        return self.fcGrid.valid && self.fcGrid.group === self.FREQGROUP_3G3;
    };

    /* A manually loaded JSON grid wins over the built-in table, so an odd or
     * substitute vendor grid can still be used without a firmware change. */
    self.getActiveGrid = function () {
        if (self.hasCustomGrid()) {
            return self.customVtxTable;
        }
        if (self.fcIs3G3()) {
            return self.BUILTIN_3G3_GRIDS[self.fcGrid.effective] || null;
        }
        return null;
    };

    /*
     * Custom VTX grid (loaded from a user JSON file). When present it overrides
     * the built-in band/channel/power tables in Configuration and Programming.
     * It is persisted in localStorage so it survives tab switches and restarts.
     */
    self.CUSTOM_GRID_STORAGE_KEY = 'vtx_custom_grid';
    self.customVtxTable = null;

    self.FREQ_MIN_MHZ = 100;
    self.FREQ_MAX_MHZ = 6000;
    self.POWER_MAX_MW = 10000;

    self.normalizeGrid = function (raw) {
        if (!raw || typeof raw !== 'object') {
            throw new Error('Invalid JSON: expected an object');
        }

        /* Vendor files are usually exported by Betaflight, which nests everything
         * under "vtx_table". Accept both that and our flat layout. */
        var src = (raw.vtx_table && typeof raw.vtx_table === 'object') ? raw.vtx_table : raw;

        var rawBands = src.bands || src.bands_list;
        if (!Array.isArray(rawBands) || rawBands.length < 1 || rawBands.length > 8) {
            throw new Error('Field "bands" must be an array of 1..8 bands');
        }

        var channelCount = null;
        var bands = [];
        for (var i = 0; i < rawBands.length; i++) {
            var b = rawBands[i];
            var freqs = b ? b.frequencies : null;
            if (!Array.isArray(freqs) || freqs.length < 1 || freqs.length > 8) {
                throw new Error('Band ' + (i + 1) + ': "frequencies" must be an array of 1..8 channels');
            }
            if (channelCount === null) {
                channelCount = freqs.length;
            } else if (freqs.length !== channelCount) {
                throw new Error('All bands must have the same number of channels');
            }
            for (var c = 0; c < freqs.length; c++) {
                var f = freqs[c];
                if (typeof f !== 'number' || f < self.FREQ_MIN_MHZ || f > self.FREQ_MAX_MHZ) {
                    throw new Error('Band ' + (i + 1) + ' channel ' + (c + 1) + ': frequency must be ' + self.FREQ_MIN_MHZ + '..' + self.FREQ_MAX_MHZ + ' MHz');
                }
            }
            bands.push({
                name: (b.name ? String(b.name) : ('Band ' + (i + 1))),
                frequencies: freqs.slice()
            });
        }

        var rawPower = src.powerlevels || src.powerLevels || src.powerlevels_list;
        if (!Array.isArray(rawPower) || rawPower.length < 1 || rawPower.length > 8) {
            throw new Error('Field "powerlevels" must be an array of 1..8 levels');
        }
        /* A level is either a bare number (mW) or Betaflight's {value, label}, where
         * value is the code sent to the VTX and label is what the pilot should see. */
        var powerLevels = [];
        var powerLabels = [];
        for (var p = 0; p < rawPower.length; p++) {
            var entry = rawPower[p];
            var pw = (entry && typeof entry === 'object') ? entry.value : entry;
            if (typeof pw !== 'number' || pw < 0 || pw > self.POWER_MAX_MW) {
                throw new Error('Power level ' + (p + 1) + ': must be 0..' + self.POWER_MAX_MW);
            }
            powerLevels.push(pw);
            var lbl = (entry && typeof entry === 'object' && entry.label) ? String(entry.label).trim() : '';
            powerLabels.push(lbl);
        }

        return {
            name: (raw.name || src.name) ? String(raw.name || src.name) : 'Custom VTX Grid',
            protocol: (raw.protocol || src.protocol) ? String(raw.protocol || src.protocol) : '',
            bands_list: bands,
            powerLevels: powerLevels,
            powerLabels: powerLabels
        };
    };

    self.hasCustomGrid = function () {
        return !!(self.customVtxTable && self.customVtxTable.bands_list && self.customVtxTable.bands_list.length);
    };

    self.setCustomGrid = function (normalized) {
        self.customVtxTable = normalized;
        try {
            window.localStorage.setItem(self.CUSTOM_GRID_STORAGE_KEY, JSON.stringify(normalized));
        } catch (e) { /* storage unavailable */ }
    };

    self.clearCustomGrid = function () {
        self.customVtxTable = null;
        try {
            window.localStorage.removeItem(self.CUSTOM_GRID_STORAGE_KEY);
        } catch (e) { /* storage unavailable */ }
    };

    self.loadCustomGridFromStorage = function () {
        try {
            var stored = window.localStorage.getItem(self.CUSTOM_GRID_STORAGE_KEY);
            if (stored) {
                self.customVtxTable = self.normalizeGrid(JSON.parse(stored));
            }
        } catch (e) {
            self.customVtxTable = null;
        }
    };

    /* Bands as {code, name} for the Configuration dropdown. */
    self.getBands = function () {
        var grid = self.getActiveGrid();
        if (grid) {
            return grid.bands_list.map(function (b, idx) {
                return { code: idx + 1, name: b.name };
            });
        }
        return self.BANDS;
    };

    self.getChannelCount = function () {
        var grid = self.getActiveGrid();
        if (grid) {
            return grid.bands_list[0].frequencies.length;
        }
        return self.CHANNEL_MAX;
    };

    self.getFrequency = function (band, channel) {
        var grid = self.getActiveGrid();
        if (grid) {
            var b = grid.bands_list[band - 1];
            if (b && b.frequencies[channel - 1] != null) {
                return b.frequencies[channel - 1];
            }
        }
        return null;
    };

    /* Programming operand lists: [{value, label}] for ops Set VTx Band/Channel/Power. */
    self.getProgrammingBandList = function () {
        return self.getBands().map(function (b) {
            return { value: b.code, label: b.name };
        });
    };

    self.getProgrammingChannelList = function () {
        var list = [];
        var count = self.getChannelCount();
        var activeGrid = self.getActiveGrid();
        var refBand = activeGrid ? activeGrid.bands_list[0] : null;
        for (var i = 1; i <= count; i++) {
            var label = 'CH ' + i;
            if (refBand && refBand.frequencies[i - 1] != null) {
                label = 'CH ' + i + ' (' + refBand.frequencies[i - 1] + ' MHz)';
            }
            list.push({ value: i, label: label });
        }
        return list;
    };

    self.getProgrammingPowerList = function () {
        var list = [];
        var grid = self.getActiveGrid();
        if (grid) {
            var labels = grid.powerLabels || [];
            for (var i = 0; i < grid.powerLevels.length; i++) {
                list.push({
                    value: i + 1,
                    label: labels[i] || (grid.powerLevels[i] + ' mW')
                });
            }
        } else {
            for (var j = 1; j <= 5; j++) {
                list.push({ value: j, label: 'Level ' + j });
            }
        }
        return list;
    };

    self.loadCustomGridFromStorage();

    return self;
})();
