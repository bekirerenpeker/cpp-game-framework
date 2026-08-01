#include "EngineInclude.hpp"
#include "test_funcs.hpp"

using namespace Engine;

namespace {

// Container-only trees, so this needs no window, no GL context and no font -- the
// solver is pure arithmetic and every number below can be checked by hand.
void logTree(const char* label, IdType root, Vec2 rootSize)
{
    const std::vector<UILayoutNode>& nodes = UILayoutCalculator::get().calculate(root, rootSize);
    LOG_INFO("=== {} (root {}x{}) ===", label, rootSize.x, rootSize.y);
    for (size_t i = 0; i < nodes.size(); i++) {
        LOG_INFO(
            "  [{}] pos ({}, {}) size ({} x {})  minW {} maxW {}", i, nodes[i].x, nodes[i].y,
            nodes[i].width, nodes[i].height, nodes[i].minWidth, nodes[i].maxWidth
        );
    }
}

UILayoutConfig row(float gap = 0.0f, UIEdges padding = {})
{
    UILayoutConfig config;
    config.direction = UILayoutDirection::Row;
    config.gap = gap;
    config.padding = padding;
    return config;
}

UILayoutConfig fixedBox(float w, float h)
{
    UILayoutConfig config;
    config.width = UISizeSpec::fixed(w);
    config.height = UISizeSpec::fixed(h);
    return config;
}

}   // namespace

int ui_layout_test()
{
    UIManager& ui = UIManager::get();

    // 1. Fit row: the parent's intrinsic width is the children plus the gaps, and a
    //    Fixed root width is what the whole solve is anchored to.
    {
        UILayoutConfig rootLayout = row(10.0f, UIEdges(8.0f));
        rootLayout.width = UISizeSpec::fit();
        rootLayout.height = UISizeSpec::fit();
        IdType root = ui.addContainer(INVALID_ID, rootLayout);
        ui.addContainer(root, fixedBox(50.0f, 20.0f));
        ui.addContainer(root, fixedBox(70.0f, 30.0f));
        ui.addContainer(root, fixedBox(30.0f, 10.0f));
        // 50+70+30 = 150, + 2 gaps (20) + padding (16) = 186 wide; tallest child 30 + 16 = 46
        logTree("fit row: expect root 186 x 46, children at x=8, 68, 148", root, Vec2(186, 46));
    }

    // 2. Two growers split the leftover evenly.
    {
        IdType root = ui.addContainer(INVALID_ID, row());
        UILayoutConfig grow;
        grow.width = UISizeSpec::grow();
        grow.height = UISizeSpec::fixed(20.0f);
        ui.addContainer(root, grow);
        ui.addContainer(root, grow);
        logTree("two growers in 300: expect 150 + 150", root, Vec2(300, 40));
    }

    // 3. A grower capped by its own sizing.max stops absorbing, and the surplus is
    //    left for alignMain to place rather than being forced into a sibling.
    {
        UILayoutConfig rootLayout = row();
        rootLayout.alignMain = UIAlign::End;
        IdType root = ui.addContainer(INVALID_ID, rootLayout);

        UILayoutConfig capped;
        capped.width = UISizeSpec::grow();
        capped.width.max = 80.0f;
        capped.height = UISizeSpec::fixed(20.0f);
        ui.addContainer(root, capped);
        ui.addContainer(root, fixedBox(40.0f, 20.0f));
        logTree(
            "capped grower + alignMain End: expect 80 + 40 pushed to x=180", root, Vec2(300, 40)
        );
    }

    // 4. Column with a centred cross axis.
    {
        UILayoutConfig rootLayout;
        rootLayout.direction = UILayoutDirection::Column;
        rootLayout.alignCross = UIAlign::Center;
        rootLayout.gap = 5.0f;
        IdType root = ui.addContainer(INVALID_ID, rootLayout);
        ui.addContainer(root, fixedBox(100.0f, 20.0f));
        ui.addContainer(root, fixedBox(60.0f, 20.0f));
        logTree(
            "column, cross centre in 200: expect x=50 and x=70, y=0 and y=25", root, Vec2(200, 100)
        );
    }

    // 5. Percent resolves against the parent's inner box.
    {
        IdType root = ui.addContainer(INVALID_ID, row(0.0f, UIEdges(10.0f)));
        UILayoutConfig half;
        half.width = UISizeSpec::percent(0.5f);
        half.height = UISizeSpec::fixed(20.0f);
        ui.addContainer(root, half);
        logTree("percent 0.5 of inner 180: expect width 90", root, Vec2(200, 40));
    }

    // 6. A floating child is excluded from the main-axis sum, the max and the
    //    n-1 gap fence-post, so its siblings lay out as if it were not there.
    {
        IdType root = ui.addContainer(INVALID_ID, row(10.0f));
        ui.addContainer(root, fixedBox(50.0f, 20.0f));
        ui.addContainer(root, fixedBox(50.0f, 20.0f));

        UILayoutConfig floater = fixedBox(30.0f, 30.0f);
        floater.isFloating = true;
        floater.floating.anchorX = UIAlign::End;
        floater.floating.selfX = UIAlign::End;
        ui.addContainer(root, floater);
        logTree(
            "floating child: siblings still at x=0 and x=60, floater at x=170", root, Vec2(200, 40)
        );
    }

    // 7. Overflow: three 100-wide Fit children in a 200 root are levelled down
    //    toward their content floor, which for a Fixed child is its own size.
    {
        IdType root = ui.addContainer(INVALID_ID, row());
        UILayoutConfig flexible;
        flexible.width = UISizeSpec::fit();
        flexible.width.min = 20.0f;
        flexible.height = UISizeSpec::fixed(20.0f);
        for (int i = 0; i < 3; i++) ui.addContainer(root, flexible);
        logTree(
            "three Fit children with min 20 in 200: expect no overflow past floors", root,
            Vec2(200, 40)
        );
    }

    return 0;
}
