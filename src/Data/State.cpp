void updateTickCache() {
    g_tickActionsCache.clear();
    g_tickActionsCache.reserve(g_frameActions.size());

    for (const auto& [key, value] : g_frameActions) {
        FrameAction action = value;

        action.frameWindow =
            afwc::normalizeFrameWindow(action.frameWindow);

        action.ifCount =
            afwc::normalizeIfCount(action.ifCount);

        g_tickActionsCache.push_back(action);
    }

    // Keep multiple actions at the same frame as separate actions.
    // Stable sorting preserves their relative order for equal frame numbers.
    afwc::stableSortByFrame(g_tickActionsCache);
}