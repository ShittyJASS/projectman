#define _CRT_SECURE_NO_WARNINGS
#include "stdlib.h"
#include "dirprobe.h"
#include "stdlib.h"
#include <Windows.h>
#include <stdio.h>
// #include <keywordtree.h>

char *bigBuffer;

static inline void append_file(FILE *dest, FILE *src, const size_t bufferSize);
void free_bigbuffer(void);

int execute_application(int argc, char **argv) {

	if (argc != 3) {
		MessageBoxA(NULL, "Follow argumentation: <map path> <output file>", "ProjectMan: Bad input error", MB_OK | MB_ICONERROR);
		return 1;
	}

	DP_Entry map;
	bigBuffer = malloc(4096);
	atexit(free_bigbuffer);

	map.path.len = (uint32_t) strlen(argv[1]);
	map.path.mem = bigBuffer;
	memcpy(map.path.mem, argv[1], map.path.len);
	map.path.mem[map.path.len] = 0;
	if (!dp_entry_parse(&map)) {
		MessageBoxA(NULL, "Unable to parse project path", "ProjectMan: Bad input error", MB_OK | MB_ICONERROR);
		return 1;
	}
	do {
		DWORD dwAttrib;

		memcpy(map.name.mem, "script\\", 8);
		dwAttrib = GetFileAttributesA(map.path.mem);
		if (dwAttrib == INVALID_FILE_ATTRIBUTES || !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) break;
		
		memcpy(map.name.mem, "src\\", 5);
		dwAttrib = GetFileAttributesA(map.path.mem);
		if (dwAttrib == INVALID_FILE_ATTRIBUTES || !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) break;

		return 0;
	} while (0);

	DP_Entry *dest = NULL;
	uint32_t destSize = 0;
	const char *error;

	StdLib stdLib;
	bool slEnabled = sl_init(&stdLib); /* stdlib is optional */;
	if (slEnabled) {
		error = sl_probenode(&stdLib, "bin", &dest, &destSize);
		if (error) {
			MessageBoxA(NULL, error, "ProjectMan: StdLib probe error", MB_OK | MB_ICONERROR);
			return 1;
		}
	}

	error = probe_dir(map.path.mem, &dest, &destSize);
	if (error) {
		MessageBoxA(NULL, error, "ProjectMan: Project probe error", MB_OK | MB_ICONERROR);
		return 1;
	}
	memcpy(map.name.mem, argv[1] + (map.name.mem - map.path.mem), map.path.len - (map.name.mem - map.path.mem) + 1);

	FILE *f = fopen(argv[2], "ab");
	if (!f) {
		free(dest);
		MessageBoxA(NULL, "Unable to create output file", "ProjectMan: Filesystem error", MB_OK | MB_ICONERROR);
		return 1;
	}

	if (slEnabled) {
		/*SL_String *objs;
		uint32_t objsSize;
		sl_getnewlibs(&stdLib, &map, &objs, &objsSize);
		while (objsSize--) {
		FILE *g = sl_probeobjfile(&stdLib, objs[objsSize].mem);
		if (!g) {
		sprintf(bigBuffer, "Unable to access object data of resource: %s", objs[objsSize].mem);
		free(dest);
		free(objs);
		MessageBoxA(NULL, bigBuffer, "ProjectMan: Filesystem error", MB_OK | MB_ICONERROR);
		return 1;
		}
		append_file(f, g, buffer, 4096);
		fclose(g);
		}
		free(dest);*/
		fclose(stdLib.header);
	}

	// KeywordTree *presentLibs = kt_new_root();
	bool zinc = false;

	for (uint32_t i = destSize; i--;) {
		if (dest[i].ext.len == 1 && *dest[i].ext.mem == 'j') {
			if (zinc) {
				fwrite("\n//! endzinc\n", 1, 13, f);
				zinc = false;
			}
			else fputc('\n', f);
		}
		else if (dest[i].ext.len == 2 && dest[i].ext.mem[0] == 'z' && dest[i].ext.mem[1] == 'n') {
			if (!zinc) {
				fwrite("\n//! zinc\n", 1, 10, f);
				zinc = true;
			}
			else fputc('\n', f);
		}
		else continue;
		// void **ktEntryData = kt_get_ptr(presentLibs, dest[i].name.mem, dest[i].name.len);
		// if (ktEntryData) continue;

		FILE *g = fopen(dest[i].path.mem, "rb");
		if (!g) {
			sprintf(bigBuffer, "Unable to access resource: %s", dest[i].name.mem);
			free(dest);
			MessageBoxA(NULL, bigBuffer, "ProjectMan: Filesystem error", MB_OK | MB_ICONERROR);
			return 1;
		}

		append_file(f, g, 4096);
		fclose(g);
	}
	if (zinc) fwrite("\n//! endzinc", 1, 12, f);
	fclose(f);

	if (destSize) {
		do free(dest[--destSize].path.mem);
		while (destSize);
		free(dest);
	}

	return 0;
}

static inline void append_file(FILE *dest, FILE *src, const size_t bufferSize) {
	size_t read;
	do {
		read = fread(bigBuffer, 1, bufferSize, src);
		if (read) fwrite(bigBuffer, 1, read, dest);
	} while (read == bufferSize);
}

void free_bigbuffer(void) {
	free(bigBuffer);
}
