#!/usr/bin/env python3
"""Compare GPU AA outputs against a 4x-resolution reference, in linear RGB.

Run after: encore test --filter anti_aliasing_gpu
Uses only the Python standard library. Raw BGRA files are written by the GPU test.
"""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1] / 'target' / 'aa'
METHODS = ('off', 'fxaa', 'smaa', 'msaa2', 'msaa4', 'taa')
WIDTH, HEIGHT = 256, 192

def load(name, width=WIDTH, height=HEIGHT):
    data = (ROOT / (name + '.bgra')).read_bytes()
    assert len(data) == width * height * 4, f'incomplete image: {name}'
    return data

def srgb(value):
    value /= 255
    return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4

LINEAR = tuple(srgb(i) for i in range(256))
images = {method: load('headless-' + method) for method in METHODS}
reference = load('reference-4x', WIDTH * 4, HEIGHT * 4)
foreground = []
for y in range(HEIGHT):
    for x in range(WIDTH):
        pixel = images['off'][(y * WIDTH + x) * 4: (y * WIDTH + x) * 4 + 3]
        if min(pixel) > 90 and max(pixel) - min(pixel) < 25:
            foreground.append((x, y))
assert len(foreground) > 20, 'the fixture must contain a visible object'
x0 = max(min(x for x, _ in foreground) - 3, 0)
y0 = max(min(y for _, y in foreground) - 3, 0)
x1 = min(max(x for x, _ in foreground) + 4, WIDTH)
y1 = min(max(y for _, y in foreground) + 4, HEIGHT)
errors = dict.fromkeys(METHODS, 0.)
for y in range(y0, y1):
    for x in range(x0, x1):
        for channel in range(3):
            ideal = sum(LINEAR[reference[((y * 4 + dy) * WIDTH * 4 + x * 4 + dx) * 4 + channel]]
                        for dy in range(4) for dx in range(4)) / 16
            offset = (y * WIDTH + x) * 4 + channel
            for method in METHODS:
                errors[method] += (LINEAR[images[method][offset]] - ideal) ** 2
samples = (x1 - x0) * (y1 - y0) * 3
errors = {method: total / samples for method, total in errors.items()}
assert errors['off'] > 0
for method in METHODS[1:]:
    assert errors[method] < errors['off'] * .95, f'{method} fails to improve the silhouette: {errors}'
report = {'reference': '4x resolution / 16 raster samples, linear RGB',
          'bounds': [x0, y0, x1, y1], 'mse': errors,
          'improvement_percent': {k: 100 * (1 - v / errors['off']) for k, v in errors.items()}}
(ROOT / 'image-quality.json').write_text(json.dumps(report, indent=2) + '\n')
print('All AA methods improve silhouette error against the supersampled reference.')
