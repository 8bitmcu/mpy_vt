#!/bin/sh

rm ../modules/scripts/fonts/*


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
python3 fontconvert.py \
  ../fonts/Tamzen/Tamzen6x12r.ttf \
  -b ../fonts/Tamzen/Tamzen6x12b.ttf \
  -u 12 > ../modules/scripts/fonts/tamzen_mpy_11.py


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
