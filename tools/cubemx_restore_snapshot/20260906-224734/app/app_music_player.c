#include "app_music_player.h"

#include "app_event.h"
#include "bsp_max98357a.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "i2s.h"
#include <string.h>

#define APP_MUSIC_PLAYER_QUEUE_LEN      4U
#define APP_MUSIC_PLAYER_IN_BYTES       512U
#define APP_MUSIC_PLAYER_OUT_WORDS      1024U
#define APP_MUSIC_PLAYER_TIMEOUT_MS     1000U
#define APP_MUSIC_PLAYER_TONE_FREQ_HZ   440U
#define APP_MUSIC_PLAYER_TONE_RATE_HZ   48000U
#define APP_MUSIC_PLAYER_TONE_DURATION_MS 1000U
#define APP_MUSIC_PLAYER_TONE_TABLE_LEN  32U

typedef enum
{
    APP_MUSIC_PLAYER_CMD_PLAY = 0,
    APP_MUSIC_PLAYER_CMD_TONE_TEST,
    APP_MUSIC_PLAYER_CMD_STOP
} app_music_player_cmd_type_t;

typedef struct
{
    app_music_player_cmd_type_t type;
    app_wav_track_t track;
} app_music_player_cmd_t;

static QueueHandle_t g_music_cmd_queue;
static StaticQueue_t g_music_cmd_queue_buf;
static uint8_t g_music_cmd_queue_storage[APP_MUSIC_PLAYER_QUEUE_LEN * sizeof(app_music_player_cmd_t)];
static app_music_player_status_t g_music_status;
static uint8_t g_music_in_buf[APP_MUSIC_PLAYER_IN_BYTES];
static uint16_t g_music_out_buf[APP_MUSIC_PLAYER_OUT_WORDS];
static const int16_t g_music_tone_table[APP_MUSIC_PLAYER_TONE_TABLE_LEN] = {
    0, 6393, 12539, 18204, 23170, 27245, 30273, 32137,
    32767, 32137, 30273, 27245, 23170, 18204, 12539, 6393,
    0, -6393, -12539, -18204, -23170, -27245, -30273, -32137,
    -32767, -32137, -30273, -27245, -23170, -18204, -12539, -6393
};

static void app_music_player_reset_status(void);
static void app_music_player_set_state(app_music_player_state_t state, app_wav_status_t status, app_music_player_error_t error);
static void app_music_player_set_sd_enabled(bool enabled);
static void app_music_player_set_test_stage(app_music_player_test_stage_t stage);
static bool app_music_player_send_cmd(const app_music_player_cmd_t *cmd);
static void app_music_player_play_track(const app_wav_track_t *track);
static void app_music_player_play_tone_test(void);
static size_t app_music_player_prepare_pcm(const uint8_t *in, size_t in_len, uint16_t *out, size_t out_cap, const app_wav_info_t *info);
static size_t app_music_player_generate_tone(uint16_t *out, size_t out_cap, uint32_t frames_to_generate, uint32_t *phase, uint32_t phase_step);
static uint16_t app_music_player_le16(const uint8_t *data);
static app_music_player_error_t app_music_player_error_from_wav_status(app_wav_status_t status);

void app_music_player_init(void)
{
    if (g_music_cmd_queue == NULL) {
        g_music_cmd_queue = xQueueCreateStatic(APP_MUSIC_PLAYER_QUEUE_LEN,
                                               sizeof(app_music_player_cmd_t),
                                               g_music_cmd_queue_storage,
                                               &g_music_cmd_queue_buf);
    }

    app_music_player_reset_status();
    if (g_music_cmd_queue != NULL) {
        xQueueReset(g_music_cmd_queue);
    }

    (void)BSP_MAX98357A_Init();
}

