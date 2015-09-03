// Copyright 2009  Segher Boessenkool  <segher@kernel.crashing.org>
// This code is licensed to you under the terms of the GNU GPL, version 2;
// see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt


#include "loader.h"

#ifdef FAT_TEST
#include <stdio.h>
#endif


#define RAW_BUF 0x200
static u8 raw_buf[RAW_BUF] __attribute__((aligned(32)));

static int raw_read(u32 sector)
{
	static u32 current = -1;

	if (current == sector)
		return 0;
	current = sector;

	return sd_read_sector(raw_buf, sector);
}

static u64 partition_start_offset;

static int read(u8 *data, u64 offset, u32 len)
{
	offset += partition_start_offset;

	while (len) {
		u32 buf_off = offset % RAW_BUF;
		u32 n;

		n = RAW_BUF - buf_off;
		if (n > len)
			n = len;

		int err = raw_read(offset / RAW_BUF);
		if (err)
			return err;

		memcpy(data, raw_buf + buf_off, n);

		data += n;
		offset += n;
		len -= n;
	}

	return 0;
}


static u32 bytes_per_cluster;
static u32 root_entries;
static u32 clusters;
static u32 fat_type;	// 12, 16, or 32

static u64 fat_offset;
static u64 root_offset;
static u64 data_offset;


static u32 get_fat(u32 cluster)
{
	u8 fat[4];

	u32 offset_bits = cluster*fat_type;
	int err = read(fat, fat_offset + offset_bits/8, 4);
	if (err)
		return 0;

	u32 res = le32(fat) >> (offset_bits % 8);
	res &= (1 << fat_type) - 1;
	res &= 0x0fffffff;		// for FAT32

	return res;
}


static u64 extent_offset;
static u32 extent_len;
static u32 extent_next_cluster;

static void get_extent(u32 cluster)
{
	extent_len = 0;
	extent_next_cluster = 0;

	if (cluster == 0) {	// Root directory.
		if (fat_type != 32) {
			extent_offset = root_offset;
			extent_len = 0x20*root_entries;

			return;
		}
		cluster = root_offset;
	}

	if (cluster - 2 >= clusters)
		return;

	extent_offset = data_offset + (u64)bytes_per_cluster*(cluster - 2);

	for (;;) {
		extent_len += bytes_per_cluster;

		u32 next_cluster = get_fat(cluster);

		if (next_cluster - 2 >= clusters)
			break;

		if (next_cluster != cluster + 1) {
			extent_next_cluster = next_cluster;
			break;
		}

		cluster = next_cluster;
	}
}


static int read_extent(u8 *data, u32 len)
{
	while (len) {
		if (extent_len == 0)
			return -1;

		u32 this = len;
		if (this > extent_len)
			this = extent_len;

		int err = read(data, extent_offset, this);
		if (err)
			return err;

		extent_offset += this;
		extent_len -= this;

		data += this;
		len -= this;

		if (extent_len == 0 && extent_next_cluster)
			get_extent(extent_next_cluster);
	}

	return 0;
}


int fat_read(void *data, u32 len)
{
	return read_extent(data, len);
}


static u8 fat_name[11];

static u8 ucase(char c)
{
	if (c >= 'a' && c <= 'z')
		return c - 'a' + 'A';

	return c;
}

static const char *parse_component(const char *path)
{
	u32 i = 0;

	while (*path == '/')
		path++;

	while (*path && *path != '/' && *path != '.') {
		if (i < 8)
			fat_name[i++] = ucase(*path);
		path++;
	}

	while (i < 8)
		fat_name[i++] = ' ';

	if (*path == '.')
		path++;

	while (*path && *path != '/') {
		if (i < 11)
			fat_name[i++] = ucase(*path);
		path++;
	}

	while (i < 11)
		fat_name[i++] = ' ';

	if (fat_name[0] == 0xe5)
		fat_name[0] = 0x05;

	return path;
}


u32 fat_file_size;

int fat_open(const char *name)
{
	u32 cluster = 0;

	while (*name) {
		get_extent(cluster);

		name = parse_component(name);

		while (extent_len) {
			u8 dir[0x20];

			int err = read_extent(dir, 0x20);
			if (err)
				return err;

			if (dir[0] == 0)
				return -1;

			if (dir[0x0b] & 0x08)	// volume label or LFN
				continue;
			if (dir[0x00] == 0xe5)	// deleted file
				continue;

			if (!!*name != !!(dir[0x0b] & 0x10))	// dir vs. file
				continue;

			if (memcmp(fat_name, dir, 11) == 0) {
				cluster = le16(dir + 0x1a);
				if (fat_type == 32)
					cluster |= le16(dir + 0x14) << 16;

				if (*name == 0) {
					fat_file_size = le32(dir + 0x1c);
					get_extent(cluster);

					return 0;
				}

				break;
			}
		}
	}

	return -1;
}


