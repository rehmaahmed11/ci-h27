#ifndef CLEANROOM_TOKEN_LEDGER_H
#define CLEANROOM_TOKEN_LEDGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TOKEN_LEDGER_OK = 0,
    TOKEN_LEDGER_ERR = -1,
    TOKEN_LEDGER_ERR_INSUFFICIENT = -2
} token_ledger_ret_t;

typedef struct {
    uint32_t balance;
    uint32_t session_token;
} token_ledger_t;

token_ledger_ret_t token_ledger_init(token_ledger_t *l);
token_ledger_ret_t token_ledger_debit(token_ledger_t *l, uint32_t amount);
token_ledger_ret_t token_ledger_credit(token_ledger_t *l, uint32_t amount);
bool token_ledger_insufficient(const token_ledger_t *l, uint32_t amount);

/* Persistence hooks (P4): optional save/load callbacks for the debit ledger. */
typedef int (*token_ledger_store_cb_t)(void *ctx, const token_ledger_t *l);
typedef int (*token_ledger_load_cb_t)(void *ctx, token_ledger_t *l);
void token_ledger_set_store(void *ctx, token_ledger_store_cb_t s, token_ledger_load_cb_t ld);
void token_ledger_set_memory_store(token_ledger_t *l);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_TOKEN_LEDGER_H */