bool app_music_player_request_play(const app_wav_track_t *track)
{
    app_music_player_cmd_t cmd;

    if (track == NULL || track->path[0] == '\0') {
        app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_INVALID_PARAM, APP_MUSIC_PLAYER_ERROR_INVALID_PARAM);
        return false;
    }
    if (g_music_cmd_queue == NULL) {
        app_music_player_init();
    }
    if (g_music_cmd_queue == NULL) {
        app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_ERROR, APP_MUSIC_PLAYER_ERROR_QUEUE_FULL);
        return false;
    }

    cmd.type = APP_MUSIC_PLAYER_CMD_PLAY;
    cmd.track = *track;
    if (!app_music_player_send_cmd(&cmd)) {
        app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_ERROR, APP_MUSIC_PLAYER_ERROR_QUEUE_FULL);
        return false;
    }

    return true;
}

bool app_music_player_request_tone_test(void)
{
    app_music_player_cmd_t cmd;

    if (g_music_cmd_queue == NULL) {
        app_music_player_init();
    }
    if (g_music_cmd_queue == NULL) {
        app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_ERROR, APP_MUSIC_PLAYER_ERROR_QUEUE_FULL);
        return false;
    }

    cmd.type = APP_MUSIC_PLAYER_CMD_TONE_TEST;
    memset(&cmd.track, 0, sizeof(cmd.track));
    if (!app_music_player_send_cmd(&cmd)) {
        app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_ERROR, APP_MUSIC_PLAYER_ERROR_QUEUE_FULL);
        return false;
    }

    return true;
}

void app_music_player_request_stop(void)
{
    app_music_player_cmd_t cmd;

    if (g_music_cmd_queue == NULL) {
        return;
    }

    cmd.type = APP_MUSIC_PLAYER_CMD_STOP;
    memset(&cmd.track, 0, sizeof(cmd.track));
    (void)app_music_player_send_cmd(&cmd);
}

app_music_player_state_t app_music_player_get_state(void)
{
    app_music_player_state_t state;

    taskENTER_CRITICAL();
    state = g_music_status.state;
    taskEXIT_CRITICAL();

    return state;
}

void app_music_player_get_status(app_music_player_status_t *status)
{
    if (status == NULL) {
        return;
    }

    taskENTER_CRITICAL();
    *status = g_music_status;
    taskEXIT_CRITICAL();
}

void app_music_player_task(void *argument)
{
    app_music_player_cmd_t cmd;

    (void)argument;

    for (;;) {
        if (g_music_cmd_queue == NULL) {
            vTaskDelay(pdMS_TO_TICKS(10U));
            continue;
        }

        if (xQueueReceive(g_music_cmd_queue, &cmd, portMAX_DELAY) != pdPASS) {
            continue;
        }

        if (cmd.type == APP_MUSIC_PLAYER_CMD_STOP) {
            app_music_player_set_state(APP_MUSIC_PLAYER_STATE_IDLE, APP_WAV_OK, APP_MUSIC_PLAYER_ERROR_NONE);
            app_event_clear(APP_EVT_MUSIC_PLAYING);
            (void)BSP_MAX98357A_Enable(false);
            continue;
        }

        if (cmd.type == APP_MUSIC_PLAYER_CMD_TONE_TEST) {
            app_music_player_play_tone_test();
            continue;
        }

        app_music_player_play_track(&cmd.track);
    }
}

static void app_music_player_reset_status(void)
{
    taskENTER_CRITICAL();
    memset(&g_music_status, 0, sizeof(g_music_status));
    g_music_status.state = APP_MUSIC_PLAYER_STATE_IDLE;
    g_music_status.last_status = APP_WAV_OK;
    g_music_status.last_error = APP_MUSIC_PLAYER_ERROR_NONE;
    g_music_status.sd_enabled = false;
    g_music_status.test_stage = APP_MUSIC_PLAYER_TEST_STAGE_NONE;
    g_music_status.test_sd_seen_high = false;
    g_music_status.test_tx_sent = false;
    taskEXIT_CRITICAL();
}

static void app_music_player_set_state(app_music_player_state_t state, app_wav_status_t status, app_music_player_error_t error)
{
    taskENTER_CRITICAL();
    g_music_status.state = state;
    g_music_status.last_status = status;
    g_music_status.last_error = error;
    if (state != APP_MUSIC_PLAYER_STATE_PLAYING) {
        g_music_status.sd_enabled = false;
    }
    taskEXIT_CRITICAL();
}

