#include "app_wav.h"

#include "fatfs.h"
#include <stdio.h>
#include <string.h>

#define APP_WAV_RIFF_ID          0x46464952UL
#define APP_WAV_WAVE_ID          0x45564157UL
#define APP_WAV_FMT_ID           0x20746D66UL
#define APP_WAV_DATA_ID          0x61746164UL
#define APP_WAV_FORMAT_PCM       1U

static uint16_t app_wav_le16(const uint8_t *data);
static uint32_t app_wav_le32(const uint8_t *data);
static char app_wav_ascii_lower(char c);
static bool app_wav_has_wav_ext(const char *name);
static app_wav_status_t app_wav_scan_dir(const char *dir, const char *prefix,
                                         app_wav_track_t *tracks, size_t max_tracks,
                                         size_t *track_count);
static app_wav_status_t app_wav_make_path(const char *path, char out[APP_WAV_PATH_MAX]);
static app_wav_status_t app_wav_read_exact(FIL *file, void *buffer, UINT len);
static app_wav_status_t app_wav_parse_header(app_wav_file_t *wav);

app_wav_status_t app_wav_mount(void)
{
    FRESULT res = f_mount(&USERFatFS, USERPath, 1);

    return (res == FR_OK) ? APP_WAV_OK : APP_WAV_MOUNT_FAILED;
}

app_wav_status_t app_wav_scan_root(app_wav_track_t *tracks, size_t max_tracks, size_t *track_count)
{
    app_wav_status_t status;

    if (track_count != NULL) {
        *track_count = 0;
    }

    if (tracks == NULL || max_tracks == 0U || track_count == NULL) {
        return APP_WAV_INVALID_PARAM;
    }

    status = app_wav_mount();
    if (status != APP_WAV_OK) {
        return status;
    }

    status = app_wav_scan_dir("/", "", tracks, max_tracks, track_count);
    if (status != APP_WAV_OK || *track_count >= max_tracks) {
        return status;
    }

    return app_wav_scan_dir("/Music", "/Music", tracks, max_tracks, track_count);
}

app_wav_status_t app_wav_open(app_wav_file_t *wav, const char *path)
{
    char full_path[APP_WAV_PATH_MAX];
    app_wav_status_t status;
    FRESULT res;

    if (wav == NULL || path == NULL || path[0] == '\0') {
        return APP_WAV_INVALID_PARAM;
    }

    memset(wav, 0, sizeof(*wav));

    status = app_wav_mount();
    if (status != APP_WAV_OK) {
        return status;
    }

    status = app_wav_make_path(path, full_path);
    if (status != APP_WAV_OK) {
        return status;
    }

    res = f_open(&wav->file, full_path, FA_READ);
    if (res != FR_OK) {
        return APP_WAV_OPEN_FAILED;
    }

    wav->opened = true;
    status = app_wav_parse_header(wav);
    if (status != APP_WAV_OK) {
        app_wav_close(wav);
        return status;
    }

    return app_wav_rewind_data(wav);
}

app_wav_status_t app_wav_read(app_wav_file_t *wav, void *buffer, uint32_t bytes_to_read, uint32_t *bytes_read)
{
    UINT br = 0;
    uint32_t remain;
    FRESULT res;

    if (bytes_read != NULL) {
        *bytes_read = 0;
    }

    if (wav == NULL || buffer == NULL || !wav->opened) {
        return APP_WAV_INVALID_PARAM;
    }

    if (bytes_to_read == 0U || wav->data_read >= wav->info.data_size) {
        return APP_WAV_OK;
    }

    remain = wav->info.data_size - wav->data_read;
    if (bytes_to_read > remain) {
        bytes_to_read = remain;
    }

    res = f_read(&wav->file, buffer, (UINT)bytes_to_read, &br);
    if (res != FR_OK) {
        return APP_WAV_READ_FAILED;
    }

    wav->data_read += br;
    if (bytes_read != NULL) {
        *bytes_read = br;
    }

    return APP_WAV_OK;
}

app_wav_status_t app_wav_rewind_data(app_wav_file_t *wav)
{
    if (wav == NULL || !wav->opened) {
        return APP_WAV_INVALID_PARAM;
    }

    if (f_lseek(&wav->file, wav->info.data_offset) != FR_OK) {
        return APP_WAV_SEEK_FAILED;
    }

    wav->data_read = 0;
    return APP_WAV_OK;
}

void app_wav_close(app_wav_file_t *wav)
{
    if (wav == NULL || !wav->opened) {
        return;
    }

    (void)f_close(&wav->file);
    wav->opened = false;
}

static uint16_t app_wav_le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t app_wav_le32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static char app_wav_ascii_lower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }

    return c;
}

static bool app_wav_has_wav_ext(const char *name)
{
    size_t len;
    const char *ext;

    if (name == NULL) {
        return false;
    }

    len = strlen(name);
    if (len < 4U) {
        return false;
    }

    ext = &name[len - 4U];
    return ext[0] == '.' &&
           app_wav_ascii_lower(ext[1]) == 'w' &&
           app_wav_ascii_lower(ext[2]) == 'a' &&
           app_wav_ascii_lower(ext[3]) == 'v';
}

