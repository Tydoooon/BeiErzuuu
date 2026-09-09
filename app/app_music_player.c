#include "app_music_player.h"

#include "app_event.h"
#include "app_fault.h"
#include "app_ui.h"
#include "bsp_max98357a.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "i2s.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* 命令队列 / 状态                                                     */
/* ------------------------------------------------------------------ */
#define APP_MUSIC_PLAYER_QUEUE_LEN          4U

/* ------------------------------------------------------------------ */
/* 流式播放参数                                                        */
/* ------------------------------------------------------------------ */
/* 每片 DMA 发送缓冲的 16bit word 数。
 * 本工程 TF 卡是 GPIO 软件 SPI（bsp_tfcard.c），读卡偏慢：
 * 缓冲越大，单片播放时间越长，读卡越不容易掉队。
 *   4096 words = 2048 个立体声帧：44.1kHz 下单片约 46ms（需读 ~8KB）。
 * 注意：F407 的 DMA1/DMA2 无法访问 CCMRAM，此缓冲必须留在 SRAM1，
 *      而 SRAM1 仅 128KB 且已被 LVGL/FreeRTOS 堆/USB 占用，无法再翻倍。 */
#define APP_MUSIC_PLAYER_HALF_WORDS         4096U
/* 单片最多需要的原始 PCM 字节数（16bit 双声道时 = 帧数 * 4） */
#define APP_MUSIC_PLAYER_IN_BYTES           8192U
/* 读卡分片大小：兼顾软件 SPI 连续吞吐和 LCD/UI 获得调度机会。 */
#define APP_MUSIC_PLAYER_READ_CHUNK         2048U
/* 等待一次 DMA 完成事件的超时 */
#define APP_MUSIC_PLAYER_WAIT_TIMEOUT_MS    2000U

typedef enum
{
    APP_MUSIC_PLAYER_CMD_PLAY = 0,
    APP_MUSIC_PLAYER_CMD_STOP,
    APP_MUSIC_PLAYER_CMD_SEEK,
    APP_MUSIC_PLAYER_CMD_SCAN
} app_music_player_cmd_type_t;

typedef struct
{
    app_music_player_cmd_type_t type;
    app_wav_track_t track;
    uint32_t offset;      /* CMD_SEEK 的目标字节偏移 */
} app_music_player_cmd_t;

typedef struct
{
    uint32_t seq;
    app_wav_status_t status;
    size_t count;
    app_wav_track_t tracks[APP_MUSIC_PLAYER_MAX_TRACKS];
} app_music_player_scan_result_t;

static QueueHandle_t g_music_cmd_queue;
static StaticQueue_t g_music_cmd_queue_buf;
static uint8_t g_music_cmd_queue_storage[APP_MUSIC_PLAYER_QUEUE_LEN * sizeof(app_music_player_cmd_t)];
static app_music_player_status_t g_music_status;
static app_music_player_scan_result_t g_music_scan_result;
static bool g_music_scan_pending;

/* 播放缓冲（静态，避免占用 FreeRTOS 堆） */
static uint8_t  g_music_in_buf[APP_MUSIC_PLAYER_IN_BYTES];        /* TF 卡读出的原始 PCM */
static uint16_t g_music_tx[2][APP_MUSIC_PLAYER_HALF_WORDS];       /* I2S DMA 双缓冲 */

/* 音频任务与 ISR 之间的流控状态 */
static volatile uint16_t *g_armed;        /* 已填好、等待发送完成中断自动续传的缓冲；NULL=无 */
static volatile bool      g_dma_running;  /* 当前是否有一个 DMA 正在播放 */
static volatile bool      g_stream_eof;   /* PCM 已读完（任务置 1） */
static volatile bool      g_stop_req;     /* 请求停止（任务置 1，中断据此不再续传） */
static volatile bool      g_seek_pending; /* 有 seek 待处理（任务置 1） */
static volatile uint32_t  g_seek_offset;  /* seek 目标字节偏移 */
static volatile bool      g_i2s_error;    /* I2S/DMA 出错 */
static TaskHandle_t       g_audio_task_h;

