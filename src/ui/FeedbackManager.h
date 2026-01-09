#ifndef FEEDBACK_MANAGER_H
#define FEEDBACK_MANAGER_H

#include <Arduino.h>

namespace Vanguard {

/**
 * @brief Manages audio (buzzer) and haptic (motor) feedback.
 * 
 * Implements the "Geiger Counter" RSSI-to-Tone mapping.
 */
class FeedbackManager {
public:
    static FeedbackManager& getInstance();

    /**
     * @brief Initialize hardware (M5Cardputer buzzer/motor)
     */
    void init();

    /**
     * @brief Play a single beep
     */
    void beep(uint32_t freq = 2000, uint32_t duration = 50);

    /**
     * @brief Trigger a haptic pulse
     */
    void pulse(uint32_t duration = 100);

    /**
     * @brief Update Geiger Counter sound based on RSSI
     * 
     * @param rssi Signal strength (-100 to 0)
     */
    void updateGeiger(int8_t rssi);

    /**
     * @brief Enable/disable feedback
     */
    void setEnabled(bool enabled) { m_enabled = enabled; }

private:
    FeedbackManager();
    bool m_enabled = true;
    uint32_t m_lastGeigerMs = 0;
};

} // namespace Vanguard

#endif // FEEDBACK_MANAGER_H
