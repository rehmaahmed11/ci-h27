#ifndef CLEANROOM_MENU_H
#define CLEANROOM_MENU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Clean-room branded menu subsystem (P7):
 *   - brand identity: "AMS" with support hotline "+923060706989"
 *   - modern sleek presentation tokens (deep surface, accent, radius, density)
 *   - backend-agnostic render hook so the final GUI layer plugs in here
 */

#define MENU_MAX_SECTIONS 8u
#define MENU_MAX_STACK    8u
#define MENU_MAX_ENTRIES  8u

/* Semantic menu key codes (GUI-agnostic). */
#define MENU_KEY_NONE 0
#define MENU_KEY_UP   0x01
#define MENU_KEY_DOWN 0x02
#define MENU_KEY_OK   0x03
#define MENU_KEY_BACK 0x04

typedef struct {
    const char *brand;
    const char *phone;
    const char *tagline;
} menu_brand_t;

typedef struct {
    uint32_t bg;
    uint32_t surface;
    uint32_t accent;
    uint32_t text;
    uint32_t text_dim;
    uint8_t corner_radius;
    uint8_t item_height;
    uint8_t font_scale;
} menu_theme_t;

typedef enum {
    MENU_RET_OK = 0,
    MENU_RET_ERR = -1,
    MENU_RET_ERR_STATE = -2
} menu_ret_t;

typedef enum {
    MENU_VIEW_HOME = 0,
    MENU_VIEW_LIVE_TV = 1,
    MENU_VIEW_YOUTUBE = 2,
    MENU_VIEW_IPTV = 3,
    MENU_VIEW_CAM = 4,
    MENU_VIEW_SETTINGS = 5,
    MENU_VIEW_UPDATE = 6,
    MENU_VIEW_ABOUT = 7
} menu_view_t;

typedef enum {
    MENU_ITEM_NONE = 0,
    MENU_ITEM_LIVE_TV = 1,
    MENU_ITEM_YOUTUBE = 2,
    MENU_ITEM_IPTV = 3,
    MENU_ITEM_CAM = 4,
    MENU_ITEM_SETTINGS = 5,
    MENU_ITEM_UPDATE = 6,
    MENU_ITEM_ABOUT = 7
} menu_item_id_t;

typedef struct {
    menu_item_id_t id;
    const char *label;
    menu_view_t target;   /* navigation destination is a view, not an item id */
    bool enabled;
} menu_entry_t;

typedef struct {
    const char *title;
    menu_entry_t entries[MENU_MAX_ENTRIES];
    size_t entry_count;
} menu_section_t;

typedef struct {
    menu_brand_t brand;
    menu_theme_t theme;
    menu_section_t sections[MENU_MAX_SECTIONS];
    size_t section_count;
    size_t stack[MENU_MAX_STACK];
    size_t stack_count;
    size_t current;
    size_t current_entry;
    int (*render)(void *ctx, menu_view_t view, const menu_theme_t *theme);
    void *ctx;
} menu_t;

/* The public API is spelled `cleanroom_menu_*`; the struct is spelled `menu_t`.
 * Provide the alias so both names refer to the same type. */
typedef menu_t cleanroom_menu_t;

void menu_theme_default(menu_theme_t *t);
menu_ret_t cleanroom_menu_init(cleanroom_menu_t *m, const menu_brand_t *b);
menu_ret_t cleanroom_menu_add_section(cleanroom_menu_t *m, const menu_section_t *s);
menu_ret_t cleanroom_menu_push(cleanroom_menu_t *m, size_t section_index);
menu_ret_t cleanroom_menu_navigate(cleanroom_menu_t *m, menu_view_t v);
menu_ret_t cleanroom_menu_back(cleanroom_menu_t *m);
menu_ret_t cleanroom_menu_select(cleanroom_menu_t *m, menu_item_id_t id);
menu_ret_t cleanroom_menu_handle_key(cleanroom_menu_t *m, int key);
const char *cleanroom_menu_brand_name(const cleanroom_menu_t *m);
const char *cleanroom_menu_brand_phone(const cleanroom_menu_t *m);
const char *cleanroom_menu_view_title(menu_view_t v);
int cleanroom_menu_render_hook(void *ctx, menu_view_t view, const menu_theme_t *theme);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_MENU_H */
