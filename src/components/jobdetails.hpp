#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>

#include "../api/slurmjobs.hpp"
#include "reason_decoder.hpp"

namespace ui {

inline ftxui::Component jobdetails(const api::DetailedJob& job) {
    using namespace ftxui;

    return Renderer([job] {
        Color status_color = Color::Default;
        if      (job.status == "RUNNING")   status_color = Color::Green;
        else if (job.status == "PENDING")   status_color = Color::Yellow;
        else if (job.status == "COMPLETED") status_color = Color::Blue;
        else if (job.status == "FAILED")    status_color = Color::Red;
        else if (job.status == "CANCELLED") status_color = Color::Magenta;

        std::vector<Element> elements = {
            hbox({text("Job ID: "), text(job.id) | color(Color::Magenta)}),
            text("Name: " + job.name),
            text("Submit time: " + job.submitTime),
            text("Nodes: " + std::to_string(job.nodes)),
            hbox({
                text("Time: "),
                text(job.elapsedTime.empty() ? "N/A" : job.elapsedTime) | color(Color::BlueLight),
                text(" / "),
                text(job.maxTime),
            }),
            hbox({
                text("Partition: "), text(job.partition),
            }),
            hbox({
                text("Constraints: "), text(job.constraints.empty() ? "None" : job.constraints),
            }),
            hbox({text("Status: "), text(job.status) | color(status_color)}),
        };

        if (job.status == "PENDING" && !job.reason.empty() && job.reason != "None") {
            auto info = decodeReason(job.reason);
            elements.push_back(hbox({
                text("Reason: "),
                text(job.reason) | bold | color(Color::Yellow),
                text(" - "),
                text(info.description) | dim,
            }));
            if (!info.suggestion.empty()) {
                elements.push_back(hbox({
                    text("  -> ") | color(Color::Green),
                    text(info.suggestion) | color(Color::Green),
                }));
            }
        }

        return vbox(elements, text(" "));
    });
}

}
