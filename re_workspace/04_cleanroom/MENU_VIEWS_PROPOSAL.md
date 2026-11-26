# CLEAN-ROOM MENU VIEWS PROPOSAL — "AMS" BRANDED MODERN SLEEK UI

**Branding:** AMS · Support hotline **+923060706989**
**Module:** `src/menu.h` / `src/menu.c` (clean-room P7)
**Style:** Modern sleek — deep navy surface, cyan accent, rounded corners, generous spacing.

## 1. Basis (what was in the extraction folder)
The original firmware's OSD/menu layer was partitioned separately
(`osd_string`, `MenuBackground`, `defaultdb`, `userdb`) and referenced
views such as Home, Live TV / CAM, YouTube, IPTV, Settings. These were
recovered as strings and partition layout only — no UI code was
re-implemented verbatim (clean-room constraint).

## 2. Enhanced menu views (better approach)
The clean-room menu engine uses a **view stack** (push/back) with
backend-agnostic render hooks and centralized branding + theme tokens.
Proposed views:

| # | View | Purpose (enhanced) |
|---|------|--------------------|
| 0 | **Home** | AMS-branded landing grid: Live TV, YouTube, IPTV, CAM, Settings, Update, About |
| 1 | **Live TV** | Tuner/source list (Sat/Cable/DVB), SAT>IP/RTSP/DLNA targets with status badges |
| 2 | **YouTube** | Sessions / channels, player status, API key state |
| 3 | **IPTV** | Panel connections (login/activate/run), VOD |
| 4 | **CAM** | Camera list, CCcam clients, token state |
| 5 | **Settings** | Theme (sleek dark), guardrails (streams/LAN-only), network, audio/video |
| 6 | **Update** | Firmware OTA status (recovered `HEAD` frame protocol) |
| 7 | **About** | AMS branding, +923060706989, version |

## 3. Modern sleek theme tokens
- Background: `0x0B0E14` (deep navy-black)
- Surface: `0x151A23` (raised panel)
- Accent: `0x00C2FF` (cyan)
- Text: `0xF2F5F9` / dim: `0x8A94A6`
- Corner radius: 18 px, item height 64 px, font scale 1x.

## 4. Navigation model (enhanced)
- View stack: `push` / `back` / `navigate(view)` / `select(item)` / `handle_key(...)`.
- Keys: UP / DOWN / OK / BACK.
- Backend-agnostic render hook: final GUI layer (framebuffer / OSD)
  implements `int render(void *ctx, menu_view_t, theme)`.
- Branding centralized in `menu_brand_t` ("AMS", "+923060706989").

## 5. Implementation status
- Module implemented: `src/menu.h`, `src/menu.c` (P7).
- Self-test added: `test_menu()` in `src/main.c`.
- Makefile updated: `src/menu.c` added to SRCS.
- Pending: real OSD renderer on target hardware (GUI layer) — source-only
  deliverable provides the model + theme + navigation contract.
