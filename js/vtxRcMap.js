/*global $,VTX,chrome*/
'use strict';

let VtxRcMapCollection = function () {

    let self = {},
        data = [],
        $container;

    const MAX_ENTRIES = 20;
    const RC_CHANNEL_COUNT = 16;
    const VALUE_MIN = 750;
    const VALUE_MAX = 2250;

    self.getMaxEntryCount = function () {
        return MAX_ENTRIES;
    };

    self.getRcChannelCount = function () {
        return RC_CHANNEL_COUNT;
    };

    self.get = function () {
        return data;
    };

    self.getCount = function () {
        return data.length;
    };

    self.flush = function () {
        data = [];
    };

    self.put = function (entry) {
        data.push(entry);
    };

    /* An entry with no channel or no pair is an unused firmware slot */
    self.putFromFc = function (rcChannel, band, channel, rangeStart, rangeEnd) {
        if (!rcChannel || !band || !channel) {
            return;
        }
        data.push({
            rcChannel: rcChannel,
            band: band,
            channel: channel,
            rangeStart: rangeStart,
            rangeEnd: rangeEnd
        });
    };

    self.extractBuffer = function () {
        let buffer = [];
        buffer.push(data.length & 0xFF);
        data.forEach(function (entry) {
            buffer.push(entry.rcChannel & 0xFF);
            buffer.push(entry.band & 0xFF);
            buffer.push(entry.channel & 0xFF);
            buffer.push(entry.rangeStart & 0xFF);
            buffer.push((entry.rangeStart >> 8) & 0xFF);
            buffer.push(entry.rangeEnd & 0xFF);
            buffer.push((entry.rangeEnd >> 8) & 0xFF);
        });
        return buffer;
    };

    /* A VTX can only be on one pair at a time, so two entries of the same RC
     * channel must never match the same value. */
    self.getConflicts = function () {
        let conflicts = [];
        for (let i = 0; i < data.length; i++) {
            for (let j = i + 1; j < data.length; j++) {
                if (data[i].rcChannel !== data[j].rcChannel) {
                    continue;
                }
                if (data[i].rangeStart <= data[j].rangeEnd && data[j].rangeStart <= data[i].rangeEnd) {
                    conflicts.push([i, j]);
                }
            }
        }
        return conflicts;
    };

    self.isValid = function () {
        if (self.getConflicts().length) {
            return false;
        }
        return data.every(function (entry) {
            return entry.rcChannel >= 1 && entry.rcChannel <= RC_CHANNEL_COUNT &&
                entry.band >= 1 && entry.channel >= 1 &&
                entry.rangeStart >= VALUE_MIN && entry.rangeEnd <= VALUE_MAX &&
                entry.rangeEnd >= entry.rangeStart;
        });
    };

    function getPairList() {
        let list = [];
        let bands = VTX.getBands();
        let channelCount = VTX.getChannelCount();
        bands.forEach(function (band) {
            for (let channel = 1; channel <= channelCount; channel++) {
                let frequency = VTX.getFrequency(band.code, channel);
                list.push({
                    band: band.code,
                    channel: channel,
                    label: band.name + VTX.getChannelLabel(channel) +
                        (frequency ? ' (' + frequency + ' MHz)' : '')
                });
            }
        });
        return list;
    }

    function renderRow(index, entry, $tbody, pairs) {
        $tbody.append('<tr>\
            <td class="vtxmap_cell__index"></td>\
            <td class="vtxmap_cell__channel"></td>\
            <td class="vtxmap_cell__mode"></td>\
            <td class="vtxmap_cell__value"></td>\
            <td class="vtxmap_cell__pair"></td>\
            <td class="vtxmap_cell__delete"></td>\
        </tr>');

        let $row = $tbody.find('tr:last');
        $row.find('.vtxmap_cell__index').html(index);

        let $channel = $('<select class="vtxmap_element__channel"></select>');
        for (let i = 1; i <= RC_CHANNEL_COUNT; i++) {
            $channel.append('<option value="' + i + '">CH' + i + '</option>');
        }
        $channel.val(entry.rcChannel);
        $channel.change(function () {
            entry.rcChannel = parseInt($(this).val(), 10);
            self.render();
        });
        $row.find('.vtxmap_cell__channel').append($channel);

        let isRange = entry.rangeStart !== entry.rangeEnd;
        let $mode = $('<select class="vtxmap_element__mode"></select>');
        $mode.append('<option value="exact">' + chrome.i18n.getMessage('vtxMapModeExact') + '</option>');
        $mode.append('<option value="range">' + chrome.i18n.getMessage('vtxMapModeRange') + '</option>');
        $mode.val(isRange ? 'range' : 'exact');
        $mode.change(function () {
            if ($(this).val() === 'exact') {
                entry.rangeEnd = entry.rangeStart;
            } else if (entry.rangeEnd === entry.rangeStart) {
                entry.rangeEnd = Math.min(VALUE_MAX, entry.rangeStart + 50);
            }
            self.render();
        });
        $row.find('.vtxmap_cell__mode').append($mode);

        let $value = $row.find('.vtxmap_cell__value');
        if (isRange) {
            $value.append('<input type="number" class="vtxmap_element__start" min="' + VALUE_MIN + '" max="' + VALUE_MAX + '" step="1" />');
            $value.append('<span class="vtxmap_element__separator"> &ndash; </span>');
            $value.append('<input type="number" class="vtxmap_element__end" min="' + VALUE_MIN + '" max="' + VALUE_MAX + '" step="1" />');
            $value.find('.vtxmap_element__start').val(entry.rangeStart).change(function () {
                entry.rangeStart = parseInt($(this).val(), 10);
                self.render();
            });
            $value.find('.vtxmap_element__end').val(entry.rangeEnd).change(function () {
                entry.rangeEnd = parseInt($(this).val(), 10);
                self.render();
            });
        } else {
            $value.append('<input type="number" class="vtxmap_element__start" min="' + VALUE_MIN + '" max="' + VALUE_MAX + '" step="1" />');
            $value.find('.vtxmap_element__start').val(entry.rangeStart).change(function () {
                entry.rangeStart = parseInt($(this).val(), 10);
                entry.rangeEnd = entry.rangeStart;
                self.render();
            });
        }

        let $pair = $('<select class="vtxmap_element__pair"></select>');
        pairs.forEach(function (pair) {
            $pair.append('<option value="' + pair.band + ':' + pair.channel + '">' + pair.label + '</option>');
        });
        $pair.val(entry.band + ':' + entry.channel);
        if (!$pair.val() && pairs.length) {
            entry.band = pairs[0].band;
            entry.channel = pairs[0].channel;
            $pair.val(entry.band + ':' + entry.channel);
        }
        $pair.change(function () {
            let parts = $(this).val().split(':');
            entry.band = parseInt(parts[0], 10);
            entry.channel = parseInt(parts[1], 10);
            self.render();
        });
        $row.find('.vtxmap_cell__pair').append($pair);

        let $delete = $('<a href="#" class="vtxmap_element__delete" title="' +
            chrome.i18n.getMessage('vtxMapDelete') + '">&times;</a>');
        $delete.click(function () {
            data.splice(index, 1);
            self.render();
            return false;
        });
        $row.find('.vtxmap_cell__delete').append($delete);

        return $row;
    }

    self.render = function () {
        if (!$container) {
            return;
        }

        let $tbody = $container.find('.vtxmap__table tbody');
        $tbody.empty();

        let pairs = getPairList();

        let rows = data.map(function (entry, index) {
            return renderRow(index, entry, $tbody, pairs);
        });

        self.getConflicts().forEach(function (pair) {
            rows[pair[0]].addClass('vtxmap__row--conflict');
            rows[pair[1]].addClass('vtxmap__row--conflict');
        });

        let gridName = VTX.getActiveGridName();
        $container.find('.vtxmap__grid').text(gridName
            ? chrome.i18n.getMessage('programmingVtxGridInUse', [gridName])
            : chrome.i18n.getMessage('programmingVtxGridUnknown'));

        $container.find('.vtxmap__error').text(self.isValid()
            ? ''
            : chrome.i18n.getMessage('vtxMapOverlapError'));

        $container.find('.vtxmap__add').toggle(data.length < MAX_ENTRIES);
    };

    self.init = function ($element) {
        $container = $element;

        $container.find('.vtxmap__add').click(function () {
            if (data.length >= MAX_ENTRIES) {
                return false;
            }
            let pairs = getPairList();
            let last = data.length ? data[data.length - 1] : null;
            data.push({
                rcChannel: last ? last.rcChannel : 12,
                band: pairs.length ? pairs[0].band : 1,
                channel: pairs.length ? pairs[0].channel : 1,
                rangeStart: last ? Math.min(VALUE_MAX, last.rangeEnd + 1) : 1000,
                rangeEnd: last ? Math.min(VALUE_MAX, last.rangeEnd + 1) : 1000
            });
            self.render();
            return false;
        });
    };

    return self;
};
