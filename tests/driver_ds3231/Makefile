include ../Makefile.tests_common

FEATURES_REQUIRED = periph_i2c
FEATURES_OPTIONAL = periph_i2c_reconfigure

FEATURES_REQUIRED += periph_rtc


USEMODULE += ds3231
USEMODULE += xtimer
USEMODULE += shell


FEATURES_REQUIRED += periph_rtc
FEATURES_OPTIONAL += periph_rtc_ms
FEATURES_OPTIONAL += periph_rtc_mem


# USEMODULE += ds3231_int
# CFLAGS +="-DDS3231_PARAM_INT_PIN=(GPIO_PIN(PA,28))"

include $(RIOTBASE)/Makefile.include
