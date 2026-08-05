#include "graphics/ui/theme/UIThemeManager.hpp"

namespace Engine {

namespace {

const std::string BODY_STYLE = "body";

}   // namespace

UIThemeManager::UIThemeManager()
{
    m_colors = UIThemeColors::resolve({});
    resetTextStyles();
}

// font is left null throughout: that already means "the UI's font", which resolves to
// FontLoader's default further down. A theme naming a font it does not own would outlive
// it just as easily.
void UIThemeManager::resetTextStyles()
{
    const UIThemeColors& c = m_colors;

    m_textStyles["h1"] = {.color = c.text, .size = 20.0f};
    m_textStyles["h2"] = {.color = c.text, .size = 17.0f};
    m_textStyles["h3"] = {.color = c.text, .size = 15.0f};
    m_textStyles["body"] = {.color = c.text, .size = 14.0f};
    m_textStyles["label"] = {.color = c.textMuted, .size = 13.0f};
    m_textStyles["caption"] = {.color = c.textSubtle, .size = 11.0f};
    m_textStyles["button"] = {.color = c.onAccent, .size = 15.0f};
    m_textStyles["title"] = {.color = c.text, .size = 14.0f};
}

void UIThemeManager::setTheme(const UITheme& theme)
{
    m_colors = UIThemeColors::resolve(theme.colors);
    m_metrics = theme.metrics;

    // Rebuilt first so the built-ins pick up the new colours, then the caller's own
    // entries go over the top -- naming "body" restyles it, naming anything else adds it.
    resetTextStyles();
    for (const auto& [name, style] : theme.textStyles) m_textStyles[name].combine(style);
}

void UIThemeManager::setColors(const UIThemeColorsSpec& colors)
{
    m_colors = UIThemeColors::resolve(colors);
    resetTextStyles();
}

void UIThemeManager::setMetrics(const UIThemeMetrics& metrics) { m_metrics = metrics; }

void UIThemeManager::setTextStyle(std::string_view name, const UITextStyle& style)
{
    m_textStyles[std::string(name)].combine(style);
}

const UITextStyle& UIThemeManager::getTextStyle(std::string_view name) const
{
    auto found = m_textStyles.find(std::string(name));
    if (found != m_textStyles.end()) return found->second;
    return m_textStyles.find(BODY_STYLE)->second;
}

}   // namespace Engine
