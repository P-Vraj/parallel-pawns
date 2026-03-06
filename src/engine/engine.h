#pragma once

#include "evaluation.h"
#include "geometry.h"
#include "move_gen/attacks.h"
#include "position.h"
#include "types.h"
#include "util.h"
#include "zobrist.h"

inline void engine_init() {
    zobrist::init_zobrist();
    attacks::init_attack_tables();
    geom::init_geometry_tables();
}