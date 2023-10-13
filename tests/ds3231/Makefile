include ../Makefile.tests_common

FEATURES_REQUIRED = periph_i2c
FEATURES_OPTIONAL = periph_i2c_reconfigure

USEMODULE += ds3231
USEMODULE += xtimer

FEATURES_OPTIONAL += periph_gpio_irq

USEMODULE += shell
USEMODULE += shell_cmds_default

FEATURES_REQUIRED += periph_rtc
FEATURES_OPTIONAL += periph_rtc_ms
FEATURES_OPTIONAL += periph_rtc_mem
# FEATURES_REQUIRED += periph_rtt
# FEATURES_OPTIONAL += periph_rtt_ms
# FEATURES_OPTIONAL += periph_rtt_mem


#USEMODULE += ds3231_int
#CFLAGS +="-DDS3231_PARAM_INT_PIN=(GPIO_PIN(PA,15))"

include $(RIOTBASE)/Makefile.include
