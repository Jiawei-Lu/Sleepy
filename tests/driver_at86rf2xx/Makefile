INCLUDES += -I$(APPDIR)
BOARD ?= samr21-xpro

include ../Makefile.tests_common

USEMODULE += shell
USEMODULE += shell_cmds_default

# USEMODULE += test_utils_netdev_ieee802154_minimal

# define the driver to be used for selected boards
ifneq (,$(filter samr21-xpro,$(BOARD)))
  DRIVER := at86rf233
endif
ifneq (,$(filter iotlab-m3,$(BOARD)))
  DRIVER := at86rf231
endif
ifneq (,$(filter samr30-xpro,$(BOARD)))
  DRIVER := at86rf212b
endif

# use the at86rf212b as fallback device
DRIVER ?= at86rf212b

# include the selected driver
USEMODULE += $(DRIVER)

CFLAGS += -DEVENT_THREAD_STACKSIZE_DEFAULT=1024

include $(RIOTBASE)/Makefile.include
