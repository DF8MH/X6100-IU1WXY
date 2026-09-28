#ifndef SSTV_DECODER_H
#define SSTV_DECODER_H

#include <stdint.h>

#define SSTV_SAMPLE_RATE 11025
#define SSTV_MAX_W 320
#define SSTV_MAX_H 256
#define SSTV_ROBOT36_W 320
#define SSTV_ROBOT36_H 240

typedef enum {
    SSTV_MODE_UNKNOWN = 0,
    SSTV_MODE_ROBOT8_BW,
    SSTV_MODE_ROBOT36,
    SSTV_MODE_ROBOT72,
    SSTV_MODE_MARTIN_M1,
    SSTV_MODE_MARTIN_M2,
    SSTV_MODE_SCOTTIE_S1,
    SSTV_MODE_SCOTTIE_S2,
    SSTV_MODE_SCOTTIE_DX
} sstv_mode_t;

typedef struct {
    sstv_mode_t mode;
    int vis;
    int width;
    int height;
    const char *name;
} sstv_info_t;

/* Detect the SSTV VIS header in a mono 11025 Hz PCM WAV.
 * Returns 0 on success, <0 on error.  The filename is never used to
 * determine the mode.
 */
int sstv_detect_wav(const char *path, sstv_info_t *info);

/* Current image decoder.  Detection is automatic; at this stage only
 * Robot 36 image decoding is implemented.  rgb must hold at least
 * SSTV_MAX_W*SSTV_MAX_H*3 bytes.  Returns decoded rows, or <0.
 */
int sstv_decode_wav(const char *path, uint8_t *rgb, sstv_info_t *info);

/* Decode an already-captured 11025 Hz mono sample buffer. No VIS re-detection. */
int sstv_decode_samples(const float *samples, long count, sstv_mode_t mode, uint8_t *rgb, sstv_info_t *info);

/* Manual continuous RX decoder. The capture may start/end anywhere inside a
 * transmission and contains no VIS header. The normal Auto decoder path is
 * intentionally left untouched; this wrapper only supplies a synthetic
 * pre-roll so the existing robust per-mode decoders can lock to the first
 * usable sync found in the arbitrary capture. */
int sstv_decode_cont_samples(const float *samples, long count, sstv_mode_t mode, uint8_t *rgb, sstv_info_t *info);

#endif
