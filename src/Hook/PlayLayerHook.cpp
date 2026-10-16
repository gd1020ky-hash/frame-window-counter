
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/binding/CheckpointObject.hpp>

#include "../Data/State.hpp"
#include "../Data/FrameActionRange.hpp"
#include "../Common.hpp"
#include "../Audio/SoundManager.hpp"
#include "../UI/FrameActionPopup.hpp"

#include <algorithm>
#include <vector>
#include <map>
#include <cmath>
#include <cstddef>

using namespace geode::prelude;

struct ActiveMarker {
    Ref<CCNode> node;
    CCPoint worldPos;
};

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        int m_currentFrame = 0;
        int m_lastFrame = -1;

        std::map<void*, int> m_checkpointMap;
        std::vector<int> m_checkpointFrameStack;

        int m_pendingRestoreFrame = -1;
        bool m_restoreFrameArmed = false;
        bool m_checkpointRemovalActive = false;
        int m_checkpointRemovalRestoreFrame = -1;

        std::map<int, int> m_hudCounts;
        Ref<CCNode> m_hudNode = nullptr;
        Ref<CCNode> m_precNode = nullptr;

        int m_lastPrecIndex = -1;
        bool m_wasCalculating = false;

        std::vector<ActiveMarker> m_activeMarkers;
        std::map<int, Ref<CCLabelBMFont>> m_countLabels;

        int m_lastSpawnFrame1P = -1;
        int m_lastSpawnFrame2P = -1;

#if defined(GEODE_IS_MOBILE)
        bool m_isLevelEnd = false;