static app_wav_status_t app_wav_scan_dir(const char *dir, const char *prefix,
                                         app_wav_track_t *tracks, size_t max_tracks,
                                         size_t *track_count)
{
    DIR dir_obj;
    FILINFO info;
    FRESULT res;
    char path[APP_WAV_PATH_MAX];
    size_t count = *track_count;
    size_t prefix_len = strlen(prefix);

    if (dir == NULL || prefix == NULL) {
        return APP_WAV_INVALID_PARAM;
    }

    if (count >= max_tracks) {
        return APP_WAV_OK;
    }

    if (app_wav_make_path(dir, path) != APP_WAV_OK) {
        return APP_WAV_INVALID_PARAM;
    }

    res = f_opendir(&dir_obj, path);
    if (res == FR_NO_PATH) {
        return APP_WAV_OK;
    }
    if (res != FR_OK) {
        return APP_WAV_OPEN_FAILED;
    }

    while (count < max_tracks) {
        res = f_readdir(&dir_obj, &info);
        if (res != FR_OK) {
            (void)f_closedir(&dir_obj);
            return APP_WAV_READ_FAILED;
        }
        if (info.fname[0] == '\0') {
            break;
        }
        if ((info.fattrib & AM_DIR) != 0U || !app_wav_has_wav_ext(info.fname)) {
            continue;
        }

        size_t name_len = strlen(info.fname);
        if (name_len >= sizeof(tracks[count].name)) {
            continue;
        }
        if ((prefix_len + 1U + name_len + 1U) > sizeof(tracks[count].path)) {
            continue;
        }

        memcpy(tracks[count].name, info.fname, name_len + 1U);
        memcpy(tracks[count].path, prefix, prefix_len);
        tracks[count].path[prefix_len] = '/';
        memcpy(&tracks[count].path[prefix_len + 1U], info.fname, name_len + 1U);
        tracks[count].size = (uint32_t)info.fsize;
        count++;
    }

    (void)f_closedir(&dir_obj);
    *track_count = count;
    return APP_WAV_OK;
}

static app_wav_status_t app_wav_make_path(const char *path, char out[APP_WAV_PATH_MAX])
{
    size_t prefix_len;
    size_t path_len;

    if (strchr(path, ':') != NULL) {
        if (strlen(path) >= APP_WAV_PATH_MAX) {
            return APP_WAV_INVALID_PARAM;
        }
        (void)strcpy(out, path);
        return APP_WAV_OK;
    }

    prefix_len = strlen(USERPath);
    path_len = strlen(path);
    if ((prefix_len + path_len + 1U) >= APP_WAV_PATH_MAX) {
        return APP_WAV_INVALID_PARAM;
    }

    (void)strcpy(out, USERPath);
    if (prefix_len > 0U && out[prefix_len - 1U] == '/' && path[0] == '/') {
        (void)strcat(out, path + 1);
    } else {
        (void)strcat(out, path);
    }

    return APP_WAV_OK;
}

static app_wav_status_t app_wav_read_exact(FIL *file, void *buffer, UINT len)
{
    UINT br = 0;

    if (f_read(file, buffer, len, &br) != FR_OK || br != len) {
        return APP_WAV_READ_FAILED;
    }

    return APP_WAV_OK;
}

static app_wav_status_t app_wav_parse_header(app_wav_file_t *wav)
{
    uint8_t header[12];
    bool fmt_found = false;
    bool data_found = false;

    if (app_wav_read_exact(&wav->file, header, sizeof(header)) != APP_WAV_OK) {
        return APP_WAV_READ_FAILED;
    }

    if (app_wav_le32(&header[0]) != APP_WAV_RIFF_ID ||
        app_wav_le32(&header[8]) != APP_WAV_WAVE_ID) {
        return APP_WAV_UNSUPPORTED;
    }

    while (!data_found) {
        uint8_t chunk[8];
        uint32_t chunk_id;
        uint32_t chunk_size;
        DWORD next_chunk;

        if (app_wav_read_exact(&wav->file, chunk, sizeof(chunk)) != APP_WAV_OK) {
            return APP_WAV_READ_FAILED;
        }

        chunk_id = app_wav_le32(&chunk[0]);
        chunk_size = app_wav_le32(&chunk[4]);
        next_chunk = f_tell(&wav->file) + chunk_size + (chunk_size & 1U);

        if (chunk_id == APP_WAV_FMT_ID) {
            uint8_t fmt[16];

            if (chunk_size < sizeof(fmt)) {
                return APP_WAV_UNSUPPORTED;
            }
            if (app_wav_read_exact(&wav->file, fmt, sizeof(fmt)) != APP_WAV_OK) {
                return APP_WAV_READ_FAILED;
            }

            wav->info.audio_format = app_wav_le16(&fmt[0]);
            wav->info.channels = app_wav_le16(&fmt[2]);
            wav->info.sample_rate = app_wav_le32(&fmt[4]);
            wav->info.byte_rate = app_wav_le32(&fmt[8]);
            wav->info.block_align = app_wav_le16(&fmt[12]);
            wav->info.bits_per_sample = app_wav_le16(&fmt[14]);
            fmt_found = true;
        } else if (chunk_id == APP_WAV_DATA_ID) {
            if (!fmt_found || wav->info.audio_format != APP_WAV_FORMAT_PCM) {
                return APP_WAV_UNSUPPORTED;
            }

            wav->info.data_offset = f_tell(&wav->file);
            wav->info.data_size = chunk_size;
            data_found = true;
        }

        if (!data_found && f_lseek(&wav->file, next_chunk) != FR_OK) {
            return APP_WAV_SEEK_FAILED;
        }
    }

    return APP_WAV_OK;
}
