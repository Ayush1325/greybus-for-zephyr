# SPDX-License-Identifier: BSD-3-Clause

zephyr_include_directories(../../../include)

zephyr_library()

zephyr_library_sources(
  greybus-stubs.c
  greybus-core.c
  greybus_messages.c
  greybus_transport.c
  greybus_heap.c
  greybus_cport.c
  platform/manifest.c
  platform/service.c
  platform/certificate.c
  platform/bus.c
  platform/control.c
)

if(CONFIG_GREYBUS_TLS_BUILTIN)
  set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)

  if(CONFIG_GREYBUS_TLS_CLIENT_VERIFY_OPTIONAL
     OR CONFIG_GREYBUS_TLS_CLIENT_VERIFY_REQUIRED)
    generate_inc_file_for_target(app ${CONFIG_GREYBUS_TLS_BUILTIN_CA_CERT}
                                 ${gen_dir}/greybus_tls_builtin_ca_cert.inc)
  endif()

  generate_inc_file_for_target(app ${CONFIG_GREYBUS_TLS_BUILTIN_SERVER_CERT}
                               ${gen_dir}/greybus_tls_builtin_server_cert.inc)

  generate_inc_file_for_target(
    app ${CONFIG_GREYBUS_TLS_BUILTIN_SERVER_PRIVKEY}
    ${gen_dir}/greybus_tls_builtin_server_privkey.inc)
endif()

zephyr_library_sources_ifdef(CONFIG_GREYBUS_XPORT_TCPIP
                             platform/transport-tcpip.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_XPORT_UART
                             platform/transport-uart.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_CONTROL control-gpb.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_AUDIO audio.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_CAMERA camera.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_GPIO gpio.c platform/gpio.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_HID hid.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_I2C i2c.c platform/i2c.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_LIGHTS lights.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_LOOPBACK loopback.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_POWER_SUPPLY power_supply.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_PWM pwm-protocol.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_SDIO sdio.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_SPI spi.c platform/spi.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_UART uart.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_USB usb.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_VIBRATOR vibrator.c)
