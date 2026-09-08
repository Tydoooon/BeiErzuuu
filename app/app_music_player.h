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
    APP_MUSIC_PLAYER_ERROR_SD_LOW
} app_music_player_error_t;

typedef struct
{
    app_music_player_state_t state;
    app_wav_status_t last_status;
    app_music_player_error_t last_error;
    app_wav_info_t wav_info;
    char track_name[APP_WAV_NAME_MAX];
    char track_path[APP_WAV_PATH_MAX];
    uint32_t bytes_played;
} app_music_player_status_t;

void app_music_player_init(void);
bool app_music_player_request_play(const app_wav_track_t *track);
void app_music_player_request_stop(void);
app_music_player_state_t app_music_player_get_state(void);
void app_music_player_get_status(app_music_player_status_t *status);
void app_music_player_task(void *argument);

#ifdef __cplusplus
}
#endif

#endif
