UNIT_DISABLE := $(if $(CONFIG_TARGET_MANAGER_FSM),n,y)

UNIT_NAME := test_fsm_webroot

UNIT_TYPE := TEST_BIN

UNIT_SRC := test_webroot.c

UNIT_DEPS := src/lib/log
UNIT_DEPS += src/lib/osa
UNIT_DEPS += src/lib/const
UNIT_DEPS += src/lib/common
UNIT_DEPS += src/lib/json_util
UNIT_DEPS += src/lib/ovsdb
UNIT_DEPS += src/qm/qm_conn
UNIT_DEPS += src/lib/unity
UNIT_DEPS += 3rdparty/webroot/src/lib/bccsdk
