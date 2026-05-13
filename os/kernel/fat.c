#include "fat.h"
#include "block.h"
#include "heap.h"
#include "diag.h"

static fat_context_t g_fat;
static UINT8 *g_sector_buffer;

static void fat_toupper_str(CHAR8 *dst, const CHAR8 *src, UINTN len, UINTN max_len) {
    UINTN max = len < max_len ? len : max_len;
    for (UINTN i = 0; i < max; ++i) {
        if (src[i] >= 'a' && src[i] <= 'z') {
            dst[i] = src[i] - 32;
        } else {
            dst[i] = src[i];
        }
    }
    if (max_len > 0 && max_len > 0) {
        dst[max_len - 1] = 0;
    }
}

static UINT32 cluster_to_sector(UINT32 cluster) {
    if (g_fat.type == FAT_TYPE_32) {
        return g_fat.data_start_sector + (cluster - 2) * g_fat.sectors_per_cluster;
    }
    return g_fat.data_start_sector + (cluster - 2) * g_fat.sectors_per_cluster;
}

static UINT32 fat_read_next_cluster(UINT32 current_cluster) {
    if (g_fat.type == FAT_TYPE_12) {
        UINTN fat_offset = (current_cluster * 3) / 2;
        UINTN entry_offset = fat_offset % g_fat.bytes_per_sector;
        UINTN sector = fat_offset / g_fat.bytes_per_sector;

        UINT8 temp[2];
        if (!block_read(g_fat.device_index, g_fat.fat_start_sector + sector, 1, temp)) {
            return 0xFFFFFFFF;
        }

        UINT16 value;
        if (entry_offset == g_fat.bytes_per_sector - 1) {
            value = temp[0] | ((temp[1] & 0x0F) << 8);
        } else {
            value = temp[0] | ((temp[1] & 0x0F) << 8);
        }

        if (current_cluster % 2 == 0) {
            return value & 0x0FFF;
        } else {
            return (value >> 4) & 0x0FFF;
        }
    } else if (g_fat.type == FAT_TYPE_16) {
        UINT16 cluster16;
        if (!block_read(g_fat.device_index, g_fat.fat_start_sector, 1, &cluster16)) {
            return 0xFFFFFFFF;
        }
        return cluster16;
    } else if (g_fat.type == FAT_TYPE_32) {
        UINT32 cluster32;
        if (!block_read(g_fat.device_index, g_fat.fat_start_sector, 1, &cluster32)) {
            return 0xFFFFFFFF;
        }
        return cluster32 & 0x0FFFFFFF;
    }
    return 0xFFFFFFFF;
}

