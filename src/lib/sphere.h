// Data/SPHERE.DAT: 256 znormalizowanych wektorów (tablica normalnych dla .3db). Opis: docs/formaty/sphere_dat.md
#pragma once

#include "lib/model3db.h"

#include <cstdint>
#include <vector>

namespace ow
{

// 256 × vec3 f32. Pusty wektor przy złym rozmiarze danych.
std::vector<Vec3f> parseSphereDat(const std::vector<uint8_t>& data);

} // namespace ow
