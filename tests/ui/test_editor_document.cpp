#include <catch2/catch_test_macros.hpp>

#include "ui/editor_window/editor_document.hpp"
#include "ui/editor_window/grid_geometry.hpp"

using namespace sky;

namespace {

KeyMap sample()
{
    return KeyMap({{500, {"i"}}, {0, {"y", "u"}}, {1000, {}}});
}

}  // namespace

TEST_CASE("editor cells are the key map times in order")
{
    const EditorDocument document(sample());
    REQUIRE(document.cellCount() == 3);
    CHECK(document.timeAt(0) == 0);
    CHECK(document.timeAt(1) == 500);
    CHECK(document.keysAt(0) == std::vector<std::string>{"y", "u"});
    CHECK(document.keysAt(2).empty());
}

TEST_CASE("toggling adds and removes keys and tracks unsaved changes")
{
    EditorDocument document(sample());
    CHECK_FALSE(document.isDirty());
    CHECK(document.toggle(2, "p"));
    CHECK(document.hasKey(2, "p"));
    CHECK(document.isDirty());
    CHECK_FALSE(document.toggle(2, "p"));
    CHECK_FALSE(document.isDirty());
    CHECK_FALSE(document.toggle(0, "q"));
    CHECK_FALSE(document.toggle(9, "y"));
}

TEST_CASE("discard really restores the saved notes")
{
    EditorDocument document(sample());
    document.toggle(0, "y");
    document.toggle(1, "/");
    document.discard();
    CHECK(document.keysAt(0) == std::vector<std::string>{"y", "u"});
    CHECK(document.keysAt(1) == std::vector<std::string>{"i"});
    CHECK_FALSE(document.isDirty());
}

TEST_CASE("saving makes the current notes the new baseline")
{
    EditorDocument document(sample());
    document.toggle(1, "o");
    document.markSaved();
    CHECK_FALSE(document.isDirty());
    document.discard();
    CHECK(document.hasKey(1, "o"));
    CHECK(document.keyMap().entries()[1].keys == std::vector<std::string>{"i", "o"});
}

TEST_CASE("grid geometry lays out four 16:9 columns and hit-tests cells")
{
    const GridGeometry grid(870, 9);
    CHECK(grid.rowCount() == 3);
    CHECK(grid.cellWidth() == (870 - 10 - 45) / 4.0);
    const QRectF first = grid.cellRect(0);
    CHECK(first.left() == 5);
    CHECK(first.height() == first.width() * 9 / 16);
    CHECK(grid.cellAt(first.center()) == 0);
    CHECK(grid.cellAt(grid.cellRect(8).center()) == 8);
    CHECK(grid.cellAt(grid.captionRect(0).center()) == -1);
    CHECK(grid.cellAt(QPointF(grid.cellRect(3).right() + 5, 20)) == -1);
    const QRectF slot = grid.dotSlot(first, 14);
    CHECK(slot.right() <= first.right() - GridGeometry::kCellPadding + 0.001);
    CHECK(slot.bottom() <= first.bottom() - GridGeometry::kCellPadding + 0.001);
}
