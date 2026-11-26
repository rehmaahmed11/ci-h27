/*
 * cleanroom/token_ledger.c
 * -------------------------
 * Clean-room CAM token debit/credit ledger (P3 integration layer).
 * Models the recovered "Token <n>", "b8Insufficient_Tokens %d" accounting.
 */

#include "token_ledger.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

static void token_ledger_maybe_save(token_ledger_t *l);

token_ledger_ret_t token_ledger_init(token_ledger_t *l)
{
    if (l == NULL) { return TOKEN_LEDGER_ERR; }
    l->balance = 0u;
    l->session_token = 0u;
    return TOKEN_LEDGER_OK;
}

token_ledger_ret_t token_ledger_debit(token_ledger_t *l, uint32_t amount)
{
    if ((l == NULL) || (amount == 0u)) { return TOKEN_LEDGER_ERR; }
    if (token_ledger_insufficient(l, amount)) { return TOKEN_LEDGER_ERR_INSUFFICIENT; }
    l->balance -= amount;
    l->session_token += amount;
    token_ledger_maybe_save(l);
    return TOKEN_LEDGER_OK;
}

token_ledger_ret_t token_ledger_credit(token_ledger_t *l, uint32_t amount)
{
    if ((l == NULL) || (amount == 0u)) { return TOKEN_LEDGER_ERR; }
    l->balance += amount;
    l->session_token += amount;
    token_ledger_maybe_save(l);
    return TOKEN_LEDGER_OK;
}

/* Optional persistence hooks for the token debit ledger (P4). */
static void *g_ledger_ctx = NULL;
static token_ledger_store_cb_t g_ledger_store_cb = NULL;
static token_ledger_load_cb_t g_ledger_load_cb = NULL;

static token_ledger_t g_ledger_mem;
static int token_ledger_store_mem(void *ctx, const token_ledger_t *l)
{
    (void)ctx;
    g_ledger_mem = *l;
    return 0;
}
static int token_ledger_load_mem(void *ctx, token_ledger_t *l)
{
    (void)ctx;
    *l = g_ledger_mem;
    return 0;
}

void token_ledger_set_store(void *ctx, token_ledger_store_cb_t s, token_ledger_load_cb_t ld)
{
    if ((ctx == NULL) || (s == NULL) || (ld == NULL)) { return; }
    g_ledger_ctx = ctx;
    g_ledger_store_cb = s;
    g_ledger_load_cb = ld;
}

void token_ledger_set_memory_store(token_ledger_t *l)
{
    token_ledger_set_store(l, token_ledger_store_mem, token_ledger_load_mem);
}

/* Internal: persist after mutation when a store hook is installed. */
static void token_ledger_maybe_save(token_ledger_t *l)
{
    if ((g_ledger_store_cb != NULL) && (l != NULL)) { (void)g_ledger_store_cb(g_ledger_ctx, l); }
}

bool token_ledger_insufficient(const token_ledger_t *l, uint32_t amount)
{
    if (l == NULL) { return true; }
    return (amount > l->balance);
}
