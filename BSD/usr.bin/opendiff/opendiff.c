/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * opendiff: shows two files' differences, with the reference's command's
 * arguments:
 *
 *   opendiff file1 file2 [-ancestor ancestorFile] [-merge mergeFile]
 *
 * Minkar has no comparison app yet (the maintainer's choice, 2026-11-23):
 * the differences open in Text Editor, as a unified diff it highlights
 * (from diff -u, or diff -ru for two directories). With -merge, the files
 * are merged into mergeFile, which opens instead: with -ancestor, by diff3
 * -m (each side's changes kept, a conflict marked); without one, every
 * difference is marked as a conflict (<<<<<<< file1, =======, >>>>>>>
 * file2) for the user to resolve.
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdint.h>
#include <unistd.h>

extern char **environ;

static void
usage(void)
{
	fprintf(stderr, "usage: opendiff <file1> <file2> [-ancestor <ancestorFile>] [-merge <mergeFile>]\n");
	exit(1);
}

/* A program's output into a file; its exit status. */
static int
runInto(const char *const argv[], const char *path)
{
	posix_spawn_file_actions_t actions;
	pid_t pid;
	int status;

	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_addopen(&actions, 1, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if ((errno = posix_spawn(&pid, argv[0], &actions, NULL, (char **)(uintptr_t)argv, environ)) != 0)
		err(1, "%s", argv[0]);
	posix_spawn_file_actions_destroy(&actions);
	if (waitpid(pid, &status, 0) < 0)
		err(1, "waitpid");
	return WIFEXITED(status) ? WEXITSTATUS(status) : 2;
}

static void
openInEditor(const char *path)
{
	const char *argv[] = { "/usr/bin/open", "-a", "TextEditor", path, NULL };
	pid_t pid;
	int status;

	if ((errno = posix_spawn(&pid, argv[0], NULL, NULL, (char **)(uintptr_t)argv, environ)) != 0)
		err(1, "open");
	waitpid(pid, &status, 0);
}

typedef struct {
	char **lines;
	size_t count;
} Lines;

static Lines
readLines(const char *path)
{
	Lines result = { NULL, 0 };
	FILE *file = fopen(path, "r");
	char *line = NULL;
	size_t size = 0;
	ssize_t length;

	if (file == NULL)
		err(1, "%s", path);
	while ((length = getline(&line, &size, file)) >= 0) {
		result.lines = realloc(result.lines, sizeof(char *) * (result.count + 1));
		if (result.lines == NULL)
			err(1, NULL);
		result.lines[result.count++] = strdup(line);
	}
	free(line);
	fclose(file);
	return result;
}

static void
writeLines(FILE *out, Lines *lines, size_t from, size_t to)
{
	for (size_t i = from; i < to && i < lines->count; i++) {
		fputs(lines->lines[i], out);
		size_t length = strlen(lines->lines[i]);
		if (length == 0 || lines->lines[i][length - 1] != '\n')
			fputc('\n', out);
	}
}

/* Without an ancestor: diff's hunks (normal output: "Lc R", "La R",
   "Ld R") as conflicts between the two files' lines, the rest as is. */
static void
markConflicts(const char *left, const char *right, const char *diffOutput, const char *merge)
{
	Lines a = readLines(left), b = readLines(right), hunks = readLines(diffOutput);
	FILE *out = fopen(merge, "w");
	size_t at = 0;

	if (out == NULL)
		err(1, "%s", merge);
	for (size_t h = 0; h < hunks.count; h++) {
		char *line = hunks.lines[h], *p = line, kind;
		long l1, l2, r1, r2;

		if (line[0] < '0' || line[0] > '9')
			continue;
		l1 = strtol(p, &p, 10);
		l2 = *p == ',' ? strtol(p + 1, &p, 10) : l1;
		kind = *p++;
		r1 = strtol(p, &p, 10);
		r2 = *p == ',' ? strtol(p + 1, &p, 10) : r1;
		/* The left lines the hunk replaces (none for an addition, after
		   line l1). */
		size_t leftFrom = kind == 'a' ? (size_t)l1 : (size_t)l1 - 1;
		size_t leftTo = kind == 'a' ? (size_t)l1 : (size_t)l2;
		size_t rightFrom = kind == 'd' ? 0 : (size_t)r1 - 1;
		size_t rightTo = kind == 'd' ? 0 : (size_t)r2;

		writeLines(out, &a, at, leftFrom);
		fprintf(out, "<<<<<<< %s\n", left);
		writeLines(out, &a, leftFrom, leftTo);
		fputs("=======\n", out);
		writeLines(out, &b, rightFrom, rightTo);
		fprintf(out, ">>>>>>> %s\n", right);
		at = leftTo;
	}
	writeLines(out, &a, at, a.count);
	fclose(out);
}

int
main(int argc, char *argv[])
{
	const char *files[2] = { NULL, NULL }, *ancestor = NULL, *merge = NULL;
	int fileCount = 0;
	char diffPath[PATH_MAX];
	struct stat st1, st2;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-ancestor") == 0 && i + 1 < argc)
			ancestor = argv[++i];
		else if (strcmp(argv[i], "-merge") == 0 && i + 1 < argc)
			merge = argv[++i];
		else if (argv[i][0] == '-' && argv[i][1] != '\0')
			usage();
		else if (fileCount < 2)
			files[fileCount++] = argv[i];
		else
			usage();
	}
	if (fileCount != 2)
		usage();
	if (stat(files[0], &st1) != 0)
		err(1, "%s", files[0]);
	if (stat(files[1], &st2) != 0)
		err(1, "%s", files[1]);
	if (ancestor != NULL && access(ancestor, R_OK) != 0)
		err(1, "%s", ancestor);

	if (merge != NULL) {
		if (S_ISDIR(st1.st_mode) || S_ISDIR(st2.st_mode))
			errx(1, "-merge takes two files, not directories");
		if (ancestor != NULL) {
			const char *diff3[] = { "/usr/bin/diff3", "-m", files[0], ancestor, files[1], NULL };
			if (runInto(diff3, merge) > 1)
				errx(1, "diff3 could not merge the files");
		} else {
			char hunks[PATH_MAX];
			snprintf(hunks, sizeof(hunks), "%s/opendiff.XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
			int fd = mkstemp(hunks);
			if (fd < 0)
				err(1, "%s", hunks);
			close(fd);
			const char *diff[] = { "/usr/bin/diff", files[0], files[1], NULL };
			if (runInto(diff, hunks) > 1)
				errx(1, "diff could not compare the files");
			markConflicts(files[0], files[1], hunks, merge);
			unlink(hunks);
		}
		openInEditor(merge);
		return 0;
	}

	/* The differences, in a file named after the two, for Text Editor. */
	const char *base1 = strrchr(files[0], '/') ? strrchr(files[0], '/') + 1 : files[0];
	const char *base2 = strrchr(files[1], '/') ? strrchr(files[1], '/') + 1 : files[1];
	char folder[PATH_MAX];
	snprintf(folder, sizeof(folder), "%s/opendiff.XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
	if (mkdtemp(folder) == NULL)
		err(1, "%s", folder);
	/* A folder of its own: the window is named after the two files. */
	snprintf(diffPath, sizeof(diffPath), "%s/%s vs %s.diff", folder, base1, base2);
	int status;
	if (S_ISDIR(st1.st_mode) || S_ISDIR(st2.st_mode)) {
		const char *diff[] = { "/usr/bin/diff", "-ru", files[0], files[1], NULL };
		status = runInto(diff, diffPath);
	} else {
		const char *diff[] = { "/usr/bin/diff", "-u", files[0], files[1], NULL };
		status = runInto(diff, diffPath);
	}
	if (status > 1)
		errx(1, "diff could not compare the files");
	if (status == 0) {
		FILE *out = fopen(diffPath, "w");
		if (out != NULL) {
			fprintf(out, "%s and %s are the same.\n", files[0], files[1]);
			fclose(out);
		}
	}
	openInEditor(diffPath);
	return 0;
}
