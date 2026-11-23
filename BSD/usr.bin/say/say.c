/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * say: speaks text, with the reference's command's options:
 *
 *   say [-v voice] [-r rate] [-o out] [-f in | message]
 *
 * The voice is made by eSpeak NG (/usr/libexec/espeak-ng, GPL-3.0, a
 * process of its own: the maintainer's choice, 2026-11-23), which writes it
 * as WAV; say plays it (afplay) or keeps it (-o: AIFF, or WAV for a .wav
 * name). The voices are eSpeak NG's languages; -v '?' lists them in the
 * reference's layout, and -v takes a voice's name, a language (es) or a
 * locale (es_MX). Without -v, the user's first preferred language speaks.
 * The message is the arguments, else the file -f names ('-': standard
 * input), else standard input.
 */

#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define ESPEAK		"/usr/libexec/espeak-ng"
#define AFPLAY		"/usr/bin/afplay"

extern char **environ;

static void
usage(void)
{
	fprintf(stderr, "Usage: say [-v voice] [-r rate] [-o out] [-f in | message]\n");
	exit(1);
}

/* A program's standard output, whole (NULL if it cannot run). */
static char *
capture(const char *const argv[], size_t *length)
{
	int out[2];
	pid_t pid;
	posix_spawn_file_actions_t actions;
	char *buffer = NULL;
	size_t size = 0, used = 0;
	ssize_t got;
	int status;

	if (pipe(out) != 0)
		return NULL;
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_adddup2(&actions, out[1], 1);
	posix_spawn_file_actions_addclose(&actions, out[0]);
	posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
	if (posix_spawn(&pid, argv[0], &actions, NULL, (char *const *)argv, environ) != 0) {
		posix_spawn_file_actions_destroy(&actions);
		close(out[0]);
		close(out[1]);
		return NULL;
	}
	posix_spawn_file_actions_destroy(&actions);
	close(out[1]);
	for (;;) {
		if (used + 4096 > size) {
			size = size ? size * 2 : 65536;
			if ((buffer = realloc(buffer, size)) == NULL)
				err(1, NULL);
		}
		if ((got = read(out[0], buffer + used, size - used - 1)) <= 0)
			break;
		used += (size_t)got;
	}
	close(out[0]);
	waitpid(pid, &status, 0);
	if (buffer != NULL)
		buffer[used] = '\0';
	*length = used;
	return buffer;
}

/* A sentence for the voices list, in the languages Minkar's desktop has. */
static const char *
sample(const char *language)
{
	if (strncmp(language, "es", 2) == 0)
		return "Hola, soy una voz en español.";
	if (strncmp(language, "en", 2) == 0)
		return "Hello, I am an English voice.";
	if (strncmp(language, "fr", 2) == 0)
		return "Bonjour, je suis une voix française.";
	if (strncmp(language, "de", 2) == 0)
		return "Hallo, ich bin eine deutsche Stimme.";
	if (strncmp(language, "it", 2) == 0)
		return "Ciao, sono una voce italiana.";
	if (strncmp(language, "pt", 2) == 0)
		return "Olá, eu sou uma voz em português.";
	return "";
}

/* A language code as a locale: en-us as en_US, es-419 as es_419. */
static void
localeOf(const char *language, char *out, size_t size)
{
	size_t i;

	for (i = 0; language[i] != '\0' && i + 1 < size; i++) {
		char c = language[i];
		out[i] = c;
		if (c == '-') {
			out[i] = '_';
			for (size_t k = i + 1; language[k] != '\0' && k + 1 < size; k++)
				out[k] = (char)toupper((unsigned char)language[k]);
			i = strlen(language);
			if (i >= size)
				i = size - 1;
			break;
		}
	}
	out[i] = '\0';
}

typedef struct {
	char language[32];
	char name[64];
} Voice;

/* eSpeak NG's voices: its --voices table (Pty Language Age/Gender
   VoiceName File Other Languages). */
static int
loadVoices(Voice **voices)
{
	const char *argv[] = { ESPEAK, "--voices", NULL };
	size_t length;
	char *text = capture(argv, &length), *line, *rest;
	int count = 0;

	*voices = NULL;
	if (text == NULL)
		return 0;
	rest = text;
	while ((line = strsep(&rest, "\n")) != NULL) {
		char pty[8], language[32], gender[16], name[64];
		if (sscanf(line, "%7s %31s %15s %63s", pty, language, gender, name) != 4 || !isdigit((unsigned char)pty[0]))
			continue;
		*voices = realloc(*voices, sizeof(Voice) * (size_t)(count + 1));
		if (*voices == NULL)
			err(1, NULL);
		snprintf((*voices)[count].language, sizeof((*voices)[count].language), "%s", language);
		for (char *c = name; *c != '\0'; c++)
			if (*c == '_')
				*c = ' ';
		snprintf((*voices)[count].name, sizeof((*voices)[count].name), "%s", name);
		count++;
	}
	free(text);
	return count;
}

static void
listVoices(void)
{
	Voice *voices;
	int count = loadVoices(&voices);
	char locale[40];

	if (count == 0)
		errx(1, "no voices: the speech synthesizer is missing");
	for (int i = 0; i < count; i++) {
		localeOf(voices[i].language, locale, sizeof(locale));
		printf("%-19s %-8s # %s\n", voices[i].name, locale, sample(voices[i].language));
	}
	free(voices);
}

/* The voice asked for, as eSpeak NG's language: a voice's name, a
   language or a locale (its language when the locale is not a voice). */
static void
chooseVoice(const char *wanted, char *language, size_t size)
{
	Voice *voices;
	int count = loadVoices(&voices);
	char asked[64];
	size_t i;

	for (i = 0; wanted[i] != '\0' && i + 1 < sizeof(asked); i++)
		asked[i] = wanted[i] == '_' ? '-' : (char)tolower((unsigned char)wanted[i]);
	asked[i] = '\0';
	for (int k = 0; k < count; k++)
		if (strcasecmp(voices[k].name, wanted) == 0 || strcmp(voices[k].language, asked) == 0) {
			snprintf(language, size, "%s", voices[k].language);
			free(voices);
			return;
		}
	/* es-mx: es */
	char *dash = strchr(asked, '-');
	if (dash != NULL) {
		*dash = '\0';
		for (int k = 0; k < count; k++)
			if (strcmp(voices[k].language, asked) == 0) {
				snprintf(language, size, "%s", voices[k].language);
				free(voices);
				return;
			}
	}
	free(voices);
	errx(1, "Voice `%s' not found.", wanted);
}

/* The user's first preferred language, as a voice. */
static void
defaultVoice(char *language, size_t size)
{
	CFArrayRef preferred = CFLocaleCopyPreferredLanguages();
	char code[32] = "en";

	if (preferred != NULL && CFArrayGetCount(preferred) > 0)
		CFStringGetCString(CFArrayGetValueAtIndex(preferred, 0), code, sizeof(code), kCFStringEncodingUTF8);
	if (preferred != NULL)
		CFRelease(preferred);
	for (char *c = code; *c != '\0'; c++)
		if (*c == '_')
			*c = '-';
	/* "es-ES" as eSpeak NG's "es", "en-US" as "en-us". */
	if (strncmp(code, "en-US", 5) == 0 || strcmp(code, "en") == 0)
		snprintf(language, size, "en-us");
	else {
		char *dash = strchr(code, '-');
		if (dash != NULL)
			*dash = '\0';
		snprintf(language, size, "%s", code);
	}
}

/* The text to speak: all of a file or of standard input. */
static char *
readAll(FILE *file)
{
	size_t size = 0, used = 0, got;
	char *buffer = NULL;

	do {
		if (used + 4096 > size) {
			size = size ? size * 2 : 8192;
			if ((buffer = realloc(buffer, size)) == NULL)
				err(1, NULL);
		}
		got = fread(buffer + used, 1, size - used - 1, file);
		used += got;
	} while (got > 0);
	buffer[used] = '\0';
	return buffer;
}

/* The voice speaking text, as WAV, into path. */
static void
synthesize(const char *language, const char *rate, const char *text, const char *path)
{
	int in[2], out;
	pid_t pid;
	posix_spawn_file_actions_t actions;
	const char *argv[8];
	int n = 0, status;

	argv[n++] = ESPEAK;
	argv[n++] = "--stdout";
	argv[n++] = "-v";
	argv[n++] = language;
	if (rate != NULL) {
		argv[n++] = "-s";
		argv[n++] = rate;
	}
	argv[n] = NULL;
	if ((out = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
		err(1, "%s", path);
	if (pipe(in) != 0)
		err(1, "pipe");
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_adddup2(&actions, in[0], 0);
	posix_spawn_file_actions_adddup2(&actions, out, 1);
	posix_spawn_file_actions_addclose(&actions, in[1]);
	if ((errno = posix_spawn(&pid, ESPEAK, &actions, NULL, (char *const *)argv, environ)) != 0)
		err(1, "%s", ESPEAK);
	posix_spawn_file_actions_destroy(&actions);
	close(in[0]);
	close(out);
	size_t length = strlen(text);
	for (size_t done = 0; done < length;) {
		ssize_t w = write(in[1], text + done, length - done);
		if (w <= 0)
			break;
		done += (size_t)w;
	}
	close(in[1]);
	waitpid(pid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		errx(1, "the speech synthesizer failed");
}

/* The WAV as an AIFF file: the same samples, big-endian. */
static void
toAIFF(const char *wav, const char *aiff)
{
	CFURLRef in = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)wav, (CFIndex)strlen(wav), false);
	CFURLRef out = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)aiff, (CFIndex)strlen(aiff), false);
	ExtAudioFileRef source = NULL, destination = NULL;
	AudioStreamBasicDescription format, file;
	UInt32 size = sizeof(format);
	char buffer[32768];

	if (ExtAudioFileOpenURL(in, &source) != noErr ||
	    ExtAudioFileGetProperty(source, kExtAudioFileProperty_FileDataFormat, &size, &format) != noErr)
		errx(1, "cannot read the voice's samples");
	file = format;
	file.mFormatFlags = kAudioFormatFlagIsBigEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	if (ExtAudioFileCreateWithURL(out, kAudioFileAIFFType, &file, NULL, kAudioFileFlags_EraseFile, &destination) != noErr)
		errx(1, "%s: cannot write it", aiff);
	ExtAudioFileSetProperty(source, kExtAudioFileProperty_ClientDataFormat, sizeof(format), &format);
	ExtAudioFileSetProperty(destination, kExtAudioFileProperty_ClientDataFormat, sizeof(format), &format);
	for (;;) {
		AudioBufferList list;
		UInt32 frames = sizeof(buffer) / format.mBytesPerFrame;
		list.mNumberBuffers = 1;
		list.mBuffers[0].mNumberChannels = format.mChannelsPerFrame;
		list.mBuffers[0].mDataByteSize = sizeof(buffer);
		list.mBuffers[0].mData = buffer;
		if (ExtAudioFileRead(source, &frames, &list) != noErr || frames == 0)
			break;
		if (ExtAudioFileWrite(destination, frames, &list) != noErr)
			errx(1, "%s: cannot write it", aiff);
	}
	ExtAudioFileDispose(source);
	ExtAudioFileDispose(destination);
	CFRelease(in);
	CFRelease(out);
}

