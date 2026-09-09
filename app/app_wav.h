#ifndef __APP_WAV_H
#define __APP_WAV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ff.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_WAV_PATH_MAX       96U
#define APP_WAV_NAME_MAX       64U

typedef enum
{
    APP_WAV_OK = 0,
    APP_WAV_ERROR,
    APP_WAV_INVALID_PARAM,
    APP_WAV_MOUNT_FAILED,
    APP_WAV_OPEN_FAILED,
    APP_WAV_UNSUPPORTED,
    APP_WAV_READ_FAILED,
    APP_WAV_SEEK_FAILED
} app_wav_status_t;

typedef struct
{
    uint16_t audio_format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t data_offset;
    uint32_t data_size;
} app_wav_info_t;

typedef struct
{
    char name[APP_WAV_NAME_MAX];
    char path[APP_WAV_PATH_MAX];
    uint32_t size;
} app_wav_track_t;

typedef struct
{
    FIL file;
    app_wav_info_t info;
    uint32_t data_read;
    bool opened;
} app_wav_file_t;

/* 增量扫描上下文：由音频任务分步驱动，每步之间让出 CPU 以避免阻塞 UI。
 * DIR 句柄跨步保持打开，因此调用方必须保证扫描期间独占 FatFs/TF 卡。 */
typedef struct
{
    DIR dir;
    char path_buf[APP_WAV_PATH_MAX];
    char prefix[APP_WAV_PATH_MAX];
    size_t prefix_len;
    uint8_t phase;       /* 0=根目录, 1=/Music, 2=完成 */
    bool dir_open;
    app_wav_status_t status;
} app_wav_scan_ctx_t;

app_wav_status_t app_wav_mount(void);
app_wav_status_t app_wav_scan_root(app_wav_track_t *tracks, size_t max_tracks, size_t *track_count);
app_wav_status_t app_wav_scan_ctx_begin(app_wav_scan_ctx_t *ctx);
bool app_wav_scan_ctx_step(app_wav_scan_ctx_t *ctx, app_wav_track_t *tracks,
                           size_t max_tracks, size_t *track_count, size_t budget);
void app_wav_scan_ctx_end(app_wav_scan_ctx_t *ctx);
app_wav_status_t app_wav_open(app_wav_file_t *wav, const char *path);
app_wav_status_t app_wav_read(app_wav_file_t *wav, void *buffer, uint32_t bytes_to_read, uint32_t *bytes_read);
app_wav_status_t app_wav_rewind_data(app_wav_file_t *wav);
app_wav_status_t app_wav_seek_data(app_wav_file_t *wav, uint32_t byte_offset);
void app_wav_close(app_wav_file_t *wav);

#ifdef __cplusplus
}
#endif

#endif
