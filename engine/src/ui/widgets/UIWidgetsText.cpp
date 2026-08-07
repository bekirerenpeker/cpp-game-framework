#include "ui/widgets/UIWidgets.hpp"
#include "ui/UIStateStore.hpp"
#include "ui/UiManager.hpp"
#include "core/Time.hpp"
#include "core/input/Input.hpp"
#include "core/window_management/ViewContext.hpp"
#include "core/window_management/Window.hpp"
#include "graphics/text/FontLoader.hpp"
#include "graphics/text/TextLayoutCalculator.hpp"
#include "graphics/text/TextMetrics.hpp"
#include "utils/Utf8.hpp"
#include "utils/math/MathFuncs.hpp"
#include <cstdio>
#include <vector>

namespace Engine {

namespace UIWidgets {

namespace {

constexpr float CARET_BLINK_SECONDS = 0.53f;
constexpr float CARET_WIDTH = 1.0f;
// How much stays visible past the caret when it drives the view, so typing at an edge does
// not leave the caret sitting exactly on the boundary.
constexpr float SCROLL_MARGIN = 8.0f;
// Design units per wheel tick when a single-line field scrolls itself sideways.
constexpr float SCROLL_WHEEL_STEP = 36.0f;
constexpr float STEPPER_GAP = 2.0f;
// Stored in place of a desired column to mean "take it from where the caret is now". Any
// horizontal move or edit puts this back, which is what makes Up/Down remember the column
// across short lines but forget it the moment the user moves sideways.
constexpr float NO_DESIRED_COLUMN = -1.0f;

// One field holds focus at a time, so one buffer is all a number field's in-progress text
// ever needs. Kept here rather than in UIStateStore because that stores floats, and this
// is the one piece of widget state that is genuinely a string.
uint64_t g_editingKey = 0;
std::string g_editingText;

// Where the caret sits, in the text's own space before any scrolling.
struct CaretPoint
{
    float x = 0.0f;
    float top = 0.0f;
    float height = 0.0f;
};

// What a field hands the editing core so the core does not have to know which field it is.
struct EditContext
{
    const Font* font = nullptr;
    TextStyle style;
    float wrapWidth = UI_UNBOUNDED;
    float viewportHeight = 0.0f;
    bool multiline = false;
    uint maxLength = 0;
    bool (*filter)(uint32_t codepoint) = nullptr;
};

// Deliberately unscaled: every width worked out here goes back into a layout config, and
// UIManager scales those on the way into the node. Measuring at the scaled size would
// scale it twice.
TextStyle designStyle(const UITextStyle& style) { return style.resolve(1.0f); }

const Font* resolveFieldFont(const UITextConfig& config)
{
    const Font* font = config.font ? config.font : UIManager::get().getFont();
    return FontLoader::get().resolve(font);
}

// The widget cannot read the leaf it is about to declare -- the leaf's own block is solved
// later in the frame and UIManager::clear deletes it before the next one. So the caret gets
// its own block, laid out by the same calculator with the same text, style and width, which
// is what keeps a caret x on the glyph it points at. One block for the whole UI: only a
// focused field needs geometry and only one field is ever focused. Static rather than
// stored per field because TextBlock has copy *and* move deleted, so no container can hold
// one -- and it caches internally, so re-solving only happens when something changed.
const TextBlock& layoutFor(const EditContext& context, const std::string& text)
{
    static TextBlock block;

    block.setFont(context.font);
    block.setText(text);
    block.setStyle(context.style);
    block.setWrapEnabled(context.multiline);
    block.setAlignH(TextAlignH::Left);
    block.setAlignV(TextAlignV::Top);
    block.setOverflow(TextOverflow::Visible);

    TextLayoutCalculator::get().calculate(block, context.wrapWidth);
    return block;
}

// Byte [start, end) per visual line. The runs carry the offsets -- they are views into the
// block's own string -- but emitRun drops empty ones, so a blank line has no run to ask.
// That case has an exact answer rather than a guess: wrapping always puts at least one
// glyph on a line, so a line can only be empty because an explicit newline produced it,
// and it therefore starts one byte past the previous line's end.
void lineRanges(const TextBlock& block, std::vector<std::pair<size_t, size_t>>& out)
{
    const std::string& text = block.getText();
    const std::vector<TextRun>& runs = block.getRuns();
    const char* base = text.data();

    out.clear();
    size_t prevEnd = 0;
    for (const TextLine& line : block.getLines()) {
        size_t start, end;
        if (line.runCount == 0) {
            start = out.empty() ? 0 : prevEnd + 1;
            end = start;
        } else {
            const TextRun& first = runs[line.firstRun];
            const TextRun& last = runs[line.firstRun + line.runCount - 1];
            start = (size_t)(first.text.data() - base);
            end = (size_t)(last.text.data() - base) + last.text.size();
        }

        out.push_back({start, end});
        prevEnd = end;
    }

    // A block with no lines at all still has one caret position.
    if (out.empty()) out.push_back({0, 0});
}

size_t lineOfIndex(const std::vector<std::pair<size_t, size_t>>& lines, size_t index)
{
    for (size_t i = 0; i < lines.size(); i++) {
        // The end is inclusive for a caret: sitting just past the last glyph of a wrapped
        // line belongs to that line, not the next one.
        if (index <= lines[i].second) return i;
    }
    return lines.size() - 1;
}

float measureRange(const EditContext& context, const std::string& text, size_t from, size_t to)
{
    if (!context.font || to <= from) return 0.0f;
    if (to > text.size()) to = text.size();
    return TextMetrics::measure(
        *context.font, context.style, std::string_view(text).substr(from, to - from)
    );
}

// The byte index whose glyph boundary sits nearest x, within one line. Walks rather than
// binary-searches: kerning makes each step depend on the one before it, so there is
// nothing to search over.
size_t indexAtOffset(const EditContext& context, std::string_view line, float x)
{
    if (!context.font || x <= 0.0f) return 0;

    float pen = 0.0f;
    uint32_t prev = 0;
    size_t i = 0;
    while (i < line.size()) {
        size_t start = i;
        uint32_t codepoint = Utf8::next(line, i);
        if (codepoint == 0) break;

        GlyphStep glyphStep = TextMetrics::step(*context.font, context.style, codepoint, prev);
        prev = glyphStep.kerningPrev;

        float next = pen + glyphStep.total();
        // Past the middle of a glyph the caret belongs after it, which is what makes a
        // click feel like it lands where the pointer is rather than always before.
        if (x < pen + (next - pen) * 0.5f) return start;
        if (x < next) return i;
        pen = next;
    }
    return line.size();
}

CaretPoint indexToPoint(
    const EditContext& context, const TextBlock& block, const std::string& text, size_t index
)
{
    std::vector<std::pair<size_t, size_t>> lines;
    lineRanges(block, lines);

    size_t lineIndex = lineOfIndex(lines, index);
    const std::vector<TextLine>& blockLines = block.getLines();

    CaretPoint point;
    point.x = measureRange(context, text, lines[lineIndex].first, index);
    if (lineIndex < blockLines.size()) {
        point.top = blockLines[lineIndex].top;
        point.height = blockLines[lineIndex].height;
    } else {
        point.height = TextMetrics::lineStep(*context.font, context.style);
    }
    return point;
}

size_t pointToIndex(
    const EditContext& context, const TextBlock& block, const std::string& text, float x, float y
)
{
    std::vector<std::pair<size_t, size_t>> lines;
    lineRanges(block, lines);
    const std::vector<TextLine>& blockLines = block.getLines();

    size_t lineIndex = 0;
    for (size_t i = 0; i < blockLines.size(); i++) {
        if (y < blockLines[i].top + blockLines[i].height) {
            lineIndex = i;
            break;
        }
        lineIndex = i;
    }
    if (lineIndex >= lines.size()) lineIndex = lines.size() - 1;

    auto [start, end] = lines[lineIndex];
    std::string_view line = std::string_view(text).substr(start, end - start);
    return start + indexAtOffset(context, line, x);
}

enum class CharClass
{
    Space,
    Word,
    Other
};

CharClass classOf(uint32_t codepoint)
{
    if (codepoint == ' ' || codepoint == '\t' || codepoint == '\n' || codepoint == '\r')
        return CharClass::Space;
    // Everything non-ASCII counts as a word character, so an accented letter is part of the
    // word it sits in rather than a boundary inside it.
    if (codepoint >= 0x80) return CharClass::Word;
    if (codepoint == '_') return CharClass::Word;
    if (codepoint >= '0' && codepoint <= '9') return CharClass::Word;
    if (codepoint >= 'A' && codepoint <= 'Z') return CharClass::Word;
    if (codepoint >= 'a' && codepoint <= 'z') return CharClass::Word;
    return CharClass::Other;
}

CharClass classAfter(const std::string& text, size_t index)
{
    if (index >= text.size()) return CharClass::Space;
    size_t cursor = index;
    return classOf(Utf8::next(text, cursor));
}

CharClass classBefore(const std::string& text, size_t index)
{
    if (index == 0) return CharClass::Space;
    size_t cursor = index;
    return classOf(Utf8::prev(text, cursor));
}

// Forward stops at the start of the next word, backward at the start of this one -- the
// asymmetry is deliberate and is what every editor does, because it makes Ctrl+Right then
// Ctrl+Left return to where you started.
size_t wordBoundary(const std::string& text, size_t index, bool forward)
{
    if (forward) {
        CharClass current = classAfter(text, index);
        while (index < text.size() && classAfter(text, index) == current &&
               current != CharClass::Space)
            Utf8::next(text, index);
        while (index < text.size() && classAfter(text, index) == CharClass::Space)
            Utf8::next(text, index);
        // A run of spaces with nothing after it still has to advance, or the key does
        // nothing at the end of the text.
        if (index == 0 && !text.empty()) Utf8::next(text, index);
        return index;
    }

    while (index > 0 && classBefore(text, index) == CharClass::Space) Utf8::prev(text, index);

    CharClass current = classBefore(text, index);
    while (index > 0 && classBefore(text, index) == current) Utf8::prev(text, index);
    return index;
}

void wordAt(const std::string& text, size_t index, size_t& outStart, size_t& outEnd)
{
    CharClass current = classAfter(text, index);
    if (current == CharClass::Space && index > 0) current = classBefore(text, index);

    outStart = index;
    while (outStart > 0 && classBefore(text, outStart) == current) Utf8::prev(text, outStart);

    outEnd = index;
    while (outEnd < text.size() && classAfter(text, outEnd) == current) Utf8::next(text, outEnd);
}

bool isPrintable(uint32_t codepoint)
{
    // Control characters arrive as key events, never as text -- except that a few
    // platforms still emit them here, and inserting one would put an unrenderable byte
    // in the caller's string.
    return codepoint >= 0x20 && codepoint != 0x7F;
}

void clampIndex(int& index, const std::string& text)
{
    if (index < 0) index = 0;
    if (index > (int)text.size()) index = (int)text.size();

    // The caller owns the string and may have rewritten it since, so a stored index can
    // land mid-sequence; walk it back onto a boundary rather than splitting a codepoint.
    while (index > 0 && index < (int)text.size() &&
           ((unsigned char)text[(size_t)index] & 0xC0) == 0x80)
        index--;
}

bool hasSelection(int caret, int anchor) { return caret != anchor; }
int selectionMin(int caret, int anchor) { return caret < anchor ? caret : anchor; }
int selectionMax(int caret, int anchor) { return caret > anchor ? caret : anchor; }

// The single place "what is selected goes away first" lives, so typing, Backspace, Delete
// and paste cannot disagree about it.
bool deleteSelection(std::string& text, int& caret, int& anchor)
{
    if (!hasSelection(caret, anchor)) return false;

    int from = selectionMin(caret, anchor);
    int to = selectionMax(caret, anchor);
    text.erase((size_t)from, (size_t)(to - from));
    caret = anchor = from;
    return true;
}

Window* activeWindow() { return ViewContext::get().getActiveWindow(); }

std::string selectedText(const std::string& text, int caret, int anchor)
{
    if (!hasSelection(caret, anchor)) return {};

    int from = selectionMin(caret, anchor);
    int to = selectionMax(caret, anchor);
    return text.substr((size_t)from, (size_t)(to - from));
}

void insertFiltered(
    std::string& text, int& caret, int& anchor, std::string_view incoming,
    const EditContext& context, bool& outChanged
)
{
    size_t i = 0;
    while (i < incoming.size()) {
        size_t start = i;
        uint32_t codepoint = Utf8::next(incoming, i);
        if (codepoint == 0) break;

        // A newline is content in a text area and nothing anywhere else, so pasting a
        // paragraph into a one-line field folds it flat instead of corrupting the layout.
        if (codepoint == '\n' || codepoint == '\r') {
            if (!context.multiline) {
                if (codepoint == '\r') continue;
                text.insert((size_t)caret, " ");
                caret += 1;
                anchor = caret;
                outChanged = true;
                continue;
            }
            if (codepoint == '\r') continue;
        } else if (!isPrintable(codepoint)) {
            continue;
        } else if (context.filter && !context.filter(codepoint)) {
            continue;
        }

        if (context.maxLength > 0 && Utf8::countCodepoints(text) >= context.maxLength) break;

        std::string_view bytes = incoming.substr(start, i - start);
        text.insert((size_t)caret, bytes);
        caret += (int)bytes.size();
        anchor = caret;
        outChanged = true;
    }
}

// Every caret move goes through here, so "shift extends, anything else collapses" is stated
// once rather than at each of the dozen places a caret can move.
void moveCaret(int& caret, int& anchor, int target, bool extend)
{
    caret = target;
    if (!extend) anchor = caret;
}

struct EditResult
{
    bool isChanged = false;
    bool isCommitted = false;
};

EditResult applyEdits(
    std::string& text, int& caret, int& anchor, float& desiredColumn, const EditContext& context
)
{
    Input::Uncaptured input = Input::get().uncaptured();
    EditResult result;

    bool shift = input.keyHeld(KeyCode::LeftShift) || input.keyHeld(KeyCode::RightShift);
    bool ctrl = input.keyHeld(KeyCode::LeftControl) || input.keyHeld(KeyCode::RightControl);

    // Any horizontal move or edit drops the remembered column; only Up/Down keep it, which
    // is what lets a walk down through short lines come back to the original column.
    bool resetColumn = false;

    if (ctrl && input.keyPressed(KeyCode::A)) {
        anchor = 0;
        caret = (int)text.size();
        resetColumn = true;
    }

    if (ctrl && (input.keyPressed(KeyCode::C) || input.keyPressed(KeyCode::X))) {
        Window* window = activeWindow();
        if (window && hasSelection(caret, anchor))
            window->setClipboardText(selectedText(text, caret, anchor));

        if (input.keyPressed(KeyCode::X) && deleteSelection(text, caret, anchor)) {
            result.isChanged = true;
            resetColumn = true;
        }
    }

    if (ctrl && input.keyPressed(KeyCode::V)) {
        Window* window = activeWindow();
        std::string pasted = window ? window->getClipboardText() : std::string();
        if (!pasted.empty()) {
            deleteSelection(text, caret, anchor);
            insertFiltered(text, caret, anchor, pasted, context, result.isChanged);
            resetColumn = true;
        }
    }

    // Typed text after the clipboard, so a Ctrl+V that some layout also reports as a
    // character cannot paste and type in the same frame.
    std::string_view typed = ctrl ? std::string_view {} : Input::get().uncaptured().getTypedText();
    if (!typed.empty()) {
        deleteSelection(text, caret, anchor);
        insertFiltered(text, caret, anchor, typed, context, result.isChanged);
        resetColumn = true;
    }

    if (input.keyRepeated(KeyCode::Enter) || input.keyRepeated(KeyCode::KPEnter)) {
        if (!context.multiline || ctrl) {
            result.isCommitted = true;
        } else {
            deleteSelection(text, caret, anchor);
            insertFiltered(text, caret, anchor, "\n", context, result.isChanged);
            resetColumn = true;
        }
    }

    if (input.keyRepeated(KeyCode::Backspace)) {
        if (deleteSelection(text, caret, anchor)) {
            result.isChanged = true;
        } else if (caret > 0) {
            size_t index = (size_t)caret;
            if (ctrl) index = wordBoundary(text, index, false);
            else Utf8::prev(text, index);

            text.erase(index, (size_t)caret - index);
            caret = anchor = (int)index;
            result.isChanged = true;
        }
        resetColumn = true;
    }

    if (input.keyRepeated(KeyCode::Delete)) {
        if (deleteSelection(text, caret, anchor)) {
            result.isChanged = true;
        } else if (caret < (int)text.size()) {
            size_t index = (size_t)caret;
            if (ctrl) index = wordBoundary(text, index, true);
            else Utf8::next(text, index);

            text.erase((size_t)caret, index - (size_t)caret);
            anchor = caret;
            result.isChanged = true;
        }
        resetColumn = true;
    }

    if (input.keyRepeated(KeyCode::Left)) {
        size_t index = (size_t)caret;
        // A plain Left with a selection collapses to its near edge rather than moving,
        // which is what makes arrowing out of a selection land where the eye expects.
        if (!shift && hasSelection(caret, anchor)) index = (size_t)selectionMin(caret, anchor);
        else if (ctrl) index = wordBoundary(text, index, false);
        else if (caret > 0) Utf8::prev(text, index);

        moveCaret(caret, anchor, (int)index, shift);
        resetColumn = true;
    }

    if (input.keyRepeated(KeyCode::Right)) {
        size_t index = (size_t)caret;
        if (!shift && hasSelection(caret, anchor)) index = (size_t)selectionMax(caret, anchor);
        else if (ctrl) index = wordBoundary(text, index, true);
        else if (caret < (int)text.size()) Utf8::next(text, index);

        moveCaret(caret, anchor, (int)index, shift);
        resetColumn = true;
    }

    // Everything below needs line geometry, and re-solving is cached, so this is one
    // layout per frame no matter how many of these keys fired.
    bool needsLines = input.keyRepeated(KeyCode::Home) || input.keyRepeated(KeyCode::End) ||
                      input.keyRepeated(KeyCode::Up) || input.keyRepeated(KeyCode::Down) ||
                      input.keyRepeated(KeyCode::PageUp) || input.keyRepeated(KeyCode::PageDown);

    if (needsLines) {
        const TextBlock& block = layoutFor(context, text);
        std::vector<std::pair<size_t, size_t>> lines;
        lineRanges(block, lines);

        clampIndex(caret, text);
        size_t lineIndex = lineOfIndex(lines, (size_t)caret);

        if (input.keyRepeated(KeyCode::Home)) {
            int target = ctrl ? 0 : (int)lines[lineIndex].first;
            moveCaret(caret, anchor, target, shift);
            resetColumn = true;
        }
        if (input.keyRepeated(KeyCode::End)) {
            int target = ctrl ? (int)text.size() : (int)lines[lineIndex].second;
            moveCaret(caret, anchor, target, shift);
            resetColumn = true;
        }

        int lineDelta = 0;
        if (input.keyRepeated(KeyCode::Up)) lineDelta -= 1;
        if (input.keyRepeated(KeyCode::Down)) lineDelta += 1;

        if (context.multiline) {
            float lineStep = TextMetrics::lineStep(*context.font, context.style);
            int page = lineStep > 0.0f ? (int)(context.viewportHeight / lineStep) : 1;
            if (page < 1) page = 1;

            if (input.keyRepeated(KeyCode::PageUp)) lineDelta -= page;
            if (input.keyRepeated(KeyCode::PageDown)) lineDelta += page;
        }

        if (lineDelta != 0) {
            if (desiredColumn <= NO_DESIRED_COLUMN)
                desiredColumn = measureRange(context, text, lines[lineIndex].first, (size_t)caret);

            int targetLine = (int)lineIndex + lineDelta;
            if (targetLine < 0) targetLine = 0;
            if (targetLine >= (int)lines.size()) targetLine = (int)lines.size() - 1;

            auto [start, end] = lines[(size_t)targetLine];
            std::string_view line = std::string_view(text).substr(start, end - start);
            int target = (int)(start + indexAtOffset(context, line, desiredColumn));

            moveCaret(caret, anchor, target, shift);
            // Explicitly not reset: this is the one move that keeps the column.
            resetColumn = false;
        }
    }

    if (resetColumn) desiredColumn = NO_DESIRED_COLUMN;

    clampIndex(caret, text);
    clampIndex(anchor, text);
    return result;
}

// Enough of the view moves to bring the caret back inside, and no more, so the text only
// shifts when it has to -- a view that recentred every keystroke reads as jitter.
float followCaret(float scroll, float caretMin, float caretMax, float viewport, float content)
{
    if (viewport <= 0.0f) return 0.0f;

    float margin = Math::min(SCROLL_MARGIN, viewport * 0.5f);
    if (caretMax - scroll > viewport - margin) scroll = caretMax - viewport + margin;
    if (caretMin - scroll < margin) scroll = caretMin - margin;

    return Math::clamp(scroll, 0.0f, Math::max(content - viewport + margin, 0.0f));
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

// Everything both fields do between opening their own container and closing it. The two
// differ only in their defaults and in how they scroll, so this is where all of the
// editing, the caret and the selection actually live.
struct FieldParts
{
    UILayoutConfig textLayout;
    UITextConfig textConfig;
    UILayoutConfig caretLayout;
    UIContainerStyleSpec caretStyle;
    UIContainerStyleSpec selectionStyle;
    UITextStyle placeholderStyle;
    const std::string* placeholder = nullptr;
};

// Returns the scroll the caller should apply, in text space, and reports what happened.
struct EditFrame
{
    EditResult result;
    Vec2 scroll = VEC2_ZERO;
    bool keepsFocus = false;
};

EditFrame runField(
    std::string& text, const UINodeState& field, const EditContext& context,
    const FieldParts& parts, Vec2 contentOrigin, Vec2 viewport
)
{
    UIStateStore& store = UIStateStore::get();
    EditFrame frame;

    int caret = store.getInt(field.persistentKey, "caret", (int)text.size());
    int anchor = store.getInt(field.persistentKey, "anchor", caret);
    clampIndex(caret, text);
    clampIndex(anchor, text);

    bool wasFocused = store.flag(field.persistentKey, "fieldFocused");
    float desiredColumn = store.value(field.persistentKey, "desiredColumn", NO_DESIRED_COLUMN);
    int caretBefore = caret;

    // A multi-line field is a real scroll container, so its offset is the one the solver
    // and the scrollbars already share -- keeping a second copy here would move the text
    // twice for every wheel tick. A single-line field has no bar and owns its own.
    Vec2 scroll = context.multiline ? store.systemState(field.persistentKey).scroll :
                                      store.getVec2(field.persistentKey, "textScroll");

    // The button stays down after a double click, so without this the very next frame's
    // drag would read as an ordinary one and collapse the word straight back to a caret.
    UIStateFlag wordSelected = store.flag(field.persistentKey, "wordSelected");

    // Pointer first, so a click that also lands a keystroke this frame edits at the new
    // caret rather than the old one.
    if (field.isPressed || field.isActive) {
        const TextBlock& block = layoutFor(context, text);
        Vec2 local = unscale(field.localMousePos) - contentOrigin + scroll;
        int hit = (int)pointToIndex(context, block, text, local.x, local.y);

        if (field.isDoubleClicked) {
            size_t start, end;
            wordAt(text, (size_t)hit, start, end);
            anchor = (int)start;
            caret = (int)end;
            wordSelected = true;
        } else if (field.isPressed) {
            bool shift = Input::get().uncaptured().keyHeld(KeyCode::LeftShift) ||
                         Input::get().uncaptured().keyHeld(KeyCode::RightShift);
            // Shift-click extends from wherever the selection was anchored, the same as
            // shift-arrow does -- so it must not move the anchor.
            caret = hit;
            if (!shift) anchor = hit;
            wordSelected = false;
        } else if (!wordSelected) {
            // Dragging only ever moves the caret; the anchor stays where the press put it,
            // which is the whole of mouse selection.
            caret = hit;
        }
        desiredColumn = NO_DESIRED_COLUMN;
    }
    if (!field.isActive) wordSelected = false;

    if (field.isFocused) {
        frame.result = applyEdits(text, caret, anchor, desiredColumn, context);

        if (frame.result.isCommitted || Input::get().uncaptured().keyPressed(KeyCode::Escape))
            UIManager::get().clearFocus();

        // Any edit restarts the blink, so the caret is never invisible at the moment the
        // user is looking for it.
        if (frame.result.isChanged || field.isPressed)
            store.value(field.persistentKey, "caretBlinkStart") = Time::get().currTime();
    }

    // What gets remembered is whether focus is still held *after* this frame's keys, not
    // what it was when the node was declared. Recording the latter would make a commit key
    // fire twice: once here, and once more next frame as a focus loss.
    frame.keepsFocus = field.isFocused && UIManager::get().getFocusedKey() == field.persistentKey;
    if (wasFocused && !frame.keepsFocus) frame.result.isCommitted = true;

    store.flag(field.persistentKey, "fieldFocused") = frame.keepsFocus;
    store.setInt(field.persistentKey, "caret", caret);
    store.setInt(field.persistentKey, "anchor", anchor);
    store.value(field.persistentKey, "desiredColumn") = desiredColumn;

    const TextBlock& block = layoutFor(context, text);
    CaretPoint point = indexToPoint(context, block, text, (size_t)caret);
    Vec2 content = block.getBounds();

    // A one-line field has no scrollbar to reach for and nowhere else for a wheel to go,
    // so the wheel drives it sideways -- but only while it actually overflows, or hovering
    // a short field would swallow the gesture from the panel behind it.
    if (!context.multiline && field.isHovered && content.x > viewport.x) {
        float wheel = field.scrollDelta.x + field.scrollDelta.y;
        scroll.x -= wheel * SCROLL_WHEEL_STEP;
    }

    // The view chases the caret only on the frames the caret actually moved, never simply
    // because the field is focused. Chasing it every frame would mean the wheel could not
    // move the view off the caret at all: the scroll would snap back the frame after each
    // tick, which reads as the whole area being stuck to the caret. Taking focus counts as
    // a move so a field tabbed into shows its caret straight away.
    bool caretMoved = caret != caretBefore || frame.result.isChanged || !wasFocused;
    if (frame.keepsFocus && caretMoved) {
        if (context.multiline) {
            scroll.y =
                followCaret(scroll.y, point.top, point.top + point.height, viewport.y, content.y);
        } else {
            scroll.x = followCaret(scroll.x, point.x, point.x, viewport.x, content.x);
        }
    }

    if (!context.multiline)
        scroll.x = Math::clamp(scroll.x, 0.0f, Math::max(content.x - viewport.x, 0.0f));

    if (context.multiline) {
        // Written back to the same place the solver reads it, so resolveScroll clamps it
        // against the real content size this frame rather than a frame later.
        store.systemState(field.persistentKey).scroll = scroll;
    } else {
        store.setVec2(field.persistentKey, "textScroll", scroll);
    }
    frame.scroll = scroll;

    // Highlights come before the text leaf so preorder paint order puts them behind the
    // glyphs rather than over them.
    if (frame.keepsFocus && hasSelection(caret, anchor)) {
        std::vector<std::pair<size_t, size_t>> lines;
        lineRanges(block, lines);
        const std::vector<TextLine>& blockLines = block.getLines();

        size_t from = (size_t)selectionMin(caret, anchor);
        size_t to = (size_t)selectionMax(caret, anchor);

        for (size_t i = 0; i < lines.size() && i < blockLines.size(); i++) {
            size_t lineFrom = from > lines[i].first ? from : lines[i].first;
            size_t lineTo = to < lines[i].second ? to : lines[i].second;
            if (lineFrom > lineTo) continue;

            float x0 = measureRange(context, text, lines[i].first, lineFrom);
            float x1 = measureRange(context, text, lines[i].first, lineTo);
            // A selection that swallowed a line break shows as a sliver past the last
            // glyph, which is how every editor says "the newline is in here too".
            if (lineTo == lines[i].second && to > lines[i].second) x1 += SCROLL_MARGIN * 0.5f;
            if (x1 <= x0) continue;

            UILayoutConfig highlight;
            highlight.width = UISizeSpec::fixed(x1 - x0);
            highlight.height = UISizeSpec::fixed(blockLines[i].height);
            highlight.isFloating = true;
            highlight.floating = UIFloatingConfig {
                .offset = contentOrigin + Vec2(x0, blockLines[i].top) - scroll,
            };

            openContainer(highlight, parts.selectionStyle);
            closeContainer();
        }
    }

    bool showPlaceholder = text.empty() && parts.placeholder && !parts.placeholder->empty();
    UITextConfig textConfig = parts.textConfig;
    textConfig.text = showPlaceholder ? *parts.placeholder : text;
    textConfig.wrapEnabled = context.multiline;
    // Clip bounds the glyphs to the *leaf's* own box, which is right for a wrapped area
    // whose leaf is as tall as its content, and wrong for a one-line field: there the leaf
    // would be the viewport, so scrolling would eat the text from the right instead of
    // moving a window across it. The field's own Hidden overflow does that job already.
    textConfig.overflow = context.multiline ? TextOverflow::Clip : TextOverflow::Visible;
    if (showPlaceholder) textConfig.style.combine(parts.placeholderStyle);

    UILayoutConfig textLayout = parts.textLayout;
    // The leaf is an ordinary child, so in a scroll container positionChildren has already
    // subtracted the offset for it -- doing it here as well would move the text twice. The
    // caret and the highlights below do need it by hand: floating children do not scroll.
    if (!context.multiline) {
        // Fixed at the text's own width, so the leaf is the whole string and the offset
        // slides it under the field's clip. A Fit leaf would be squeezed to the viewport
        // by the shrink pass and take the glyphs with it.
        textLayout.width = UISizeSpec::fixed(Math::max(content.x, viewport.x));
        textLayout.offset = -scroll;
    }
    addTextLeaf(textLayout, textConfig);

    if (frame.keepsFocus) {
        float blinkStart = store.value(field.persistentKey, "caretBlinkStart");
        float phase = Time::get().currTime() - blinkStart;
        bool visible = Math::mod(phase, CARET_BLINK_SECONDS * 2.0f) < CARET_BLINK_SECONDS;

        if (visible) {
            UILayoutConfig caretLayout = parts.caretLayout;
            caretLayout.width = UISizeSpec::fixed(CARET_WIDTH);
            caretLayout.height = UISizeSpec::fixed(point.height);
            caretLayout.isFloating = true;
            caretLayout.floating = UIFloatingConfig {
                .offset = contentOrigin + Vec2(point.x, point.top) - scroll,
            };

            openContainer(caretLayout, parts.caretStyle);
            closeContainer();
        }
    }

    return frame;
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
        .selectionStyle = {.backgroundColor = colors.accentMuted},
        .placeholderStyle = {.color = colors.foregroundSubtle},
    };

    defaultConfig.fieldLayout.combine(config.fieldLayout);
    defaultConfig.fieldStyle.combine(config.fieldStyle);
    defaultConfig.textLayout.combine(config.textLayout);
    defaultConfig.textConfig.style.combine(config.textConfig.style);
    defaultConfig.caretLayout.combine(config.caretLayout);
    defaultConfig.caretStyle.combine(config.caretStyle);
    defaultConfig.selectionStyle.combine(config.selectionStyle);
    defaultConfig.placeholderStyle.combine(config.placeholderStyle);

    UINodeState field =
        openContainer(defaultConfig.fieldLayout, defaultConfig.fieldStyle, config.key);

    UITextConfig textConfig = config.textConfig;
    textConfig.style = defaultConfig.textConfig.style;

    EditContext context;
    context.font = resolveFieldFont(textConfig);
    context.style = designStyle(textConfig.style);
    context.multiline = false;
    context.maxLength = config.maxLength;
    context.filter = config.filter;

    const UIEdges& padding = *defaultConfig.fieldLayout.padding;
    Vec2 viewport(unscale(field.size.x) - padding.horizontal(), unscale(field.size.y));
    // A single-line field centres its one line, so the caret and the highlight have to
    // start from the same place the glyphs do rather than from the top of the box.
    float lineHeight = context.font ? TextMetrics::lineStep(*context.font, context.style) : 0.0f;
    Vec2 contentOrigin(padding.left, Math::max((viewport.y - lineHeight) * 0.5f, 0.0f));

    FieldParts parts;
    parts.textLayout = defaultConfig.textLayout;
    parts.textConfig = textConfig;
    parts.caretLayout = defaultConfig.caretLayout;
    parts.caretStyle = defaultConfig.caretStyle;
    parts.selectionStyle = defaultConfig.selectionStyle;
    parts.placeholderStyle = defaultConfig.placeholderStyle;
    parts.placeholder = &config.placeholder;

    EditFrame frame = runField(text, field, context, parts, contentOrigin, viewport);
    closeContainer();

    return {
        .node = field,
        .isChanged = frame.result.isChanged,
        .isEditing = frame.keepsFocus,
        .isReleased = frame.result.isCommitted,
    };
}

UIInputState textArea(std::string& text, const TextAreaConfig& config)
{
    const UIThemeColors& colors = UITheming::colors();
    const UIThemeMetrics& metrics = UITheming::metrics();

    UITextConfig textConfig = config.textConfig;
    UITextStyle bodyStyle = UITheming::textStyle("body");
    bodyStyle.combine(config.textConfig.style);
    textConfig.style = bodyStyle;

    EditContext context;
    context.font = resolveFieldFont(textConfig);
    context.style = designStyle(textConfig.style);
    context.multiline = true;
    context.maxLength = config.maxLength;

    float lineStep = context.font ? TextMetrics::lineStep(*context.font, context.style) : 16.0f;
    uint rows = config.rows > 0 ? config.rows : 1;

    TextAreaConfig defaultConfig = {
        .areaLayout =
            {
                         .width = UISizeSpec::grow(),
                         .height = UISizeSpec::fixed(lineStep * (float)rows + metrics.spacing.sm * 2.0f),
                         .padding = UIEdges(metrics.spacing.sm),
                         .direction = UILayoutDirection::Column,
                         },
        .areaStyle =
            {
                         .backgroundColor = colors.surfaceSunken,
                         .borderColor = colors.border,
                         .borderWidth = metrics.borderWidth.thin,
                         .borderRadius = metrics.radius.sm,
                         .overflow = UIOverflow::Scroll,
                         .blockInput = true,
                         .focusable = true,
                         .cursor = UICursor::Text,
                         .onHover = {.borderColor = colors.borderStrong},
                         .onFocused = {.borderColor = colors.borderFocus},
                         },
        .textLayout = {.width = UISizeSpec::grow()},
        .textConfig = textConfig,
        .caretLayout = {},
        .caretStyle = {.backgroundColor = colors.foreground},
        .selectionStyle = {.backgroundColor = colors.accentMuted},
        .placeholderStyle = {.color = colors.foregroundSubtle},
    };

    defaultConfig.areaLayout.combine(config.areaLayout);
    defaultConfig.areaStyle.combine(config.areaStyle);
    defaultConfig.textLayout.combine(config.textLayout);
    defaultConfig.caretLayout.combine(config.caretLayout);
    defaultConfig.caretStyle.combine(config.caretStyle);
    defaultConfig.selectionStyle.combine(config.selectionStyle);
    defaultConfig.placeholderStyle.combine(config.placeholderStyle);

    UINodeState area = openContainer(defaultConfig.areaLayout, defaultConfig.areaStyle, config.key);

    const UIEdges& padding = *defaultConfig.areaLayout.padding;
    Vec2 viewport(
        unscale(area.size.x) - padding.horizontal(), unscale(area.size.y) - padding.vertical()
    );
    context.wrapWidth = Math::max(viewport.x, 1.0f);
    context.viewportHeight = viewport.y;

    FieldParts parts;
    parts.textLayout = defaultConfig.textLayout;
    parts.textConfig = defaultConfig.textConfig;
    parts.caretLayout = defaultConfig.caretLayout;
    parts.caretStyle = defaultConfig.caretStyle;
    parts.selectionStyle = defaultConfig.selectionStyle;
    parts.placeholderStyle = defaultConfig.placeholderStyle;
    parts.placeholder = &config.placeholder;

    // A floating child anchors to its parent's *outer* rect, but the text leaf sits inside
    // the padding -- so the caret and the highlights have to be pushed in by hand or they
    // sit one padding up and to the left of the glyphs they mark.
    EditFrame frame = runField(text, area, context, parts, padding.topLeft(), viewport);
    closeContainer();

    return {
        .node = area,
        .isChanged = frame.result.isChanged,
        .isEditing = frame.keepsFocus,
        .isReleased = frame.result.isCommitted,
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
        // Two of them stack inside one control height, so the height is fixed by that; the
        // width is the only room there is to make them worth aiming at.
        UILayoutConfig stepperLayout = {
            .width = UISizeSpec::fixed(metrics.controlHeight),
            .height = UISizeSpec::fixed((metrics.controlHeight - STEPPER_GAP) * 0.5f),
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

        UITextStyle glyphStyle = UITheming::textStyle("body");
        glyphStyle.color = colors.foreground;

        openContainer({.gap = STEPPER_GAP, .direction = UILayoutDirection::Column});

        UINodeState up = openContainer(stepperLayout, stepperStyle, "stepUp");
        text("+", {.textConfig = {.style = glyphStyle}});
        closeContainer();

        UINodeState down = openContainer(stepperLayout, stepperStyle, "stepDown");
        text("-", {.textConfig = {.style = glyphStyle}});
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
