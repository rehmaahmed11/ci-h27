/*
 * cleanroom/menu.c
 * -----------------
 * Clean-room branded menu subsystem (P7): view-stack navigation, selection,
 * modern sleek presentation tokens and backend-agnostic render hook.
 */

#include "menu.h"
#include <stddef.h>
#include <stdbool.h>

void menu_theme_default(menu_theme_t *t)
{
    if (t == NULL) { return; }
    t->bg = 0x0B0E14FFu;
    t->surface = 0x151A23FFu;
    t->accent = 0x00C2FFFFu;
    t->text = 0xF2F5F9FFu;
    t->text_dim = 0x8A94A6FFu;
    t->corner_radius = 18u;
    t->item_height = 64u;
    t->font_scale = 1u;
}

menu_ret_t cleanroom_menu_init(cleanroom_menu_t *m, const menu_brand_t *b)
{
    if ((m == NULL) || (b == NULL)) { return MENU_RET_ERR; }
    m->brand = *b;
    menu_theme_default(&m->theme);
    m->section_count = 0u;
    m->stack_count = 0u;
    m->current = 0u;
    m->current_entry = 0u;
    m->ctx = NULL;
    m->render = cleanroom_menu_render_hook;
    return MENU_RET_OK;
}

menu_ret_t cleanroom_menu_add_section(cleanroom_menu_t *m, const menu_section_t *s)
{
    if ((m == NULL) || (s == NULL)) { return MENU_RET_ERR; }
    if (m->section_count >= (size_t)MENU_MAX_SECTIONS) { return MENU_RET_ERR; }
    m->sections[m->section_count] = *s;
    m->section_count += 1u;
    return MENU_RET_OK;
}

menu_ret_t cleanroom_menu_push(cleanroom_menu_t *m, size_t section_index)
{
    if ((m == NULL) || (m->stack_count >= (size_t)MENU_MAX_STACK)) { return MENU_RET_ERR; }
    if (section_index >= m->section_count) { return MENU_RET_ERR; }
    m->stack[m->stack_count] = section_index;
    m->stack_count += 1u;
    m->current = section_index;
    m->current_entry = 0u;
    return MENU_RET_OK;
}

menu_ret_t cleanroom_menu_navigate(cleanroom_menu_t *m, menu_view_t v)
{
    size_t i;
    size_t j;

    if (m == NULL) { return MENU_RET_ERR; }
    if (v == MENU_VIEW_HOME) { return cleanroom_menu_push(m, 0u); }
    for (i = 0; i < m->section_count; ++i) {
        for (j = 0; j < m->sections[i].entry_count; ++j) {
            if (m->sections[i].entries[j].target == v) {
                return cleanroom_menu_push(m, i);
            }
        }
    }
    return MENU_RET_ERR_STATE;
}

menu_ret_t cleanroom_menu_back(cleanroom_menu_t *m)
{
    if (m == NULL) { return MENU_RET_ERR; }
    if (m->stack_count == 0u) { return MENU_RET_ERR_STATE; }
    if (m->stack_count > 1u) { m->stack_count -= 1u; }
    m->current = m->stack[m->stack_count - 1u];
    m->current_entry = 0u;
    return MENU_RET_OK;
}

menu_ret_t cleanroom_menu_select(cleanroom_menu_t *m, menu_item_id_t id)
{
    menu_section_t *s;
    size_t i;

    if ((m == NULL) || (id == MENU_ITEM_NONE)) { return MENU_RET_ERR; }
    if (m->stack_count == 0u) { return MENU_RET_ERR_STATE; }
    s = &m->sections[m->current];
    for (i = 0; i < s->entry_count; ++i) {
        if (s->entries[i].id == id) {
            if (s->entries[i].enabled == false) { return MENU_RET_ERR; }
            return cleanroom_menu_navigate(m, s->entries[i].target);
        }
    }
    return MENU_RET_ERR_STATE;
}

menu_ret_t cleanroom_menu_handle_key(cleanroom_menu_t *m, int key)
{
    menu_section_t *s;
    size_t i;

    (void)i;
    if ((m == NULL) || (m->stack_count == 0u)) { return MENU_RET_ERR; }
    s = &m->sections[m->current];
    if (key == MENU_KEY_BACK) { return cleanroom_menu_back(m); }
    if ((key != MENU_KEY_UP) && (key != MENU_KEY_DOWN) && (key != MENU_KEY_OK)) { return MENU_RET_OK; }
    if (s->entry_count == 0u) { return MENU_RET_OK; }
    if (key == MENU_KEY_DOWN) {
        m->current_entry = (m->current_entry + 1u) % s->entry_count;
        return MENU_RET_OK;
    }
    if (key == MENU_KEY_UP) {
        if (m->current_entry > 0u) { m->current_entry -= 1u; }
        return MENU_RET_OK;
    }
    {
        menu_ret_t r = cleanroom_menu_select(m, s->entries[m->current_entry].id);
        m->current_entry = 0u;
        return r;
    }
}

const char *cleanroom_menu_brand_name(const cleanroom_menu_t *m)
{
    if (m == NULL) { return ""; }
    return m->brand.brand;
}

const char *cleanroom_menu_brand_phone(const cleanroom_menu_t *m)
{
    if (m == NULL) { return ""; }
    return m->brand.phone;
}

const char *cleanroom_menu_view_title(menu_view_t v)
{
    if (v == MENU_VIEW_HOME) { return "Home"; }
    if (v == MENU_VIEW_LIVE_TV) { return "Live TV"; }
    if (v == MENU_VIEW_YOUTUBE) { return "YouTube"; }
    if (v == MENU_VIEW_IPTV) { return "IPTV"; }
    if (v == MENU_VIEW_CAM) { return "CAM Security"; }
    if (v == MENU_VIEW_SETTINGS) { return "Settings"; }
    if (v == MENU_VIEW_UPDATE) { return "Update"; }
    return "About";
}

int cleanroom_menu_render_hook(void *ctx, menu_view_t view, const menu_theme_t *theme)
{
    (void)ctx;
    (void)view;
    (void)theme;
    return MENU_RET_OK;
}
