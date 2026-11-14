#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Minkar's system sounds, made here from sines, glides and envelopes
# (no recordings, nothing from elsewhere): 16-bit stereo AIFF at
# 44.1 kHz. The same script always makes the same files.
#
#   gen-sounds.py OUTDIR

import math
import os
import struct
import sys

RATE = 44100


def envelope(t, attack, decay):
    """A short rise, then an exponential fall."""
    rise = min(1.0, t / attack) if attack > 0 else 1.0
    return rise * math.exp(-t / decay)


def tone(freqs, seconds, decay, attack=0.002, start=0.0):
    """Partials (frequency, level) that ring and fade together."""
    n = int(seconds * RATE)
    out = [0.0] * n
    for i in range(n):
        t = i / RATE
        if t < start:
            continue
        e = envelope(t - start, attack, decay)
        out[i] = e * sum(level * math.sin(2 * math.pi * f * (t - start)) for f, level in freqs)
    return out


def sweep(f0, f1, seconds, decay, attack=0.003):
    """A tone gliding from f0 to f1 (exponentially)."""
    n = int(seconds * RATE)
    out, phase = [], 0.0
    for i in range(n):
        t = i / RATE
        f = f0 * (f1 / f0) ** (t / seconds)
        phase += 2 * math.pi * f / RATE
        out.append(envelope(t, attack, decay) * (math.sin(phase) + 0.25 * math.sin(2 * phase)))
    return out


def glide(f0, f1, seconds, decay, attack):
    """A soft tone falling from f0 to f1, with a touch of its octave."""
    n = int(seconds * RATE)
    out, phase = [], 0.0
    for i in range(n):
        t = i / RATE
        phase += 2 * math.pi * f0 * (f1 / f0) ** (t / seconds) / RATE
        out.append(envelope(t, attack, decay) * (math.sin(phase) + 0.15 * math.sin(2 * phase)))
    return out


def mix(*parts):
    n = max(len(p) for p in parts)
    return [sum(p[i] for p in parts if i < len(p)) for i in range(n)]


def normalize(samples, peak):
    top = max(abs(v) for v in samples) or 1.0
    out = [v * peak / top for v in samples]
    fade = int(0.004 * RATE)    # no click at the end
    for i in range(min(fade, len(out))):
        out[-1 - i] *= i / fade
    return out


def extended(rate):
    """An 80-bit extended float, as AIFF stores its rate."""
    exponent = int(math.floor(math.log2(rate)))
    mantissa = int(rate * 2 ** (63 - exponent))
    return struct.pack('>HQ', exponent + 16383, mantissa)


def write_aiff(path, samples, width=0.12):
    """Stereo, the right channel a few samples later for a little width."""
    delay = int(width * 1e-3 * RATE)
    frames = len(samples) + delay
    data = bytearray()
    for i in range(frames):
        left = samples[i] if i < len(samples) else 0.0
        right = samples[i - delay] if 0 <= i - delay < len(samples) else 0.0
        data += struct.pack('>hh', int(round(left * 32767)), int(round(right * 32767)))
    comm = struct.pack('>hIh', 2, frames, 16) + extended(RATE)
    ssnd = struct.pack('>II', 0, 0) + bytes(data)
    body = b'AIFF' + b'COMM' + struct.pack('>I', len(comm)) + comm + \
        b'SSND' + struct.pack('>I', len(ssnd)) + ssnd
    with open(path, 'wb') as f:
        f.write(b'FORM' + struct.pack('>I', len(body)) + body)


SOUNDS = {
    # The alert (NSBeep): a bright bell, two inharmonic partials.
    'Chime': lambda: tone([(880, 1.0), (1320, 0.45), (2640, 0.12)], 0.6, 0.16),
    # Something to look at: two soft rising notes.
    'Notice': lambda: mix(tone([(659.25, 1.0), (1318.5, 0.2)], 0.45, 0.09),
                          tone([(880, 1.0), (1760, 0.2)], 0.45, 0.11, start=0.11)),
    # Something went wrong: a low, short, rounded buzz.
    'Error': lambda: tone([(196, 1.0), (392, 0.5), (588, 0.3), (784, 0.15)], 0.45, 0.12, attack=0.004),
    # Sent: a quick rise.
    'Sent': lambda: sweep(420, 1400, 0.28, 0.09),
    # Received: two falling notes.
    'Received': lambda: mix(tone([(1046.5, 1.0), (2093, 0.15)], 0.5, 0.08),
                            tone([(783.99, 1.0), (1568, 0.15)], 0.5, 0.12, start=0.09)),
    # Moved to the Trash: a soft tone gliding down, as something dropping.
    'Trash': lambda: glide(760, 190, 0.45, 0.16, attack=0.02),
    # A pop: a tiny falling blip.
    'Pop': lambda: sweep(900, 380, 0.09, 0.025, attack=0.001),
    # A tink: very short and high.
    'Tink': lambda: tone([(2349, 1.0), (4698, 0.25)], 0.16, 0.03, attack=0.001),
}


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else '.'
    os.makedirs(out, exist_ok=True)
    for name, make in SOUNDS.items():
        write_aiff(os.path.join(out, name + '.aiff'), normalize(make(), 0.45))


if __name__ == '__main__':
    main()
