# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources(
  vector_table.S
  reset.S
  fault_s.S
  fault.c
  exc_exit.S
  scb.c
  irq_init.c
  thread_abort.c
  )

# When CONFIG_BOOTLOADER_MCUBOOT is set the firmware image
# is chain-loadable and does not need to forward interrupts.
if(!CONFIG_BOOTLOADER_MCUBOOT)

zephyr_linker_sources_ifdef(CONFIG_SW_VECTOR_RELAY
  ROM_START
  SORT_KEY 0x0relay_vectors
  relay_vector_table.ld
  )

endif()

zephyr_linker_sources_ifdef(CONFIG_SW_VECTOR_RELAY
  RAM_SECTIONS
  vt_pointer_section.ld
  )
