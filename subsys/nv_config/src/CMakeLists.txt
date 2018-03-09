zephyr_include_directories(${ZEPHYR_BASE}/ext/utils)

#mbedtls-base64 library header
zephyr_include_directories($ENV{ZEPHYR_BASE}/ext/lib/crypto/mbedtls/include)

zephyr_include_directories($ENV{ZEPHYR_BASE}/ext/fs/nffs/include)

zephyr_sources(
  config_store.c
  config.c
  config_init.c
  config_line.c
  )

zephyr_sources_ifdef(CONFIG_NV_CONFIG_FS config_file.c)
zephyr_sources_ifdef(CONFIG_NV_CONFIG_FCB config_fcb.c)
#zephyr_sources_ifdef(CONFIG_NV_CONFIG_SYSTEM_CLI config_cli.c)
zephyr_sources_ifdef(CONFIG_NV_CONFIG_SYSTEM_MCUMGR config_json_line.c config_nmgr.c)

if (CONFIG_NV_CONFIG_SYSTEM_MCUMGR)
  target_include_directories(MCUMGR INTERFACE
    include
    )

  target_link_libraries(MCUMGR INTERFACE
    zephyr_interface
    TINYCBOR
    )
  zephyr_include_directories($ENV{ZEPHYR_BASE}/ext/lib/mgmt/mcumgr/cborattr/include)
  zephyr_include_directories($ENV{ZEPHYR_BASE}/ext/lib/encoding/tinycbor/src)
  zephyr_include_directories($ENV{ZEPHYR_BASE}/ext/lib/mgmt/mcumgr/mgmt/include)
endif()
