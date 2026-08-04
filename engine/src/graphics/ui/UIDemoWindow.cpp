#include "graphics/ui/UIDemoWindow.hpp"
#include "graphics/ui/UIWidgets.hpp"

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
    static Color pickedColor(0.30f, 0.62f, 0.95f);

    bool hoveringTooltipTarget = false;

    openWindow("Demo Window");

    toolbarMenu({"Widgets", "Sliders", "Color"}, selectedMenu);

    switch (selectedMenu) {
    case 0:
        text("Text leaf and simple widgets");
        horizontalDivider();

        button("Button");
        hoveringTooltipTarget = button("Hover for a tooltip").isHovered;

        horizontalDivider();

        checkBox("Checkbox A", checkedA);
        checkBox("Checkbox B", checkedB);

        horizontalDivider();

        radioGroup({"First", "Second", "Third"}, radioChoice);
        break;

    case 1:
        text("Sliders");
        horizontalDivider();

        sliderFloat("Float", sliderValue, 0.0f, 1.0f);
        sliderInt("Int", stepCount, 0, 10);
        break;

    case 2:
        text("Color picker");
        horizontalDivider();

        colorPicker(pickedColor);
        break;

    default: break;
    }

    closeWindow();

    // Outside the window, since the window clips its own content and a tooltip has to
    // be able to sit past that edge.
    tooltip("A floating tooltip that follows the cursor", hoveringTooltipTarget);
}

}   // namespace UIWidgets

}   // namespace Engine