#endif
    };

    bool init(
        GJGameLevel* level,
        bool useReplay,
        bool dontCreateObjects
    ) {
        if (!PlayLayer::init(
            level,
            useReplay,
            dontCreateObjects
        )) {
            return false;
        }

        g_lastAutoSaveTime = std::chrono::steady_clock::now();
        loadModData();

        m_fields->m_currentFrame = 0;
        m_fields->m_lastFrame = -1;

        m_fields->m_checkpointMap.clear();
        m_fields->m_checkpointFrameStack.clear();

        m_fields->m_pendingRestoreFrame = -1;
        m_fields->m_restoreFrameArmed = false;
        m_fields->m_checkpointRemovalActive = false;
        m_fields->m_checkpointRemovalRestoreFrame = -1;

        m_fields->m_lastPrecIndex = -1;
        m_fields->m_wasCalculating = false;

        m_fields->m_hudCounts.clear();
        m_fields->m_activeMarkers.clear();
        m_fields->m_countLabels.clear();

        m_fields->m_lastSpawnFrame1P = -1;
        m_fields->m_lastSpawnFrame2P = -1;

#if defined(GEODE_IS_MOBILE)
        m_fields->m_isLevelEnd = false;
        this->createMobileShortcutBtn();
#endif

        this->rebuildHUD();
        this->schedule(
            schedule_selector(MyPlayLayer::onMyTick)
        );

        return true;
    }

    void rememberCheckpoint(CheckpointObject* cp) {
        if (!cp) return;

        auto key = static_cast<void*>(cp);

        if (m_fields->m_checkpointMap.contains(key)) {
            return;
        }

        const int frame = m_fields->m_currentFrame;

        m_fields->m_checkpointMap[key] = frame;
        m_fields->m_checkpointFrameStack.push_back(frame);

        geode::log::debug(
            "FrameAction: checkpoint pushed at physical frame {} "
            "({:p}), stack size {}",
            frame,
            key,
            m_fields->m_checkpointFrameStack.size()
        );
    }

    int getSavedCheckpointFrame(CheckpointObject* cp) {
        if (cp && this->m_checkpointArray) {
            for (
                unsigned int i = 0;
                i < this->m_checkpointArray->count();
                i++
            ) {
                auto currentCp =
                    static_cast<CheckpointObject*>(
                        this->m_checkpointArray->objectAtIndex(i)
                    );

                if (currentCp == cp) {
                    if (
                        i <
                        m_fields->m_checkpointFrameStack.size()
                    ) {
                        return m_fields
                            ->m_checkpointFrameStack[i];
                    }

                    break;
                }
            }
        }

        if (cp) {
            auto it = m_fields->m_checkpointMap.find(
                static_cast<void*>(cp)
            );

            if (it != m_fields->m_checkpointMap.end()) {
                return it->second;
            }
        }

        if (!m_fields->m_checkpointFrameStack.empty()) {
            return m_fields->m_checkpointFrameStack.back();
        }

        return -1;
    }

    CheckpointObject* createCheckpoint() {
        auto cp = PlayLayer::createCheckpoint();

        if (cp) {
            rememberCheckpoint(cp);
        }

        return cp;
    }

    void storeCheckpoint(CheckpointObject* cp) {
        PlayLayer::storeCheckpoint(cp);
    }

    void removeCheckpoint(bool p0) {
        if (
            this->m_isPracticeMode &&
            !m_fields->m_checkpointFrameStack.empty()
        ) {
            const int beforeStackSize =
                static_cast<int>(
                    m_fields->m_checkpointFrameStack.size()
                );

            if (!p0 || beforeStackSize == 1) {
                m_fields->m_checkpointFrameStack.pop_back();

                if (
                    !m_fields->m_checkpointFrameStack.empty()
                ) {
                    m_fields->m_checkpointRemovalActive = true;

                    m_fields->m_checkpointRemovalRestoreFrame =
                        m_fields->m_checkpointFrameStack.back();

                    armCheckpointRestore(
                        m_fields->m_checkpointFrameStack.back()
                    );
                }
                else {
                    m_fields->m_checkpointRemovalActive = false;
                    m_fields->m_checkpointRemovalRestoreFrame = -1;
                    m_fields->m_pendingRestoreFrame = -1;
                    m_fields->m_restoreFrameArmed = false;
                }
            }
        }

        PlayLayer::removeCheckpoint(p0);

        m_fields->m_checkpointMap.clear();

        if (this->m_checkpointArray) {
            const unsigned int count =
                this->m_checkpointArray->count();

            const unsigned int stackCount =
                static_cast<unsigned int>(
                    m_fields->m_checkpointFrameStack.size()
                );

            for (
                unsigned int i = 0;
                i < count && i < stackCount;
                i++
            ) {
                auto cp = static_cast<CheckpointObject*>(
                    this->m_checkpointArray->objectAtIndex(i)
                );

                if (cp) {
                    m_fields->m_checkpointMap[
                        static_cast<void*>(cp)
                    ] = m_fields->m_checkpointFrameStack[i];
                }
            }
        }

        if (
            m_fields->m_checkpointRemovalActive &&
            !m_fields->m_checkpointFrameStack.empty()
        ) {
            m_fields->m_checkpointRemovalRestoreFrame =
                m_fields->m_checkpointFrameStack.back();

            armCheckpointRestore(
                m_fields->m_checkpointFrameStack.back()
            );
        }
    }

    void armCheckpointRestore(int frame) {
        if (frame < 0) return;

        m_fields->m_pendingRestoreFrame = frame;
        m_fields->m_restoreFrameArmed = true;
    }

    void applyRestoredFrame() {
        if (!m_fields->m_restoreFrameArmed) return;

        const int frame = m_fields->m_pendingRestoreFrame;

        if (frame < 0) {
            m_fields->m_restoreFrameArmed = false;
            return;
        }

        m_fields->m_currentFrame = frame;
        m_fields->m_lastFrame = frame - 1;

        m_fields->m_lastPrecIndex = -1;
        g_forcePrecRedraw = true;

        m_fields->m_hudCounts.clear();
        m_fields->m_lastSpawnFrame1P = -1;
        m_fields->m_lastSpawnFrame2P = -1;

        geode::log::debug(
            "FrameAction: restored physical frame {}",
            frame
        );

        m_fields->m_restoreFrameArmed = false;
        m_fields->m_pendingRestoreFrame = -1;
    }

    void loadFromCheckpoint(CheckpointObject* p0) {
        int checkpointFrame = -1;

        if (m_fields->m_checkpointRemovalActive) {
            checkpointFrame =
                m_fields->m_checkpointRemovalRestoreFrame;
        }
        else {
            checkpointFrame = getSavedCheckpointFrame(p0);
        }

        if (checkpointFrame >= 0) {
            armCheckpointRestore(checkpointFrame);
        }

        PlayLayer::loadFromCheckpoint(p0);

        applyRestoredFrame();
    }

    void processCheckpoints() {
        PlayLayer::processCheckpoints();

        if (m_fields->m_checkpointRemovalActive) {
            armCheckpointRestore(
                m_fields->m_checkpointRemovalRestoreFrame
            );

            applyRestoredFrame();
        }
        else if (m_fields->m_restoreFrameArmed) {
            applyRestoredFrame();
        }
        else if (
            this->m_isPracticeMode &&
            m_fields->m_currentFrame == 0 &&
            !m_fields->m_checkpointFrameStack.empty()
        ) {
            armCheckpointRestore(
                m_fields->m_checkpointFrameStack.back()
            );

            applyRestoredFrame();
        }
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (m_fields->m_checkpointRemovalActive) {
            armCheckpointRestore(
                m_fields->m_checkpointRemovalRestoreFrame
            );

            applyRestoredFrame();

            m_fields->m_checkpointRemovalActive = false;
            m_fields->m_checkpointRemovalRestoreFrame = -1;
        }
        else if (m_fields->m_restoreFrameArmed) {
            applyRestoredFrame();
        }
    }

#if defined(GEODE_IS_MOBILE)

    void createMobileShortcutBtn() {
        if (!this->m_uiLayer) return;

        auto winSize =
            CCDirector::sharedDirector()->getWinSize();

        auto sprite =
            CCSprite::createWithSpriteFrameName(
                "GJ_optionsBtn02_001.png"
            );

        if (!sprite) {
            sprite = CCSprite::createWithSpriteFrameName(
                "GJ_optionsBtn_001.png"
            );
        }

        if (!sprite) {
            sprite = CCSprite::createWithSpriteFrameName(
                "GJ_menuBtn_001.png"
            );
        }

        if (!sprite) return;

        sprite->setScale(0.6f);
        sprite->setOpacity(180);

        auto btn = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(MyPlayLayer::onOpenModMenu)
        );

        btn->setID("mobile-shortcut-btn"_spr);

        auto menu = CCMenu::create();
        menu->setID("mobile-shortcut-menu"_spr);
        menu->setZOrder(100);
        menu->setPosition(
            winSize.width - 32.f,
            32.f
        );

        menu->addChild(btn);
        this->m_uiLayer->addChild(menu);
    }

    void onOpenModMenu(CCObject*) {
        if (m_fields->m_isLevelEnd) {
            return;
        }

        auto scene =
            CCDirector::sharedDirector()->getRunningScene();

        if (!scene) return;

        if (
            auto existing =
                scene->getChildByID("FrameActionPopup"_spr)
        ) {
            existing->removeFromParentAndCleanup(true);
            return;
        }

        if (auto popup = FrameActionPopup::create()) {
            popup->setID("FrameActionPopup"_spr);
            popup->showInstant();
        }
    }

    void pauseGame(bool p0) {
        closeAllModPopups();
        PlayLayer::pauseGame(p0);
    }

    void levelComplete() {
        m_fields->m_isLevelEnd = true;
        closeAllModPopups();
        PlayLayer::levelComplete();
    }