static void app_music_player_reset_status(void);
static void app_music_player_set_state(app_music_player_state_t state, app_wav_status_t status, app_music_player_error_t error);
static bool app_music_player_send_cmd(const app_music_player_cmd_t *cmd);
static void app_music_player_do_scan(void);
static void app_music_player_play_track(const app_wav_track_t *track);
static app_music_player_error_t app_music_player_stream(app_wav_file_t *wav);
static uint32_t app_music_player_fill(uint16_t *dst, app_wav_file_t *wav, const app_wav_info_t *info);
static uint32_t app_music_player_pcm_to_words(const uint8_t *in, uint32_t in_bytes,
                                              uint16_t *out, uint32_t out_cap_words,
                                              const app_wav_info_t *info, uint8_t volume);
static uint16_t app_music_player_le16(const uint8_t *data);
static int app_music_player_wait_event(uint32_t timeout_ms);
static void app_music_player_poll_command(void);
static app_music_player_error_t app_music_player_error_from_wav_status(app_wav_status_t status);
static void app_music_player_stream_reset_flags(void);
static void app_music_player_audio_cleanup(void);

/* ================================================================== */
/* HAL I2S 回调（__weak 覆盖）—— 负责“无缝续传”与异常上报            */
/* ================================================================== */

/**
 * @brief 一块 DMA 发送完成。若任务已经填好下一片（g_armed），立即续传，
 *        保证 I2S 不出现块间空洞；否则停流（正常播完或下溢）。
 */
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    uint16_t *next;
    BaseType_t higher = pdFALSE;

    if (hi2s != &hi2s3) {
        return;
    }

    next = (uint16_t *)g_armed;
    g_armed = NULL;

    if (next != NULL && !g_stop_req && !g_i2s_error) {
        if (BSP_MAX98357A_TxStartDma(next, APP_MUSIC_PLAYER_HALF_WORDS) == BSP_MAX98357A_OK) {
            /* 续传成功：DMA 保持运行 */
            (void)xTaskNotifyFromISR(g_audio_task_h, 1UL, eSetBits, &higher);
            portYIELD_FROM_ISR(higher);
            return;
        }
        /* 提交失败，走停止分支 */
    }

    g_dma_running = false;
    __HAL_I2S_DISABLE(hi2s);
    CLEAR_BIT(hi2s->Instance->CR2, SPI_CR2_TXDMAEN);

    (void)xTaskNotifyFromISR(g_audio_task_h, 1UL, eSetBits, &higher);
    portYIELD_FROM_ISR(higher);
}

void HAL_I2S_ErrorCallback(I2S_HandleTypeDef *hi2s)
{
    BaseType_t higher = pdFALSE;

    if (hi2s != &hi2s3) {
        return;
    }

    g_i2s_error = true;
    g_dma_running = false;
    __HAL_I2S_DISABLE(hi2s);
    CLEAR_BIT(hi2s->Instance->CR2, SPI_CR2_TXDMAEN);

    (void)xTaskNotifyFromISR(g_audio_task_h, 1UL, eSetBits, &higher);
    portYIELD_FROM_ISR(higher);
}

/* ================================================================== */
/* 对外 API                                                            */
/* ================================================================== */

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

    (void)BSP_MAX98357A_Init();   /* 功放 SD 拉低 + I2S 默认 44.1k */
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

void app_music_player_request_stop(void)
{
    app_music_player_cmd_t cmd;

    if (g_music_cmd_queue == NULL) {
        return;
    }

    cmd.type = APP_MUSIC_PLAYER_CMD_STOP;
    memset(&cmd.track, 0, sizeof(cmd.track));
    (void)xQueueSendToFront(g_music_cmd_queue, &cmd, 0);
}

void app_music_player_request_seek(uint32_t byte_offset)
{
    app_music_player_cmd_t cmd;

    if (g_music_cmd_queue == NULL) {
        return;
    }

    cmd.type = APP_MUSIC_PLAYER_CMD_SEEK;
    memset(&cmd.track, 0, sizeof(cmd.track));
    cmd.offset = byte_offset;
    (void)app_music_player_send_cmd(&cmd);
}

void app_music_player_request_scan(void)
{
    app_music_player_cmd_t cmd;
    bool queued;

    if (g_music_cmd_queue == NULL) {
        return;
    }

    taskENTER_CRITICAL();
    if (g_music_scan_pending) {
        taskEXIT_CRITICAL();
        return;
    }
    g_music_scan_pending = true;
    taskEXIT_CRITICAL();

    cmd.type = APP_MUSIC_PLAYER_CMD_SCAN;
    memset(&cmd.track, 0, sizeof(cmd.track));
    queued = app_music_player_send_cmd(&cmd);
    if (!queued) {
        taskENTER_CRITICAL();
        g_music_scan_pending = false;
        taskEXIT_CRITICAL();
    }
}

