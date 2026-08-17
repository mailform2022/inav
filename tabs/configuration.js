/*global chrome,GUI,FC_CONFIG,$,mspHelper,googleAnalytics,ADVANCED_CONFIG,VTX_CONFIG,CONFIG,MSPChainerClass,BOARD_ALIGNMENT,TABS,MISC*/
'use strict';

TABS.configuration = {};

TABS.configuration.initialize = function (callback, scrollPosition) {

    if (GUI.active_tab != 'configuration') {
        GUI.active_tab = 'configuration';
        googleAnalytics.sendAppView('Configuration');
    }

    var loadChainer = new MSPChainerClass();

    var loadChain = [
        mspHelper.loadFeatures,
        mspHelper.loadArmingConfig,
        mspHelper.loadSensorAlignment,
        mspHelper.loadAdvancedConfig,
        mspHelper.loadVTXConfig,
        mspHelper.loadBoardAlignment,
        mspHelper.loadCurrentMeterConfig,
        mspHelper.loadMiscV2
    ];

    loadChainer.setChain(loadChain);
    loadChainer.setExitPoint(load_html);
    loadChainer.execute();

    var saveChainer = new MSPChainerClass();

    var saveChain = [
        mspHelper.saveAccTrim,
        mspHelper.saveArmingConfig,
        mspHelper.saveAdvancedConfig,
        mspHelper.saveVTXConfig,
        mspHelper.saveCurrentMeterConfig,
        mspHelper.saveMiscV2,
        saveSettings,
        mspHelper.saveToEeprom
    ];

    function saveSettings(onComplete) {
        Settings.saveInputs().then(onComplete);
    }

    saveChainer.setChain(saveChain);
    saveChainer.setExitPoint(reboot);

    function reboot() {
        //noinspection JSUnresolvedVariable
        GUI.log(chrome.i18n.getMessage('configurationEepromSaved'));

        GUI.tab_switch_cleanup(function () {
            MSP.send_message(MSPCodes.MSP_SET_REBOOT, false, false, reinitialize);
        });
    }

    function reinitialize() {
        //noinspection JSUnresolvedVariable
        GUI.log(chrome.i18n.getMessage('deviceRebooting'));
        GUI.handleReconnect($('.tab_configuration a'));
    }

    function load_html() {
        GUI.load("./tabs/configuration.html", Settings.processHtml(process_html));
    }

    function process_html() {

        let i;

        // generate features
        var features = FC.getFeatures();

        var features_e = $('.features');
        for (i = 0; i < features.length; i++) {
            var row_e,
                tips = [],
                feature_tip_html = '';

            if (features[i].showNameInTip) {
                tips.push(chrome.i18n.getMessage("manualEnablingTemplate").replace("{name}", features[i].name));
            }

            if (features[i].haveTip) {
                tips.push(chrome.i18n.getMessage("feature" + features[i].name + "Tip"));
            }

            if (tips.length > 0) {
                feature_tip_html = '<div class="helpicon cf_tip" title="' + tips.join("<br><br>") + '"></div>';
            }

            row_e = $('<div class="checkbox">' +
                '<input type="checkbox" data-bit="' + features[i].bit + '" class="feature toggle" name="' + features[i].name + '" title="' + features[i].name + '"' +
                ' id="feature-' + features[i].bit + '" ' +
                '>' +
                '<label for="feature-' + features[i].bit + '">' +
                '<span data-i18n="feature' + features[i].name + '"></span>' +
                '</label>' +
                feature_tip_html +
                '</div>');

            features_e.each(function () {
                if ($(this).hasClass(features[i].group)) {
                    $(this).after(row_e);
                }
            });
        }

        helper.features.updateUI($('.tab-configuration'), FEATURES);

        // translate to user-selected language
        localize();

        // VTX
        var config_vtx = $('.config-vtx');
        config_vtx.show();

        // Friendlier names than the raw CLI enum values the setting table provides.
        var VTX_GRID_LABELS = {};
        VTX_GRID_LABELS[VTX.GRID_AUTO] = 'configurationVTXGridTypeAuto';
        VTX_GRID_LABELS[VTX.GRID_SX33] = 'configurationVTXGridTypeSX33';
        VTX_GRID_LABELS[VTX.GRID_TX3339] = 'configurationVTXGridTypeTX3339';
        VTX_GRID_LABELS[VTX.GRID_NONAME1] = 'configurationVTXGridTypeNoname1';
        VTX_GRID_LABELS[VTX.GRID_FF37] = 'configurationVTXGridTypeFF37';

        function renderVtxGridType() {
            var $wrapper = $('#vtx_grid_type_wrapper');
            var $select = $('#vtx_3g3_grid');

            // The setting only exists on 3.3GHz-capable firmware, and only means
            // anything while the 3.3GHz frequency group is selected.
            if (!$select.length || !$select.find('option').length || !VTX.fcIs3G3()) {
                $wrapper.hide();
                return;
            }
            $wrapper.show();

            $select.find('option').each(function () {
                var key = VTX_GRID_LABELS[parseInt($(this).val())];
                var label = key ? chrome.i18n.getMessage(key) : null;
                if (label) {
                    $(this).text(label);
                }
            });

            var g = VTX.fcGrid;
            var effectiveName = (VTX.BUILTIN_3G3_GRIDS[g.effective] || {}).name || '?';
            var $detect = $('#vtx_grid_detect');

            if (g.configured === VTX.GRID_AUTO) {
                var detectKey = 'configurationVTXGridDetect_' + (VTX.DETECT_NAMES[g.detect] || 'none');
                var how = chrome.i18n.getMessage(detectKey) || VTX.DETECT_NAMES[g.detect];
                $detect.text(chrome.i18n.getMessage('configurationVTXGridDetected', [
                    effectiveName, how, String(g.freqMin), String(g.freqMax), String(g.powerMax)
                ]));
                // A fallback means the VTX said nothing usable - the grid is a guess.
                var uncertain = (VTX.DETECT_NAMES[g.detect] === 'fallback' || VTX.DETECT_NAMES[g.detect] === 'none');
                $detect.toggleClass('vtx_custom_grid_status--error', uncertain)
                       .toggleClass('vtx_custom_grid_status--ok', !uncertain);
            } else {
                $detect.removeClass('vtx_custom_grid_status--error').addClass('vtx_custom_grid_status--ok')
                       .text(chrome.i18n.getMessage('configurationVTXGridForced', [effectiveName]));
            }

            // Band/channel labels follow the grid, so re-render on change. The value
            // itself is written to the FC by Settings.saveInputs() on Save.
            $select.off('change.vtxgrid').on('change.vtxgrid', function () {
                var picked = parseInt($(this).val());
                VTX.fcGrid.configured = picked;
                if (picked !== VTX.GRID_AUTO) {
                    VTX.fcGrid.effective = picked;
                }
                renderVtxGridType();
                renderVtxDeviceSettings();
            });
        }

        function renderVtxGridStatus() {
            var $status = $('#vtx_grid_status');
            if (VTX.hasCustomGrid()) {
                var t = VTX.customVtxTable;
                var summary = chrome.i18n.getMessage('configurationVTXCustomGridLoaded', [
                    t.name,
                    String(t.bands_list.length),
                    String(t.bands_list[0].frequencies.length),
                    String(t.powerLevels.length)
                ]);
                $status.removeClass('vtx_custom_grid_status--error').addClass('vtx_custom_grid_status--ok').text(summary);
                $('#vtx_grid_clear').show();
            } else {
                $status.removeClass('vtx_custom_grid_status--error vtx_custom_grid_status--ok')
                    .text(chrome.i18n.getMessage('configurationVTXCustomGridNone'));
                $('#vtx_grid_clear').hide();
            }
        }

        function renderVtxDeviceSettings() {
            var $deviceSettings = $('#vtx_device_settings');
            if (VTX_CONFIG.device_type == VTX.DEV_UNKNOWN) {
                $deviceSettings.hide();
                return;
            }
            $deviceSettings.show();

            var bands = VTX.getBands();

            var vtx_band = $('#vtx_band');
            vtx_band.empty();
            var vtx_no_band_note = $('#vtx_no_band');
            if (VTX_CONFIG.band < VTX.BAND_MIN || VTX_CONFIG.band > bands.length) {
                var noBandName = chrome.i18n.getMessage("configurationNoBand");
                $('<option value="0">' + noBandName + '</option>').appendTo(vtx_band);
                vtx_no_band_note.show();
            } else {
                vtx_no_band_note.hide();
            }
            for (var ii = 0; ii < bands.length; ii++) {
                var band_name = bands[ii].name;
                var option = $('<option value="' + bands[ii].code + '">' + band_name + '</option>');
                if (bands[ii].code == VTX_CONFIG.band) {
                    option.prop('selected', true);
                }
                option.appendTo(vtx_band);
            }
            vtx_band.off('change').change(function () {
                VTX_CONFIG.band = parseInt($(this).val());
            });

            var vtx_channel = $('#vtx_channel');
            vtx_channel.empty();
            var channelCount = VTX.getChannelCount();
            for (var ci = VTX.CHANNEL_MIN; ci <= channelCount; ci++) {
                var chLabel = VTX.getChannelLabel(ci);
                var freq = VTX.getFrequency(VTX_CONFIG.band, ci);
                if (freq != null) {
                    chLabel += ' (' + freq + ' MHz)';
                }
                var option = $('<option value="' + ci + '">' + chLabel + '</option>');
                if (ci == VTX_CONFIG.channel) {
                    option.prop('selected', true);
                }
                option.appendTo(vtx_channel);
            }
            vtx_channel.off('change').change(function () {
                VTX_CONFIG.channel = parseInt($(this).val());
            });

            var vtx_power = $('#vtx_power');
            vtx_power.empty();
            if (VTX.getActiveGrid()) {
                var powerList = VTX.getProgrammingPowerList();
                for (var pi = 0; pi < powerList.length; pi++) {
                    var pOption = $('<option value="' + powerList[pi].value + '">' + powerList[pi].label + '</option>');
                    if (powerList[pi].value == VTX_CONFIG.power) {
                        pOption.prop('selected', true);
                    }
                    pOption.appendTo(vtx_power);
                }
            } else {
                var minPower = VTX.getMinPower(VTX_CONFIG.device_type);
                var maxPower = VTX.getMaxPower(VTX_CONFIG.device_type);
                for (var pp = minPower; pp <= maxPower; pp++) {
                    var ppOption = $('<option value="' + pp + '">' + pp + '</option>');
                    if (pp == VTX_CONFIG.power) {
                        ppOption.prop('selected', true);
                    }
                    ppOption.appendTo(vtx_power);
                }
            }
            vtx_power.off('change').change(function () {
                VTX_CONFIG.power = parseInt($(this).val());
            });

            var vtx_low_power_disarm = $('#vtx_low_power_disarm');
            vtx_low_power_disarm.empty();
            for (var li = VTX.LOW_POWER_DISARM_MIN; li <= VTX.LOW_POWER_DISARM_MAX; li++) {
                var name = chrome.i18n.getMessage("configurationVTXLowPowerDisarmValue_" + li);
                if (!name) {
                    name = li;
                }
                var lpOption = $('<option value="' + li + '">' + name + '</option>');
                if (li == VTX_CONFIG.low_power_disarm) {
                    lpOption.prop('selected', true);
                }
                lpOption.appendTo(vtx_low_power_disarm);
            }
            vtx_low_power_disarm.off('change').change(function () {
                VTX_CONFIG.low_power_disarm = parseInt($(this).val());
            });
        }

        $('#vtx_grid_load').off('click').on('click', function (e) {
            e.preventDefault();
            $('#vtx_grid_file').val('').trigger('click');
        });

        $('#vtx_grid_file').off('change').on('change', function (e) {
            var file = e.target.files && e.target.files[0];
            if (!file) {
                return;
            }
            var reader = new FileReader();
            reader.onload = function (ev) {
                var $status = $('#vtx_grid_status');
                try {
                    var normalized = VTX.normalizeGrid(JSON.parse(ev.target.result));
                    VTX.setCustomGrid(normalized);
                    renderVtxGridStatus();
                    renderVtxGridType();
                    renderVtxDeviceSettings();
                } catch (err) {
                    $status.removeClass('vtx_custom_grid_status--ok').addClass('vtx_custom_grid_status--error')
                        .text(chrome.i18n.getMessage('configurationVTXCustomGridError', [err.message]));
                }
            };
            reader.readAsText(file);
        });

        $('#vtx_grid_clear').off('click').on('click', function (e) {
            e.preventDefault();
            VTX.clearCustomGrid();
            renderVtxGridStatus();
            renderVtxGridType();
            renderVtxDeviceSettings();
        });

        renderVtxGridStatus();
        renderVtxGridType();
        renderVtxDeviceSettings();

        // for some odd reason chrome 38+ changes scroll according to the touched select element
        // i am guessing this is a bug, since this wasn't happening on 37
        // code below is a temporary fix, which we will be able to remove in the future (hopefully)
        //noinspection JSValidateTypes
        $('#content').scrollTop((scrollPosition) ? scrollPosition : 0);

        // fill board alignment
        $('input[name="board_align_yaw"]').val((BOARD_ALIGNMENT.yaw / 10.0).toFixed(1));

        // fill magnetometer
        //UPDATE: moved to GPS tab and hidden
        //$('#mag_declination').val(MISC.mag_declination);

        // fill battery voltage
        $('#voltagesource').val(MISC.voltage_source);
        $('#cells').val(MISC.battery_cells);
        $('#celldetectvoltage').val(MISC.vbatdetectcellvoltage);
        $('#mincellvoltage').val(MISC.vbatmincellvoltage);
        $('#maxcellvoltage').val(MISC.vbatmaxcellvoltage);
        $('#warningcellvoltage').val(MISC.vbatwarningcellvoltage);
        $('#voltagescale').val(MISC.vbatscale);

        // fill current
        $('#currentscale').val(CURRENT_METER_CONFIG.scale);
        $('#currentoffset').val(CURRENT_METER_CONFIG.offset / 10);

        // fill battery capacity
        $('#battery_capacity').val(MISC.battery_capacity);
        $('#battery_capacity_warning').val(Math.round(MISC.battery_capacity_warning * 100 / MISC.battery_capacity));
        $('#battery_capacity_critical').val(Math.round(MISC.battery_capacity_critical * 100 / MISC.battery_capacity));
        $('#battery_capacity_unit').val(MISC.battery_capacity_unit);

        let $i2cSpeed = $('#i2c_speed'),
            $i2cSpeedInfo = $('#i2c_speed-info');

        $i2cSpeed.change(function () {
            let $this = $(this),
                value = $this.children("option:selected").text();

            if (value == "400KHZ") {

                $i2cSpeedInfo.removeClass('ok-box');
                $i2cSpeedInfo.addClass('info-box');
                $i2cSpeedInfo.removeClass('warning-box');

                $i2cSpeedInfo.html(chrome.i18n.getMessage('i2cSpeedSuggested800khz'));
                $i2cSpeedInfo.show();

            } else if (value == "800KHZ") {
                $i2cSpeedInfo.removeClass('ok-box');
                $i2cSpeedInfo.removeClass('info-box');
                $i2cSpeedInfo.removeClass('warning-box');
                $i2cSpeedInfo.hide();
            } else {
                $i2cSpeedInfo.removeClass('ok-box');
                $i2cSpeedInfo.removeClass('info-box');
                $i2cSpeedInfo.addClass('warning-box');
                $i2cSpeedInfo.html(chrome.i18n.getMessage('i2cSpeedTooLow'));
                $i2cSpeedInfo.show();
            }

        });

        $i2cSpeed.change();

        $('a.save').click(function () {
            //UPDATE: moved to GPS tab and hidden
            //MISC.mag_declination = parseFloat($('#mag_declination').val());

            ARMING_CONFIG.auto_disarm_delay = parseInt($('input[name="autodisarmdelay"]').val());

            MISC.battery_cells = parseInt($('#cells').val());
            MISC.voltage_source = parseInt($('#voltagesource').val());
            MISC.vbatdetectcellvoltage = parseFloat($('#celldetectvoltage').val());
            MISC.vbatmincellvoltage = parseFloat($('#mincellvoltage').val());
            MISC.vbatmaxcellvoltage = parseFloat($('#maxcellvoltage').val());
            MISC.vbatwarningcellvoltage = parseFloat($('#warningcellvoltage').val());
            MISC.vbatscale = parseInt($('#voltagescale').val());

            MISC.battery_capacity = parseInt($('#battery_capacity').val());
            MISC.battery_capacity_warning = parseInt($('#battery_capacity_warning').val() * MISC.battery_capacity / 100);
            MISC.battery_capacity_critical = parseInt($('#battery_capacity_critical').val() * MISC.battery_capacity / 100);
            MISC.battery_capacity_unit = $('#battery_capacity_unit').val();

            googleAnalytics.sendEvent('Setting', 'I2CSpeed', $('#i2c_speed').children("option:selected").text());

            googleAnalytics.sendEvent('Board', 'Accelerometer', $('#sensor-acc').children("option:selected").text());
            googleAnalytics.sendEvent('Board', 'Magnetometer', $('#sensor-mag').children("option:selected").text());
            googleAnalytics.sendEvent('Board', 'Barometer', $('#sensor-baro').children("option:selected").text());
            googleAnalytics.sendEvent('Board', 'Pitot', $('#sensor-pitot').children("option:selected").text());

            for (var i = 0; i < features.length; i++) {
                var featureName = features[i].name;
                if (FC.isFeatureEnabled(featureName, features)) {
                    googleAnalytics.sendEvent('Setting', 'Feature', featureName);
                }
            }

            helper.features.reset();
            helper.features.fromUI($('.tab-configuration'));
            helper.features.execute(function () {
                CURRENT_METER_CONFIG.scale = parseInt($('#currentscale').val());
                CURRENT_METER_CONFIG.offset = Math.round(parseFloat($('#currentoffset').val()) * 10);
                saveChainer.execute();
            });
        });

        helper.interval.add('config_load_analog', function () {
            $('#batteryvoltage').val([ANALOG.voltage.toFixed(2)]);
            $('#batterycurrent').val([ANALOG.amperage.toFixed(2)]);
        }, 100, true); // 10 fps

        GUI.content_ready(callback);
    }
};

TABS.configuration.cleanup = function (callback) {
    if (callback) callback();
};
