#include "State.hpp"
#include "FrameWindowValidation.hpp"

void updateTickCache() {
    g_tickActionsCache.clear();
    g_tickActionsCache.reserve(g_frameActions.size());

    for (const auto& entry : g_frameActions) {
        FrameAction action = entry.second;

        action.frameWindow =
            afwc::normalizeFrameWindow(action.frameWindow);

        action.ifCount =
            afwc::normalizeIfCount(action.ifCount);

        g_tickActionsCache.push_back(action);
    }

    // Preserve separate actions at the same frame.
    // Stable sorting preserves their relative order.
    afwc::stableSortByFrame(g_tickActionsCache);
}