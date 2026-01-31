#ifndef ASSET_H
#define ASSET_H

#include <cute_result.h>
#include <cute_sprite.h>

CF_Result asset_load_sprite(const char* filepath, CF_Sprite* out_sprite);

#endif // ASSET_H
