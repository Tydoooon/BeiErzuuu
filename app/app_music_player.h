#ifndef __APP_MUSIC_PLAYER_H
#define __APP_MUSIC_PLAYER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_wav.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    APP_MUSIC_PLAYER_STATE_IDLE = 0,
    APP_MUSIC_PLAYER_STATE_LOADING,
    APP_MUSIC_PLAYER_STATE_PLAYING,
    APP_MUSIC_PLAYER_STATE_ERROR
} app_music_player_state_t;

typedef enum
{
    APP_MUSIC_PLAYER_ERROR_NONE = 0,
    APP_MUSIC_PLAYER_ERROR_INVALID_PARAM,
    APP_MUSIC_PLAYER_ERROR_QUEUE_FULL,
    APP_MUSIC_PLAYER_ERROR_WAV_MOUNT,
    APP_MUSIC_PLAYER_ERROR_WAV_OPEN,
    APP_MUSIC_PLAYER_ERROR_WAV_UNSUPPORTED,
    APP_MUSIC_PLAYER_ERROR_WAV_READ,
    APP_MUSIC_PLAYER_ERROR_WAV_SEEK,
    APP_MUSIC_PLAYER_ERROR_I2S_INIT,
    APP_MUSIC_PLAYER_ERROR_I2S_TRANSMIT,
    APP_MUSIC_PLAYER_ERROR_SD_LOW,
    APP_MUSIC_PLAYER_ERROR_BUZZER_OUTPUT
} app_music_player_error_t;

typedef enum
{
    APP_MUSIC_PLAYER_TEST_STAGE_NONE = 0,
    APP_MUSIC_PLAYER_TEST_STAGE_PREP,
    APP_MUSIC_PLAYER_TEST_STAGE_SD_HIGH,
    APP_MUSIC_PLAYER_TEST_STAGE_TX_OK,
    APP_MUSIC_PLAYER_TEST_STAGE_DONE
} app_music_player_test_stage_t;

typedef struct
{
    app_music_player_state_t state;
    app_wav_status_t last_status;
    app_music_player_error_t last_error;
    bool sd_enabled;
    app_music_player_test_stage_t test_stage;
    bool test_sd_seen_high;
    bool test_tx_sent;
    app_wav_info_t wav_info;
    char track_name[APP_WAV_NAME_MAX];
    char track_path[APP_WAV_PATH_MAX];
    uint32_t bytes_played;
    uint32_t diag;   /* 定位“I2S transmit failed”确切失败点的位掩码 */
} app_music_player_status_t;

/* 诊断位（status.diag） */
#define APP_MUSIC_PLAYER_DIAG_FIRST_TX_FAIL   (1u << 0)  /* 第一次 TxStartDma 就返回非 OK */
#define APP_MUSIC_PLAYER_DIAG_DMA_ERROR       (1u << 1)  /* I2S ErrorCallback 触发（DMA 传输错误 TE） */
#define APP_MUSIC_PLAYER_DIAG_TIMEOUT         (1u << 2)  /* 等 DMA 完成事件超时（TC 从未到来） */
#define APP_MUSIC_PLAYER_DIAG_UNDERRUN        (1u << 3)  /* 下溢：读卡太慢/缓冲太小 */
#define APP_MUSIC_PLAYER_DIAG_I2S_STATE_SHIFT 8u          /* 失败瞬间 hi2s3.State 记录在 bit8..15 */

void app_music_player_init(void);
bool app_music_player_request_play(const app_wav_track_t *track);
bool app_music_player_request_tone_test(void);
void app_music_player_request_stop(void);
app_music_player_state_t app_music_player_get_state(void);
void app_music_player_get_status(app_music_player_status_t *status);
uint32_t app_music_player_get_underrun_count(void);
void app_music_player_task(void *argument);

#ifdef __cplusplus
}
#endif

#endif
