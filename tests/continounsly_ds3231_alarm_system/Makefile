include ../Makefile.tests_common

FEATURES_REQUIRED = periph_i2c
FEATURES_OPTIONAL = periph_i2c_reconfigure
FEATURES_OPTIONAL += periph_gpio_irq

USEMODULE += ds3231
USEMODULE += xtimer
USEMODULE += shell
USEMODULE += pm_layered
USEMODULE += shell_cmds_default

USEMODULE += ds3231_int
CFLAGS +="-DDS3231_PARAM_INT_PIN=(GPIO_PIN(PA,15))"

include $(RIOTBASE)/Makefile.include