bool app_music_player_get_scan(uint32_t *seq, app_wav_status_t *status,
                               app_wav_track_t *tracks, size_t max_tracks,
                               size_t *track_count)
{
    size_t count;

    if (seq == NULL || status == NULL || tracks == NULL || track_count == NULL) {
        return false;
    }

    taskENTER_CRITICAL();
    if (*seq == g_music_scan_result.seq) {
        taskEXIT_CRITICAL();
        return false;
    }

    count = g_music_scan_result.count;
    if (count > max_tracks) {
        count = max_tracks;
    }
    memcpy(tracks, g_music_scan_result.tracks, count * sizeof(tracks[0]));
    *status = g_music_scan_result.status;
    *track_count = count;
    *seq = g_music_scan_result.seq;
    g_music_scan_pending = false;
    taskEXIT_CRITICAL();

    return true;
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

    g_audio_task_h = xTaskGetCurrentTaskHandle();

    for (;;) {
        if (g_music_cmd_queue == NULL) {
            vTaskDelay(pdMS_TO_TICKS(10U));
            continue;
        }

        /* 有限超时等待命令，超时也要喂心跳，供故障监控任务判断存活 */
        if (xQueueReceive(g_music_cmd_queue, &cmd, pdMS_TO_TICKS(200U)) != pdPASS) {
            app_fault_heartbeat(APP_FAULT_TASK_AUDIO);
            continue;
        }

        app_fault_heartbeat(APP_FAULT_TASK_AUDIO);

        if (cmd.type == APP_MUSIC_PLAYER_CMD_STOP) {
            g_stop_req = true;
            g_armed = NULL;
            app_music_player_audio_cleanup();
            app_music_player_set_state(APP_MUSIC_PLAYER_STATE_IDLE, APP_WAV_OK, APP_MUSIC_PLAYER_ERROR_NONE);
            app_event_clear(APP_EVT_MUSIC_PLAYING);
            continue;
        }

        if (cmd.type == APP_MUSIC_PLAYER_CMD_SCAN) {
            app_music_player_do_scan();
            continue;
        }

        if (cmd.type == APP_MUSIC_PLAYER_CMD_SEEK) {
            /* 空闲状态下收到的 seek 无意义（UI 仅在播放中发送），直接丢弃 */
            continue;
        }

        app_music_player_play_track(&cmd.track);
    }
}

/* ================================================================== */
/* 后台扫描                                                            */
/* ================================================================== */

/**
 * @brief 在音频任务里分步扫描 TF 卡的 WAV 文件，每步之间让出 CPU。
 *        播放/加载中直接跳过：remount 会使已打开的 FIL 失效，导致音乐中断。
 *        扫描完成后整体提交结果（写 seq），UI 轮询发现 seq 变化再刷新列表。
 */
static void app_music_player_do_scan(void)
{
    app_wav_scan_ctx_t ctx;
    app_wav_status_t status;
    size_t count = 0U;
    app_music_player_state_t state = app_music_player_get_state();

    if (state == APP_MUSIC_PLAYER_STATE_PLAYING ||
        state == APP_MUSIC_PLAYER_STATE_LOADING) {
        taskENTER_CRITICAL();
        g_music_scan_pending = false;
        taskEXIT_CRITICAL();
        return;   /* 播放中不重扫，保持音乐不中断 */
    }

    status = app_wav_scan_ctx_begin(&ctx);
    if (status != APP_WAV_OK) {
        taskENTER_CRITICAL();
        g_music_scan_result.seq++;
        g_music_scan_result.status = status;
        g_music_scan_result.count = 0U;
        taskEXIT_CRITICAL();
        return;
    }

    for (;;) {
        bool done = app_wav_scan_ctx_step(&ctx, g_music_scan_result.tracks,
                                          APP_MUSIC_PLAYER_MAX_TRACKS, &count, 8U);
        if (done || count >= APP_MUSIC_PLAYER_MAX_TRACKS) {
            break;
        }
        vTaskDelay(1);   /* 让出 CPU，避免阻塞 UI */
    }
    app_wav_scan_ctx_end(&ctx);
    status = ctx.status;

    taskENTER_CRITICAL();
    g_music_scan_result.seq++;
    g_music_scan_result.status = status;
    g_music_scan_result.count = count;
    taskEXIT_CRITICAL();
}

