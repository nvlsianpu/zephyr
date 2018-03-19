zephyr_include_directories(${ZEPHYR_BASE}/ext/utils)

#mbedtls-base64 library header
zephyr_include_directories($ENV{ZEPHYR_BASE}/ext/lib/crypto/mbedtls/include)

#zephyr_include_directories($ENV{ZEPHYR_BASE}/ext/fs/nffs/include)
zephyr_include_directories($ENV{ZEPHYR_BASE}/ext/fs/fat/include)

zephyr_sources(
  config_store.c
  config.c
  config_init.c
  config_line.c
  )

zephyr_sources_ifdef(CONFIG_CFGSYS_FS config_file.c)
zephyr_sources_ifdef(CONFIG_CFGSYS_FCB config_fcb.c)
