#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "../../api/slurmjobs.hpp"

namespace ui {
using namespace ftxui;

inline Element cancelModal(const Element& base, const api::DetailedJob& job) {
    Element title = text("CANCEL") | bold | color(Color::Red) | center;

    Element confirm_text = hbox({
        text("Are you sure you want to cancel job "),
        text(job.name + " (") | bold,
        text(job.id) | bold | color(Color::Magenta),
        text(")") | bold,
        text("?"),
    }) | center;

    Element confirm_text_2 = text("This action is undoable.") | bold | center;

    Element footer = hbox({
        text("Y") | bold | color(Color::Green),
        text(": YES  ") | dim,
        text("N") | bold | color(Color::Red),
        text(": NO") | dim,
    }) | center;

    return dbox({
        base,
        hbox({
            text("   "),
            vbox({
                title,
                text(""),
                text(""),
                confirm_text,
                text(""),
                confirm_text_2,
                text(""),
                text(""),
                text(""),
                footer
            }),
            text("   ")
        }) | border | clear_under | center,
    });
}

}