/* ================================================================== */
/* 状态维护                                                            */
/* ================================================================== */

static void app_music_player_reset_status(void)
{
    taskENTER_CRITICAL();
    memset(&g_music_status, 0, sizeof(g_music_status));
    g_music_status.state = APP_MUSIC_PLAYER_STATE_IDLE;
    g_music_status.last_status = APP_WAV_OK;
    g_music_status.last_error = APP_MUSIC_PLAYER_ERROR_NONE;
    taskEXIT_CRITICAL();
}

static void app_music_player_set_state(app_music_player_state_t state, app_wav_status_t status, app_music_player_error_t error)
{
    taskENTER_CRITICAL();
    g_music_status.state = state;
    g_music_status.last_status = status;
    g_music_status.last_error = error;
    taskEXIT_CRITICAL();
}

static bool app_music_player_send_cmd(const app_music_player_cmd_t *cmd)
{
    if (cmd == NULL || g_music_cmd_queue == NULL) {
        return false;
    }

    return xQueueSend(g_music_cmd_queue, cmd, 0) == pdPASS;
}

static void app_music_player_stream_reset_flags(void)
{
    g_armed = NULL;
    g_dma_running = false;
    g_stream_eof = false;
    g_stop_req = false;
    g_i2s_error = false;
}

/* ================================================================== */
/* 播放一首 WAV（DMA 流式）                                            */
/* ================================================================== */

static void app_music_player_play_track(const app_wav_track_t *track)
{
    app_wav_file_t wav;
    app_wav_status_t status;
    app_music_player_error_t err;
    uint32_t bytes_played = 0U;

    if (track == NULL) {
        return;
    }

    /* 清除上一次可能残留的 seek 状态（例如 seek 与 I2S 错误并发时） */
    g_seek_pending = false;
    g_seek_offset = 0U;

    app_music_player_set_state(APP_MUSIC_PLAYER_STATE_LOADING, APP_WAV_OK, APP_MUSIC_PLAYER_ERROR_NONE);
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
        return;
    }

    taskENTER_CRITICAL();
    g_music_status.wav_info = wav.info;
    taskEXIT_CRITICAL();

    /* 只支持标准 PCM 16/8bit、1/2 声道 */
    if (wav.info.audio_format != 1U ||
        (wav.info.bits_per_sample != 8U && wav.info.bits_per_sample != 16U) ||
        wav.info.channels == 0U ||
        wav.info.channels > 2U ||
        wav.info.sample_rate == 0U ||
        wav.info.byte_rate == 0U ||
        wav.info.block_align == 0U) {
        app_wav_close(&wav);
        app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_UNSUPPORTED, APP_MUSIC_PLAYER_ERROR_WAV_UNSUPPORTED);
        app_event_clear(APP_EVT_MUSIC_PLAYING);
        return;
    }

    /* I2S 按文件采样率重新初始化 */
    if (BSP_MAX98357A_SetSampleRate(wav.info.sample_rate) != BSP_MAX98357A_OK) {
        app_wav_close(&wav);
        app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_ERROR, APP_MUSIC_PLAYER_ERROR_I2S_INIT);
        app_event_clear(APP_EVT_MUSIC_PLAYING);
        return;
    }

    /* 开功放（SD=1）并回读确认 */
    (void)BSP_MAX98357A_Enable(true);
    {
        bool sd_enabled = false;
        if (BSP_MAX98357A_GetEnableState(&sd_enabled) != BSP_MAX98357A_OK || !sd_enabled) {
            app_wav_close(&wav);
            (void)BSP_MAX98357A_Enable(false);
            app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_ERROR, APP_MUSIC_PLAYER_ERROR_SD_LOW);
            app_event_clear(APP_EVT_MUSIC_PLAYING);
            return;
        }
    }
    app_music_player_set_state(APP_MUSIC_PLAYER_STATE_PLAYING, APP_WAV_OK, APP_MUSIC_PLAYER_ERROR_NONE);
    app_event_set(APP_EVT_MUSIC_PLAYING);

    for (;;) {
        err = app_music_player_stream(&wav);

        if (g_seek_pending && !g_i2s_error) {
            /* 播放中 seek：软停 DMA（不关功放）、跳转、重新起播 */
            uint32_t target = g_seek_offset;

            g_seek_pending = false;
            g_armed = NULL;
            g_dma_running = false;
            (void)BSP_MAX98357A_TxStopDma();
            if (app_wav_seek_data(&wav, target) != APP_WAV_OK) {
                err = APP_MUSIC_PLAYER_ERROR_WAV_SEEK;
                break;
            }
            continue;
        }
        break;
    }

    bytes_played = wav.data_read;
    taskENTER_CRITICAL();
    g_music_status.bytes_played = bytes_played;
    taskEXIT_CRITICAL();

    /* 收尾：停 DMA、关功放 */
    app_music_player_audio_cleanup();
    app_wav_close(&wav);
    app_event_clear(APP_EVT_MUSIC_PLAYING);

    if (g_stop_req && err == APP_MUSIC_PLAYER_ERROR_NONE && !g_i2s_error) {
        /* 用户主动停止 */
        app_music_player_set_state(APP_MUSIC_PLAYER_STATE_IDLE, APP_WAV_OK, APP_MUSIC_PLAYER_ERROR_NONE);
        return;
    }
    if (err == APP_MUSIC_PLAYER_ERROR_NONE && g_i2s_error) {
        err = APP_MUSIC_PLAYER_ERROR_I2S_TRANSMIT;
    }
    if (err != APP_MUSIC_PLAYER_ERROR_NONE) {
        app_music_player_set_state(APP_MUSIC_PLAYER_STATE_ERROR, APP_WAV_ERROR, err);
        return;
    }
    app_music_player_set_state(APP_MUSIC_PLAYER_STATE_IDLE, APP_WAV_OK, APP_MUSIC_PLAYER_ERROR_NONE);
}

