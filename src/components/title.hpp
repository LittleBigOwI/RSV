#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace ui {

inline ftxui::Component title(std::string content) {
    using namespace ftxui;

    return Renderer([content] {
        return hbox({
            filler(),
            text(content) | bold | center,
            filler()
        }) | size(HEIGHT, EQUAL, 3);
    });
}

}
