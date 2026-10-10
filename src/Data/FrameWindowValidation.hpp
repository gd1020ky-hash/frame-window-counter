
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace afwc {

inline constexpr int MAX_IF_COUNT = 10000;

inline double normalizeFrameWindow(double value) {
    if (!std::isfinite(value) || value <= 0.0) {
        return 1.0;
    }

    return value;
}

inline int normalizeIfCount(int value) {
    return std::clamp(value, 1, MAX_IF_COUNT);
}

struct ExpandedInput {
    double frame;
    double window;
};

inline std::vector<ExpandedInput> expandFrameWindow(
    double baseFrame,
    double frameWindow,
    int ifCount
) {
    if (!std::isfinite(baseFrame)) {
        baseFrame = 0.0;
    }

    frameWindow = normalizeFrameWindow(frameWindow);
    ifCount = normalizeIfCount(ifCount);

    std::vector<ExpandedInput> result;
    result.reserve(static_cast<std::size_t>(ifCount));

    if (ifCount == 1) {
        result.push_back({baseFrame, frameWindow});
        return result;
    }

    const double k = static_cast<double>(ifCount);
    const double subWindow = 1.0 / k;

    const double mainWindow = std::max(
        frameWindow - (k - 1.0) / k,
        subWindow
    );

    result.push_back({baseFrame, mainWindow});

    for (int s = 1; s < ifCount; ++s) {
        result.push_back({
            baseFrame + static_cast<double>(s) / k,
            subWindow
        });
    }

    return result;
}

template <class Action>
inline void stableSortByFrame(std::vector<Action>& actions) {
    std::stable_sort(
        actions.begin(),
        actions.end(),
        [](const Action& a, const Action& b) {
            return a.frame < b.frame;
        }
    );
}

} // namespace afwc
