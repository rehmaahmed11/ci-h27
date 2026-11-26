/*
 * cleanroom/yt_proxy.h
 * --------------------
 * Clean-room YouTube proxy / player module — P1 subsystem.
 *
 * Recovered surface:
 *   https://www.googleapis.com/youtube/v3
 *   https://www.youtube.com/watch?v=
 *   https://u2b.sysio.info:18000/u2b/parse_token
 *   https://u2b.sysio.info:18000/u2b/parse
 *   ytplayer.config / ytplayer.load = function(){args}
 *   args keys: url_encoded_fmt_stream_map, assets, js, videoCategories?part=snippet,
 *             videos?part=snippet.<ver>
 */

#ifndef CLEANROOM_YT_PROXY_H
#define CLEANROOM_YT_PROXY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YT_API_BASE   "https://www.googleapis.com/youtube/v3"
#define YT_WATCH_URL  "https://www.youtube.com/watch?v="
#define YT_TOKEN_URL "https://u2b.sysio.info:18000/u2b/parse_token"
#define YT_PARSE_URL "https://u2b.sysio.info:18000/u2b/parse"

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
    char api_key[32];
    char customer[32];
    char model[32];
    char chispet_id[32];
    char key_name[32];
} yt_proxy_t;

yt_ret_t yt_proxy_init(yt_proxy_t *y, const char *api_key);
yt_ret_t yt_player_config(const char *fmt_stream_map, char **js, char **cat_url);
yt_ret_t yt_tokens_parse(const char *json, char **token_out);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_YT_PROXY_H */
