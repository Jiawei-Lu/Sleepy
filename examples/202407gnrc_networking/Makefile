include $(CURDIR)/../Makefile.tests_common

# name of your application
APPLICATION = gnrc_networking

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../../test_RPL/RIOT

CFLAGS += -DCONFIG_IEEE802154_DEFAULT_SUBGHZ_CHANNEL=0 
CFLAGS += -DCONFIG_IEEE802154_DEFAULT_SUBGHZ_PAGE=0 
CFLAGS += -DTHREAD_STACKSIZE_MAIN=\(3*THREAD_STACKSIZE_DEFAULT\)

# Include packages that pull up and auto-init the link layer.
# NOTE: 6LoWPAN will be included if IEEE802.15.4 devices are present
USEMODULE += netdev_default
USEMODULE += auto_init_gnrc_netif
# Activate ICMPv6 error messages
USEMODULE += gnrc_icmpv6_error
# Specify the mandatory networking module for a IPv6 routing node
USEMODULE += gnrc_ipv6_router_default
# Add a routing protocol
USEMODULE += gnrc_rpl
USEMODULE += auto_init_gnrc_rpl
USEMODULE += gnrc_pktdump
# Additional networking modules that can be dropped if not needed
USEMODULE += gnrc_icmpv6_echo
USEMODULE += shell_cmd_gnrc_udp

USEMODULE += gcoap
CFLAGS += -DCONFIG_GCOAP_PDU_BUF_SIZE=1024
CFLAGS += -DGCOAP_RESPONSE_TIMEOUT=5000

# Add also the shell, some shell commands
USEMODULE += shell
USEMODULE += shell_cmds_default
USEMODULE += ps
USEMODULE += uri_parser
USEMODULE += netstats_l2
USEMODULE += netstats_ipv6
USEMODULE += netstats_rpl

# Required by gcoap example
USEMODULE += od
USEMODULE += fmt
USEMODULE += netutils
USEMODULE += random

# Ztimer
USEMODULE += ztimer ztimer_usec ztimer_msec ztimer_periph_rtc   

FEATURES_REQUIRED += periph_rtc

# Optionally include DNS support. This includes resolution of names at an
# upstream DNS server and the handling of RDNSS options in Router Advertisements
# to auto-configure that upstream DNS server.
# USEMODULE += sock_dns              # include DNS client
# USEMODULE += gnrc_ipv6_nib_dns     # include RDNSS option handling

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Instead of simulating an Ethernet connection, we can also simulate
# an IEEE 802.15.4 radio using ZEP
USE_ZEP ?= 0

# set the ZEP port for native
ZEP_PORT_BASE ?= 17754
ifeq (1,$(USE_ZEP))
  USEMODULE += socket_zep
endif

# Pass through application configuration to the docker container, if
# BUILD_IN_DOCKER=1 is used:
DOCKER_ENV_VARS += LWIP_IPV4
DOCKER_ENV_VARS += LWIP_IPV6
DOCKER_ENV_VARS += USE_ZEP
DOCKER_ENV_VARS += ZEP_PORT_BASE

#DS3231
FEATURES_REQUIRED = periph_i2c
FEATURES_OPTIONAL = periph_i2c_reconfigure
FEATURES_OPTIONAL += periph_gpio_irq
USEMODULE += pm_layered
USEMODULE += ds3231
USEMODULE += ds3231_int
CFLAGS +="-DDS3231_PARAM_INT_PIN=(GPIO_PIN(PA,15))"


# Uncomment the following 2 lines to specify static link lokal IPv6 address
# this might be useful for testing, in cases where you cannot or do not want to
# run a shell with ifconfig to get the real link lokal address.
#IPV6_STATIC_LLADDR ?= '"fe80::cafe:cafe:cafe:1"'
#CFLAGS += -DCONFIG_GNRC_IPV6_STATIC_LLADDR=$(IPV6_STATIC_LLADDR)

# Uncomment this to join RPL DODAGs even if DIOs do not contain
# DODAG Configuration Options (see the doc for more info)
# CFLAGS += -DCONFIG_GNRC_RPL_DODAG_CONF_OPTIONAL_ON_JOIN

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

EXTERNAL_BOARD_DIRS += $(RIOTBASE)/tests/build_system/external_board_dirs/esp-ci-boards

include $(RIOTBASE)/Makefile.include

# Set a custom channel if needed
include $(RIOTMAKE)/default-radio-settings.inc.mk

# For now this goes after the inclusion of Makefile.include so Kconfig symbols
# are available. Only set configuration via CFLAGS if Kconfig is not being used
# for this module.
ifndef CONFIG_KCONFIG_MODULE_GCOAP
## Uncomment to redefine port, for example use 61616 for RFC 6282 UDP compression.
#GCOAP_PORT = 5683
#CFLAGS += -DCONFIG_GCOAP_PORT=$(GCOAP_PORT)

## Uncomment to redefine request token length, max 8.
#GCOAP_TOKENLEN = 2
#CFLAGS += -DCONFIG_GCOAP_TOKENLEN=$(GCOAP_TOKENLEN)

# Increase from default for confirmable block2 follow-on requests
GCOAP_RESEND_BUFS_MAX ?= 2
CFLAGS += -DCONFIG_GCOAP_RESEND_BUFS_MAX=$(GCOAP_RESEND_BUFS_MAX)
endif
