<!--
Copyright (C) 2009 - 2021 Marc Vester (XaserLE)
Copyright (C) 2009 - 2021 Settlers Freaks <sf-team at siedler25.org>

SPDX-License-Identifier: GPL-3.0-or-later
-->

# Return To The Roots Mapeditor

## About
------
This is a re-engineered version of the Bluebytes (C) Siedler 2 Gold (C) Mapeditor.

Used librarys: SDL and a modified version of SGE.

## Installation
-------------
Compile as part of [RTTR](https://github.com/Return-To-The-Roots/s25client/).
Put your Siedler 2 Gold (C) files in the S2 folder created (there is a file "put your S2-Installation in here").
Don't create a subfolder. DATA and GFX have to be in the same directory as the above file.

## Settings
--------
The editor stores its settings (also changeable under "Options" in the main menu) in `s25edit.ini` in the RTTR user
data directory, with the same sections and names as the game in `CONFIG.INI`:

- `[video]`
  - `windowed_width`, `windowed_height`: Size of the window in window mode, in pixels at 100% display scaling (also
    kept in fullscreen)
  - `displayMode`: `0` for window mode, `1` for fullscreen (using the native resolution of the display)
  - `gui_scale`: GUI scale in percent, `0` for automatic (the display scaling). It is limited so that at least 800x600
    remain visible.
- `[interface]`
  - `map_scroll_mode`: How the map scrolls with the right mouse button: `1` keeps the cursor in place, which moves the
    cursor back to its position after every movement. This doesn't work with graphics tablets or VMs with mouse
    integration, which report the position of the device. `2` drags the map with the cursor instead.

Older versions stored `width`, `height` and `fullscreen` in the section `[editor]`, which is still read.


## License
--------

Originally created by Marc Vester (XaserLE) on [Launchpad](https://code.launchpad.net/~xaserle/s25rttr/s25edit) and released under GPLv2 or later.

Adapted and integrated into Return to the Roots in 2017 and re-released under GPLv3.

Original License:
```
Copyright (c) 2009 - 2012 Marc Vester (XaserLE)

This file is part of Return to the Roots Mapeditor.

Return To The Roots Mapeditor is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

Return To The Roots Mapeditor is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Return To The Roots Mapeditor. If not, see <http://www.gnu.org/licenses/>.
```
