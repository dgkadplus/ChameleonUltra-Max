#ifndef MF1_OFFLINE_CLONE_H
#define MF1_OFFLINE_CLONE_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Offline full-card clone for Mifare Classic.
 *
 * Reads a physical Mifare Classic card (Mini/1K/2K/4K) with the RC522 reader,
 * recovers sector keys with an on-device common-key dictionary
 * (mf1_toolbox_check_keys_of_sectors, which also harvests readable key B
 * from sector trailers) and then reads every block and writes the whole
 * card (UID/ATQA/SAK/ATS + data + keys) into the current emulation slot.
 *
 * This is fully offline: no host is needed. It is NOT a crypto1 break:
 * sectors protected by unknown, non-default keys cannot be recovered
 * offline (RAM on the nRF52840 is far too small for darkside/nested
 * key recovery). For such cards use the host-assisted attack flow
 * (darkside / nested / hardnested) in the companion app and then load
 * the dump into the slot.
 */

typedef enum {
    MF1_OFFLINE_CLONE_IDLE = 0,      // no job ran yet
    MF1_OFFLINE_CLONE_RUNNING,       // job currently executing
    MF1_OFFLINE_CLONE_DONE_OK,       // card fully cloned into the slot
    MF1_OFFLINE_CLONE_DONE_NO_CARD,  // no (readable) card in the field
    MF1_OFFLINE_CLONE_DONE_NOT_MF1,  // card is not a Mifare Classic
    MF1_OFFLINE_CLONE_DONE_KEYS_MISSING, // some sector keys not in dictionary
    MF1_OFFLINE_CLONE_DONE_READ_FAIL,    // a block could not be read
    MF1_OFFLINE_CLONE_DONE_APPLY_FAIL,   // writing the slot failed
} mf1_offline_clone_state_t;

// Run one full clone job. The caller must already be in reader mode with the
// antenna available (this function manages reset/antenna itself). Blocking:
// typically a few seconds, worst case (nothing matches the dictionary on a
// 4K card) a few minutes; feeds the watchdog.
// Returns STATUS_HF_TAG_OK on success, an error status otherwise. The final
// state is also kept for mf1_offline_clone_job_state().
uint16_t mf1_offline_clone_execute(uint8_t slot);

// Ask the main loop to run a clone job on the active slot (non-blocking
// request, e.g. from a command processor). Only one request is remembered.
void mf1_offline_clone_job_request(void);

// Consume a pending job request (main loop side).
bool mf1_offline_clone_job_take_request(void);

// Last (or current) job state and number of sectors whose keys were missing.
mf1_offline_clone_state_t mf1_offline_clone_job_state(void);
uint8_t mf1_offline_clone_job_missing_sectors(void);

#endif
