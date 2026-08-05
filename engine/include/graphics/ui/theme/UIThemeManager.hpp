#pragma once

#include "UITheme.hpp"
#include "utils/Singleton.hpp"
#include <string>
#include <string_view>
#include <unordered_map>

namespace Engine {

// External to the UI system on purpose: nothing in UIManager, the solver or the renderer
// reads a colour or a text style from here. Widgets do, and so may a caller building a
// container by hand. The single exception is getScale(), which UIManager applies to every
// node -- see UIThemeMetrics.
class UIThemeManager : public Singleton<UIThemeManager>
{
    friend class Singleton<UIThemeManager>;

  private:
    UIThemeColors m_colors;
    UIThemeMetrics m_metrics;
    std::unordered_map<std::string, UITextStyle> m_textStyles;

  public:
    void setTheme(const UITheme& theme);

    void setColors(const UIThemeColorsSpec& colors);
    void setMetrics(const UIThemeMetrics& metrics);
    void setTextStyle(std::string_view name, const UITextStyle& style);

    const UIThemeColors& getColors() const { return m_colors; }
    const UIThemeMetrics& getMetrics() const { return m_metrics; }
    float getScale() const { return m_metrics.scale; }

    // An unknown name falls back to "body" rather than an empty style, so a typo reads
    // as wrong-looking text instead of invisible text.
    const UITextStyle& getTextStyle(std::string_view name) const;

  private:
    UIThemeManager();
    ~UIThemeManager() = default;

    void resetTextStyles();
};

namespace UITheming {

inline const UIThemeColors& colors() { return UIThemeManager::get().getColors(); }
inline const UIThemeMetrics& metrics() { return UIThemeManager::get().getMetrics(); }
inline const UITextStyle& textStyle(std::string_view name)
{
    return UIThemeManager::get().getTextStyle(name);
}

}   // namespace UITheming

}   // namespace Engine
