#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

namespace ui {

inline ftxui::Component helpModal(std::function<void()> on_close) {
    using namespace ftxui;

    auto content = Renderer([&] {
        Element help_box_1 = vbox({
            text("HELP") | bold | center,
            text(""),
            text("Navigation") | bold | color(Color::BlueLight),
            hbox({text("  Arrows          "), text("Navigate job list") | dim}),
            hbox({text("  Wheel           "), text("Scroll details/logs") | dim}),
            text(""),
        });

        Element help_box_2 = vbox({
            text(""),
            text("Actions") | bold | color(Color::BlueLight),
            hbox({text("  r               "), text("Refresh jobs") | dim}),
            hbox({text("  c               "), text("Cancel job (with confirmation)") | dim}),
            text(""),
        });

        Element help_box_3 = vbox({
            text(""),
            text("Views") | bold | color(Color::BlueLight),
            hbox({text("  p               "), text("Partitions view (sinfo)") | dim}),
            hbox({text("  l               "), text("Logs view (stdout/stderr, scrollable)") | dim}),
            hbox({text("  a               "), text("History (sacct) - filter with ←→") | dim}),
            hbox({text("  u               "), text("User quota (sacctmgr limits)") | dim}),
            text(""),
        });

        Element help_box_4 = vbox({
            text(""),
            text("Other") | bold | color(Color::BlueLight),
            hbox({text("  h / ?           "), text("Show this help") | dim}),
            hbox({text("  q / Escape      "), text("Quit application") | dim}),
            text(""),
            text(""),
            text("Press any key to close") | dim | center,
        });

        Element help_box_total = vbox({
            help_box_1,
            separator(),
            help_box_2,
            separator(),
            help_box_3,
            separator(),
            help_box_4,
        });

        return hbox({
            text("  "),
            help_box_total,
            text("  ")             
        })| border | clear_under | center;
;
    });

    return CatchEvent(content, [on_close](Event e) {
        if (e.is_character() || e == Event::Escape || e == Event::Return) {
            on_close();
            return true;
        }
        return false;
    });
}

}