#ifdef FAT_TEST
static void print_dir_entry(u8 *dir)
{
	int i, n;

	if (dir[0x0b] & 0x08)	// volume label or LFN
		return;
	if (dir[0x00] == 0xe5)	// deleted file
		return;

	if (fat_type == 32) {
		fprintf(stderr, "#%04x", le16(dir + 0x14));
		fprintf(stderr, "%04x  ", le16(dir + 0x1a));
	} else
		fprintf(stderr, "#%04x  ", le16(dir + 0x1a));	// start cluster
	u16 date = le16(dir + 0x18);
	fprintf(stderr, "%04d-%02d-%02d ", 1980 + (date >> 9), (date >> 5) & 0x0f, date & 0x1f);
	u16 time = le16(dir + 0x16);
	fprintf(stderr, "%02d:%02d:%02d  ", time >> 11, (time >> 5) & 0x3f, 2*(time & 0x1f));
	fprintf(stderr, "%10d  ", le32(dir + 0x1c));	// file size
	u8 attr = dir[0x0b];
	for (i = 0; i < 6; i++)
		fprintf(stderr, "%c", (attr & (1 << i)) ? "RHSLDA"[i] : ' ');
	fprintf(stderr, "  ");
	for (n = 8; n && dir[n - 1] == ' '; n--)
		;
	for (i = 0; i < n; i++)
		fprintf(stderr, "%c", dir[i]);
	for (n = 3; n && dir[8 + n - 1] == ' '; n--)
		;
	if (n) {
		fprintf(stderr, ".");
		for (i = 0; i < n; i++)
			fprintf(stderr, "%c", dir[8 + i]);
	}

	fprintf(stderr, "\n");
}


int print_dir(u32 cluster)
{
	u8 dir[0x20];

	get_extent(cluster);

	while (extent_len) {
		int err = read_extent(dir, 0x20);
		if (err)
			return err;

		if (dir[0] == 0)
			break;

		print_dir_entry(dir);
	}

	return 0;
}
#endif


static int fat_init_fs(const u8 *sb)
{
	u32 bytes_per_sector = le16(sb + 0x0b);
	u32 sectors_per_cluster = sb[0x0d];
	bytes_per_cluster = bytes_per_sector * sectors_per_cluster;

	u32 reserved_sectors = le16(sb + 0x0e);
	u32 fats = sb[0x10];
	root_entries = le16(sb + 0x11);
	u32 total_sectors = le16(sb + 0x13);
	u32 sectors_per_fat = le16(sb + 0x16);

	// For FAT16 and FAT32:
	if (total_sectors == 0)
		total_sectors = le32(sb + 0x20);

	// For FAT32:
	if (sectors_per_fat == 0)
		sectors_per_fat = le32(sb + 0x24);

	// XXX: For FAT32, we might want to look at offsets 28, 2a
	// XXX: We _do_ need to look at 2c

	u32 fat_sectors = sectors_per_fat * fats;
	u32 root_sectors = (0x20*root_entries + bytes_per_sector - 1)
	                   / bytes_per_sector;

	u32 fat_start_sector = reserved_sectors;
	u32 root_start_sector = fat_start_sector + fat_sectors;
	u32 data_start_sector = root_start_sector + root_sectors;

	clusters = (total_sectors - data_start_sector) / sectors_per_cluster;

	if (clusters < 0x0ff5)
		fat_type = 12;
	else if (clusters < 0xfff5)
		fat_type = 16;
	else
		fat_type = 32;

	fat_offset = (u64)bytes_per_sector*fat_start_sector;
	root_offset = (u64)bytes_per_sector*root_start_sector;
	data_offset = (u64)bytes_per_sector*data_start_sector;

	if (fat_type == 32)
		root_offset = le32(sb + 0x2c);

#ifdef FAT_TEST
	fprintf(stderr, "bytes_per_sector    = %08x\n", bytes_per_sector);
	fprintf(stderr, "sectors_per_cluster = %08x\n", sectors_per_cluster);
	fprintf(stderr, "bytes_per_cluster   = %08x\n", bytes_per_cluster);
	fprintf(stderr, "root_entries        = %08x\n", root_entries);
	fprintf(stderr, "clusters            = %08x\n", clusters);
	fprintf(stderr, "fat_type            = %08x\n", fat_type);
	fprintf(stderr, "fat_offset          = %012llx\n", fat_offset);
	fprintf(stderr, "root_offset         = %012llx\n", root_offset);
	fprintf(stderr, "data_offset         = %012llx\n", data_offset);
#endif

	return 0;
}


static int is_fat_fs(const u8 *sb)
{
	// Bytes per sector should be 512, 1024, 2048, or 4096
	u32 bps = le16(sb + 0x0b);
	if (bps < 0x0200 || bps > 0x1000 || bps & (bps - 1))
		return 0;

	// Media type should be f0 or f8,...,ff
	if (sb[0x15] < 0xf8 && sb[0x15] != 0xf0)
		return 0;

	// If those checks didn't fail, it's FAT.  We hope.
	return 1;
}


int fat_init(void)
{
	u8 buf[0x200];
	int err;

	partition_start_offset = 0;
	err = read(buf, 0, 0x200);
	if (err)
		return err;

	if (le16(buf + 0x01fe) != 0xaa55)	// Not a DOS disk.
		return -1;

	if (is_fat_fs(buf))
		return fat_init_fs(buf);

	// Maybe there's a partition table?  Let's try the first partition.
	if (buf[0x01c2] == 0)
		return -1;

	partition_start_offset = 0x200ULL*le32(buf + 0x01c6);

	err = read(buf, 0, 0x200);
	if (err)
		return err;

	if (is_fat_fs(buf))
		return fat_init_fs(buf);

	return -1;
}
