#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "rc522.h"
#include "mf1_toolbox.h"
#include "nfc_mf1.h"
#include "tag_emulation.h"
#include "tag_base_type.h"
#include "bsp_wdt.h"
#include "bsp_delay.h"
#include "app_status.h"
#include "mf1_offline_clone.h"

#define NRF_LOG_MODULE_NAME mf1_offline_clone
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

// Common key dictionary (well known public default/system keys of Mifare
// Classic deployments). Each entry is 6 bytes, MSB first.
#define MF1_OFFLINE_CLONE_DICT_LEN 78
static const uint8_t m_common_keys[MF1_OFFLINE_CLONE_DICT_LEN][6] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // transport default
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}, // NXP / MAD key A
    {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7}, // NXP public key B (MAD / NTAG)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5},
    {0x4D, 0x3A, 0x99, 0xC3, 0x51, 0xDD},
    {0x1A, 0x98, 0x2C, 0x7E, 0x45, 0x9A},
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
    {0x71, 0x3F, 0x2C, 0x95, 0xC0, 0xB3},
    {0x5A, 0x3A, 0x5C, 0xD4, 0x5A, 0x3A},
    {0x0E, 0x1E, 0xD6, 0x96, 0x04, 0xB9},
    {0x71, 0x29, 0xC1, 0x4B, 0x02, 0x7F},
    {0x0F, 0x13, 0x37, 0x69, 0x96, 0x35},
    {0xB5, 0xFF, 0x67, 0xCB, 0xA9, 0x5F},
    {0x0F, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x02},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x03},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x04},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x05},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x06},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x07},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x08},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x09},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0A},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0B},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0D},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0E},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0F},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x10},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x11},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x12},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x13},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x14},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x15},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x16},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x17},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x19},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1A},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1B},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1C},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1D},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1E},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1F},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x20},
    {0x44, 0x4F, 0x4E, 0x4E, 0x45, 0x53}, // "DONNES"
    {0x49, 0x44, 0x49, 0x44, 0x49, 0x44}, // "IDIDID"
    {0x45, 0x51, 0x55, 0x41, 0x4C, 0x53}, // "EQUALS"
    {0x41, 0x30, 0x41, 0x31, 0x41, 0x30}, // "A0A1A0"
    {0x62, 0x6C, 0x6F, 0x62, 0x62, 0x65}, // "blobbe"
    {0x6B, 0x6C, 0x6F, 0x62, 0x62, 0x65}, // "klobbe"
    {0x54, 0x68, 0x69, 0x6E, 0x67, 0x73}, // "Things"
    {0x4D, 0x61, 0x72, 0x69, 0x6E, 0x61}, // "Marina"
    {0x31, 0x39, 0x38, 0x38, 0x30, 0x34},
    {0x31, 0x39, 0x39, 0x32, 0x31, 0x30},
    {0x31, 0x32, 0x33, 0x34, 0x35, 0x36}, // "123456"
    {0x31, 0x32, 0x33, 0x34, 0x35, 0x67}, // "12345g"
    {0x33, 0x30, 0x33, 0x30, 0x30, 0x30}, // "303000"
    {0x31, 0x31, 0x31, 0x31, 0x31, 0x31}, // "111111"
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30}, // "000000"
    {0x39, 0x39, 0x39, 0x39, 0x39, 0x39}, // "999999"
    {0x61, 0x61, 0x61, 0x61, 0x61, 0x61}, // "aaaaaa"
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x66}, // "ffffff"
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x67}, // "ffffffg"
    {0x46, 0x46, 0x46, 0x46, 0x46, 0x46}, // "FFFFFF" ascii
    {0x31, 0x32, 0x33, 0x33, 0x32, 0x31}, // "123321"
    {0x31, 0x32, 0x33, 0x34, 0x35, 0x37},
    {0x38, 0x38, 0x38, 0x38, 0x38, 0x38}, // "888888"
    {0x38, 0x36, 0x37, 0x35, 0x33, 0x30}, // "867530"
    {0x32, 0x37, 0x30, 0x38, 0x38, 0x39}, // "270889"
    {0x39, 0x30, 0x39, 0x30, 0x39, 0x30}, // "909090"
    {0x31, 0x34, 0x31, 0x34, 0x31, 0x34}, // "141414"
    {0x36, 0x39, 0x36, 0x39, 0x36, 0x39}, // "696969"
    {0x4B, 0x44, 0x45, 0x4C, 0x4F, 0x47}, // "KDELOG"
    {0x41, 0x42, 0x43, 0x44, 0x45, 0x46}, // "ABCDEF"
    {0x4D, 0x49, 0x46, 0x41, 0x52, 0x45}, // "MIFARE"
};

// job bookkeeping
static volatile mf1_offline_clone_state_t m_job_state = MF1_OFFLINE_CLONE_IDLE;
static volatile uint8_t m_job_missing_sectors = 0;
static volatile bool m_job_request = false;

