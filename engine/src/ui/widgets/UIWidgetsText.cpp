#include "ui/widgets/UIWidgets.hpp"
#include "ui/UIStateStore.hpp"
#include "ui/UiManager.hpp"
#include "core/Time.hpp"
#include "core/input/Input.hpp"
#include "graphics/text/FontLoader.hpp"
#include "graphics/text/TextMetrics.hpp"
#include "utils/Utf8.hpp"
#include "utils/math/MathFuncs.hpp"
#include <cstdio>

namespace Engine {

namespace UIWidgets {

namespace {

constexpr float CARET_BLINK_SECONDS = 0.53f;
constexpr float CARET_WIDTH = 1.0f;
// How much of the field stays visible past the caret when it drives the view, so typing
// at the right edge does not leave the caret sitting exactly on the boundary.
constexpr float SCROLL_MARGIN = 8.0f;

// One field holds focus at a time, so one buffer is all a number field's in-progress text
// ever needs. Kept here rather than in UIStateStore because that stores floats, and this
// is the one piece of widget state that is genuinely a string.
uint64_t g_editingKey = 0;
std::string g_editingText;

// Deliberately unscaled: every width worked out here goes back into a layout config, and
// UIManager scales those on the way into the node. Measuring at the scaled size would
// scale it twice.
TextStyle designStyle(const UITextStyle& style) { return style.resolve(1.0f); }

const Font* resolveFieldFont(const UITextConfig& config)
{
    const Font* font = config.font ? config.font : UIManager::get().getFont();
    return FontLoader::get().resolve(font);
}

float measureUpTo(const Font* font, const TextStyle& style, std::string_view text, size_t end)
{
    if (!font || end == 0) return 0.0f;
    if (end > text.size()) end = text.size();
    return TextMetrics::measure(*font, style, text.substr(0, end));
}

// The byte index whose glyph boundary sits nearest x. Walks rather than binary-searches:
// a caret only ever asks about one line of a field, and kerning makes each step depend on
// the one before it, so there is nothing to search over.
size_t indexAtOffset(const Font* font, const TextStyle& style, std::string_view text, float x)
{
    if (!font || x <= 0.0f) return 0;

    float pen = 0.0f;
    uint32_t prev = 0;
    size_t i = 0;
    while (i < text.size()) {
        size_t start = i;
        uint32_t codepoint = Utf8::next(text, i);
        if (codepoint == 0) break;

        GlyphStep glyphStep = TextMetrics::step(*font, style, codepoint, prev);
        prev = glyphStep.kerningPrev;

        float next = pen + glyphStep.total();
        // Past the middle of a glyph the caret belongs after it, which is what makes a
        // click feel like it lands where the pointer is rather than always before.
        if (x < pen + (next - pen) * 0.5f) return start;
        if (x < next) return i;
        pen = next;
    }
    return text.size();
}

bool isPrintable(uint32_t codepoint)
{
    // Control characters arrive as key events, never as text -- except that a few
    // platforms still emit them here, and inserting one would put an unrenderable byte
    // in the caller's string.
    return codepoint >= 0x20 && codepoint != 0x7F;
}

void clampCaret(int& caret, const std::string& text)
{
    if (caret < 0) caret = 0;
    if (caret > (int)text.size()) caret = (int)text.size();

    // The caller owns the string and may have rewritten it since, so the stored caret can
    // land mid-sequence; walk it back onto a boundary rather than splitting a codepoint.
    while (caret > 0 && caret < (int)text.size() &&
           ((unsigned char)text[(size_t)caret] & 0xC0) == 0x80)
        caret--;
}

bool applyTyping(std::string& text, int& caret, const TextFieldConfig& config)
{
    std::string_view typed = Input::get().getTypedText();
    if (typed.empty()) return false;

    bool changed = false;
    size_t i = 0;
    while (i < typed.size()) {
        size_t start = i;
        uint32_t codepoint = Utf8::next(typed, i);
        if (codepoint == 0) break;
        if (!isPrintable(codepoint)) continue;
        if (config.filter && !config.filter(codepoint)) continue;
        if (config.maxLength > 0 && Utf8::countCodepoints(text) >= config.maxLength) break;

        std::string_view bytes = typed.substr(start, i - start);
        text.insert((size_t)caret, bytes);
        caret += (int)bytes.size();
        changed = true;
    }
    return changed;
}

bool applyEditingKeys(std::string& text, int& caret)
{
    Input& input = Input::get();
    bool changed = false;

    if (input.keyRepeated(KeyCode::Backspace) && caret > 0) {
        size_t index = (size_t)caret;
        Utf8::prev(text, index);
        text.erase(index, (size_t)caret - index);
        caret = (int)index;
        changed = true;
    }
    if (input.keyRepeated(KeyCode::Delete) && caret < (int)text.size()) {
        size_t index = (size_t)caret;
        Utf8::next(text, index);
        text.erase((size_t)caret, index - (size_t)caret);
        changed = true;
    }
    if (input.keyRepeated(KeyCode::Left) && caret > 0) {
        size_t index = (size_t)caret;
        Utf8::prev(text, index);
        caret = (int)index;
    }
    if (input.keyRepeated(KeyCode::Right) && caret < (int)text.size()) {
        size_t index = (size_t)caret;
        Utf8::next(text, index);
        caret = (int)index;
    }
    if (input.keyRepeated(KeyCode::Home)) caret = 0;
    if (input.keyRepeated(KeyCode::End)) caret = (int)text.size();

    return changed;
}

// Enough of the view moves to bring the caret back inside, and no more, so the text only
// shifts when it has to -- a view that recentred every keystroke reads as jitter.
float clampScroll(float scroll, float caretX, float innerWidth, float textWidth)
{
    if (innerWidth <= 0.0f) return 0.0f;

    float margin = Math::min(SCROLL_MARGIN, innerWidth * 0.5f);
    if (caretX - scroll > innerWidth - margin) scroll = caretX - innerWidth + margin;
    if (caretX - scroll < margin) scroll = caretX - margin;

    return Math::clamp(scroll, 0.0f, Math::max(textWidth - innerWidth + margin, 0.0f));
}

std::string formatNumber(float value, int decimals)
{
    if (decimals < 0) decimals = 0;
    if (decimals > 9) decimals = 9;

    char buffer[64];
    int written = snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    if (written <= 0) return "0";

    std::string result(buffer, (size_t)written);
    // Trailing zeros make a field unreadable while it is being typed in; the point stays
    // only when something is still behind it.
    if (result.find('.') != std::string::npos) {
        result.erase(result.find_last_not_of('0') + 1);
        if (!result.empty() && result.back() == '.') result.pop_back();
    }
    return result;
}

bool parseNumber(const std::string& text, float& out)
{
    if (text.empty()) return false;

    try {
        size_t consumed = 0;
        float parsed = std::stof(text, &consumed);
        if (consumed != text.size()) return false;
        out = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool numberFilter(uint32_t codepoint)
{
    return (codepoint >= '0' && codepoint <= '9') || codepoint == '-' || codepoint == '.';
}

bool integerFilter(uint32_t codepoint)
{
    return (codepoint >= '0' && codepoint <= '9') || codepoint == '-';
}

}   // namespace

UIInputState textField(std::string& text, const TextFieldConfig& config)
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    TextFieldConfig defaultConfig = {
        .fieldLayout =
            {
                          .width = UISizeSpec::grow(),
                          .height = UISizeSpec::fixed(metrics.controlHeight),
                          .padding = UIEdges(metrics.spacing.sm, 0.0f),
                          .alignCross = UIAlign::Center,
                          },
        .fieldStyle =
            {
                          .backgroundColor = colors.surfaceSunken,
                          .borderColor = colors.border,
                          .borderWidth = metrics.borderWidth.thin,
                          .borderRadius = metrics.radius.sm,
                          .overflow = UIOverflow::Hidden,
                          .blockInput = true,
                          .focusable = true,
                          .cursor = UICursor::Text,
                          .onHover = {.borderColor = colors.borderStrong},
                          .onFocused = {.borderColor = colors.borderFocus},
                          },
        .textLayout = {},
        .textConfig = {.style = UITheming::textStyle("body")},
        .caretLayout = {},
        .caretStyle = {.backgroundColor = colors.foreground},
        .placeholderStyle = {.color = colors.foregroundSubtle},
    };

    defaultConfig.fieldLayout.combine(config.fieldLayout);
    defaultConfig.fieldStyle.combine(config.fieldStyle);
    defaultConfig.textLayout.combine(config.textLayout);
    defaultConfig.textConfig.style.combine(config.textConfig.style);
    defaultConfig.caretLayout.combine(config.caretLayout);
    defaultConfig.caretStyle.combine(config.caretStyle);
    defaultConfig.placeholderStyle.combine(config.placeholderStyle);

    UINodeState field =
        openContainer(defaultConfig.fieldLayout, defaultConfig.fieldStyle, config.key);

    UIStateStore& store = UIStateStore::get();
    UITextConfig textConfig = config.textConfig;
    textConfig.style = defaultConfig.textConfig.style;
    textConfig.wrapEnabled = false;
    textConfig.overflow = TextOverflow::Clip;

    const Font* font = resolveFieldFont(textConfig);
    TextStyle style = designStyle(textConfig.style);

    int caret = store.getInt(field.persistentKey, "caret", (int)text.size());
    clampCaret(caret, text);

    // Focus is settled before the tree is rebuilt, so this is already this frame's answer
    // rather than last frame's -- the same guarantee every other flag on the state has.
    bool wasFocused = store.flag(field.persistentKey, "fieldFocused");
    bool isChanged = false;
    bool isCommitted = false;

    if (field.isPressed) {
        float localX = unscale(field.localMousePos.x) - defaultConfig.fieldLayout.padding->left +
                       store.value(field.persistentKey, "textScroll");
        caret = (int)indexAtOffset(font, style, text, localX);
    }

    if (field.isFocused) {
        Input& input = Input::get();
        isChanged = applyTyping(text, caret, config);
        isChanged |= applyEditingKeys(text, caret);

        if (input.keyPressed(KeyCode::Enter) || input.keyPressed(KeyCode::KPEnter)) {
            isCommitted = true;
            UIManager::get().clearFocus();
        }
        if (input.keyPressed(KeyCode::Escape)) UIManager::get().clearFocus();

        // Any edit restarts the blink, so the caret is never invisible at the moment the
        // user is looking for it.
        if (isChanged || field.isPressed)
            store.value(field.persistentKey, "caretBlinkStart") = Time::get().currTime();
    }

    // What the state store remembers is whether focus is still held *after* this frame's
    // keys, not what it was when the node was declared. Recording the latter would make
    // Enter commit twice: once here, and once more next frame as a focus loss.
    bool keepsFocus = field.isFocused && UIManager::get().getFocusedKey() == field.persistentKey;

    // Losing focus commits too: clicking away from a field is as much "I'm done" as
    // pressing Enter, and a caller writing to a database on isReleased needs both.
    if (wasFocused && !keepsFocus) isCommitted = true;
    store.flag(field.persistentKey, "fieldFocused") = keepsFocus;

    clampCaret(caret, text);
    store.setInt(field.persistentKey, "caret", caret);

    float innerWidth = unscale(field.size.x) - defaultConfig.fieldLayout.padding->horizontal();
    float caretX = measureUpTo(font, style, text, (size_t)caret);
    float textWidth = font ? TextMetrics::measure(*font, style, text) : 0.0f;

    float& scroll = store.value(field.persistentKey, "textScroll");
    scroll = clampScroll(scroll, caretX, innerWidth, textWidth);

    bool showPlaceholder = text.empty() && !config.placeholder.empty();
    textConfig.text = showPlaceholder ? config.placeholder : text;
    if (showPlaceholder) textConfig.style.combine(defaultConfig.placeholderStyle);

    UILayoutConfig textLayout = defaultConfig.textLayout;
    textLayout.offset = Vec2(-scroll, 0.0f);
    addTextLeaf(textLayout, textConfig);

    if (field.isFocused) {
        float blinkStart = store.value(field.persistentKey, "caretBlinkStart");
        float phase = Time::get().currTime() - blinkStart;
        bool visible = Math::mod(phase, CARET_BLINK_SECONDS * 2.0f) < CARET_BLINK_SECONDS;

        if (visible) {
            UILayoutConfig caretLayout = defaultConfig.caretLayout;
            caretLayout.width = UISizeSpec::fixed(CARET_WIDTH);
            caretLayout.height = UISizeSpec::percent(0.6f);
            caretLayout.isFloating = true;
            caretLayout.floating = UIFloatingConfig {
                .offset = Vec2(defaultConfig.fieldLayout.padding->left + caretX - scroll, 0.0f),
                .anchorY = UIAlign::Center,
                .selfY = UIAlign::Center,
            };

            openContainer(caretLayout, defaultConfig.caretStyle);
            closeContainer();
        }
    }

    closeContainer();

    return {
        .node = field,
        .isChanged = isChanged,
        .isEditing = field.isFocused,
        .isReleased = isCommitted,
    };
}

namespace {

// Both number fields are the same widget with a different filter and a different rounding
// on the way out, so the whole of it lives here once. Writes value only when the text in
// the field parses, and reports whether it did.
UIInputState
numberFieldImpl(float& value, const NumberFieldConfig& config, bool isInteger, bool& outWrote)
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    NumberFieldConfig defaultConfig = config;
    defaultConfig.field.filter =
        config.field.filter ? config.field.filter : (isInteger ? integerFilter : numberFilter);

