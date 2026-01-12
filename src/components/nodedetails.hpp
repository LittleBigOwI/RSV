#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include "../api/slurmjobs.hpp"

namespace ui {
using namespace ftxui;

inline Component nodedetails(const api::DetailedJob& job, int width) {
    using namespace ftxui;

    return Renderer([job, width] {
        std::vector<std::vector<Element>> rows;
        std::vector<Element> row;

        const int cell_width = 46;
        int nodes_per_row = std::max(1, width / cell_width);
        int count = 0;

        for (const auto& node : job.node_allocations) {
            Element title = text(node.node_name) | color(Color::BlueLight) | bold;

            const int cores_per_line = 20;
            int line_count = 0;
            
            std::vector<Element> core_lines;
            std::vector<Element> current_line;

            current_line.push_back(text("CPUs   : "));

            for (int i = 0; i < node.total_cores; ++i) {
                if (std::find(node.allocated_cores.begin(), node.allocated_cores.end(), i) != node.allocated_cores.end())
                    current_line.push_back(text("■") | color(Color::Blue));
                else
                    current_line.push_back(text("."));

                line_count++;
                if (line_count == cores_per_line) {
                    core_lines.push_back(hbox(current_line));
                    current_line.clear();
                    current_line.push_back(text("         "));
                    line_count = 0;
                }
            }

            if (!current_line.empty()) {
                core_lines.push_back(hbox(current_line));
            }

            Element cores_box = vbox(core_lines);

            std::vector<Element> gpu_line;
            gpu_line.push_back(text("GPUs   : "));
            for (int i = 0; i < node.total_gpus; ++i) {
                if (i < node.allocated_gpus)
                    gpu_line.push_back(text("● ") | color(Color::Blue));
                else
                    gpu_line.push_back(text("○ "));
            }
            
            if (node.total_gpus == 0) {
                gpu_line.push_back(text("None"));
            }
            
            Element gpu_box = hbox(gpu_line);

            Element cell_content = hbox({
                hbox({
                    text("  "),
                    vbox({
                        title,
                        cores_box,
                        text(" "),
                        gpu_box
                    }),
                    text("  ")
                }) | border,
                text("  ")
            });

            row.push_back(cell_content);
            count++;

            if (count % nodes_per_row == 0) {
                rows.push_back(row);
                row.clear();
            }
        }

        if (!row.empty()) rows.push_back(row);

        return gridbox(rows);
    });
}

}