/**
 * Source file for magic move bitboard generation.
 * This file contains magic bitboard tables and constants from Pradyumna Kannan.
 * Portions may be modified for integration with Latrunculi.
 *
 * The magic keys are not optimal for all squares but they are very close
 * to optimal.
 *
 * Copyright (C) 2007 Pradyumna Kannan.
 *
 * This code is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this code. Permission is granted to anyone to use this
 * code for any purpose, including commercial applications, and to alter
 * it and redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this code must not be misrepresented; you must not
 * claim that you wrote the original code. If you use this code in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original code.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "magic_tables.hpp"

namespace MagicTables {

U64 rookAttacksTable[102400];
U64 bishopAttacksTable[5248];

}  // namespace MagicTables
