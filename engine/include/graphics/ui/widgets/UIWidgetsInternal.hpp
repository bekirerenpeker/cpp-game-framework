#pragma once

namespace Engine {

class GlTexture;

namespace UIWidgets {

// Shared between widget translation units, and deliberately not part of UIWidgets.hpp:
// these are the built-in widgets' own assets, not something a caller composes with.
// Each is loaded once and then leaked, since a static GlTexture would run its GL delete
// after the context is already gone.
GlTexture* dragHandleTexture();
GlTexture* dropdownTexture();

}   // namespace UIWidgets

}   // namespace Engine
