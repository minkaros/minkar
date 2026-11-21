# ADR 0005: Audio and media

How Minkar plays and records audio and decodes media, for anyone working on the audio
family, the audio service or the media frameworks. Status: accepted, 2026-11-09 (the audio
family on 2026-11-14, codecs in the app's process on 2026-11-16); recorded 2026-11-21.

## Context

WebKit, Firefox and the designated players need CoreAudio, AudioToolbox, CoreMedia,
AVFoundation and VideoToolbox with the reference platform's API. The reference platform's
audio family is published only under the APSL 1.1, which Minkar does not take in. The
common open source stacks (GStreamer, GTK, Qt) bring their own models and licenses.

## Decision

- **No GTK, Qt or GStreamer.** WebKit's audio and video go through Minkar's own frameworks.
- **The kernel:** IOAudioFamily is Minkar's own (BSD-2) with the family's public API, and
  MinkarHDA drives HDA codecs (the codec handling ported from FreeBSD). No mixing in the
  kernel: an engine user client maps the hardware buffer to the audio service.
- **The audio service:** `audiod` owns the devices and mixes its clients in the same cycle,
  on real-time threads, from shared memory, with no locks or allocations on the audio
  path. Sample rates, channels and formats are not fixed; each device has its own clock
  for drift correction and aggregate devices; buffers go down to 32 frames.
- **Decoding:** FFmpeg (LGPL 2.1, a dynamic library cut down to decoders, demuxers and
  parsers) and dav1d (BSD-2), pinned by SHA-256 in `/usr/lib/media`. AVFoundation's
  playback decodes in `mediadecoder`, one process per session, which receives the open
  file; AudioConverter and VideoToolbox load FFmpeg in the app's own process.

## Consequences

- Audio is professional from the start: AU7–AU10 (USB audio, MIDI, Audio Units, aggregate
  devices) build on the same core ([ROADMAP.md](../ROADMAP.md#audio-and-media)).
- Audio over HDMI and DisplayPort belongs to the display drivers ([GPU.md](../GPU.md)).
- FFmpeg stays a separate dynamic library under its own license.
