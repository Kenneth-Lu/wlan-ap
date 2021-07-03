##############################################################################
#
# Packet Forwarding Manager
#
##############################################################################
UNIT_NAME := bccsdk_test
UNIT_DISABLE := yes

# Template type:
UNIT_TYPE := BIN

UNIT_SRC := src/main.c

UNIT_CFLAGS += -I$(TOP_DIR)/3rdparty/webroot/src/lib/bccsdk/inc

UNIT_EXPORT_CFLAGS := $(UNIT_CFLAGS)
UNIT_EXPORT_LDFLAGS := $(UNIT_LDFLAGS)

UNIT_DEPS := 3rdparty/webroot/src/lib/bccsdk
