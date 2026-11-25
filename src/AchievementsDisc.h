#pragma once

// Installs the CD reader rcheevos uses to identify discs: .chd images are read with libchdr (ChdDisc) and everything
// else (.cue, .iso, .bin) goes to the reader rcheevos brings. Call once, before the first game is identified.
void installDiscReader();
