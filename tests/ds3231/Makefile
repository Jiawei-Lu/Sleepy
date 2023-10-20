include ../Makefile.tests_common

FEATURES_REQUIRED = periph_i2c
FEATURES_OPTIONAL = periph_i2c_reconfigure

USEMODULE += ds3231
# USEMODULE += xtimer

FEATURES_OPTIONAL += periph_rtc
FEATURES_OPTIONAL += periph_gpio_irq

USEMODULE += shell
USEMODULE += shell_cmds_default

#Radio USEMODULE
USEMODULE += gnrc_netif
USEMODULE += netdev_default
USEMODULE += auto_init_gnrc_netif
# Activate ICMPv6 error messages
USEMODULE += gnrc_icmpv6_error
USEMODULE += ps
USEMODULE += netstats_l2
USEMODULE += netstats_ipv6
USEMODULE += ztimer ztimer_usec ztimer_msec ztimer_periph_rtc   
#ztimer_periph_rtt will conflict with periph_rtc(periph_rtc is required by pm_layered)

# use GNRC by default
LWIP_IPV4 ?= 0
LWIP_IPV6 ?= 0

ifeq (,$(filter 1, $(LWIP_IPV4) $(LWIP_IPV6)))
  USEMODULE += auto_init_gnrc_netif
  # Specify the mandatory networking modules
  USEMODULE += gnrc_ipv6_default
  # Additional networking modules that can be dropped if not needed
  USEMODULE += gnrc_icmpv6_echo
else
  USEMODULE += lwip_netdev

  ifeq (1,$(LWIP_IPV4))
    USEMODULE += ipv4_addr

    USEMODULE += lwip_arp
    USEMODULE += lwip_ipv4
    USEMODULE += lwip_dhcp_auto
    CFLAGS += -DETHARP_SUPPORT_STATIC_ENTRIES=1
  endif

  ifeq (1,$(LWIP_IPV6))
    USEMODULE += ipv6_addr

    USEMODULE += lwip_ipv6
    USEMODULE += lwip_ipv6_autoconfig
  endif
endif


USEMODULE += pm_layered
USEMODULE += shell
USEMODULE += shell_cmds_default

# avoid running Kconfig by default
SHOULD_RUN_KCONFIG ?=

FEATURES_REQUIRED += periph_rtc
FEATURES_OPTIONAL += periph_rtc_ms
FEATURES_OPTIONAL += periph_rtc_mem
# FEATURES_REQUIRED += periph_rtt
# FEATURES_OPTIONAL += periph_rtt_ms
# FEATURES_OPTIONAL += periph_rtt_mem


# USEMODULE += ds3231_int
# CFLAGS +="-DDS3231_PARAM_INT_PIN=(GPIO_PIN(PA,15))"

include $(RIOTBASE)/Makefile.include