static BOOLEAN read_directory_entry(UINT32 start_sector, UINTN entry_count, const CHAR8 *target_name, fat_dir_entry_t *out_entry) {
    UINTN entries_per_sector = g_fat.bytes_per_sector / sizeof(fat_dir_entry_t);
    UINTN sectors = (entry_count + entries_per_sector - 1) / entries_per_sector;

    for (UINTN sec = 0; sec < sectors; ++sec) {
        if (!block_read(g_fat.device_index, start_sector + sec, 1, g_sector_buffer)) {
            return FALSE;
        }

        for (UINTN i = 0; i < entries_per_sector; ++i) {
            fat_dir_entry_t *entry = (fat_dir_entry_t *)(g_sector_buffer + i * sizeof(fat_dir_entry_t));

            if (entry->name[0] == 0x00) {
                return FALSE;
            }
            if (entry->name[0] == 0xE5) {
                continue;
            }

            CHAR8 name_upper[13];
            fat_toupper_str(name_upper, (const CHAR8 *)entry->name, 8, 12);
            if (entry->ext[0] != ' ') {
                name_upper[8] = '.';
                fat_toupper_str(name_upper + 9, (const CHAR8 *)entry->ext, 3, 4);
            }

            if (target_name[0] == '*') {
                if ((entry->attributes & 0x08) == 0) {
                    *out_entry = *entry;
                    return TRUE;
                }
            } else {
                CHAR8 target_upper[13];
                fat_toupper_str(target_upper, (const CHAR8 *)target_name, 11, 12);
                if (__builtin_memcmp(name_upper, target_upper, 11) == 0) {
                    *out_entry = *entry;
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

static UINTN read_file_clusters(UINT32 start_cluster, UINT8 *buffer, UINTN max_size) {
    UINTN total_read = 0;
    UINT32 cluster = start_cluster;
    UINTN clusters_read = 0;
    UINTN max_clusters = (max_size / (g_fat.sectors_per_cluster * g_fat.bytes_per_sector)) + 1;

    while (cluster < 0x0FFFFFF8 && clusters_read < max_clusters) {
        UINT32 sector = cluster_to_sector(cluster);
        if (!block_read(g_fat.device_index, sector, g_fat.sectors_per_cluster, buffer + total_read)) {
            break;
        }

        total_read += g_fat.sectors_per_cluster * g_fat.bytes_per_sector;
        cluster = fat_read_next_cluster(cluster);
        clusters_read++;

        if (total_read >= max_size) {
            break;
        }
    }

    return total_read;
}

BOOLEAN fat_init(void) {
    g_fat.mounted = FALSE;
    g_fat.device_index = 0;
    g_fat.fat_buffer = NULL;
    g_sector_buffer = heap_alloc(512);
    if (g_sector_buffer == NULL) {
        diag_log(0x720U, 1, 0, 0);
        return FALSE;
    }
    diag_log(0x720U, 0, 0, 0);
    return TRUE;
}

BOOLEAN fat_mount(UINTN device_index) {
    if (g_fat.mounted) {
        return TRUE;
    }

    g_fat.device_index = device_index;

    if (!block_read(device_index, 0, 1, g_sector_buffer)) {
        diag_log(0x721U, 1, device_index, 0);
        return FALSE;
    }

    fat_bpb_t *bpb = (fat_bpb_t *)g_sector_buffer;

    g_fat.bytes_per_sector = bpb->bytes_per_sector;
    g_fat.sectors_per_cluster = bpb->sectors_per_cluster;
    g_fat.reserved_sectors = bpb->reserved_sectors;
    g_fat.num_fats = bpb->num_fats;

    if (g_fat.bytes_per_sector == 0) {
        diag_log(0x722U, 0, 0, 0);
        return FALSE;
    }

    UINT32 total_sectors;
    if (bpb->total_sectors_16 == 0) {
        total_sectors = bpb->total_sectors_32;
    } else {
        total_sectors = bpb->total_sectors_16;
    }

    g_fat.root_dir_sectors = (bpb->root_entry_count * 32 + g_fat.bytes_per_sector - 1) / g_fat.bytes_per_sector;

    if (total_sectors < 4085) {
        g_fat.type = FAT_TYPE_12;
    } else if (total_sectors < 65525) {
        g_fat.type = FAT_TYPE_16;
    } else {
        g_fat.type = FAT_TYPE_32;
    }

    if (g_fat.type == FAT_TYPE_32) {
        fat32_bs_t *bs = (fat32_bs_t *)g_sector_buffer;
        g_fat.sectors_per_fat = bs->sectors_per_fat;
        g_fat.root_dir_start = bs->root_cluster;
    } else {
        g_fat.sectors_per_fat = bpb->sectors_per_fat_16;
        g_fat.root_dir_start = g_fat.reserved_sectors + (g_fat.num_fats * g_fat.sectors_per_fat);
    }

    g_fat.fat_start_sector = g_fat.reserved_sectors;
    g_fat.data_start_sector = g_fat.reserved_sectors + (g_fat.num_fats * g_fat.sectors_per_fat) + g_fat.root_dir_sectors;
    g_fat.total_clusters = (total_sectors - g_fat.root_dir_sectors - g_fat.reserved_sectors - (g_fat.num_fats * g_fat.sectors_per_fat)) / g_fat.sectors_per_cluster;

    g_fat.mounted = TRUE;
    diag_log(0x723U, (UINT32)g_fat.type, g_fat.bytes_per_sector, (UINT32)g_fat.sectors_per_cluster);

    return TRUE;
}

BOOLEAN fat_unmount(void) {
    if (!g_fat.mounted) {
        return TRUE;
    }

    g_fat.mounted = FALSE;
    return TRUE;
}

BOOLEAN fat_read_file(const CHAR8 *filename, UINT8 *buffer, UINTN max_size, UINTN *out_size) {
    if (!g_fat.mounted || buffer == NULL || out_size == NULL) {
        return FALSE;
    }

    *out_size = 0;

    fat_dir_entry_t entry;
    BOOLEAN found = FALSE;

    if (g_fat.type == FAT_TYPE_32) {
        found = read_directory_entry(cluster_to_sector(g_fat.root_dir_start), 0, filename, &entry);
    } else {
        found = read_directory_entry(g_fat.root_dir_start, g_fat.root_dir_sectors * (g_fat.bytes_per_sector / 32), filename, &entry);
    }

    if (!found) {
        diag_log(0x724U, 1, 0, 0);
        return FALSE;
    }

    UINT32 start_cluster;
    if (g_fat.type == FAT_TYPE_32) {
        start_cluster = ((UINT32)entry.first_cluster_high << 16) | entry.first_cluster_low;
    } else {
        start_cluster = entry.first_cluster_low;
    }

    if (start_cluster == 0 || start_cluster >= 0x0FFFFFF8) {
        diag_log(0x724U, 2, start_cluster, 0);
        return FALSE;
    }

    UINTN file_size = entry.file_size;
    if (file_size > max_size) {
        file_size = max_size;
    }

    UINTN bytes_read = read_file_clusters(start_cluster, buffer, file_size);
    *out_size = bytes_read;

    diag_log(0x725U, (UINT32)found, bytes_read, file_size);
    return bytes_read > 0;
}

const CHAR16* fat_get_volume_label(void) {
    if (!g_fat.mounted) {
        return NULL;
    }
    return L"MARSDISK";
}