#include "graphics/ui/widgets/UIDemoWindow.hpp"
#include "graphics/ui/widgets/UIWidgets.hpp"

namespace Engine {

namespace UIWidgets {

void demoWindow()
{
    static int selectedMenu = 0;
    static float sliderValue = 0.5f;
    static int stepCount = 3;
    static bool checkedA = true;
    static bool checkedB = false;
    static int radioChoice = 1;
    static int filterChoice = 1;
    static int menuChoice = 0;
    static Color pickedColor(0.30f, 0.62f, 0.95f);
    static Color popupColor(0.95f, 0.55f, 0.25f);

    openWindow("Demo Window");

    toolbarMenu({"Widgets", "Sliders", "Color"}, selectedMenu);

    switch (selectedMenu) {
    case 0: {
        text("Text leaf and simple widgets");
        horizontalDivider();

        button("Button");

        openContainer({.gap = 10.0f, .direction = UILayoutDirection::Row});
        tooltip(
            "A tooltip declared inside the window, escaping its clip",
            button("Hover for a tooltip").isHovered
        );

        UINodeState centered = button("Hover for a centered tooltip");
        tooltip(
            "Anchored to the button's centre and aligned by its own middle, so it sits on the "
            "button instead of following the cursor",
            centered.isHovered,
            {.anchor = centered.pos + centered.size * 0.5f,
             .anchorAlignX = UIAlign::Center,
             .anchorAlignY = UIAlign::Center}
        );

        UINodeState above = button("Tooltip above");
        tooltip(
            "Anchored to the button's top edge, aligned by its own bottom", above.isHovered,
            {.anchor = above.pos + Vec2(above.size.x * 0.5f, 0.0f),
             .anchorOffset = Vec2(0.0f, -6.0f),
             .anchorAlignX = UIAlign::Center,
             .anchorAlignY = UIAlign::End}
        );
        closeContainer();

        horizontalDivider();

        checkBox("Checkbox A", checkedA);
        checkBox("Checkbox B", checkedB);

        horizontalDivider();

        radioGroup({"First", "Second", "Third"}, radioChoice);

        horizontalDivider();

        openContainer({.gap = 10.0f, .alignCross = UIAlign::Center});
        dropdown({"Nearest", "Bilinear", "Trilinear", "Anisotropic 16x"}, filterChoice);
        dropdown(
            {"New scene", "Open scene", "Save scene as a very long file name"}, menuChoice,
            {.label = "File"}
        );
        closeContainer();
        break;
    }

    case 1:
        text("Sliders");
        horizontalDivider();

        sliderFloat("Float", sliderValue, 0.0f, 1.0f);
        sliderInt("Int", stepCount, 0, 10);
        break;

    case 2:
        text("Popup picker");
        colorPickerPopup(popupColor);

        horizontalDivider();

        text("Inline picker");
        colorPicker(pickedColor);
        break;

    default: break;
    }

    closeWindow();
}

}   // namespace UIWidgets

}   // namespace Engine
