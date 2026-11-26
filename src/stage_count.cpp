// SPDX-License-Identifier: GPL-3.0-only
#include "stage_count.hpp"

#include <algorithm>
#include <cctype>

namespace dtvr {

int parseStageCount(const std::string& text) {
    const std::string head = text.substr(0, 4);
    size_t i = 0;
    while (i < head.size() && (head[i] == ' ' || head[i] == '\t')) ++i; // MS C atoi
    bool negative = false;
    if (i < head.size() && (head[i] == '+' || head[i] == '-')) negative = head[i++] == '-';
    int value = 0;
    while (i < head.size() && std::isdigit(static_cast<unsigned char>(head[i]))) value = value * 10 + (head[i++] - '0');
    return negative ? -value : value;
}

int stepStageCount(const std::string& field, int current, int by) {
    const int typed = parseStageCount(field);
    const int from = typed >= 1 && typed <= kMaxStages ? typed : std::clamp(current, 1, kMaxStages);
    return std::clamp(from + by, 1, kMaxStages);
}

} // namespace dtvr
