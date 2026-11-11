#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/ui/Popup.hpp>

using namespace geode::prelude;

class FPSCounter : public CCLabelBMFont {
protected:
    float m_timer = 0.f;
    int m_frames = 0;

public:
    static FPSCounter* create() {
        auto ret = new FPSCounter();
        if (ret && ret->initWithString("0 FPS", "bigFont.fnt")) {
            ret->autorelease();
            ret->setScale(0.4f);
            ret->setAnchorPoint({ 0.f, 1.f });
            ret->setID("qol-fps-counter"_spr);
            ret->setZOrder(1000);
            ret->scheduleUpdate();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    void update(float dt) override {
        m_timer += dt;
        m_frames += 1;
        if (m_timer >= 0.5f) {
            int fps = static_cast<int>(m_frames / m_timer);
            this->setString(fmt::format("{} FPS", fps).c_str());
            m_timer = 0.f;
            m_frames = 0;
        }
    }
};

class QoLPopup : public geode::Popup<> {
protected:
    bool setup() override {
        this->setTitle("QoL Mod Menu");

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        float startY = m_size.height / 2 - 40.f;
        float spacing = 28.f;
        int row = 0;

        auto addToggle = [&](const char* settingKey, const char* label) {
            auto toggler = CCMenuItemToggler::createWithStandardSprites(
                this,
                menu_selector(QoLPopup::onToggle),
                0.6f
            );
            toggler->setID(settingKey);
            toggler->toggle(Mod::get()->getSettingValue<bool>(settingKey));
            toggler->setUserObject(CCString::create(settingKey));

            auto lbl = CCLabelBMFont::create(label, "bigFont.fnt");
            lbl->setScale(0.35f);
            lbl->setAnchorPoint({ 0.f, 0.5f });

            float y = startY - row * spacing;
            toggler->setPosition({ 30.f, y });
            lbl->setPosition({ 55.f, y });

            m_mainLayer->addChild(toggler);
            m_mainLayer->addChild(lbl);
            row++;
        };

        addToggle("hide-attempts", "Hide Attempts Label");
        addToggle("hide-percentage", "Hide Percentage Label");
        addToggle("disable-shake", "Disable Screen Shake");
        addToggle("disable-flash", "Disable Flash Effects");
        addToggle("show-fps", "Show FPS Counter");
        addToggle("auto-checkpoint", "Auto Checkpoint (Practice)");
        addToggle("disable-death-effect", "Disable Death Particles");

        return true;
    }

    void onToggle(CCObject* sender) {
        auto toggler = static_cast<CCMenuItemToggler*>(sender);
        auto key = static_cast<CCString*>(toggler->getUserObject())->getCString();
        bool newValue = !toggler->isToggled();
        Mod::get()->setSettingValue(key, newValue);
    }

public:
    static QoLPopup* create() {
        auto ret = new QoLPopup();
        if (ret && ret->initAnchored(240.f, 230.f)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

class $modify(QoLPauseLayer, PauseLayer) {
    bool init(bool level) {
        if (!PauseLayer::init(level)) return false;

        auto menu = this->getChildByID("right-button-menu");
        if (!menu) {
            menu = typeinfo_cast<CCMenu*>(this->getChildByID("pause-menu"));
        }

        auto spr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
        spr->setScale(0.9f);

        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(QoLPauseLayer::onQoLMenu)
        );
        btn->setID("qol-menu-button"_spr);

        if (menu) {
            menu->addChild(btn);
            menu->updateLayout();
        } else {
            auto fallbackMenu = CCMenu::create();
            fallbackMenu->addChild(btn);
            fallbackMenu->setPosition({ 35.f, CCDirector::sharedDirector()->getWinSize().height - 35.f });
            this->addChild(fallbackMenu);
        }

        return true;
    }

    void onQoLMenu(CCObject*) {
        QoLPopup::create()->show();
    }
};

class $modify(QoLPlayLayer, PlayLayer) {
    struct Fields {
        float m_autoCheckpointTimer = 0.f;
        FPSCounter* m_fpsCounter = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        this->applyLabelVisibility();

        if (Mod::get()->getSettingValue<bool>("show-fps")) {
            auto counter = FPSCounter::create();
            auto winSize = CCDirector::sharedDirector()->getWinSize();
            counter->setPosition({ 5.f, winSize.height - 5.f });
            this->addChild(counter, 1000);
            m_fields->m_fpsCounter = counter;
        }

        return true;
    }

    void applyLabelVisibility() {
        bool hideAttempts = Mod::get()->getSettingValue<bool>("hide-attempts");
        bool hidePercent = Mod::get()->getSettingValue<bool>("hide-percentage");

        if (m_attemptLabel) m_attemptLabel->setVisible(!hideAttempts);
        if (m_percentageLabel) m_percentageLabel->setVisible(!hidePercent);
    }

    void updateAttempts() {
        PlayLayer::updateAttempts();
        this->applyLabelVisibility();
    }

    void update(float dt) {
        PlayLayer::update(dt);

        if (m_isPracticeMode && Mod::get()->getSettingValue<bool>("auto-checkpoint")) {
            m_fields->m_autoCheckpointTimer += dt;
            float interval = Mod::get()->getSettingValue<double>("auto-checkpoint-interval");
            if (m_fields->m_autoCheckpointTimer >= interval) {
                m_fields->m_autoCheckpointTimer = 0.f;
                this->createCheckpoint();
            }
        }
    }

    void shakeCamera(float duration, float strength, float interval) {
        if (Mod::get()->getSettingValue<bool>("disable-shake")) {
            return;
        }
        PlayLayer::shakeCamera(duration, strength, interval);
    }

    void flashLayer(ccColor3B color, float duration, float opacity) {
        if (Mod::get()->getSettingValue<bool>("disable-flash")) {
            return;
        }
        PlayLayer::flashLayer(color, duration, opacity);
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        if (Mod::get()->getSettingValue<bool>("disable-death-effect")) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }
        PlayLayer::destroyPlayer(player, obj);
    }
};
