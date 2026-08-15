CC ?= cc
BUILD_DIR ?= /tmp/stm32-p0-host-tests

CPPFLAGS += -I./mocks -I../../shared -I../../board -I../../bsp -I../../module -I../../bootloader
CFLAGS += -std=c99 -Wall -Wextra -Werror

P0_SOURCES := test_p0.c mock_stm32.c ../../module/system_time.c
OTA_CONTRACT_SOURCES := test_ota_contract.c ../../shared/ota_contract.c
W25_SOURCES := test_w25q128.c mock_spi1_bus.c ../../bsp/bsp_w25q128.c
TFT_SOURCES := test_tft_display.c mock_spi1_bus.c mock_tft_port.c \
	../../bsp/bsp_tft_st7789.c ../../module/ota_display.c
ESP_AT_SOURCES := test_esp_at.c mock_esp_uart.c ../../module/esp_at.c
HTTP_SOURCES := test_http_server.c ../../module/http_server.c ../../shared/ota_contract.c
UPLOAD_SOURCES := test_ota_upload.c mock_spi1_bus.c mock_internal_flash.c \
	../../module/ota_upload.c \
	../../module/ota_http.c ../../module/http_server.c \
	../../module/ota_metadata_store.c ../../module/ota_staging.c \
	../../module/ota_update_controller.c ../../module/ota_boot_request_store.c \
	../../shared/ota_contract.c ../../bsp/bsp_w25q128.c
METADATA_SOURCES := test_ota_metadata_store.c mock_spi1_bus.c \
	../../module/ota_metadata_store.c ../../module/ota_staging.c \
	../../shared/ota_contract.c ../../bsp/bsp_w25q128.c
UPDATE_SOURCES := test_ota_update.c mock_spi1_bus.c mock_internal_flash.c \
	../../module/http_server.c ../../module/ota_http.c ../../module/ota_upload.c \
	../../module/ota_metadata_store.c ../../module/ota_staging.c \
	../../module/ota_update_controller.c ../../module/ota_boot_request_store.c \
	../../shared/ota_contract.c ../../bsp/bsp_w25q128.c
BOOT_UPDATE_SOURCES := test_boot_updater.c mock_spi1_bus.c mock_internal_flash.c \
	../../bootloader/boot_updater.c ../../module/ota_boot_request_store.c \
	../../module/ota_metadata_store.c ../../module/ota_staging.c \
	../../shared/ota_contract.c ../../bsp/bsp_w25q128.c
BOOT_JUMP_SOURCES := test_boot_jump.c mock_boot_platform.c \
	../../bootloader/boot_jump.c ../../bootloader/boot_recovery.c \
	../../shared/ota_contract.c
ESP_HTTP_SOURCES := test_esp_http_bridge.c mock_esp_bridge.c \
	../../module/esp_http_bridge.c ../../module/http_server.c \
	../../shared/ota_contract.c
TARGETS := $(BUILD_DIR)/test_p0 $(BUILD_DIR)/test_ota_contract \
	$(BUILD_DIR)/test_w25q128 $(BUILD_DIR)/test_tft_display \
	$(BUILD_DIR)/test_esp_at $(BUILD_DIR)/test_http_server \
	$(BUILD_DIR)/test_ota_upload $(BUILD_DIR)/test_ota_metadata_store \
	$(BUILD_DIR)/test_ota_update $(BUILD_DIR)/test_boot_updater \
	$(BUILD_DIR)/test_boot_jump $(BUILD_DIR)/test_esp_http_bridge

.PHONY: all test

all: $(TARGETS)

$(BUILD_DIR)/test_p0: $(P0_SOURCES)
	mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(P0_SOURCES) -o "$@"

$(BUILD_DIR)/test_ota_contract: $(OTA_CONTRACT_SOURCES)
	mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OTA_CONTRACT_SOURCES) -o "$@"

$(BUILD_DIR)/test_w25q128: $(W25_SOURCES)
	mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(W25_SOURCES) -o "$@"

$(BUILD_DIR)/test_tft_display: $(TFT_SOURCES)
	mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TFT_SOURCES) -o "$@"

$(BUILD_DIR)/test_esp_at: $(ESP_AT_SOURCES)
	mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ESP_AT_SOURCES) -o "$@"

$(BUILD_DIR)/test_http_server: $(HTTP_SOURCES)
	mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(HTTP_SOURCES) -o "$@"

$(BUILD_DIR)/test_ota_upload: $(UPLOAD_SOURCES)
	mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(UPLOAD_SOURCES) -o "$@"

$(BUILD_DIR)/test_ota_metadata_store: $(METADATA_SOURCES)
	mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(METADATA_SOURCES) -o "$@"

$(BUILD_DIR)/test_ota_update: $(UPDATE_SOURCES)
	mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(UPDATE_SOURCES) -o "$@"

$(BUILD_DIR)/test_boot_updater: $(BOOT_UPDATE_SOURCES)
	mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BOOT_UPDATE_SOURCES) -o "$@"

$(BUILD_DIR)/test_boot_jump: $(BOOT_JUMP_SOURCES)
	mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BOOT_JUMP_SOURCES) -o "$@"

$(BUILD_DIR)/test_esp_http_bridge: $(ESP_HTTP_SOURCES)
	mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ESP_HTTP_SOURCES) -o "$@"

test: $(TARGETS)
	"$(BUILD_DIR)/test_p0"
	"$(BUILD_DIR)/test_ota_contract"
	"$(BUILD_DIR)/test_w25q128"
	"$(BUILD_DIR)/test_tft_display"
	"$(BUILD_DIR)/test_esp_at"
	"$(BUILD_DIR)/test_http_server"
	"$(BUILD_DIR)/test_ota_upload"
	"$(BUILD_DIR)/test_ota_metadata_store"
	"$(BUILD_DIR)/test_ota_update"
	"$(BUILD_DIR)/test_boot_updater"
	"$(BUILD_DIR)/test_boot_jump"
	"$(BUILD_DIR)/test_esp_http_bridge"