static void app_music_player_set_sd_enabled(bool enabled)
{
    taskENTER_CRITICAL();
    g_music_status.sd_enabled = enabled;
    taskEXIT_CRITICAL();
}

static void app_music_player_set_test_stage(app_music_player_test_stage_t stage)
{
    taskENTER_CRITICAL();
    g_music_status.test_stage = stage;
    taskEXIT_CRITICAL();
}

static void app_music_player_set_test_flags(bool sd_seen_high, bool tx_sent)
{
    taskENTER_CRITICAL();
    g_music_status.test_sd_seen_high = sd_seen_high;
    g_music_status.test_tx_sent = tx_sent;
    taskEXIT_CRITICAL();
}

static bool app_music_player_send_cmd(const app_music_player_cmd_t *cmd)
{
    if (cmd == NULL || g_music_cmd_queue == NULL) {
        return false;
    }

    return xQueueSend(g_music_cmd_queue, cmd, 0) == pdPASS;
}

static void app_music_player_play_track(const app_wav_track_t *track)
{
    app_wav_file_t wav;
    app_wav_status_t status;
    app_music_player_cmd_t cmd;
    uint32_t bytes_read = 0;
    uint32_t bytes_played = 0;

    if (track == NULL) {
        return;
    }

    app_music_player_set_state(APP_MUSIC_PLAYER_STATE_LOADING, APP_WAV_OK, APP_MUSIC_PLAYER_ERROR_NONE);
    app_music_player_set_test_stage(APP_MUSIC_PLAYER_TEST_STAGE_NONE);
    app_music_player_set_test_flags(false, false);
    taskENTER_CRITICAL();
    memset(g_music_status.track_name, 0, sizeof(g_music_status.track_name));
    memset(g_music_status.track_path, 0, sizeof(g_music_status.track_path));
    memcpy(g_music_status.track_name, track->name, strnlen(track->name, sizeof(g_music_status.track_name) - 1U));
    memcpy(g_music_status.track_path, track->path, strnlen(track->path, sizeof(g_music_status.track_path) - 1U));
    g_music_status.bytes_played = 0U;
    taskEXIT_CRITICAL();

    status = app_wav_open(&wav, track->path);
    if (status != APP_WAV_OK) {
        app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, status, app_music_player_error_from_wav_status(status));
        app_event_clear(APP_EVT_MUSIC_PLAYING);
        (void)BSP_MAX98357A_Enable(false);
        return;
    }

    taskENTER_CRITICAL();
    g_music_status.wav_info = wav.info;
    taskEXIT_CRITICAL();

    if (BSP_MAX98357A_SetSampleRate(wav.info.sample_rate) != BSP_MAX98357A_OK) {
        app_wav_close(&wav);
        app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_ERROR, APP_MUSIC_PLAYER_ERROR_I2S_INIT);
        app_event_clear(APP_EVT_MUSIC_PLAYING);
        (void)BSP_MAX98357A_Enable(false);
        app_music_player_set_sd_enabled(false);
        return;
    }

    (void)BSP_MAX98357A_Enable(true);
    {
        bool sd_enabled = false;
        if (BSP_MAX98357A_GetEnableState(&sd_enabled) != BSP_MAX98357A_OK || !sd_enabled) {
            app_wav_close(&wav);
            app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_ERROR, APP_MUSIC_PLAYER_ERROR_SD_LOW);
            app_event_clear(APP_EVT_MUSIC_PLAYING);
            (void)BSP_MAX98357A_Enable(false);
            app_music_player_set_sd_enabled(false);
            return;
        }
        app_music_player_set_sd_enabled(true);
    }
    app_music_player_set_state(APP_MUSIC_PLAYER_STATE_PLAYING, APP_WAV_OK, APP_MUSIC_PLAYER_ERROR_NONE);
    app_event_set(APP_EVT_MUSIC_PLAYING);

    while (g_music_status.state == APP_MUSIC_PLAYER_STATE_PLAYING) {
        size_t chunk = APP_MUSIC_PLAYER_IN_BYTES;
        size_t out_words;

        if (wav.info.block_align != 0U) {
            chunk -= (chunk % wav.info.block_align);
        }
        if (chunk == 0U) {
            chunk = wav.info.block_align != 0U ? wav.info.block_align : APP_MUSIC_PLAYER_IN_BYTES;
        }

        if (xQueuePeek(g_music_cmd_queue, &cmd, 0) == pdPASS && cmd.type == APP_MUSIC_PLAYER_CMD_STOP) {
            (void)xQueueReceive(g_music_cmd_queue, &cmd, 0);
            break;
        }

        status = app_wav_read(&wav, g_music_in_buf, (uint32_t)chunk, &bytes_read);
        if (status != APP_WAV_OK) {
            app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, status, app_music_player_error_from_wav_status(status));
            break;
        }
        if (bytes_read == 0U) {
            break;
        }

        out_words = app_music_player_prepare_pcm(g_music_in_buf, bytes_read, g_music_out_buf,
                                                 APP_MUSIC_PLAYER_OUT_WORDS, &wav.info);
        if (out_words == 0U) {
            app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_UNSUPPORTED, APP_MUSIC_PLAYER_ERROR_WAV_UNSUPPORTED);
            break;
        }

        if (BSP_MAX98357A_TransmitWords(g_music_out_buf, (uint16_t)out_words, APP_MUSIC_PLAYER_TIMEOUT_MS) != BSP_MAX98357A_OK) {
            app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_ERROR, APP_MUSIC_PLAYER_ERROR_I2S_TRANSMIT);
            break;
        }

        bytes_played += bytes_read;
        taskENTER_CRITICAL();
        g_music_status.bytes_played = bytes_played;
        taskEXIT_CRITICAL();
    }

    app_wav_close(&wav);
    (void)BSP_MAX98357A_TransmitSilence(256U, APP_MUSIC_PLAYER_TIMEOUT_MS);
    (void)BSP_MAX98357A_Enable(false);
    app_music_player_set_sd_enabled(false);
    app_event_clear(APP_EVT_MUSIC_PLAYING);

    if (g_music_status.state != APP_MUSIC_PLAYER_STATE_ERROR) {
        app_music_player_set_state(APP_MUSIC_PLAYER_STATE_IDLE, APP_WAV_OK, APP_MUSIC_PLAYER_ERROR_NONE);
    }
}