/**
 * @brief 核心播放循环：读卡填充双缓冲 -> DMA 发送，完成中断自动续传。
 */
static app_music_player_error_t app_music_player_stream(app_wav_file_t *wav)
{
    app_music_player_error_t err = APP_MUSIC_PLAYER_ERROR_NONE;
    uint16_t *play_buf = g_music_tx[0];
    uint16_t *armed_next = NULL;
    uint32_t got;

    app_music_player_stream_reset_flags();

    got = app_music_player_fill(g_music_tx[0], wav, &wav->info);
    if (got == 0U) {
        /* 空文件/无数据 */
        return APP_MUSIC_PLAYER_ERROR_WAV_READ;
    }

    if (BSP_MAX98357A_TxStartDma(g_music_tx[0], APP_MUSIC_PLAYER_HALF_WORDS) != BSP_MAX98357A_OK) {
        return APP_MUSIC_PLAYER_ERROR_I2S_TRANSMIT;
    }
    g_dma_running = true;
    play_buf = g_music_tx[0];

    /* 播放第一片的同时，预读第二片并武装（由第一片的完成中断续传） */
    got = app_music_player_fill(g_music_tx[1], wav, &wav->info);
    if (got > 0U) {
        armed_next = g_music_tx[1];
        g_armed = g_music_tx[1];
    } else {
        armed_next = NULL;
        g_stream_eof = true;   /* 文件在第一片内结束 */
    }

    while (!g_stop_req && !g_i2s_error) {
        app_music_player_poll_command();
        if (g_stop_req || g_i2s_error) {
            break;
        }

        /* 等待一个“某片播完”事件 */
        if (app_music_player_wait_event(APP_MUSIC_PLAYER_WAIT_TIMEOUT_MS) != 1) {
            if (!g_stop_req && !g_i2s_error) {
                err = APP_MUSIC_PLAYER_ERROR_I2S_TRANSMIT;
            }
            break;
        }
        if (g_stop_req || g_i2s_error) {
            break;
        }

        if (!g_dma_running) {
            /* DMA 已停：若已读完则正常结束；否则说明数据供应不及 */
            if (g_stream_eof) {
                break;
            }
            err = APP_MUSIC_PLAYER_ERROR_I2S_TRANSMIT;
            break;
        }

        /* 事件语义：play_buf 播完；之前武装的 armed_next 已被中断续传开播。
         * freed 现在空闲，可填入下一段 PCM。 */
        {
            uint16_t *freed = play_buf;
            play_buf = armed_next;

            got = app_music_player_fill(freed, wav, &wav->info);
            if (got == 0U) {
                /* 文件读完：不再武装；等当前正在播的缓冲结束，ISR 会自动停流 */
                g_stream_eof = true;
                g_armed = NULL;
                while (!g_stop_req && !g_i2s_error && g_dma_running) {
                    if (app_music_player_wait_event(APP_MUSIC_PLAYER_WAIT_TIMEOUT_MS) != 1) {
                        if (!g_stop_req && !g_i2s_error) {
                            err = APP_MUSIC_PLAYER_ERROR_I2S_TRANSMIT;
                        }
                        break;
                    }
                }
                break;
            }

            armed_next = freed;
            g_armed = freed;
        }
    }

    return err;
}