void mf1_offline_clone_job_request(void) {
    m_job_request = true;
    // mark as running right away so a host polling the status can tell a
    // queued/running job apart from a stale result
    job_set_state(MF1_OFFLINE_CLONE_RUNNING, 0);
}

bool mf1_offline_clone_job_take_request(void) {
    if (m_job_request) {
        m_job_request = false;
        return true;
    }
    return false;
}

mf1_offline_clone_state_t mf1_offline_clone_job_state(void) {
    return m_job_state;
}

uint8_t mf1_offline_clone_job_missing_sectors(void) {
    return m_job_missing_sectors;
}

static void job_set_state(mf1_offline_clone_state_t state, uint8_t missing) {
    m_job_state = state;
    m_job_missing_sectors = missing;
}

// Map SAK to the matching emulation tag type, 0 if not a Mifare Classic.
static tag_specific_type_t mf1_type_from_sak(uint8_t sak) {
    switch (sak) {
        case 0x09:
            return TAG_TYPE_MIFARE_Mini;
        case 0x08:
            return TAG_TYPE_MIFARE_1024;
        case 0x18:
            return TAG_TYPE_MIFARE_2048;
        case 0x38:
            return TAG_TYPE_MIFARE_4096;
        default:
            return TAG_TYPE_UNDEFINED;
    }
}

static uint8_t mf1_sector_count(tag_specific_type_t type) {
    switch (type) {
        case TAG_TYPE_MIFARE_Mini:
            return 5;
        case TAG_TYPE_MIFARE_1024:
            return 16;
        case TAG_TYPE_MIFARE_2048:
            return 32;
        case TAG_TYPE_MIFARE_4096:
            return 40;
        default:
            return 0;
    }
}

static void mf1_sector_geometry(uint8_t sector, uint8_t *base_block, uint8_t *block_count) {
    if (sector < 32) {
        *base_block = sector * 4;
        *block_count = 4;
    } else {
        *base_block = 128 + (sector - 32) * 16;
        *block_count = 16;
    }
}

