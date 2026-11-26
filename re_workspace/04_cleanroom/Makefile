CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2 -Isrc
# SSL_CTX_new/SSL_connect/SSL_read/SSL_write live in libssl, and the EVP
# primitives in libcrypto; libssl must come first because it depends on it.
LDLIBS  ?= -lssl -lcrypto

SRCDIR  := src
INCDIR  := .
BUILDDIR := build

SRCS    := $(SRCDIR)/main.c \
           $(SRCDIR)/common.c \
           $(SRCDIR)/fw_update.c \
           $(SRCDIR)/fw_nvm.c \
           $(SRCDIR)/cam_keys.c \
           $(SRCDIR)/cam_server.c \
           $(SRCDIR)/crypto_tls.c \
           $(SRCDIR)/cccam_client.c \
           $(SRCDIR)/net_transport.c \
           $(SRCDIR)/yt_proxy.c \
           $(SRCDIR)/iptv_backend.c \
           $(SRCDIR)/dvb_ts_mux.c \
           $(SRCDIR)/satip_server.c \
           $(SRCDIR)/rtsp_server.c \
           $(SRCDIR)/dlna_service.c \
           $(SRCDIR)/guardrails.c \
           $(SRCDIR)/cam_frame.c \
           $(SRCDIR)/http_client.c \
           $(SRCDIR)/yt_api.c \
           $(SRCDIR)/iptv_client.c \
           $(SRCDIR)/orchestrator.c \
           $(SRCDIR)/token_ledger.c \
           $(SRCDIR)/net_socket.c \
           $(SRCDIR)/rtsp_server_full.c \
           $(SRCDIR)/satip_server_full.c \
           $(SRCDIR)/dlna_server.c \
           $(SRCDIR)/crypto_ctr.c \
           $(SRCDIR)/runtime.c \
           $(SRCDIR)/rtsp_server_loop.c \
           $(SRCDIR)/ssdp_server_loop.c \
           $(SRCDIR)/crypto_key_schedule.c \
           $(SRCDIR)/runtime_e2e.c \
           $(SRCDIR)/menu.c
OBJS    := $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

TARGET   := $(BUILDDIR)/cleanroom_p0

all: $(TARGET)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean
