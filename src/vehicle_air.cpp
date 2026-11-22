// SPDX-License-Identifier: GPL-3.0-only
#include "vehicle_air.hpp"

namespace dtvr {
namespace {

constexpr uint16_t kField = 0x01, kRoad = 0x02, kSeaRoute = 0x04, kWater = 0x08; // europe.pit bits
constexpr int kTop = 0x18, kRate = 2; // seg2:17A9; 1 << DS:34 with DS:34 = 1

} // namespace

VehicleAir restingAir(uint16_t fieldCell) {
    return (fieldCell & kWater) ? VehicleAir{2, kTop} : VehicleAir{};
}

void airTick(VehicleAir& air, bool stepStart, uint16_t ahead, bool moving, uint16_t ahead3) {
    if (stepStart && (ahead & (kRoad | kSeaRoute)) == (kRoad | kSeaRoute) && (air.state == 0 || air.state == 3)) {
        air.state = 1; // seg2:27EE: rise from the ground
        air.height = 0;
    }
    if (moving && (air.state == 1 || air.state == 2) && (ahead3 & kField) && (ahead3 & kRoad))
        air.state = 3; // seg2:29D1: a land field three cells ahead
    if (air.state == 1) {
        if (air.height < kTop) air.height += kRate;
        else air.state = 2;
    } else if (air.state == 3) {
        if (air.height > 0) air.height -= kRate;
        else air.state = 0;
    }
}

} // namespace dtvr
