/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * mdutil: the metadata index's state, with the reference's command's
 * options. There is no index yet (the maintainer's choice, 2026-11-23:
 * mdfind searches the disk), so every volume reports indexing disabled;
 * -i off and -E have nothing to do, -i on says it cannot be turned on.
 */

#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <unistd.h>

static void
usage(void)
{
	fprintf(stderr, "Usage: mdutil -pEsa -i (on|off) -d volume ...\n"
	    "\tUtility to manage Spotlight indexes.\n"
	    "\t-i (on|off)  Turn indexing on or off.\n"
	    "\t-d           Disable Spotlight activity for volume (re-enable using -i on).\n"
	    "\t-E           Erase and rebuild index.\n"
	    "\t-s           Print indexing status.\n"
	    "\t-a           Apply command to all stores on all volumes.\n"
	    "\t-p           Publish metadata.\n"
	    "\t-V vol       Apply command to all stores on the specified volume.\n"
	    "\t-v           Display verbose information.\n"
	    "NOTE: Run as owner for network homes, otherwise run as root.\n");
	exit(1);
}

/* The mount point a path is on, as the command names a volume. */
static const char *
volumeOf(const char *path, char *buffer, size_t size)
{
	struct statfs fs;

	if (statfs(path, &fs) != 0)
		return NULL;
	snprintf(buffer, size, "%s", fs.f_mntonname);
	return buffer;
}

int
main(int argc, char *argv[])
{
	int status = 0, all = 0, erase = 0, ch;
	const char *indexing = NULL;
	char volume[PATH_MAX];

	while ((ch = getopt(argc, argv, "i:dEsapV:vXL")) != -1) {
		switch (ch) {
		case 'i':
			if (strcmp(optarg, "on") != 0 && strcmp(optarg, "off") != 0)
				usage();
			indexing = optarg;
			break;
		case 'd':
			indexing = "off";
			break;
		case 'E':
			erase = 1;
			break;
		case 'a':
			all = 1;
			break;
		case 's': case 'p': case 'v': case 'X': case 'L':
			break;
		case 'V':
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (all) {
		struct statfs *mounts;
		int count = getmntinfo(&mounts, MNT_NOWAIT);

		for (int i = 0; i < count; i++)
			if (!(mounts[i].f_flags & MNT_DONTBROWSE))
				printf("%s:\n\tIndexing disabled.\n", mounts[i].f_mntonname);
		return indexing != NULL && strcmp(indexing, "on") == 0;
	}
	if (argc == 0)
		usage();
	for (int i = 0; i < argc; i++) {
		if (volumeOf(argv[i], volume, sizeof(volume)) == NULL) {
			warn("%s", argv[i]);
			status = 1;
			continue;
		}
		printf("%s:\n", volume);
		if (indexing != NULL && strcmp(indexing, "on") == 0) {
			printf("\tIndexing could not be enabled: this system has no metadata index yet.\n");
			status = 1;
		} else
			printf("\tIndexing disabled.%s\n", erase ? " (nothing to erase)" : "");
	}
	return status;
}
