// SPDX-License-Identifier: GPL-2.0+
/*
 * SPI Flash interface for UEFI variables
 *
 * Copyright (c) 2023, Shantur Rathore
 */

#define LOG_CATEGORY LOGC_EFI

#include <efi_loader.h>
#include <efi_variable.h>
#include <spi_flash.h>
#include <dm.h>

efi_status_t efi_var_to_sf(void)
{
	efi_status_t ret;
	struct efi_var_file *buf;
	loff_t len;
	struct udevice *sfdev;

	ret = efi_var_collect(&buf, &len, EFI_VARIABLE_NON_VOLATILE);
	if (len > EFI_VAR_BUF_SIZE) {
		log_err("EFI var buffer length more than target SF size");
		ret = EFI_OUT_OF_RESOURCES;
		goto error;
	}

	log_debug("%s - Got buffer to write buf->len : %d\n", __func__, buf->length);

	if (ret != EFI_SUCCESS)
		goto error;

	ret = uclass_get_device(UCLASS_SPI_FLASH, CONFIG_EFI_VARIABLE_SF_DEVICE_INDEX, &sfdev);
	if (ret)
		goto error;

	ret = spi_flash_erase_dm(sfdev, CONFIG_EFI_VARIABLE_SF_OFFSET, EFI_VAR_BUF_SIZE);
	log_debug("%s - Erased SPI Flash offset %lx\n", __func__, CONFIG_EFI_VARIABLE_SF_OFFSET);
	if (ret)
		goto error;

	ret = spi_flash_write_dm(sfdev, CONFIG_EFI_VARIABLE_SF_OFFSET, len, buf);
	log_debug("%s - Wrote buffer to SPI Flash : %ld\n", __func__, ret);

	if (ret)
		goto error;

	ret = EFI_SUCCESS;
error:
	if (ret)
		log_err("Failed to persist EFI variables in SF\n");
	free(buf);
	return ret;
}

efi_status_t efi_var_from_sf(void)
{
	struct efi_var_file *buf;
	efi_status_t ret;
	struct udevice *sfdev;

	buf = calloc(1, EFI_VAR_BUF_SIZE);
	if (!buf) {
		log_err("%s - Unable to allocate buffer\n", __func__);
		return EFI_OUT_OF_RESOURCES;
	}

	ret = uclass_get_device(UCLASS_SPI_FLASH, 0, &sfdev);
	if (ret)
		goto error;

	ret = spi_flash_read_dm(sfdev, CONFIG_EFI_VARIABLE_SF_OFFSET,
		EFI_VAR_BUF_SIZE, buf);

	log_debug("%s - read buffer buf->length: %x\n", __func__, buf->length);

	if (ret || buf->length < sizeof(struct efi_var_file)) {
		log_err("%s - buffer read from SPI Flash isn't valid\n", __func__);
		goto error;
	}

	ret = efi_var_restore(buf, false);
	if (ret != EFI_SUCCESS)
		log_err("%s - Unable to restore EFI variables from buffer\n");

	ret = EFI_SUCCESS;
error:
	free(buf);
	return ret;
}
