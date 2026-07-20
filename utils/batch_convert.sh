#!/bin/sh

rm ../modules/scripts/fonts/*

touch ../modules/scripts/fonts/__init__.py


# Terminus
TERMINUS_LICENSE_HEADER='"""
Terminus Font License
(c) 2020 Dimitar Zhekov.
Licensed under the SIL Open Font License 1.1
"""
'
for size in 12 14; do
  (
    echo "$TERMINUS_LICENSE_HEADER"
    python3 fontconvert.py \
      ../fonts/terminus-ttf-4.49.3/TerminusTTF-4.49.3.ttf \
      -b ../fonts/terminus-ttf-4.49.3/TerminusTTF-Bold-4.49.3.ttf \
      -i ../fonts/terminus-ttf-4.49.3/TerminusTTF-Italic-4.49.3.ttf \
      -bi ../fonts/terminus-ttf-4.49.3/TerminusTTF-Bold-Italic-4.49.3.ttf \
      -u "$size"
  ) > "../modules/scripts/fonts/terminus_mpy_${size}.py"
done


# Cozette
COZETTE_LICENSE_HEADER='"""
Cozette Font License
Copyright (c) 2020 Samhain <samhain@moonwit.ch> & contributors <https://github.com/the-moonwitch/Cozette/contributors>
Distributed under the terms of the MIT License.
"""
'
(
  echo "$COZETTE_LICENSE_HEADER"
  python3 fontconvert.py \
    ../fonts/cozette/CozetteVector.ttf \
    -b ../fonts/cozette/CozetteVectorBold.ttf \
    -u 13
) > "../modules/scripts/fonts/cozette_mpy_13.py"


# Tamzen
TAMZEN_LICENSE_HEADER='"""
Tamzen Font License
Copyright 2011 Suraj N. Kurapati <https://github.com/sunaku/tamzen-font>

Tamzen font is free. You are hereby granted permission to use, copy, 
modify, and distribute it as you see fit.

Tamzen font is provided "as is" without any express or implied warranty. 
The author makes no representations about the suitability of this font 
for a particular purpose. In no event will the author be held liable 
for damages arising from the use of this font.
"""
'
(
  echo "$TAMZEN_LICENSE_HEADER"
  python3 fontconvert.py \
    ../fonts/Tamzen/Tamzen6x12r.ttf \
    -b ../fonts/Tamzen/Tamzen6x12b.ttf \
    -u 12
) > "../modules/scripts/fonts/tamzen_mpy_11.py"


# Gohu
GOHU_LICENSE_HEADER='"""
Gohu Font License
Copyright 2015 by Hugo Chargois
Distributed under the terms of the WTFPL version 2.
"""
'
for size in 11 14; do
  (
    echo "$GOHU_LICENSE_HEADER"
    python3 fontconvert.py \
      ../fonts/gohu/GohuFont-Medium.ttf \
      -b ../fonts/gohu/GohuFont-Bold.ttf \
      -u "$size"
  ) > "../modules/scripts/fonts/gohu_mpy_${size}.py"
done


# Spleen
SPLEEN_LICENSE_HEADER='"""
Spleen Font License
Copyright (c) 2018-2026, Frédéric Cambus.
Licensed under the BSD 2-Clause License.
"""
'
(
  echo "$SPLEEN_LICENSE_HEADER"
  python3 fontconvert.py \
    ../fonts/spleen/spleen-5x8.bdf \
    -u 8
) > "../modules/scripts/fonts/spleen_mpy_8.py"

(
  echo "$SPLEEN_LICENSE_HEADER"
  python3 fontconvert.py \
    ../fonts/spleen/spleen-6x12.bdf \
    -u 12
) > "../modules/scripts/fonts/spleen_mpy_12.py"


# Scientifica
SCIENTIFICA_LICENSE_HEADER='"""
Scientifica Font License
Copyright (c) 2020 Akshay Oppiliappan <nerdy@peppe.rs>
Licensed under the SIL Open Font License 1.1
"""
'
(
  echo "$SCIENTIFICA_LICENSE_HEADER"
  python3 fontconvert.py \
    ../fonts/scientifica/ttf/scientifica.ttf \
    -b ../fonts/scientifica/ttf/scientificaBold.ttf \
    -i ../fonts/scientifica/ttf/scientificaItalic.ttf \
    -u 11
) > "../modules/scripts/fonts/scientifica_mpy_10.py"


# Unifont (Chess Game)
# NOTE: Unifont mixes 8px-wide glyphs (Latin/ASCII, most of what CHARACTERS
# covers) with 16px-wide glyphs (CJK, and most symbol/pictographic blocks --
# including the chess pieces we actually want this font for). This project's
# renderer has no single-width concept of per-glyph width, so the base font
# is forced to width=8 for correctly-sized, non-wasteful ASCII text, and the
# chess glyphs are pulled separately via --wide-unicode into a WIDE_FONT
# block rendered at 2x that width (16px) -- st.c's st_wcwidth() marks those
# codepoints ATTR_WIDE so the terminal reserves two cells for them, and
# fb.c's xdrawline() renders WIDE_FONT glyphs across both.
UNIFONT_LICENSE_HEADER='"""
GNU Unifont License
Copyright Roman Czyborra, Paul Hardy, and contributors.
Dual-licensed under the SIL Open Font License 1.1 and GNU GPL v2+ with the
GNU Font Embedding Exception; used here under the SIL Open Font License 1.1.
"""
'
(
  echo "$UNIFONT_LICENSE_HEADER"
  python3 fontconvert.py \
    ../fonts/unifont/unifont-17.0.05.bdf \
    -u 16 --force-width 8 --wide-unicode
) > "../modules/scripts/fonts/unifont_mpy_16.py"


# Siji
# NOTE: GPLv2, no font-embedding exception (unlike Unifont) -- including
# this makes the compiled firmware a GPLv2 combined work, same as frotz.
# See README.md / LICENSE.md.
SIJI_LICENSE_HEADER='"""
Siji Font License
(c) stark and contributors. Based on Stlarch, with glyphs drawn from
FontAwesome and other icon packs.
Licensed under the GNU General Public License v2.0.
"""
'
(
  echo "$SIJI_LICENSE_HEADER"
  python3 fontconvert.py \
    ../fonts/siji/siji.bdf \
    -u 12 --icons --wide-unicode
) > "../modules/scripts/fonts/siji_mpy_12.py"

# Curated 6-icon set (clock, speaker, wifi, battery, bluetooth, mem) --
# see STATUSBAR_ICONS in fontconvert.py. This is the one actually meant
# for real use (e.g. statusbar.py); siji_mpy_12/10 above carry the full
# ~400-icon set and exist for browsing/testing via icontest.py.
(
  echo "$SIJI_LICENSE_HEADER"
  python3 fontconvert.py \
    ../fonts/siji/siji.bdf \
    -u 12 --icons --wide-unicode --statusbar-icons
) > "../modules/scripts/fonts/siji_mpy_statusbar_12.py"
