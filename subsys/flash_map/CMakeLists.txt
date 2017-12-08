zephyr_sources_ifdef(CONFIG_FLASH_MAP flash_map.c)

if(CONFIG_FLASH_MAP)
zephyr_sources_ifndef(CONFIG_FLASH_MAP_CUSTOM flash_map_default.c)
endif()
