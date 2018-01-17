.. _flash-partitions:

Flash partitions nodes explanation
##################################

In each of the <boards>.dts files, the flash partitions layout can be described. It must be put inside the 'partitions' node in the 'flash0' node.

Mcuboot related partitions
**************************

boot_partition - This is the partition where the bootloader is expected to be placed. mcuboot is linked into this partition.

slot0_partition - This is the partition where the current, executable application image is expected to be placed. Any bootable application must be linked into this area.

slot1_partition - This is the partition where the downloaded application image is collected. mcuboot can load this image to slot0_partition and run it. The size of this partition must be the same as the size of the slot0_partition.

scratch_partition: - this area is used for images swapping support. See the  `mcuboot documentation`_ for more details.

.. _mcuboot documentation: https://github.com/runtimeco/mcuboot/blob/master/docs/design.md#image-slots

By default a code-partition to link into is not selected.

If zephyr code-partition is not chosen, the image will be linked into the entire flash device.
If it points to an individual partition, the code will be linked to, and restricted to that partition.

File system related partitions
******************************
nffs_partition - this is the area where NFFS is expected to have its partition.

Example
*******

For an example of a DTS file containing such a description, see the `nrf52840_pca10056.dts`_ file.

.. _nrf52840_pca10056.dts: https://github.com/zephyrproject-rtos/zephyr/blob/master/boards/arm/nrf52840_pca10056/nrf52840_pca10056.dts
