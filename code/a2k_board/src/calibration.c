#include <zephyr/kernel.h>

#if defined(CONFIG_AW2_CALIBRATION)

#include <zephyr/fs/nvs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
#include "a2k_board/drv.h"
#include "a2k_board/calibration.h"

LOG_MODULE_REGISTER(a2k_calibration, LOG_LEVEL_INF);

#define NVS_PARTITION_LABEL "nvs"

static struct nvs_fs   s_nvs;
static a2k_drv_t       s_status = { .name = "calibration" };

int calibration_init(void)
{
	s_status.enabled = true;

	const struct flash_area *fa;
	int rc = flash_area_open(FIXED_PARTITION_ID(nvs_partition), &fa);
	if (rc) {
		LOG_ERR("calibration: flash_area_open failed: %d", rc);
		s_status.err = rc;
		return rc;
	}

	s_nvs.flash_device = flash_area_get_device(fa);
	s_nvs.offset       = FIXED_PARTITION_OFFSET(nvs_partition);
	s_nvs.sector_size  = 2048;   /* STM32F103 flash page = 2 KB */
	s_nvs.sector_count = FIXED_PARTITION_SIZE(nvs_partition) / 2048;
	flash_area_close(fa);

	rc = nvs_mount(&s_nvs);
	if (rc) {
		LOG_ERR("calibration: nvs_mount failed: %d", rc);
		s_status.err = rc;
		return rc;
	}

	s_status.ok = true;
	LOG_INF("calibration: NVS mounted, sector_size=%u sector_count=%u",
		s_nvs.sector_size, s_nvs.sector_count);
	return 0;
}

const a2k_drv_t *calibration_status(void) { return &s_status; }

int calibration_save(const uint8_t *blob, size_t len)
{
	/* key=1 reserved for the calibration blob */
	return nvs_write(&s_nvs, 1, blob, len);
}

int calibration_load(uint8_t *blob, size_t len)
{
	return nvs_read(&s_nvs, 1, blob, len);
}

#endif /* CONFIG_AW2_CALIBRATION */