/**
 * @brief 从 WAV data 区读“一个 DMA 片”的 PCM 并转换为 I2S 立体声字。
 * @return 本次从文件消耗的字节数；0 表示文件已读完。
 */
static uint32_t app_music_player_fill(uint16_t *dst, app_wav_file_t *wav, const app_wav_info_t *info)
{
    uint32_t in_max;
    uint32_t remain;
    uint32_t to_read;
    uint32_t bytes_read = 0U;
    uint8_t volume = app_ui_get_system_volume();

    if (dst == NULL || wav == NULL || info == NULL || info->block_align == 0U) {
        return 0U;
    }

    in_max = ((uint32_t)APP_MUSIC_PLAYER_HALF_WORDS / 2U) * info->block_align;
    if (in_max > APP_MUSIC_PLAYER_IN_BYTES) {
        in_max = APP_MUSIC_PLAYER_IN_BYTES;
    }

    if (wav->data_read >= wav->info.data_size) {
        return 0U;
    }
    remain = wav->info.data_size - wav->data_read;
    to_read = (in_max < remain) ? in_max : remain;

    /* 分片读 + 让出 CPU：整片 8KB 软件 SPI 忙等会饿死同优先级/低优先级任务，
     * 导致界面卡顿；每读一小片就让出一次，UI 得以在播放间隙刷新。 */
    while (bytes_read < to_read) {
        uint32_t chunk = to_read - bytes_read;
        uint32_t got = 0U;

        if (chunk > APP_MUSIC_PLAYER_READ_CHUNK) {
            chunk = APP_MUSIC_PLAYER_READ_CHUNK;
        }

        if (app_wav_read(wav, g_music_in_buf + bytes_read, chunk, &got) != APP_WAV_OK) {
            return 0U;
        }
        if (got == 0U) {
            break;
        }
        bytes_read += got;

        app_fault_heartbeat(APP_FAULT_TASK_AUDIO);

        if (bytes_read < to_read) {
            vTaskDelay(1);   /* 让出 CPU，供 LCD/UI 任务运行 */
        }
    }

    if (bytes_read == 0U) {
        return 0U;
    }

    /* 实时回填“已播放字节数”，供 UI 进度条读取。
     * 否则 bytes_played 只在整曲流式结束后更新一次，进度条整首停在 0%。 */
    taskENTER_CRITICAL();
    g_music_status.bytes_played = wav->data_read;
    taskEXIT_CRITICAL();

    {
        uint32_t out_words = app_music_player_pcm_to_words(g_music_in_buf, bytes_read,
                                                           dst, APP_MUSIC_PLAYER_HALF_WORDS,
                                                           info, volume);
        /* 不足一缓冲（文件尾部）时用静音补齐，避免残留旧数据 */
        if (out_words < APP_MUSIC_PLAYER_HALF_WORDS) {
            memset((uint8_t *)dst + out_words * 2U, 0,
                   (APP_MUSIC_PLAYER_HALF_WORDS - out_words) * 2U);
        }
    }

    return bytes_read;
}

/**
 * @brief 把 WAV 原始 PCM 字节转成 I2S 立体声 16bit word，并应用音量。
 *        in_bytes 必须是完整帧（block_align 的整数倍），调用方保证。
 * @return 写出的 word 数（不足一缓冲时由调用方补零）。
 */