    UILayoutConfig rowLayout = {
        .width = UISizeSpec::grow(),
        .gap = metrics.spacing.xs,
        .alignCross = UIAlign::Center,
    };
    // The row rather than the field is what identifies the edit buffer: its key is known
    // before the field is declared, which is the order the buffer has to be chosen in.
    UINodeState row = openContainer(rowLayout, {}, config.key);

    int decimals = isInteger ? 0 : config.decimals;
    bool ownsBuffer = g_editingKey == row.persistentKey;

    // Only a field being edited shows its own text. An idle one re-formats the bound value
    // every frame, so a value changed anywhere else appears the moment editing stops.
    std::string local;
    if (!ownsBuffer) local = formatNumber(value, decimals);
    std::string& shown = ownsBuffer ? g_editingText : local;

    UIInputState state = textField(shown, defaultConfig.field);

    outWrote = false;
    if (state.isEditing) {
        // Claim on the first focused frame, carrying across whatever the field already
        // holds -- a click that also positioned the caret must not lose that keystroke.
        g_editingKey = row.persistentKey;
        if (!ownsBuffer) g_editingText = shown;

        float parsed = value;
        if (parseNumber(g_editingText, parsed)) {
            value = Math::clamp(parsed, config.minValue, config.maxValue);
            outWrote = true;
        }
    } else if (ownsBuffer) {
        g_editingKey = 0;
        g_editingText.clear();
    }