static void app_music_player_play_tone_test(void)
{
    uint32_t sample_rate = APP_MUSIC_PLAYER_TONE_RATE_HZ;
    uint32_t total_frames;
    uint32_t frames_played = 0U;
    uint32_t phase = 0U;
    uint32_t phase_step;
    app_music_player_cmd_t cmd;

    total_frames = (sample_rate * APP_MUSIC_PLAYER_TONE_DURATION_MS) / 1000U;
    phase_step = (uint32_t)(((uint64_t)APP_MUSIC_PLAYER_TONE_FREQ_HZ << 32) / sample_rate);

    app_music_player_set_state(APP_MUSIC_PLAYER_STATE_LOADING, APP_WAV_OK, APP_MUSIC_PLAYER_ERROR_NONE);
    app_music_player_set_test_stage(APP_MUSIC_PLAYER_TEST_STAGE_PREP);
    app_music_player_set_test_flags(false, false);
    taskENTER_CRITICAL();
    memset(g_music_status.track_name, 0, sizeof(g_music_status.track_name));
    memset(g_music_status.track_path, 0, sizeof(g_music_status.track_path));
    memcpy(g_music_status.track_name, "Amp test tone", sizeof("Amp test tone"));
    memcpy(g_music_status.track_path, "48kHz PCM selftest", sizeof("48kHz PCM selftest"));
    g_music_status.wav_info.audio_format = 1U;
    g_music_status.wav_info.channels = 2U;
    g_music_status.wav_info.sample_rate = sample_rate;
    g_music_status.wav_info.byte_rate = sample_rate * 4U;
    g_music_status.wav_info.block_align = 4U;
    g_music_status.wav_info.bits_per_sample = 16U;
    g_music_status.wav_info.data_offset = 0U;
    g_music_status.wav_info.data_size = total_frames * 4U;
    g_music_status.bytes_played = 0U;
    taskEXIT_CRITICAL();

    if (BSP_MAX98357A_SetSampleRate(sample_rate) != BSP_MAX98357A_OK) {
        app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_ERROR, APP_MUSIC_PLAYER_ERROR_I2S_INIT);
        app_music_player_set_test_stage(APP_MUSIC_PLAYER_TEST_STAGE_PREP);
        app_event_clear(APP_EVT_MUSIC_PLAYING);
        (void)BSP_MAX98357A_Enable(false);
        app_music_player_set_sd_enabled(false);
        return;
    }

    (void)BSP_MAX98357A_Enable(true);
    {
        bool sd_enabled = false;
        if (BSP_MAX98357A_GetEnableState(&sd_enabled) != BSP_MAX98357A_OK || !sd_enabled) {
            app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_ERROR, APP_MUSIC_PLAYER_ERROR_SD_LOW);
            app_music_player_set_test_stage(APP_MUSIC_PLAYER_TEST_STAGE_PREP);
            app_event_clear(APP_EVT_MUSIC_PLAYING);
            (void)BSP_MAX98357A_Enable(false);
            app_music_player_set_sd_enabled(false);
            app_music_player_set_test_flags(false, false);
            return;
        }
        app_music_player_set_sd_enabled(true);
        app_music_player_set_test_flags(true, false);
        app_music_player_set_test_stage(APP_MUSIC_PLAYER_TEST_STAGE_SD_HIGH);
    }
    app_music_player_set_state(APP_MUSIC_PLAYER_STATE_PLAYING, APP_WAV_OK, APP_MUSIC_PLAYER_ERROR_NONE);
    app_event_set(APP_EVT_MUSIC_PLAYING);

    while (frames_played < total_frames && g_music_status.state == APP_MUSIC_PLAYER_STATE_PLAYING) {
        size_t chunk_frames = (size_t)(APP_MUSIC_PLAYER_OUT_WORDS / 2U);
        size_t out_words;

        if ((uint32_t)chunk_frames > (total_frames - frames_played)) {
            chunk_frames = (size_t)(total_frames - frames_played);
        }

        if (xQueuePeek(g_music_cmd_queue, &cmd, 0) == pdPASS && cmd.type == APP_MUSIC_PLAYER_CMD_STOP) {
            (void)xQueueReceive(g_music_cmd_queue, &cmd, 0);
            break;
        }

        out_words = app_music_player_generate_tone(g_music_out_buf, APP_MUSIC_PLAYER_OUT_WORDS,
                                                   (uint32_t)chunk_frames, &phase, phase_step);
        if (out_words == 0U) {
            app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_ERROR, APP_MUSIC_PLAYER_ERROR_I2S_TRANSMIT);
            break;
        }

        if (BSP_MAX98357A_TransmitWords(g_music_out_buf, (uint16_t)out_words, APP_MUSIC_PLAYER_TIMEOUT_MS) != BSP_MAX98357A_OK) {
            app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_ERROR, APP_MUSIC_PLAYER_ERROR_I2S_TRANSMIT);
            break;
        }
        app_music_player_set_test_flags(true, true);
        app_music_player_set_test_stage(APP_MUSIC_PLAYER_TEST_STAGE_TX_OK);

        frames_played += (uint32_t)chunk_frames;
        taskENTER_CRITICAL();
        g_music_status.bytes_played = frames_played * 4U;
        taskEXIT_CRITICAL();
    }

    (void)BSP_MAX98357A_TransmitSilence(256U, APP_MUSIC_PLAYER_TIMEOUT_MS);
    (void)BSP_MAX98357A_Enable(false);
    app_music_player_set_sd_enabled(false);
    if (g_music_status.state != APP_MUSIC_PLAYER_STATE_ERROR && g_music_status.test_tx_sent) {
        app_music_player_set_test_stage(APP_MUSIC_PLAYER_TEST_STAGE_DONE);
    }
    app_event_clear(APP_EVT_MUSIC_PLAYING);

    if (g_music_status.state != APP_MUSIC_PLAYER_STATE_ERROR) {
        app_music_player_set_state(APP_MUSIC_PLAYER_STATE_IDLE, APP_WAV_OK, APP_MUSIC_PLAYER_ERROR_NONE);
    }
}

