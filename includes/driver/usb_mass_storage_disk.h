/**
 * @file usb_mass_storage_disk.h
 * @brief USB Mass Storage Disk
 */

#ifndef ___USB_MASS_STORAGE_DISK_H
#define ___USB_MASS_STORAGE_DISK_H

#include <types.h>
#include <disk.h>
#include <driver/usb.h>


uint64_t      usb_mass_storage_get_disk_count(void);
usb_driver_t* usb_mass_storage_get_disk_by_id(uint64_t id);

disk_t* usb_mass_storage_disk_impl_open(usb_driver_t* usb_mass_storage, uint8_t lun);

#endif /* ___USB_MASS_STORAGE_DISK_H */
