include ../Makefile.drivers_common
include ../Makefile.pkg_common

USEMODULE += io1_xplained
USEMODULE += ztimer_msec


#FATS_VFS#
USEMODULE += fatfs_vfs
FEATURES_OPTIONAL += periph_rtc
FEATURES_REQUIRED += periph_spi
# remove this if you don't want to format your SD card
USEMODULE += fatfs_vfs_format
CFLAGS += -DCONFIG_FATFS_FORMAT_ALLOC_STATIC=1
FATFS_IMAGE_FILE_SIZE_MIB ?= 128
ifneq (,$(filter native native64,$(BOARD)))
  USEMODULE += mtd_native

  #overwrite default mtd_native-config to use fat image as flash device
  MTD_NATIVE_FILENAME    ?= \"./bin/riot_fatfs_disk.img\"
  MTD_SECTOR_NUM  ?= \(\(\(FATFS_IMAGE_FILE_SIZE_MIB\)*1024*1024\)/MTD_SECTOR_SIZE\)
  CFLAGS += -DMTD_NATIVE_FILENAME=$(MTD_NATIVE_FILENAME)
  CFLAGS += -DFATFS_IMAGE_FILE_SIZE_MIB=$(FATFS_IMAGE_FILE_SIZE_MIB)
  CFLAGS += -DMTD_SECTOR_NUM=$(MTD_SECTOR_NUM)
else
  USEMODULE += mtd_sdcard
endif


include $(RIOTBASE)/Makefile.include
