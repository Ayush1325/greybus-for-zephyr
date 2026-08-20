# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

An out-of-tree **Zephyr module** implementing the Greybus protocol framework (`zephyr/module.yml` declares it). It cannot be built on its own — it must live inside a west workspace alongside `zephyr` (the checkout path is conventionally `modules/lib/greybus`). `west.yml` here is a self-manifest for CI/standalone workspaces; it pulls `zephyr` plus `hal_ti`, `cmsis_6`, `mcuboot`, `mbedtls`, `tf-psa-crypto`.

Reference hardware is BeagleConnect Freedom (`beagleconnect_freedom@C7/cc1352p7`) talking to a Linux Greybus host. Tests run on `native_sim`.

## Commands

Run from the west workspace top directory (`west topdir`), with the Zephyr env sourced.

```bash
# All tests + samples (this is what CI does; module dir is `greybus` in CI)
west twister -T modules/lib/greybus

# Just the integration tests
west twister -p native_sim -T modules/lib/greybus/tests

# A single scenario — scenario id is <path under -T root>/<name in testcase.yaml>
west twister -p native_sim -T modules/lib/greybus/tests \
  -s greybus/integration/gpio/integration.gpio

# Build the sample (see docs/modules/ROOT/pages/building.adoc for flashing)
west build -b beagleconnect_freedom --sysbuild -p modules/lib/greybus/samples/basic \
  -- -DEXTRA_CONF_FILE="transport-tcpip.conf;802154-subg.conf"

# Dump the generated manifest without any hardware
west build -b beagleconnect_freedom --sysbuild -p modules/lib/greybus/samples/basic \
  -- -DEXTRA_CONF_FILE="transport-dummy.conf"
```

Formatting: `clang-format` with the checked-in `.clang-format` (Zephyr/Linux style — 8-wide hard tabs, 100 col, `SortIncludes: Never`).

Docs (Antora) — matches `.github/workflows/static.yml`, output lands in `build/site`:

```bash
docker run -v $PWD:/antora:Z --rm -t ghcr.io/beagleboard/antora-docker:main \
  antora-playbook-local.yml --fetch --stacktrace
```

Every `.c`/`.h` needs an SPDX header and copyright line — the Scancode workflow fails the PR otherwise. The project is a mix of Apache-2.0 (newer code) and BSD-3-Clause (code inherited from Google/Project Ara); keep the existing header when editing an old file.

## Architecture

### Three roles, selected by Kconfig

`CONFIG_GREYBUS_NODE` (default y), `CONFIG_GREYBUS_APBRIDGE`, `CONFIG_GREYBUS_SVC` each pull in a different source set (see `subsys/greybus/CMakeLists.txt`). A build can be a node, or an APBridge+SVC host, or both. Most of the code is the node path.

- **Node** — `greybus-core.c`, `greybus_cport.c`, `control-gpb.c`, `platform/*`, all the protocol files.
- **APBridge** — `apbridge.c` + `interfaces.c`, routes messages between `struct gb_interface`s (`include/greybus/apbridge.h`).
- **SVC** — `svc.c`, module insert/remove notifications (`include/greybus/svc.h`).

### Node message flow

1. A transport backend receives bytes, builds a `struct gb_message`, and calls `greybus_rx_handler(cport, msg)`.
2. That enqueues onto `gb_rx_msgq` (sized `GREYBUS_CPORT_COUNT * 2`) — it does **not** run the handler inline.
3. A dedicated rx thread (`gb_pending_message_worker`, priority 5, 1280-byte stack) dequeues and dispatches to `gb_cport_get(cport)->driver->op_handler(priv, msg, cport)`.
4. Handlers reply via `gb_transport_message_send()` or the `gb_transport_message_*response_send*()` helpers in `subsys/greybus/greybus_transport.h`.

`GB_PING_TYPE` (0x00) is answered by the core before dispatch. `SYS_INIT(greybus_service_init, APPLICATION, CONFIG_GREYBUS_SERVICE_INIT_PRIORITY)` in `platform/service.c` starts everything automatically unless `CONFIG_GREYBUS_SERVICE=n`.

### Message ownership — the main source of bugs

`struct gb_message` is variable-length (`header` + flexible `payload[]`), allocated from a dedicated heap (`gb_alloc`/`gb_free`, sized by `CONFIG_GREYBUS_HEAP_MEM_POOL_SIZE`, default 2048). Rules:

- `gb_transport_message_send()` does **not** take ownership — the caller frees.
- `gb_transport_message_response_success_send()` and `gb_transport_message_empty_response_send()` **do** free the request (and their own response).
- `gb_transport_message_empty_response_send_no_free()` frees nothing and builds the response on the stack (no payload → no allocation).
- An `op_handler` is handed ownership of `msg` and must free it on every path, including the `default:` case of its type switch. `i2c.c` is a compact, idiomatic example.

Allocation can fail; handlers generally reply `GB_OP_NO_MEMORY` rather than dropping. Use `gb_errno_to_op_result()` to map a Zephyr `-errno` to a Greybus result byte.

### CPorts and the manifest come from devicetree, at compile time

