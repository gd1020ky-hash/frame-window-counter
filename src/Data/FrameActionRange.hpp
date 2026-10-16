
#pragma once

#include <algorithm>

namespace afwc {

template <typename Actions>
auto firstActionAfterFrame(
    Actions& actions,
    int lastProcessedFrame
) {
    return std::upper_bound(
        actions.begin(),
        actions.end(),
        lastProcessedFrame,
        [](int frame, const auto& action) {
            return frame < action.frame;
        }
    );
}

template <typename Action>
bool isActionDue(
    const Action& action,
    int currentFrame
) {
    return action.frame <= currentFrame;
}

} // namespace afwc
