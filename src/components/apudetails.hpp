#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>

#include "../api/slurmjobs.hpp"
#include "reason_decoder.hpp"

namespace ui {

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

inline ftxui::Component apudetails(const api::DetailedJob& job) {
    using namespace ftxui;

    return Renderer([job] {
        std::string apuType = startsWith(job.node_allocations.at(0).node_name, "romeo-c") ? "ARM (romeo-a)" : "COMPUTE (romeo-c)";

        return vbox({
            hbox({text("APU Type: "), text(apuType) | color(Color::Magenta)}),
            hbox({text("CPUs: "), text(std::to_string(job.cpus)) | dim}),
            hbox({text("GPUs: "), text(std::to_string(job.gpus)) | dim}),
        }) | flex;
    });
}

}
