###############################################################################
#
# WebRoot C SDK
#
###############################################################################
UNIT_NAME := fsm_brightcloud

UNIT_DISABLE := $(if $(CONFIG_MANAGER_FSM),n,y)

# Template type.
# If compiled with clang, assume a native unit test target
# and build a static library
ifneq (,$(findstring clang,$(CC)))
	UNIT_TYPE := LIB
else
	UNIT_TYPE := SHLIB
	UNIT_DIR := lib
endif

UNIT_SRC := src/fsm_bc_service.c
UNIT_SRC += src/fsm_bc_policy.c
UNIT_SRC += src/base64.c
UNIT_SRC += src/bc_alloc.c
UNIT_SRC += src/bc_cache.c
UNIT_SRC += src/bccsdk.c
UNIT_SRC += src/bc_db.c
UNIT_SRC += src/bc_http.c
UNIT_SRC += src/bc_lcp.c
UNIT_SRC += src/bc_net.c
UNIT_SRC += src/bc_poll.c
UNIT_SRC += src/bc_proto.c
UNIT_SRC += src/bc_queue.c
UNIT_SRC += src/bc_rtu.c
UNIT_SRC += src/bc_stats.c
UNIT_SRC += src/bc_string.c
UNIT_SRC += src/bc_tld.c
UNIT_SRC += src/bc_url.c
UNIT_SRC += src/bc_util.c
UNIT_SRC += src/icache.c
UNIT_SRC += src/md5.c

UNIT_CFLAGS := -I$(UNIT_PATH)/inc
UNIT_CFLAGS += -Isrc/fsm/inc
UNIT_CFLAGS += -DHAS_OPENSSL

UNIT_EXPORT_CFLAGS := $(UNIT_CFLAGS)
UNIT_EXPORT_LDFLAGS := -lcrypto -lssl

UNIT_DEPS := src/lib/log
UNIT_DEPS += src/lib/fsm_policy
