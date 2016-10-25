#pragma once

#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

const char *DP_ERROR_ALLOC = "Memory allocation error";
const char *DP_ERROR_LONGPATH = "Too long path name";
const char *DP_ERROR_OPENDIR = "Unable to open directory";
const char *DP_ERROR_PARSE_NAME = "Unable to parse file name";

/* Character sequence accompanied by the length of said sequence */
typedef struct {
	char *mem;
	uint32_t len;
} DP_String;

/* File caught from probe_dir
* Note that path.mem needs to be freed upon deallocation */
typedef struct {
	/* Null-terminated full path of entry */
	DP_String path;
	
	/* Part of path which specifies file name */
	DP_String name;
	
	/* Part of path which specifies file extension */
	DP_String ext;
} DP_Entry;

/* Initializes name and ext
* Note that path needs to be set */
static inline bool dp_entry_parse(DP_Entry *e) {
	char *probe = e->path.mem + e->path.len;
	char bu = *e->path.mem;
	*e->path.mem = '\\';
	while (*--probe != '\\');
	if (probe == e->path.mem) return false;
	*e->path.mem = bu;
	e->name.mem = ++probe;
	e->path.mem[e->path.len] = '.';
	while (*++probe != '.');
	e->path.mem[e->path.len] = 0;
	e->name.len = (uint32_t) (probe - e->name.mem);
	e->ext.mem = probe + (!!*probe);
	e->ext.len = (uint32_t) (e->path.mem + e->path.len - e->ext.mem);
	return true;
}

static inline const char *probe_dir_enum(char *path, uint32_t pathLen, DP_Entry **dest, uint32_t *destSize, uint32_t *reserve) {
	if (pathLen > 255) return DP_ERROR_LONGPATH;

	if (path[pathLen - 1] != '\\') path[pathLen++] = '\\';
	path[pathLen] = 0;
	
	DIR *dir = opendir(path);
	if (!dir) return DP_ERROR_OPENDIR;

	struct dirent *ent;

	while ((ent = readdir(dir))) {
		if (ent->d_type == DT_DIR) {
			if (*ent->d_name == '.') continue;
			memcpy(path + pathLen, ent->d_name, ent->d_namlen);
			const char *error = probe_dir_enum(path, pathLen + (uint32_t) ent->d_namlen, dest, destSize, reserve);
			path[pathLen] = 0;
			if (error) {
				closedir(dir);
				return error;
			}
		} else if (ent->d_type == DT_REG) {
			if (*destSize == *reserve) {
				DP_Entry *newBlock = (DP_Entry*) realloc(*dest, sizeof(DP_Entry) * (*reserve = *reserve * 3 / 2));
				if (!newBlock) {
					closedir(dir);
					return DP_ERROR_ALLOC;
				}
				*dest = newBlock;
			}
			(*dest)[*destSize].path.mem = (char*) malloc(pathLen + ent->d_namlen + 1);
			if (!(*dest)[*destSize].path.mem) {
				closedir(dir);
				return DP_ERROR_ALLOC;
			}

			(*dest)[*destSize].path.len = (uint32_t) ent->d_namlen + pathLen;
			memcpy((*dest)[*destSize].path.mem, path, pathLen);
			memcpy((*dest)[*destSize].path.mem + pathLen, ent->d_name, ent->d_namlen);
			(*dest)[*destSize].path.mem[(*dest)[*destSize].path.len] = 0;
			dp_entry_parse(&(*dest)[*destSize]);

			++*destSize;
		}
	}

	closedir(dir);

	return NULL;
}

const char *probe_dir(const char *path, DP_Entry **dest, uint32_t *destSize) {
	uint32_t reserve = *destSize + 32;
	
	if (*destSize) {
		DP_Entry *newBlock = (DP_Entry*) realloc(*dest, sizeof(DP_Entry) * (*destSize + reserve));
		if (!newBlock) {
			free(*dest);
			return DP_ERROR_ALLOC;
		}
		*dest = newBlock;
	} else {
		*dest = (DP_Entry*) malloc(sizeof(DP_Entry)*reserve);
		if (!*dest) return DP_ERROR_ALLOC;
	}

	char localPath[MAX_PATH];
	uint32_t pathLen = (uint32_t) strlen(path);
	memcpy(localPath, path, pathLen);

	const char *error = probe_dir_enum(localPath, pathLen, dest, destSize, &reserve);
	if (error) {
		free(*dest);
		return error;
	}
	
	if (*destSize) {
		DP_Entry *newBlock = (DP_Entry*) realloc(*dest, sizeof(DP_Entry) * *destSize);
		if (!newBlock) {
			free(*dest);
			return DP_ERROR_ALLOC;
		}
		*dest = newBlock;
	}
	else free(*dest);

	return NULL;
}
