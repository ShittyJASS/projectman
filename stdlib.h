#pragma once

#include <stdint.h>
#include "dirprobe.h"
#include <stdbool.h>
#include <Windows.h>
#include <assert.h>

/* Character sequence accompanied by the length of said sequence */
typedef struct {
	char *mem;
	uint32_t len;
} SL_String;

/* StdLib structure */
typedef struct {
	char path[MAX_PATH];
	char *pt0;
	FILE *header;
	uint8_t headerSize;
} StdLib;

FILE *sl_probemetafile(StdLib *stdLib, char *fname) {
	memcpy(stdLib->pt0, "meta\\", 5);
	memcpy(stdLib->pt0 + 5, fname, strlen(fname) + 1);
	FILE *f = fopen(stdLib->path, "r+b");
	if (!f) {
		f = fopen(stdLib->path, "w+b");
		SetFileAttributesA(stdLib->path, FILE_ATTRIBUTE_HIDDEN);
		assert(f);
		if (!f) abort();
		fwrite(&(uint8_t) { 0 }, 1, 1, f);
		fseek(f, 0, SEEK_SET);
	}
	return f;
}

FILE *sl_probeobjfile(StdLib *stdLib, char *fname) {
	memcpy(stdLib->pt0, "obj\\", 4);
	memcpy(stdLib->pt0 + 4, fname, strlen(fname) + 1);
	return fopen(stdLib->path, "r+b");
}

static inline void sl_getnewlibs_seektofpos(FILE *f, DP_String *key, char *buffer, uint64_t *bitmask);

/* Returns names of libs with ObjMerger directives not yet considered */
bool sl_getnewlibs(StdLib *stdLib, DP_Entry *map, SL_String **dest, uint32_t *destSize) {
	*destSize = 0;
	*dest = (SL_String*) malloc(sizeof(SL_String) * stdLib->headerSize);
	if (!*dest) return false;
	uint64_t mask;
	FILE *f;
	uint8_t len;

	char buffer[MAX_PATH];
	memcpy(buffer, stdLib->path, stdLib->pt0 - stdLib->path);
	memcpy(buffer + (stdLib->pt0 - stdLib->path), ".backup", 7);
	char bu = map->name.mem[map->name.len];
	map->name.mem[map->name.len] = 0;
	memcpy(stdLib->pt0, "meta\\", 5);
	memcpy(stdLib->pt0 + 5, map->name.mem, map->name.len + 1);
	DWORD dwAttrib = GetFileAttributesA(stdLib->path);
	if (dwAttrib != INVALID_FILE_ATTRIBUTES && dwAttrib & FILE_ATTRIBUTE_NORMAL) {
		CopyFileA(stdLib->path, buffer, FALSE);
		f = fopen(stdLib->path, "rb");
		if (!f) abort();
		sl_getnewlibs_seektofpos(f, (DP_String*) &map->path, buffer, &mask);
	} else {
		DeleteFileA(buffer);
		f = fopen(stdLib->path, "wb");
	}
	map->name.mem[map->name.len] = bu;

	fseek(stdLib->header, 1, SEEK_SET);
	for (int i = 0; i < stdLib->headerSize; ++i) {
		fread(&len, 1, 1, stdLib->header);
		if (mask & ((uint64_t)1)<<i) {
			fseek(stdLib->header, len, SEEK_CUR);
		} else {
			(*dest[*destSize]).len = len;
			(*dest[*destSize]).mem = malloc(len + 1);
			fread((*dest[*destSize]).mem, 1, len, stdLib->header);
			memcpy((*dest[*destSize]).mem + (*dest[*destSize]).len, "_Obj.j", 7);
			(*dest[*destSize]).mem[len] = 0;
			mask |= ((uint64_t) 1) << i;
			++*destSize;
		}
	}
	SL_String *newBlock = (SL_String*) realloc(*dest, sizeof(SL_String) * (*destSize)?*destSize:1);
	if (!newBlock) {
		free(*dest);
		fclose(f);
		return false;
	}
	*dest = newBlock;
	if (!fwrite(&mask, 8, 1, f)) {
		free(*dest);
		fclose(f);
		return false;
	}
	fclose(f);
	return true;
}

const char *sl_probenode(StdLib *stdLib, char *subpath, DP_Entry **dest, uint32_t *destSize) {
	memcpy(stdLib->pt0, subpath, strlen(subpath) + 1);
	return probe_dir(stdLib->path, dest, destSize);
}

bool sl_init(StdLib *dest) {
	GetModuleFileNameA(NULL, dest->path, MAX_PATH);
	dest->pt0 = strrchr(dest->path, '\\');
	if (!dest->pt0) {
		return false;
	}

	memcpy(dest->pt0 + 1, "stdlib\\", 8);
	dest->pt0 += 8;
	DWORD dwAttrib = GetFileAttributesA(dest->path);
	if (dwAttrib == INVALID_FILE_ATTRIBUTES || !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
		return false;
	}

	memcpy(dest->pt0, "bin", 4);
	CreateDirectoryA(dest->path, NULL);
	memcpy(dest->pt0, "obj", 3);
	CreateDirectoryA(dest->path, NULL);
	memcpy(dest->pt0, "meta", 5);
	if (CreateDirectoryA(dest->path, NULL)) {
		SetFileAttributesA(dest->path, FILE_ATTRIBUTE_HIDDEN);
	}

	dest->header = sl_probemetafile(dest, ".header");
	fread(&dest->headerSize, 1, 1, dest->header);

	return true;
}

static inline void sl_getnewlibs_seektofpos(FILE *f, DP_String *key, char *buffer, uint64_t *bitmask) {
	uint8_t clusterSize;
	uint8_t len;
	fread(&clusterSize, 1, 1, f);
	for (;;) {
		if (clusterSize--) {
			fread(&len, 1, 1, f);
			if (len == key->len) {
				fread(buffer, 1, len, f);
				if (!memcmp(key->mem, buffer, len)) {
					fread(bitmask, 8, 1, f);
					fseek(f, -8, SEEK_CUR);
					break;
				}
				else fseek(f, 8, SEEK_CUR);
			}
			else fseek(f, len + 8, SEEK_CUR);
		}
		else {
			fseek(f, 0, SEEK_SET);
			fread(&clusterSize, 1, 1, f);
			++clusterSize;
			fseek(f, 0, SEEK_SET);
			fwrite(&clusterSize, 1, 1, f);
			fseek(f, 0, SEEK_END);
			len = (uint8_t) key->len;
			fwrite(&len, 1, 1, f);
			fwrite(key->mem, 1, key->len, f);
			*bitmask = 0;
			break;
		}
	}
}