uint16_t mf1_offline_clone_execute(uint8_t slot) {
    job_set_state(MF1_OFFLINE_CLONE_RUNNING, 0);

    picc_14a_tag_t tag;

    pcd_14a_reader_reset();
    pcd_14a_reader_antenna_on();
    bsp_delay_ms(8);

    // 1. find the card
    uint16_t status = pcd_14a_reader_scan_auto(&tag);
    if (status != STATUS_HF_TAG_OK) {
        pcd_14a_reader_antenna_off();
        NRF_LOG_WARNING("offline clone: no card found");
        job_set_state(MF1_OFFLINE_CLONE_DONE_NO_CARD, 0);
        return status;
    }

    // 2. make sure it is a Mifare Classic we can emulate
    tag_specific_type_t card_type = mf1_type_from_sak(tag.sak);
    if (card_type == TAG_TYPE_UNDEFINED) {
        pcd_14a_reader_antenna_off();
        NRF_LOG_WARNING("offline clone: not a Mifare Classic (sak=%02x)", tag.sak);
        job_set_state(MF1_OFFLINE_CLONE_DONE_NOT_MF1, 0);
        return STATUS_HF_ERR_STAT;
    }
    uint8_t sector_count = mf1_sector_count(card_type);

    // 3. switch the slot to the detected card type if needed; this also loads
    //    the current FDS content into the shared buffer. On any failure below
    //    the original slot type is restored, which reloads the previous
    //    content from FDS and leaves the slot untouched. (A degenerate slot
    //    configured as TAG_TYPE_UNDEFINED cannot be restored through the
    //    public API - such slots only exist with a corrupted config.)
    tag_slot_specific_type_t slot_types;
    tag_emulation_get_specific_types_by_slot(slot, &slot_types);
    tag_specific_type_t original_type = slot_types.tag_hf;
    if (original_type != card_type) {
        tag_emulation_change_type(slot, card_type);
    }

    tag_data_buffer_t *buffer = get_buffer_by_tag_type(card_type);
    if (buffer == NULL) {
        pcd_14a_reader_antenna_off();
        tag_emulation_change_type(slot, original_type); // best effort restore
        job_set_state(MF1_OFFLINE_CLONE_DONE_APPLY_FAIL, 0);
        return STATUS_HF_ERR_STAT;
    }
    nfc_tag_mf1_information_t *info = (nfc_tag_mf1_information_t *)buffer->buffer;

    // 4. build the dictionary: common keys + UID derived candidates
    mf1_key_t dict[MF1_OFFLINE_CLONE_DICT_LEN + 2];
    uint8_t dict_len = 0;
    for (uint8_t i = 0; i < MF1_OFFLINE_CLONE_DICT_LEN; i++) {
        memcpy(dict[dict_len++].key, m_common_keys[i], 6);
    }
    // some deployments derive keys from the UID
    memcpy(dict[dict_len].key, tag.uid, 4);
    dict[dict_len].key[4] = tag.uid[0];
    dict[dict_len].key[5] = tag.uid[1];
    dict_len++;
    memcpy(dict[dict_len].key, tag.uid, 4);
    dict[dict_len].key[4] = 0xFF;
    dict[dict_len].key[5] = 0xFF;
    dict_len++;

    // 5. dictionary attack (also harvests readable key B from the trailers).
    //    Mask bits per sector: 0b10 = skip key A, 0b01 = skip key B, so
    //    0b11 skips a sector completely - used to skip the sectors that do
    //    not exist on the detected card size.
    mf1_toolbox_check_keys_of_sectors_in_t in = {
        .mask = {{0}},
        .keys_len = dict_len,
        .keys = dict,
    };
    for (uint8_t i = sector_count; i < 40; i++) {
        uint8_t mask_shift = 6 - i % 4 * 2;
        in.mask.b[i / 4] |= 0b11 << mask_shift;
    }
    mf1_toolbox_check_keys_of_sectors_out_t out;
    status = mf1_toolbox_check_keys_of_sectors(&in, &out);
    if (status != STATUS_HF_TAG_OK) {
        NRF_LOG_WARNING("offline clone: card lost during key check (status=%02x)", status);
        pcd_14a_reader_antenna_off();
        tag_emulation_change_type(slot, original_type); // restore slot type + content
        job_set_state(MF1_OFFLINE_CLONE_DONE_NO_CARD, 0);
        return status;
    }

    // count the sectors with at least one key missing
    uint8_t missing = 0;
    for (uint8_t i = 0; i < sector_count; i++) {
        uint8_t mask_shift = 6 - i % 4 * 2;
        uint8_t found = (out.found.b[i / 4] >> mask_shift) & 0b11;
        if (found == 0) {
            missing++;
        }
    }
    if (missing > 0) {
        NRF_LOG_WARNING("offline clone: %d/%d sectors have unknown keys", missing, sector_count);
        pcd_14a_reader_antenna_off();
        tag_emulation_change_type(slot, original_type); // restore slot type + content
        job_set_state(MF1_OFFLINE_CLONE_DONE_KEYS_MISSING, missing);
        return STATUS_HF_ERR_STAT;
    }

    // 6. read every block of every sector into the slot buffer
    memset(info->memory, 0, sizeof(info->memory));
    bool read_failed = false;
    for (uint8_t sector = 0; sector < sector_count && !read_failed; sector++) {
        uint8_t mask_shift = 6 - sector % 4 * 2;
        uint8_t found = (out.found.b[sector / 4] >> mask_shift) & 0b11;
        uint8_t base_block = 0;
        uint8_t block_count = 0;
        mf1_sector_geometry(sector, &base_block, &block_count);

        // key A preferred, fall back to key B
        uint8_t key_type = PICC_AUTHENT1A;
        uint8_t *key = out.keys[sector][0].key;
        if (!(found & 0b10)) {
            key_type = PICC_AUTHENT1B;
            key = out.keys[sector][1].key;
        }

        bool sector_ok = false;
        for (uint8_t retry = 0; retry < 2; retry++) {
            bsp_wdt_feed();
            if (retry > 0) {
                // the previous attempt may have killed the field, restart it
                pcd_14a_reader_reset();
                pcd_14a_reader_antenna_on();
                bsp_delay_ms(8);
            }
            status = auth_key_use_522_hw(base_block, key_type, key);
            if (status != STATUS_HF_TAG_OK) {
                continue; // re-scan + re-auth on the next try
            }
            sector_ok = true;
            for (uint8_t b = 0; b < block_count; b++) {
                uint8_t block_data[18] = {0};
                status = pcd_14a_reader_mf1_read(base_block + b, block_data);
                if (status != STATUS_HF_TAG_OK) {
                    sector_ok = false;
                    break;
                }
                memcpy(info->memory[base_block + b], block_data, NFC_TAG_MF1_DATA_SIZE);
            }
            if (sector_ok) {
                break;
            }
        }
        if (!sector_ok) {
            read_failed = true;
        }
    }

    pcd_14a_reader_antenna_off();

    if (read_failed) {
        NRF_LOG_WARNING("offline clone: block read failed, restoring slot content");
        tag_emulation_change_type(slot, original_type); // restore slot type + content
        job_set_state(MF1_OFFLINE_CLONE_DONE_READ_FAIL, 0);
        return STATUS_HF_ERR_STAT;
    }

    // 7. apply the anticollision identity of the real card (same pattern as
    //    the offline UID clone), then persist everything to flash.
    nfc_tag_14a_coll_res_entity_t *antres = &info->res_coll;
    antres->size = tag.uid_len;
    memcpy(antres->uid, tag.uid, tag.uid_len);
    memcpy(antres->atqa, tag.atqa, 2);
    antres->sak[0] = tag.sak;
    antres->ats.length = tag.ats_len;
    memcpy(antres->ats.data, tag.ats, tag.ats_len);

    tag_emulation_save();

    NRF_LOG_INFO("offline clone: card cloned into slot %d", slot);
    job_set_state(MF1_OFFLINE_CLONE_DONE_OK, 0);
    return STATUS_HF_TAG_OK;
}
