// tweaks.h - Save/load ImGui debug tweaks to JSON
//
// Persists rain, night, and collider parameters so they
// survive game restarts during development.

#pragma once

// Save current tweak values to assets/tweaks.json.
void tweaks_save(void);

// Load tweak values from assets/tweaks.json (if it exists).
void tweaks_load(void);