static int
endsWith(const char *s, const char *suffix)
{
	size_t a = strlen(s), b = strlen(suffix);
	return a >= b && strcasecmp(s + a - b, suffix) == 0;
}

int
main(int argc, char *argv[])
{
	const char *voice = NULL, *rate = NULL, *output = NULL, *input = NULL;
	char language[32], wav[PATH_MAX];
	char *text;
	int ch;

	static struct option long_options[] = {
		{ "voice", required_argument, NULL, 'v' },
		{ "rate", required_argument, NULL, 'r' },
		{ "output-file", required_argument, NULL, 'o' },
		{ "input-file", required_argument, NULL, 'f' },
		{ "file-format", required_argument, NULL, 1 },
		{ "data-format", required_argument, NULL, 1 },
		{ "channels", required_argument, NULL, 1 },
		{ "bit-rate", required_argument, NULL, 1 },
		{ "quality", required_argument, NULL, 1 },
		{ "audio-device", required_argument, NULL, 'a' },
		{ "interactive", optional_argument, NULL, 'i' },
		{ "progress", no_argument, NULL, 1 },
		{ NULL, 0, NULL, 0 },
	};
	while ((ch = getopt_long(argc, argv, "v:r:o:f:a:i", long_options, NULL)) != -1) {
		switch (ch) {
		case 'v': voice = optarg; break;
		case 'r': rate = optarg; break;
		case 'o': output = optarg; break;
		case 'f': input = optarg; break;
		case 'a': case 'i': case 1: break;
		default: usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (voice != NULL && strcmp(voice, "?") == 0) {
		listVoices();
		return 0;
	}
	if (access(ESPEAK, X_OK) != 0)
		errx(1, "the speech synthesizer (%s) is missing", ESPEAK);
	if (voice != NULL)
		chooseVoice(voice, language, sizeof(language));
	else
		defaultVoice(language, sizeof(language));

	if (argc > 0) {
		size_t length = 1;
		for (int i = 0; i < argc; i++)
			length += strlen(argv[i]) + 1;
		if ((text = calloc(1, length)) == NULL)
			err(1, NULL);
		for (int i = 0; i < argc; i++) {
			strcat(text, argv[i]);
			if (i + 1 < argc)
				strcat(text, " ");
		}
	} else if (input != NULL && strcmp(input, "-") != 0) {
		FILE *file = fopen(input, "r");
		if (file == NULL)
			err(1, "%s", input);
		text = readAll(file);
		fclose(file);
	} else
		text = readAll(stdin);

	if (output != NULL && (endsWith(output, ".wav") || endsWith(output, ".wave"))) {
		synthesize(language, rate, text, output);
		return 0;
	}
	snprintf(wav, sizeof(wav), "%s/say.XXXXXX.wav", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
	int fd = mkstemps(wav, 4);
	if (fd < 0)
		err(1, "%s", wav);
	close(fd);
	synthesize(language, rate, text, wav);
	if (output != NULL)
		toAIFF(wav, output);
	else {
		const char *play[] = { AFPLAY, wav, NULL };
		pid_t pid;
		int status;
		if (posix_spawn(&pid, AFPLAY, NULL, NULL, (char *const *)play, environ) == 0)
			waitpid(pid, &status, 0);
	}
	unlink(wav);
	return 0;
}