#endif

    void onQuit() {
        this->unschedule(
            schedule_selector(MyPlayLayer::onMyTick)
        );

        SoundManager::stopAll();
        PlayLayer::onQuit();
    }

    void recalculateAndRefreshHUD() {
        if (!g_modEnabled) {
            if (m_fields->m_hudNode) {
                m_fields->m_hudNode->removeFromParent();
                m_fields->m_hudNode = nullptr;
                m_fields->m_countLabels.clear();
            }

            return;
        }

        m_fields->m_hudCounts.clear();

        for (const auto& action : g_tickActionsCache) {
            if (
                action.shouldDraw &&
                action.frame <= m_fields->m_lastFrame
            ) {
                double fw = action.frameWindow;
                double ifVal =
                    static_cast<double>(action.ifCount);

                for (const auto& [idStr, preset] : g_labelPresets) {
                    double targetVal =
                        preset.useIF ? ifVal : fw;

                    if (
                        targetVal >= preset.minVal &&
                        targetVal <= preset.maxVal
                    ) {
                        m_fields->m_hudCounts[preset.id]++;
                    }
                }
            }
        }

        this->rebuildHUD();
    }

    void resetLevel() {
        int restoreFrame = -1;

        if (
            m_fields->m_checkpointRemovalActive &&
            m_fields->m_checkpointRemovalRestoreFrame >= 0
        ) {
            restoreFrame =
                m_fields->m_checkpointRemovalRestoreFrame;
        }
        else if (m_fields->m_pendingRestoreFrame >= 0) {
            restoreFrame = m_fields->m_pendingRestoreFrame;
        }
        else if (this->m_isPracticeMode) {
            CheckpointObject* cp = this->m_currentCheckpoint;

            if (
                !cp &&
                this->m_checkpointArray &&
                this->m_checkpointArray->count() > 0
            ) {
                cp = static_cast<CheckpointObject*>(
                    this->m_checkpointArray->lastObject()
                );
            }

            if (cp) {
                restoreFrame = getSavedCheckpointFrame(cp);
            }

            if (
                restoreFrame < 0 &&
                !m_fields->m_checkpointFrameStack.empty()
            ) {
                restoreFrame =
                    m_fields->m_checkpointFrameStack.back();
            }
        }

        if (restoreFrame >= 0) {
            armCheckpointRestore(restoreFrame);
        }

        PlayLayer::resetLevel();
        SoundManager::stopAll();

#if defined(GEODE_IS_MOBILE)
        m_fields->m_isLevelEnd = false;
#endif

        if (m_fields->m_restoreFrameArmed) {
            applyRestoredFrame();
        }
        else if (restoreFrame >= 0) {
            m_fields->m_currentFrame = restoreFrame;
            m_fields->m_lastFrame = restoreFrame - 1;

            m_fields->m_lastPrecIndex = -1;
            g_forcePrecRedraw = true;

            m_fields->m_hudCounts.clear();
            m_fields->m_lastSpawnFrame1P = -1;
            m_fields->m_lastSpawnFrame2P = -1;
        }
        else {
            m_fields->m_currentFrame = 0;
            m_fields->m_lastFrame = -1;

            m_fields->m_lastPrecIndex = -1;
            g_forcePrecRedraw = true;

            m_fields->m_hudCounts.clear();
            m_fields->m_lastSpawnFrame1P = -1;
            m_fields->m_lastSpawnFrame2P = -1;
        }

        for (auto& marker : m_fields->m_activeMarkers) {
            if (marker.node) {
                marker.node->removeFromParent();
            }
        }

        m_fields->m_activeMarkers.clear();

        for (const auto& action : g_tickActionsCache) {
            if (
                action.shouldDraw &&
                action.frame <= m_fields->m_lastFrame
            ) {
                double fw = action.frameWindow;
                double ifVal =
                    static_cast<double>(action.ifCount);

                for (const auto& [idStr, preset] : g_labelPresets) {
                    double targetVal =
                        preset.useIF ? ifVal : fw;

                    if (
                        targetVal >= preset.minVal &&
                        targetVal <= preset.maxVal
                    ) {
                        m_fields->m_hudCounts[preset.id]++;
                    }
                }
            }
        }

        this->updateHUDCounts();
        this->updatePrecisionHUD(
            m_fields->m_currentFrame
        );

        if (this->m_objectLayer) {
            auto children =
                this->m_objectLayer->getChildren();

            if (children) {
                for (
                    int i = children->count() - 1;
                    i >= 0;
                    i--
                ) {
                    auto child = static_cast<CCNode*>(
                        children->objectAtIndex(i)
                    );

                    if (
                        child->getID() ==
                        "frame-window-marker"_spr
                    ) {
                        child->removeFromParent();
                    }
                }
            }
        }
    }

    void updatePrecisionHUD(int currentFrame) {
        if (!g_modEnabled) {
            if (m_fields->m_precNode) {
                m_fields->m_precNode->removeFromParent();
                m_fields->m_precNode = nullptr;
            }

            return;
        }

        bool isCalc = g_isCalculating.load();

        if (isCalc) {
            m_fields->m_wasCalculating = true;

            if (
                !m_fields->m_precNode ||
                g_forcePrecRedraw
            ) {
                g_forcePrecRedraw = false;

                if (m_fields->m_precNode) {
                    m_fields->m_precNode->removeFromParent();
                }

                m_fields->m_precNode = CCNode::create();
                m_fields->m_precNode->setPosition({5.f, 5.f});
                m_fields->m_precNode->setID(
                    "frame-window-prec"_spr
                );

                if (this->m_uiLayer) {
                    this->m_uiLayer->addChild(
                        m_fields->m_precNode,
                        1
                    );
                }
                else {
                    this->addChild(m_fields->m_precNode, 1);
                }

                auto lbl = CCLabelBMFont::create(
                    "Calculating L*... 0.0%",
                    "bigFont.fnt"
                );

                lbl->setAnchorPoint({0.f, 0.f});
                lbl->setPosition({0.f, 0.f});
                lbl->setScale(0.35f);
                lbl->setColor({255, 200, 50});
                lbl->setID("computing-label"_spr);

                m_fields->m_precNode->addChild(lbl);
            }
            else {
                if (
                    auto lbl = static_cast<CCLabelBMFont*>(
                        m_fields->m_precNode->getChildByID(
                            "computing-label"_spr
                        )
                    )
                ) {
                    lbl->setString(
                        fmt::format(
                            "Calculating L*... {:.1f}%",
                            g_calcProgress.load()
                        ).c_str()
                    );
                }
            }

            return;
        }

        if (m_fields->m_wasCalculating) {
            m_fields->m_wasCalculating = false;
            g_forcePrecRedraw = true;
            m_fields->m_lastPrecIndex = -1;
        }

        if (g_validActions.empty()) {
            if (m_fields->m_precNode) {
                m_fields->m_precNode->removeFromParent();
                m_fields->m_precNode = nullptr;
            }

            return;
        }

        int idx = -1;

        auto it = std::upper_bound(
            g_validActions.begin(),
            g_validActions.end(),
            currentFrame,
            [](int frame, const FrameAction& a) {
                return frame < a.frame;
            }
        );

        if (it != g_validActions.begin()) {
           