static uint32_t app_music_player_pcm_to_words(const uint8_t *in, uint32_t in_bytes,
                                              uint16_t *out, uint32_t out_cap_words,
                                              const app_wav_info_t *info, uint8_t volume)
{
    uint32_t out_words = 0U;
    int32_t gain = volume > 100U ? 100 : (int32_t)volume;

    if (in == NULL || out == NULL || info == NULL) {
        return 0U;
    }
    if (gain > 100) {
        gain = 100;
    }

    if (info->bits_per_sample == 16U && info->channels == 2U) {
        uint32_t words = in_bytes / 2U;
        if (words > out_cap_words) {
            words = out_cap_words;
        }
        for (uint32_t i = 0U; i < words; i++) {
            int32_t s = (int16_t)app_music_player_le16(&in[i * 2U]);
            s = (s * gain) / 100;
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            out[out_words++] = (uint16_t)(int16_t)s;
        }
        return out_words;
    }

    if (info->bits_per_sample == 16U && info->channels == 1U) {
        uint32_t samples = in_bytes / 2U;
        if ((samples * 2U) > out_cap_words) {
            samples = out_cap_words / 2U;
        }
        for (uint32_t i = 0U; i < samples; i++) {
            int32_t s = (int16_t)app_music_player_le16(&in[i * 2U]);
            s = (s * gain) / 100;
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            out[out_words++] = (uint16_t)(int16_t)s;
            out[out_words++] = (uint16_t)(int16_t)s;
        }
        return out_words;
    }

    if (info->bits_per_sample == 8U && info->channels == 2U) {
        uint32_t samples = in_bytes / 2U;
        if ((samples * 2U) > out_cap_words) {
            samples = out_cap_words / 2U;
        }
        for (uint32_t i = 0U; i < samples; i++) {
            int32_t l = ((int32_t)in[i * 2U] - 128) << 8;
            int32_t r = ((int32_t)in[i * 2U + 1U] - 128) << 8;
            l = (l * gain) / 100;
            r = (r * gain) / 100;
            out[out_words++] = (uint16_t)(int16_t)l;
            out[out_words++] = (uint16_t)(int16_t)r;
        }
        return out_words;
    }

    if (info->bits_per_sample == 8U && info->channels == 1U) {
        if ((in_bytes * 2U) > out_cap_words) {
            in_bytes = out_cap_words / 2U;
        }
        for (uint32_t i = 0U; i < in_bytes; i++) {
            int32_t s = ((int32_t)in[i] - 128) << 8;
            s = (s * gain) / 100;
            out[out_words++] = (uint16_t)(int16_t)s;
            out[out_words++] = (uint16_t)(int16_t)s;
        }
        return out_words;
    }

    return 0U;
}

static uint16_t app_music_player_le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

/**
 * @brief 等待一次发送完成通知。
 * @return 1=收到事件；0=停止/出错被唤醒；-1=超时。
 */
static int app_music_player_wait_event(uint32_t timeout_ms)
{
    uint32_t waited = 0U;

    while (waited < timeout_ms) {
        app_music_player_poll_command();
        if (g_stop_req || g_i2s_error) {
            return 0;
        }
        if (xTaskNotifyWait(0UL, 0xFFFFFFFFUL, NULL, pdMS_TO_TICKS(50U)) == pdTRUE) {
            return 1;
        }
        waited += 50U;
    }
    return -1;
}

/**
 * @brief 检查命令队列（流式播放期间轮询）。
 *        播放期间的 SCAN/PLAY 没有意义，必须消费掉，否则会堵住后续 STOP。
 */
static void app_music_player_poll_command(void)
{
    app_music_player_cmd_t cmd;
    bool seek_pending = false;
    uint32_t seek_offset = 0U;

    if (g_music_cmd_queue == NULL) {
        return;
    }

    while (xQueueReceive(g_music_cmd_queue, &cmd, 0) == pdPASS) {
        if (cmd.type == APP_MUSIC_PLAYER_CMD_STOP) {
            g_seek_pending = false;
            g_stop_req = true;
            return;
        }
        if (cmd.type == APP_MUSIC_PLAYER_CMD_SEEK) {
            seek_offset = cmd.offset;
            seek_pending = true;
        } else if (cmd.type == APP_MUSIC_PLAYER_CMD_SCAN) {
            taskENTER_CRITICAL();
            g_music_scan_pending = false;
            taskEXIT_CRITICAL();
        }
        /* SCAN 和重复 PLAY 在当前曲目播放结束前直接丢弃。 */
    }

    if (seek_pending) {
        g_seek_offset = seek_offset;
        g_seek_pending = true;
        g_stop_req = true;   /* 中断当前流，交由 play_track 处理跳转并重新起播 */
    }
}

/* ================================================================== */
/* 收尾                                                               */
/* ================================================================== */

static void app_music_player_audio_cleanup(void)
{
    g_stop_req = true;
    g_armed = NULL;
    (void)BSP_MAX98357A_TxStopDma();   /* 仅任务上下文调用 */
    (void)BSP_MAX98357A_Enable(false);
    g_stream_eof = false;
    g_dma_running = false;
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