This is the part that requires reading several files together: `include/greybus-utils/manifest.h`, `subsys/greybus/greybus_cport.c`, and `subsys/greybus/platform/manifest.c`.

The application's devicetree has a `/zephyr,greybus` node whose children are *bundles*, each with a `compatible` from `dts/bindings/greybus/` (`zephyr,greybus-bundle-bridged-phy`, `-lights`, `-vibrator`, `-camera`, `-audio`). A bundle lists peripherals as phandles (`gpio-controllers`, `i2c-controllers`, `pwm-controllers`, `spi-controllers`, `uart-controllers`, `lights`, `led-strips`, `vibrators`, `cameras`, `audios`). See `samples/basic/boards/beagleconnect_freedom_cc1352p7.overlay`.

From that, preprocessor macros build **three things that must stay in agreement**:

- `GREYBUS_CPORT_COUNT` (a macro arithmetic expression in `manifest.h`),
- the `cports[]` array in `greybus_cport.c`, whose index *is* the cport number,
- the binary manifest emitted by `manifest_create()` in `platform/manifest.c`.

A `BUILD_ASSERT` in `greybus_cport.c` catches count/array drift. Manifest drift is *not* caught by a build assert — if you touch one, walk all three.

CPort 0 is always Control. Then, in fixed order and only if enabled: FW management + FW download, Log, Raw (`CONFIG_GREYBUS_RAW_CPORTS` of them), Loopback, then the DT-derived bundles. `manifest.h` hardcodes the resulting indices (`GREYBUS_FW_MANAGEMENT_CPORT`, `GREYBUS_LOG_CPORT`, `GREYBUS_RAW_CPORT_START`); reordering the initializers in `cports[]` silently breaks them.

Bundle IDs are assigned by a `__COUNTER__`-based `LOCAL_COUNTER` trick, so the *textual order* of entries in `cports[]` is load-bearing.

Per-cport driver state (`.priv`) is built by the `GB_*_PRIV_DATA` macros in `greybus_cport.c` — a `static` struct per DT phandle, holding the `DEVICE_DT_GET` handle plus protocol-specific fields declared in the matching `greybus_<proto>.h`.

### Transport backends

A transport is a `const struct gb_transport_backend gb_trans_backend` (`init`/`exit`/`listen`/`stop_listening`/`send`) — the symbol name is fixed, so exactly one transport is linked in, enforced by the Kconfig `choice`. Files in `subsys/greybus/transport/`:

| Kconfig | File | Notes |
|---|---|---|
| `GREYBUS_XPORT_TCPIP` | `tcpip.c` | multiplexes all cports over one socket; optional TLS |
| `GREYBUS_XPORT_UART` | `uart.c` + `hdlc/` | HDLC framing, `greybus-transport-uart` DT alias |
| `GREYBUS_XPORT_I2C` | `i2c.c` | I2C target, `greybus-transport` DT alias |
| `GREYBUS_XPORT_APBRIDGE` | `apbridge.c` | loops back into a local APBridge |
| `GREYBUS_XPORT_DUMMY` | `dummy.c` | test/size-tracking; see below |

Wire format is little-endian — always go through `sys_cpu_to_le16`/`sys_le16_to_cpu`, never assume host order.

### Adding a protocol

1. `greybus_protocols.h` (in `include/greybus/`) already has the wire structs for every Greybus class — don't redefine them.
2. New `subsys/greybus/<proto>.c` exporting `const struct gb_driver gb_<proto>_driver` with an `op_handler` (plus optional `connected`/`disconnected`).
3. `zephyr_library_sources_ifdef(CONFIG_GREYBUS_<PROTO> <proto>.c)` in `subsys/greybus/CMakeLists.txt` and a `config GREYBUS_<PROTO>` in `subsys/greybus/Kconfig` with the right `depends on` for the underlying Zephyr subsystem.
4. Wire it into `cports[]`, `GREYBUS_CPORT_COUNT`, and `bundles[]`/manifest as described above — plus a DT binding if it needs a new bundle class.
5. Update the support matrix table in `docs/modules/ROOT/pages/index.adoc`.

## Tests

`tests/greybus/integration/<proto>/` — ztest suites on `native_sim`, tagged `test_framework`. They all use `CONFIG_GREYBUS_XPORT_DUMMY=y` plus an emulated Zephyr driver (`CONFIG_GPIO_EMUL`, etc.), and a `boards/native_sim_native.overlay` supplying the `/zephyr,greybus` node.

The test pattern is: build a request with `gb_message_request_alloc()`, feed it in with `greybus_rx_handler(cport, msg)`, then pull the response back out with `gb_transport_get_message()` — a helper that only the dummy transport exports (it queues sent messages instead of transmitting). Because the rx thread does the dispatch, `gb_transport_get_message()` blocks until the response arrives; there's no manual pumping. Tests also assert `GREYBUS_CPORT_COUNT`, which makes them a real guard on the DT/cport wiring.

`drivers/video/fake_camera.c` and `drivers/audio/fake_audio.c` are simulation-only devices (`CONFIG_FAKE_CAMERA`/`CONFIG_FAKE_AUDIO`) used by the camera and audio tests; those tests source the driver Kconfig via their own local `Kconfig`.
