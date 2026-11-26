# Bundled 9×9 emoji

45 monochrome glyphs based on [Shuqi Khor's Pixel Icons](https://github.com/shuqikhor/pixel-icons),
upstream commit `92a2d8ff3e9149b87404ec099a27fb90d7399bb9`.
Copyright (c) 2023 shuqikhor. The complete MIT license is in [LICENSE](LICENSE).
The SVGs in `svg/` are unmodified upstream files.

This is a selected Unicode mapping, not a complete emoji font. It includes the
pager's bell, waving hand, thumbs, hearts, food, animals, weather, and transport.
Unsupported emojis retain the normal placeholder. Standard Unicode stays on the
wire, so other MeshCore clients display their own emoji fonts.

Adaptations: colours are converted to foreground pixels; dark details and selected
colours are knocked out to preserve detail at one bit per pixel. `mapping.json`
records those choices. The waving-hand bitmap is a local adaptation of the
thumb-up hand/cuff, with an open palm and separated fingers. Equivalent heart
colours share a glyph; spaghetti uses the noodles icon. Variation selectors,
skin-tone modifiers and joiners have zero advance; complex emoji sequences are
not composed into a single glyph.

The generated font is committed as `../fonts/PixelEmoji.c` and the map as `Map.inc`.
Normal firmware builds need no asset download, Python packages, or font encoder.
The mishmesh UI installs these at startup unless a local emoji overlay is already
registered. The picker lists one canonical Unicode character for each glyph.

Regenerate or verify using Python 3's standard library:

```sh
python3 mishmesh/text/pixel-icons/generate.py
python3 mishmesh/text/pixel-icons/generate.py --check --preview /tmp/pixel-icons.png
pio test -e native -f test_mishmesh_pixel_emoji
```

The preview arranges glyphs in `mapping.json` order, nine per row. For a screenshot
from the actual Canvas renderer, set `MISHMESH_EMOJI_PREVIEW=/tmp/pager.pgm` while
running the native test executable. Preserve this license notice when distributing
the artwork or derived firmware.
