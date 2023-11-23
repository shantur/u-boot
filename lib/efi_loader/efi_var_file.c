// SPDX-License-Identifier: GPL-2.0+
/*
 * File interface for UEFI variables
 *
 * Copyright (c) 2020, Heinrich Schuchardt
 */

#define LOG_CATEGORY LOGC_EFI

#include <common.h>
#include <charset.h>
#include <fs.h>
#include <log.h>
#include <malloc.h>
#include <mapmem.h>
#include <efi_loader.h>
#include <efi_variable.h>

#define PART_STR_LEN 10

/**
 * efi_set_blk_dev_to_system_partition() - select EFI system partition
 *
 * Set the EFI system partition as current block device.
 *
 * Return:	status code
 */
static efi_status_t __maybe_unused efi_set_blk_dev_to_system_partition(void)
{
	char part_str[PART_STR_LEN];
	int r;

	if (efi_system_partition.uclass_id == UCLASS_INVALID) {
		log_err("No EFI system partition\n");
		return EFI_DEVICE_ERROR;
	}
	snprintf(part_str, PART_STR_LEN, "%x:%x",
		 efi_system_partition.devnum, efi_system_partition.part);
	r = fs_set_blk_dev(blk_get_uclass_name(efi_system_partition.uclass_id),
			   part_str, FS_TYPE_ANY);
	if (r) {
		log_err("Cannot read EFI system partition\n");
		return EFI_DEVICE_ERROR;
	}
	return EFI_SUCCESS;
}

/**
 * efi_var_to_file() - save non-volatile variables as file
 *
 * File ubootefi.var is created on the EFI system partion.
 *
 * Return:	status code
 */
efi_status_t efi_var_to_file(void)
{
	efi_status_t ret;
	struct efi_var_file *buf;
	loff_t len;
	loff_t actlen;
	int r;

	ret = efi_var_collect(&buf, &len, EFI_VARIABLE_NON_VOLATILE);
	if (ret != EFI_SUCCESS)
		goto error;

	ret = efi_set_blk_dev_to_system_partition();
	if (ret != EFI_SUCCESS)
		goto error;

	r = fs_write(EFI_VAR_FILE_NAME, map_to_sysmem(buf), 0, len, &actlen);
	if (r || len != actlen)
		ret = EFI_DEVICE_ERROR;

error:
	if (ret != EFI_SUCCESS)
		log_err("Failed to persist EFI variables\n");
	free(buf);
	return ret;
}

/**
 * efi_var_from_file() - read variables from file
 *
 * File ubootefi.var is read from the EFI system partitions and the variables
 * stored in the file are created.
 *
 * In case the file does not exist yet or a variable cannot be set EFI_SUCCESS
 * is returned.
 *
 * Return:	status code
 */
efi_status_t efi_var_from_file(void)
{
	struct efi_var_file *buf;
	loff_t len;
	efi_status_t ret;
	int r;

	buf = calloc(1, EFI_VAR_BUF_SIZE);
	if (!buf) {
		log_err("Out of memory\n");
		return EFI_OUT_OF_RESOURCES;
	}

	ret = efi_set_blk_dev_to_system_partition();
	if (ret != EFI_SUCCESS)
		goto error;
	r = fs_read(EFI_VAR_FILE_NAME, map_to_sysmem(buf), 0, EFI_VAR_BUF_SIZE,
		    &len);
	if (r || len < sizeof(struct efi_var_file)) {
		log_err("Failed to load EFI variables\n");
		goto error;
	}
	if (buf->length != len || efi_var_restore(buf, false) != EFI_SUCCESS)
		log_err("Invalid EFI variables file\n");
error:
	free(buf);
	return EFI_SUCCESS;
}
