#ifndef FAT_H
#define FAT_H

#include "uefi.h"

#define FAT_MAX_OPEN_FILES 4
#define FAT_MAX_FILENAME 11

typedef struct {
    UINT8 name[8];
    UINT8 ext[3];
    UINT8 attributes;
    UINT8 reserved;
    UINT8 create_time_tenth;
    UINT16 create_time;
    UINT16 create_date;
    UINT16 access_date;
    UINT16 first_cluster_high;
    UINT16 modify_time;
    UINT16 modify_date;
    UINT16 first_cluster_low;
    UINT32 file_size;
} __attribute__((packed)) fat_dir_entry_t;

typedef struct {
    UINT8 jump[3];
    UINT8 oem_name[8];
    UINT16 bytes_per_sector;
    UINT8 sectors_per_cluster;
    UINT16 reserved_sectors;
    UINT8 num_fats;
    UINT16 root_entry_count;
    UINT16 total_sectors_16;
    UINT8 media_descriptor;
    UINT16 sectors_per_fat_16;
    UINT16 sectors_per_track;
    UINT16 num_heads;
    UINT32 hidden_sectors;
    UINT32 total_sectors_32;
    UINT8 drive_number;
    UINT8 reserved1;
    UINT8 boot_signature;
    UINT32 volume_id;
    UINT8 volume_label[11];
    UINT8 fs_type[8];
} __attribute__((packed)) fat_bpb_t;

typedef struct {
    UINT8 drive_number;
    UINT8 padding;
    UINT8 signature;
    UINT32 serial_number;
    UINT8 volume_label[11];
    UINT8 fs_type[8];
} __attribute__((packed)) fat12_16_bs_t;

typedef struct {
    UINT32 sectors_per_fat;
    UINT16 flags;
    UINT16 version;
    UINT32 root_cluster;
    UINT16 fsinfo_sector;
    UINT16 backup_boot_sector;
    UINT8 reserved[12];
    UINT8 drive_number;
    UINT8 reserved1;
    UINT8 signature;
    UINT32 serial_number;
    UINT8 volume_label[11];
    UINT8 fs_type[8];
} __attribute__((packed)) fat32_bs_t;

typedef enum {
    FAT_TYPE_NONE,
    FAT_TYPE_12,
    FAT_TYPE_16,
    FAT_TYPE_32
} fat_type_t;

typedef struct {
    UINTN device_index;
    fat_type_t type;
    UINT32 bytes_per_sector;
    UINT8 sectors_per_cluster;
    UINT16 reserved_sectors;
    UINT8 num_fats;
    UINT32 sectors_per_fat;
    UINT32 fat_start_sector;
    UINT32 data_start_sector;
    UINT32 root_dir_sectors;
    UINT32 root_dir_start;
    UINT32 total_clusters;
    UINT8 *fat_buffer;
    UINTN fat_buffer_size;
    BOOLEAN mounted;
} fat_context_t;

BOOLEAN fat_init(void);
BOOLEAN fat_mount(UINTN device_index);
BOOLEAN fat_unmount(void);
BOOLEAN fat_read_file(const CHAR8 *filename, UINT8 *buffer, UINTN max_size, UINTN *out_size);
const CHAR16* fat_get_volume_label(void);

#endif