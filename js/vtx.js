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

        var rawBands = raw.bands || raw.bands_list;
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

        var rawPower = raw.powerlevels || raw.powerLevels;
        if (!Array.isArray(rawPower) || rawPower.length < 1 || rawPower.length > 8) {
            throw new Error('Field "powerlevels" must be an array of 1..8 levels');
        }
        for (var p = 0; p < rawPower.length; p++) {
            var pw = rawPower[p];
            if (typeof pw !== 'number' || pw < 0 || pw > self.POWER_MAX_MW) {
                throw new Error('Power level ' + (p + 1) + ': must be 0..' + self.POWER_MAX_MW + ' mW');
            }
        }

        return {
            name: raw.name ? String(raw.name) : 'Custom VTX Grid',
            protocol: raw.protocol ? String(raw.protocol) : '',
            bands_list: bands,
            powerLevels: rawPower.slice()
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
        if (self.hasCustomGrid()) {
            return self.customVtxTable.bands_list.map(function (b, idx) {
                return { code: idx + 1, name: b.name };
            });
        }
        return self.BANDS;
    };

    self.getChannelCount = function () {
        if (self.hasCustomGrid()) {
            return self.customVtxTable.bands_list[0].frequencies.length;
        }
        return self.CHANNEL_MAX;
    };

    self.getFrequency = function (band, channel) {
        if (self.hasCustomGrid()) {
            var b = self.customVtxTable.bands_list[band - 1];
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
        var refBand = self.hasCustomGrid() ? self.customVtxTable.bands_list[0] : null;
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
        if (self.hasCustomGrid()) {
            for (var i = 0; i < self.customVtxTable.powerLevels.length; i++) {
                list.push({ value: i + 1, label: self.customVtxTable.powerLevels[i] + ' mW' });
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
