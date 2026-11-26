#ifndef CLEANROOM_YT_API_H
#define CLEANROOM_YT_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YT_API_BASE "https://www.googleapis.com/youtube/v3"

#ifndef CLEANROOM_YT_RET_DEFINED
#define CLEANROOM_YT_RET_DEFINED
typedef enum {
    YT_RET_OK = 0,
    YT_RET_ERR = -1,
    YT_RET_ERR_STATE = -2,
    YT_RET_ERR_KEY = -3
} yt_ret_t;
#endif

typedef struct {
    char api_key[64];
} yt_api_t;

yt_ret_t yt_api_init(yt_api_t *y, const char *api_key);
yt_ret_t yt_api_get_player_config(const yt_api_t *y, const char *video_id, char *out, size_t cap, size_t *len);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_YT_API_H */
