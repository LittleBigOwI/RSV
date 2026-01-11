#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>

#include "../api/slurmjobs.hpp"

namespace ui {

inline ftxui::Component jobdetails(const api::DetailedJob& job) {
    using namespace ftxui;

    return Renderer([job] {
        Color status_color = Color::Default;
        if (job.status == "RUNNING")      status_color = Color::Green;
        else if (job.status == "PENDING") status_color = Color::Yellow;
        else if (job.status == "COMPLETED") status_color = Color::Blue;
        else if (job.status == "FAILED")  status_color = Color::Red;
        else if (job.status == "CANCELLED") status_color = Color::Magenta;

        auto lines = vbox({
            hbox({text("Job ID: "), text(job.id) | color(Color::Magenta)}),
            text("Nom: " + job.name),
            text("Date de soumission: " + job.submitTime),
            text("Nombre de noeuds: " + std::to_string(job.nodes)),
            text("Durée MAX: " + job.maxTime),
            text("Partition: " + job.partition),
            hbox({text("Status: "), text(job.status) | color(status_color)}),
            text("Contraintes: " + job.constraints)
        });

        return lines | flex | size(HEIGHT, EQUAL, 10);
    });
}

}