static size_t app_music_player_prepare_pcm(const uint8_t *in, size_t in_len, uint16_t *out, size_t out_cap, const app_wav_info_t *info)
{
    size_t out_words = 0U;

    if (in == NULL || out == NULL || info == NULL || in_len == 0U) {
        return 0U;
    }

    if (info->audio_format != 1U) {
        return 0U;
    }

    if (info->bits_per_sample == 16U) {
        if (info->channels == 2U) {
            size_t words = in_len / 2U;
            if (words > out_cap) {
                return 0U;
            }
            memcpy(out, in, words * 2U);
            return words;
        }

        if (info->channels == 1U) {
            size_t samples = in_len / 2U;
            if ((samples * 2U) > out_cap) {
                return 0U;
            }
            for (size_t i = 0U; i < samples; i++) {
                uint16_t sample = app_music_player_le16(&in[i * 2U]);
                out[out_words++] = sample;
                out[out_words++] = sample;
            }
            return out_words;
        }

        return 0U;
    }

    if (info->bits_per_sample == 8U) {
        if (info->channels == 2U) {
            size_t samples = in_len / 2U;
            if ((samples * 2U) > out_cap) {
                return 0U;
            }
            for (size_t i = 0U; i < samples; i++) {
                int16_t left = (int16_t)(((int16_t)in[i * 2U] - 128) << 8);
                int16_t right = (int16_t)(((int16_t)in[i * 2U + 1U] - 128) << 8);
                out[out_words++] = (uint16_t)left;
                out[out_words++] = (uint16_t)right;
            }
            return out_words;
        }

        if (info->channels == 1U) {
            if ((in_len * 2U) > out_cap) {
                return 0U;
            }
            for (size_t i = 0U; i < in_len; i++) {
                uint16_t sample = (uint16_t)((int16_t)(((int16_t)in[i] - 128) << 8));
                out[out_words++] = sample;
                out[out_words++] = sample;
            }
            return out_words;
        }
    }

    return 0U;
}

