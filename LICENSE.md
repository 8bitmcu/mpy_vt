# License and Attribution

## Primary License
**mpy_vt** is licensed under the **MIT License**.

© 2026 Vincent (8bitmcu)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## Third-Party Component Licenses

### 1. st (Suckless Terminal)
The core terminal engine is based on `st`.
**License:** MIT/X Consortium License

© 2014-2022 Hiltjo Posthuma <hiltjo at codemadness dot org>
© 2018 Devin J. Pohly <djpohly at gmail dot com>
© 2014-2017 Quentin Rameau <quinq at fifth dot space>
© 2009-2012 Aurélien APTEL <aurelien dot aptel at gmail dot com>
© 2008-2017 Anselm R Garbe <garbeam at gmail dot com>
© 2012-2017 Roberto E. Vargas Caballero <k0ga at shike2 dot com>
© 2012-2016 Christoph Lohmann <20h at r-36 dot net>
© 2013 Eon S. Jeon <esjeon at hyunmu dot am>
© 2013 Alexander Sedov <alex0player at gmail dot com>
© 2013 Mark Edgar <medgar123 at gmail dot com>
© 2013-2014 Eric Pruitt <eric.pruitt at gmail dot com>
© 2013 Michael Forney <mforney at mforney dot org>
© 2013-2014 Markus Teich <markus dot teich at stusta dot mhn dot de>
© 2014-2015 Laslo Hunhold <dev at frign dot de>

### 2. vi (Toybox)
The `vi` implementation is derived from Toybox.
* **Copyright:** 2015 Rob Landley <rob@landley.net>, 2019 Jarno Mäkipää <jmakip87@gmail.com>.
* **License:** **0BSD** (Zero-Clause BSD).
* **Summary:** Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.

### 3. st7789_mpy
* **Copyright:** Russ Hughes.
* **License:** **MIT License**.

### 4. Terminus Font
* **Copyright:** (c) 2020 Dimitar Zhekov. 
* **License:** **SIL Open Font License 1.1**.

### 5. MicroPython
* **Copyright:** (c) Damien P. George.
* **License:** **MIT License**.

---

### Implementation Note for the T-Deck
While the original `vi` code follows the 0BSD license and the `st` engine follows the MIT/X Consortium license, the specific architectural modifications in this repository—including the **MicroPython VFS Bridge**, **NLR Exception Guarding**, **Zero-allocation Status Bar**, and **GC-safe Memory Management**—are contributed under the project's primary MIT license.