    float step = config.step;
    if (config.scrollToChange && row.isHovered && !state.isEditing) {
        float wheel = row.scrollDelta.y;
        if (wheel != 0.0f) {
            value = Math::clamp(value + wheel * step, config.minValue, config.maxValue);
            outWrote = true;
        }
    }

    if (config.showSteppers) {
        UILayoutConfig stepperLayout = {
            .width = UISizeSpec::fixed(metrics.controlHeight * 0.6f),
            .height = UISizeSpec::fixed(metrics.controlHeight * 0.5f - 1.0f),
            .alignMain = UIAlign::Center,
            .alignCross = UIAlign::Center,
        };
        UIContainerStyleSpec stepperStyle = {
            .backgroundColor = colors.surfaceRaised,
            .borderColor = colors.border,
            .borderWidth = metrics.borderWidth.thin,
            .borderRadius = metrics.radius.sm,
            .blockInput = true,
            .cursor = UICursor::Pointer,
            .onHover = {.backgroundColor = colors.surfaceHover},
            .onHeld = {.backgroundColor = colors.accentMuted},
        };
        stepperLayout.combine(config.stepperLayout);
        stepperStyle.combine(config.stepperStyle);

        openContainer({.gap = 2.0f, .direction = UILayoutDirection::Column});

        UINodeState up = openContainer(stepperLayout, stepperStyle, "stepUp");
        text("+", {.textConfig = {.style = UITheming::textStyle("caption")}});
        closeContainer();

        UINodeState down = openContainer(stepperLayout, stepperStyle, "stepDown");
        text("-", {.textConfig = {.style = UITheming::textStyle("caption")}});
        closeContainer();

        closeContainer();

        // isPressed rather than isReleased so holding one and releasing elsewhere does
        // not fire, and so a held stepper can repeat once transitions land.
        if (up.isPressed) {
            value = Math::clamp(value + step, config.minValue, config.maxValue);
            outWrote = true;
        }
        if (down.isPressed) {
            value = Math::clamp(value - step, config.minValue, config.maxValue);
            outWrote = true;
        }
    }

    closeContainer();
    return state;
}

}   // namespace

UIInputState numberFieldFloat(float& value, const NumberFieldConfig& config)
{
    bool wrote = false;
    float working = value;
    UIInputState state = numberFieldImpl(working, config, false, wrote);

    bool isChanged = wrote && working != value;
    if (wrote) value = working;

    state.isChanged = isChanged;
    return state;
}

UIInputState numberFieldInt(int& value, const NumberFieldConfig& config)
{
    bool wrote = false;
    float working = (float)value;
    UIInputState state = numberFieldImpl(working, config, true, wrote);

    // Truncating toward zero would make a stepper stall: -1 minus 0.5 rounds back to -1
    // and the value never moves. std::lround is not used because the sign has to follow
    // the value, not the platform's default rounding mode.
    int rounded = (int)(working >= 0.0f ? working + 0.5f : working - 0.5f);
    bool isChanged = wrote && rounded != value;
    if (wrote) value = rounded;

    state.isChanged = isChanged;
    return state;
}

}   // namespace UIWidgets

}   // namespace Engine