static size_t app_music_player_generate_tone(uint16_t *out, size_t out_cap, uint32_t frames_to_generate, uint32_t *phase, uint32_t phase_step)
{
    size_t out_words = 0U;

    if (out == NULL || phase == NULL || frames_to_generate == 0U) {
        return 0U;
    }

    if (out_cap < (size_t)frames_to_generate * 2U) {
        return 0U;
    }

    for (uint32_t i = 0U; i < frames_to_generate; i++) {
        uint32_t idx = (*phase >> 27) & (APP_MUSIC_PLAYER_TONE_TABLE_LEN - 1U);
        int16_t sample = g_music_tone_table[idx];

        out[out_words++] = (uint16_t)sample;
        out[out_words++] = (uint16_t)sample;
        *phase += phase_step;
    }

    return out_words;
}

static uint16_t app_music_player_le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static app_music_player_error_t app_music_player_error_from_wav_status(app_wav_status_t status)
{
    switch (status) {
    case APP_WAV_INVALID_PARAM:
        return APP_MUSIC_PLAYER_ERROR_INVALID_PARAM;
    case APP_WAV_MOUNT_FAILED:
        return APP_MUSIC_PLAYER_ERROR_WAV_MOUNT;
    case APP_WAV_OPEN_FAILED:
        return APP_MUSIC_PLAYER_ERROR_WAV_OPEN;
    case APP_WAV_UNSUPPORTED:
        return APP_MUSIC_PLAYER_ERROR_WAV_UNSUPPORTED;
    case APP_WAV_READ_FAILED:
        return APP_MUSIC_PLAYER_ERROR_WAV_READ;
    case APP_WAV_SEEK_FAILED:
        return APP_MUSIC_PLAYER_ERROR_WAV_SEEK;
    default:
        return APP_MUSIC_PLAYER_ERROR_INVALID_PARAM;
    }
}
