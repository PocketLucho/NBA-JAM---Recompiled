NBA JAM Recomp - overlays (borders)
===================================
Pictures shown around the 4:3 game picture on a wider screen. Press F8 in a game
to go through: black, blurred picture, then every image in this folder.

Use your own
------------
Put a PNG here. It is scaled to the height of the screen, keeping its shape, and
centred. The usual border is a 16:9 image (for example 1920x1080) with a
transparent 4:3 opening in the middle (1440x1080, starting 240 pixels from the
left): the game shows through the opening and the border is drawn over it, so
the border can also overlap the edge of the picture (a rounded tube, a shadow).
A PNG without any transparency is drawn behind the game instead.

RetroArch overlays
------------------
Border overlays made for RetroArch work: copy the .cfg and its .png here. The
image named by overlay0_overlay in the .cfg is used; button layouts in the file
are ignored. PNGs must be 8 bits per channel and not interlaced.

Settings (nbajam_recomp.ini): side_fill=overlay, overlay=<name without .png>,
overlay_opacity=0.1 to 1.

Use scaling "Fit" or "Smooth" (F5): the opening lines up with a picture that
fills the height of the screen. With "Integer" the picture is smaller than the
opening.

The two images that come with the program (cabinet_purple, tube_shadow) were
drawn for this project. Do not put game art in images you share.
