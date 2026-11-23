// SPDX-License-Identifier: GPL-3.0-only
#include "drago_vehicle.hpp"

#include <cstdlib>

namespace dtvr {

namespace {

// The records of the data segment (offset, delay, frames), names as rally.exe asks
// for them (the SBS files hold them in upper case).
const DragoAnimation kBikeUp{5, {"farup1", "farup2_6", "farup3_5", "farup4", "farup3_5", "farup2_6"}};     // 262A
const DragoAnimation kBikeRight{4, {"farrigt1", "farrigt2", "farrigt3", "farrigt4", "farrigt5", "farrigt6"}}; // 258E
const DragoAnimation kBikeDown{4, {"fardo1", "fardo2_6", "fardo3_5", "fardo4", "fardo3_5", "fardo2_6"}};   // 25DC
const DragoAnimation kBikeLeft{4, {"far1", "far2", "far3", "far4", "far5", "far6"}};                       // 253C
const DragoAnimation kBikeStandUp{4, {"farup1", "farup2_6", "farup3_5", "farup4", "farup3_5", "farup2_6"}}; // 2752
const DragoAnimation kBikeStandRight{6, {"farrigt2", "farrigt2", "farrigt1", "farrigt6", "farrigt6", "farrigt1"}}; // 26B6
const DragoAnimation kBikeStandDown{6, {"fardo1", "fardo2_6", "fardo3_5", "fardo4", "fardo3_5", "fardo2_6"}}; // 2704
const DragoAnimation kBikeStandLeft{5, {"far2", "far2", "far1", "far6", "far6", "far1"}};                   // 2664
const DragoAnimation kBoatUp{3, {"fabup1", "fabup2", "fabup3", "fabup4", "fabup5", "fabup6"}};             // 283C
const DragoAnimation kBoatRight{3, {"fabrigt1", "fabrigt2", "fabrigt3", "fabrigt4", "fabrigt5", "fabrigt6"}}; // 27F6
const DragoAnimation kBoatDown{3, {"fabdown1", "fabdown2", "fabdown3", "fabdown4", "fabdown5", "fabdown6"}}; // 288E
const DragoAnimation kBoatLeft{3, {"fableft1", "fableft2", "fableft3", "fableft4", "fableft5", "fableft6"}}; // 27A4
const DragoAnimation kBoatStandUp{3, {"fabup1"}};       // 28D6
const DragoAnimation kBoatStandRight{3, {"fabrigt1"}};  // 28C6
const DragoAnimation kBoatStandDown{3, {"fabdown1"}};   // 28E8
const DragoAnimation kBoatStandLeft{3, {"fableft1"}};   // 28B4
const DragoAnimation kCarUp{3, {"faup1", "faup2", "faup3", "faup4"}};             // 2970
const DragoAnimation kCarRight{3, {"farigt1", "farigt2", "farigt3", "farigt4"}};  // 2910
const DragoAnimation kCarDown{3, {"fadown1", "fadown2", "fadown3", "fadown4"}};   // 29A4
const DragoAnimation kCarLeft{3, {"faleft1", "faleft2", "faleft3", "faleft4"}};   // 2944
const DragoAnimation kHoverUp{3, {"fafup1", "fafup2", "fafup3", "fafup4"}};        // 2A34
const DragoAnimation kHoverRight{3, {"fafrigt1", "fafrigt2", "fafrigt3", "fafrigt4"}}; // 29DC
const DragoAnimation kHoverDown{3, {"fafdown1", "fafdown2", "fafdown3", "fafdown4"}}; // 2A6C
const DragoAnimation kHoverLeft{3, {"faf1", "faf2", "faf3", "faf4"}};              // 2A04
const DragoAnimation kRocketUp{3, {"rup1", "rup2", "rup3", "rup4"}};               // 2A94
const DragoAnimation kRocketRight{3, {"rright1", "rright2", "rright3", "rright4"}}; // 2AC8
const DragoAnimation kRocketDown{3, {"rdown1", "rdown2", "rdown3", "rdown4"}};     // 2AF8
const DragoAnimation kRocketLeft{3, {"rleft1", "rleft2", "rleft3", "rleft4"}};     // 2B28
const DragoAnimation kFigure[9] = {
    {0, {}},
    {3, {"fazup1", "fazup2", "fazup3", "fazup4", "fazup5", "fazup6"}},                 // 2DA0
    {3, {"fazriup1", "fazriup2", "fazriup3", "fazriup4", "fazriup5", "fazriup6"}},     // 2D5A
    {3, {"fazrigt1", "fazrigt2", "fazrigt3", "fazrigt4", "fazrigt5", "fazrigt6"}},     // 2D08
    {3, {"fazrido1", "fazrido2", "fazrido3", "fazrido4", "fazrido5", "fazrido6"}},     // 2CB6
    {3, {"fazdo1", "fazdo2_6", "fazdo3_5", "fazdo4", "fazdo3_5", "fazdo2_6"}},         // 2B6E
    {3, {"fazledo1", "fazledo2", "fazledo3", "fazledo4", "fazledo5", "fazledo6"}},     // 2BC0
    {3, {"fazleft1", "fazleft2", "fazleft3", "fazleft4", "fazleft5", "fazleft6"}},     // 2C12
    {3, {"fazleup1", "fazleup2", "fazleup3", "fazleup4", "fazleup5", "fazleup6"}}};    // 2C64
const DragoAnimation kBlimp[9] = {{0, {}},
    {0, {"zepup"}}, {0, {"zepriup"}}, {0, {"zepright"}}, {0, {"zeprido"}}, // 2DC2 2DD2 2DE4 2DF4
    {0, {"zepdown"}}, {0, {"zepledo"}}, {0, {"zepleft"}}, {0, {"zepleup"}}}; // 2E04 2E14 2E24 2E34

// Tables by direction 0..8 (null where the data segment holds 0).
using Table = std::array<const DragoAnimation*, 9>;
constexpr const DragoAnimation* N = nullptr;
const Table kBikeMove{N, &kBikeUp, N, &kBikeRight, N, &kBikeDown, N, &kBikeLeft, N};              // DS:3C0E
const Table kBoatMove{N, &kBoatUp, N, &kBoatRight, N, &kBoatDown, N, &kBoatLeft, N};              // DS:3C12
const Table kBikeStand{N, &kBikeStandUp, N, &kBikeStandRight, N, &kBikeStandDown, N, &kBikeStandLeft, N}; // DS:3B7E
const Table kBoatStand{N, &kBoatStandUp, N, &kBoatStandRight, N, &kBoatStandDown, N, &kBoatStandLeft, N}; // DS:3BA2
const Table kCar{N, &kCarUp, N, &kCarRight, N, &kCarDown, N, &kCarLeft, N};                       // DS:3C16 / 3BC6
const Table kHover{N, &kHoverUp, N, &kHoverRight, N, &kHoverDown, N, &kHoverLeft, N};             // DS:3C1A / 3BEA
const Table kRocket{N, &kRocketUp, N, &kRocketRight, N, &kRocketDown, N, &kRocketLeft, N};        // DS:3CCA
const Table kFigureTable{N, &kFigure[1], &kFigure[2], &kFigure[3], &kFigure[4], &kFigure[5], &kFigure[6],
                         &kFigure[7], &kFigure[8]};                                                // DS:3C1E
const Table kBlimpTable{N, &kBlimp[1], &kBlimp[2], &kBlimp[3], &kBlimp[4], &kBlimp[5], &kBlimp[6],
                        &kBlimp[7], &kBlimp[8]};                                                   // DS:3CD2

constexpr int kStepX[9] = {0, 0, 1, 1, 1, 0, -1, -1, -1}; // DS:3D90
constexpr int kStepY[9] = {0, -1, -1, 0, 1, 1, 1, 0, -1}; // DS:3DA2
constexpr int kNear = 0x30;                               // three cells

bool isHover(const DragoAnimation* r) {
    return r == &kHoverUp || r == &kHoverRight || r == &kHoverDown || r == &kHoverLeft;
}

size_t idx(int dir) { return dir >= 0 && dir < 9 ? size_t(dir) : 0; }

} // namespace

void DragoTrail::clear() { head_ = tail_ = count_ = 0; }

bool DragoTrail::push(int field) {
    if (!empty() && buffer_[size_t(head_ == 0 ? kSize - 1 : head_ - 1)] == field) return false;
    buffer_[size_t(head_)] = int16_t(field);
    head_ = head_ < kSize - 1 ? head_ + 1 : 0;
    ++count_;
    return true;
}

bool DragoTrail::pop() {
    if (empty()) return false;
    tail_ = tail_ < kSize - 1 ? tail_ + 1 : 0;
    --count_;
    return true;
}

int DragoTrail::front() const { return buffer_[size_t(tail_)]; }

bool DragoTrail::oneLeft() const { return tail_ == (head_ == 0 ? kSize - 1 : head_ - 1); }

void DragoTrail::cutLoop() {
    if (empty()) return;
    const int savedTail = tail_, savedCount = count_;
    const int16_t first = buffer_[size_t(tail_)];
    for (;;) {
        tail_ = tail_ < kSize - 1 ? tail_ + 1 : 0;
        --count_;
        if (buffer_[size_t(tail_)] == first) return; // the trail came back: skip the loop
        if (tail_ == head_) { tail_ = savedTail; count_ = savedCount; return; }
    }
}

int dragoDirection(int tx, int ty, int x, int y) {
    // Index sx * 4 + sy with s = 0 equal, 1 the target is larger, 2 smaller (seg2:AE8B).
    static constexpr int kTable[12] = {0, 5, 1, 0, 3, 4, 2, 0, 7, 6, 8, 0};
    const int sx = x == tx ? 0 : x < tx ? 1 : 2;
    const int sy = y == ty ? 0 : y < ty ? 1 : 2;
    return kTable[sx * 4 + sy];
}

void DragoVehicle::Running::set(const DragoAnimation* r) {
    if (r == record) return;
    record = r;
    frame = 0;
    countdown = 0;
}

void DragoVehicle::Running::advance() { // seg2:B0D6: old value negative -> reload, next frame
    if (!record) return;
    if (countdown-- >= 0) return;
    countdown = record->delay;
    if (++frame >= int(record->frames.size())) frame = 0;
}

const char* DragoVehicle::Running::name() const {
    if (!record || record->frames.empty()) return nullptr;
    return record->frames[size_t(frame) % record->frames.size()];
}

void DragoVehicle::place(int form, int holderX, int holderY, int holderFacing, int holderField,
                         const Board& board, const CellProperties& cell) {
    static constexpr int kBehind[5] = {0, 2, 3, 0, 1}; // [bp-0xE..-0xB] by the facing 1-4
    static constexpr int kSideX[4] = {0, 48, 0, -48}, kSideY[4] = {-48, 0, 48, 0}; // DS:3D7C / 3D84
    int side = kBehind[holderFacing >= 1 && holderFacing <= 4 ? holderFacing : 0];
    if (holderField >= 0 && size_t(holderField) < board.fields.size())
        for (int i = 0; i < 4 && board.fields[size_t(holderField)].next[size_t(side)] == Field::kNone; ++i)
            side = side < 3 ? side + 1 : 0;
    x_ = holderX + kSideX[side];
    y_ = holderY + kSideY[side];
    placed_ = true;
    hop_ = 0;
    height_ = 0;
    main_ = Running{};
    second_ = Running{};
    const int dir = 2 * side + 1; // the stand tables are read at d * 8 + 4 (seg2:9E2B ff.)
    const bool water = (cell(holderX >> 4, holderY >> 4) & 0x08) != 0;
    if (form == 0) {
        main_.set((water ? kBoatStand : kBikeStand)[size_t(dir)]);
    } else if (form == 1) {
        main_.set((water ? kHover : kCar)[size_t(dir)]);
        if (water) hop_ = 2, height_ = 0x18; // seg2:9E8A
    } else {
        main_.set(&kBlimp[4]); // seg2:9EB2: DS:2DF4 until the first step
    }
}

int DragoVehicle::towardsField(int field, const Board& board) const {
    if (field < 0 || size_t(field) >= board.fields.size()) return 0;
    const Field& f = board.fields[size_t(field)];
    return dragoDirection(f.x * 16, f.y * 16, x_, y_);
}

void DragoVehicle::setMain(const DragoAnimation* record) {
    if (!record) return; // seg2:AC9D
    main_.set(record);
    if (isHover(record)) { // seg2:AB09: a hop starts when he moves
        if (hop_ == 0 && dir_ != 0) {
            hop_ = 1;
            height_ = 0;
            face_ = dir_;
        }
    } else {
        hop_ = 0;
        height_ = 0;
    }
}

bool DragoVehicle::checkLanding(int dir, const CellProperties& cell) {
    if (hop_ != 2) return false;
    const int cx = x_ >> 4, cy = y_ >> 4, ax = x_ & ~15, ay = y_ & ~15;
    uint16_t ahead = 0;
    switch (dir) { // seg2:AB6C ff.: the cell three ahead and the landing point
    case 1: landX_ = ax, landY_ = ay - 0x30, ahead = cell(cx, cy - 3); break;
    case 3: landX_ = ax + 0x30, landY_ = ay, ahead = cell(cx + 3, cy); break;
    case 5: landX_ = ax, landY_ = ay + 0x30, ahead = cell(cx, cy + 3); break;
    case 7: landX_ = ax - 0x30, landY_ = ay, ahead = cell(cx - 3, cy); break;
    default: break;
    }
    if (!((ahead & 0x01) && (ahead & 0x02))) return false;
    hop_ = 3;
    face_ = dir_;
    return true;
}

void DragoVehicle::follow(int form, int dx, int dy, int holderX, int holderY, const Board& board) {
    if (form == 2) { // seg2:A9F3: straight at the holder; note the 3 for y (seg2:A9FB)
        dir_ = dragoDirection(holderX, holderY, x_, y_);
        setMain(kFigureTable[idx(dir_)]);
        const DragoAnimation* blimp = kBlimpTable[idx(dir_)];
        if (dx > kNear || dy > 3) {
            second_.set(blimp);
            face_ = dir_;
        } else {
            if (blimp) second_.set(blimp);
            face_ = dir_;
            dir_ = 0;
        }
        return;
    }
    if (form == 1 && (hop_ == 1 || hop_ == 3)) { // seg2:A732: rising or landing goes straight on
        dir_ = face_;
        return;
    }
    if (trail_.oneLeft() && dx < 4 && dy < 4) trail_.clear(); // seg2:A5C4
    const bool far = dx > kNear || dy > kNear;
    if (trail_.oneLeft() && far) {
        face_ = 0;
        dir_ = towardsField(trail_.front(), board);
    } else if (trail_.empty() && far) {
        face_ = 0;
        dir_ = dragoDirection(holderX, holderY, x_, y_);
    } else if (trail_.empty()) {
        dir_ = 0;
        face_ = dragoDirection(holderX, holderY, x_, y_);
    } else if (!trail_.oneLeft()) {
        face_ = 0;
        dir_ = towardsField(trail_.front(), board);
    } else if (face_ == 0) {
        face_ = dir_;
        dir_ = 0;
    }
}

void DragoVehicle::step(int form, int holderX, int holderY, const Board& board, const CellProperties& cell) {
    trail_.cutLoop(); // seg2:A0B9
    const int dx = std::abs(x_ - holderX), dy = std::abs(y_ - holderY);
    follow(form, dx, dy, holderX, holderY, board);
    const int cx = x_ >> 4, cy = y_ >> 4;
    if (form == 0) { // seg2:A1A6: bicycle, or boat on water (bit 8)
        const bool water = (cell(cx, cy) & 0x08) != 0;
        if (dir_) {
            setMain((water ? kBoatMove : kBikeMove)[idx(dir_)]);
        } else if (face_) {
            setMain((water ? kBoatStand : kBikeStand)[idx(face_)]);
            face_ = 0;
        }
    } else if (form == 1) { // seg2:A2F1: the car on roads, hovering over sea routes (6)
        std::array<uint16_t, 9> around{}; // [bp-0x18 + 2 dir]: own cell and the four sides
        around[0] = cell(cx, cy);
        around[1] = cell(cx, cy - 1);
        around[3] = cell(cx + 1, cy);
        around[5] = cell(cx, cy + 1);
        around[7] = cell(cx - 1, cy);
        auto choose = [&](uint16_t props, int dir, bool rockets) {
            if ((props & 0x06) == 0x06) {
                setMain(kHover[idx(dir)]);
                second_.set(nullptr);
            } else {
                setMain(kCar[idx(dir)]);
                second_.set(rockets ? kRocket[idx(dir)] : nullptr);
            }
        };
        const bool aligned = (x_ & 15) == 0 && (y_ & 15) == 0; // seg2:B255
        if (aligned) {
            if (dir_) {
                if (around[0] & 0x01) {
                    const uint16_t next = around[idx(dir_)];
                    if (next & 0x02) choose(next, dir_, true);
                } else {
                    checkLanding(dir_, cell);
                }
            } else if (face_) {
                choose(around[idx(face_)], face_, false);
            }
        } else if (dir_) {
            if (around[0] & 0x02) choose(around[0], dir_, true);
        } else if (face_) {
            choose(around[idx(face_)], face_, false);
        }
    }
    x_ += kStepX[idx(dir_)]; // seg2:A55C
    y_ += kStepY[idx(dir_)];
    if (!trail_.empty() && towardsField(trail_.front(), board) == 0) trail_.pop(); // seg2:A58D
}

void DragoVehicle::movePass(int form, int holderX, int holderY, const Board& board, const CellProperties& cell) {
    if (!placed_) return;
    if (form == 2) { // seg2:A016: three steps when far, else two
        steps_ = std::abs(x_ - holderX) > 0x90 || std::abs(y_ - holderY) > 0x90 ? 3 : 2;
    } else if (steps_ == 3) { // seg2:9FE3
        if (trail_.count() >= 2) steps_ = 4;
    } else if (trail_.count() <= 1) {
        steps_ = 3;
    }
    for (int i = 0; i < steps_; ++i) step(form, holderX, holderY, board, cell);
    if (form != 1) return;
    if (hop_ == 1) { // seg2:AFA9: rise by 2 to 0x18, then hover in place
        if (height_ < 0x18) height_ += 2;
        if (height_ >= 0x18) dir_ = 0, face_ = 0, hop_ = 2;
    }
    if (hop_ == 3) { // seg2:AFDA: sink by 2, then land on the point AB49 chose
        if (height_ > 0) height_ -= 2;
        if (height_ == 0) {
            hop_ = 0;
            dir_ = 0;
            face_ = 0;
            x_ = landX_;
            y_ = landY_;
        }
    }
}

void DragoVehicle::animatePass() {
    main_.advance();
    second_.advance();
}

std::vector<DragoVehicle::Sprite> DragoVehicle::sprites(int form) const {
    std::vector<Sprite> out;
    if (!placed_) return out;
    if (const char* n = main_.name()) { // seg2:B017 ff.
        if (form == 0) out.push_back({n, x_, y_ - 10});
        else if (form == 1) out.push_back({n, x_, y_ - height_ + 2});
        else out.push_back({n, x_ - 5, y_ - 0x2D});
    }
    if (const char* n = second_.name()) { // seg2:B153 ff.: rockets beside the car, else the blimp
        int sx = x_ - 5, sy = y_ - 0x55;
        if (second_.record == &kRocketUp) sx = x_ + 0x10, sy = y_ + 0x1C;
        else if (second_.record == &kRocketRight) sx = x_ - 7, sy = y_ + 0x12;
        else if (second_.record == &kRocketDown) sx = x_ + 0x10, sy = y_ - 4;
        else if (second_.record == &kRocketLeft) sx = x_ + 0x2A, sy = y_ + 0x12;
        out.push_back({n, sx, sy - height_});
    }
    return out;
}

} // namespace dtvr
