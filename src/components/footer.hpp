#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace ui {
using namespace ftxui;

inline Element footer() {
    return hbox({
        text(" q") | bold | color(Color::Blue),
        text(":Quit") | dim,
        text("  "),
        text("r") | bold | color(Color::Blue),
        text(":Refresh") | dim,
        text("  "),
        text("h") | bold | color(Color::Blue),
        text(":Help") | dim,
        text("  "),
        text("c") | bold | color(Color::Blue),
        text(":Cancel") | dim,
        text("  "),
        text("p") | bold | color(Color::Blue),
        text(":Parts") | dim,
        text("  "),
        text("l") | bold | color(Color::Blue),
        text(":Logs") | dim,
        text("  "),
        text("a") | bold | color(Color::Blue),
        text(":History") | dim,
        text("  "),
        text("u") | bold | color(Color::Blue),
        text(":Quota") | dim,
        text("  "),
        filler(),
    });
}

}
