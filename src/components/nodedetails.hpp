#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <map>
#include <regex>
#include "../api/slurmjobs.hpp"

namespace ui {
using namespace ftxui;

// Extract APU prefix from node name by removing trailing digits
// e.g., "romeo-a057" → "romeo-a", "romeo-gpu01" → "romeo-gpu"
inline std::string extractApuPrefix(const std::string& node_name) {
    std::regex re(R"([0-9]+$)");
    return std::regex_replace(node_name, re, "");
}

// Convert APU prefix to readable name (matching ROMEO cluster naming)
inline std::string getApuDisplayName(const std::string& prefix) {
    static const std::map<std::string, std::string> apu_names = {
        {"romeo-a",   "APU ARM (romeo-a)"},
        {"romeo-b",   "APU Standard (romeo-b)"},
        {"romeo-c",   "APU Compute (romeo-c)"},
        {"romeo-gpu", "APU GPU (romeo-gpu)"},
        {"romeo-fat", "APU Fat Nodes (romeo-fat)"},
        {"romeo-arm", "APU ARM (romeo-arm)"},
    };

    auto it = apu_names.find(prefix);
    if (it != apu_names.end()) {
        return it->second;
    }
    return "APU: " + prefix;
}

// Group structure for APU statistics
struct ApuGroup {
    std::string prefix;
    std::string display_name;
    std::vector<const api::NodeAllocation*> nodes;
    int total_allocated_cores = 0;
    int total_cores = 0;
    int total_allocated_gpus = 0;
    int total_gpus = 0;
};

// Render a single node cell
inline Element renderNodeCell(const api::NodeAllocation& node) {
    Element title = text(node.node_name) | color(Color::Cyan) | bold;

    const int cores_per_line = 20;
    std::vector<Element> core_lines;
    int line_count = 0;
    std::vector<Element> current_line;

    current_line.push_back(text("Coeurs : "));

    for (int i = 0; i < node.total_cores; ++i) {
        if (std::find(node.allocated_cores.begin(), node.allocated_cores.end(), i) != node.allocated_cores.end())
            current_line.push_back(text("■") | color(Color::Green));
        else
            current_line.push_back(text("."));

        line_count++;
        if (line_count == cores_per_line) {
            core_lines.push_back(hbox(current_line));
            current_line.clear();
            current_line.push_back(text("         ")); // 9 spaces to align with "Coeurs : "
            line_count = 0;
        }
    }

    if (!current_line.empty()) {
        while (line_count < cores_per_line) {
            current_line.push_back(text("."));
            line_count++;
        }
        core_lines.push_back(hbox(current_line));
    }

    Element cores_box = vbox(core_lines);

    std::vector<Element> gpu_line;
    gpu_line.push_back(text("GPUs   : "));
    for (int i = 0; i < node.total_gpus; ++i) {
        if (i < node.allocated_gpus)
            gpu_line.push_back(text("● ") | color(Color::Yellow));
        else
            gpu_line.push_back(text("○ "));
    }

    if (node.total_gpus == 0) {
        gpu_line.push_back(text("None"));
    }

    Element gpu_box = hbox(gpu_line);

    return hbox({
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
}

// Render APU group header with statistics (matching bash script style)
inline Element renderApuHeader(const ApuGroup& group) {
    std::string stats = "Noeuds: " + std::to_string(group.nodes.size()) +
                        " | Coeurs alloués: " + std::to_string(group.total_allocated_cores) +
                        " | GPUs alloués: " + std::to_string(group.total_allocated_gpus);

    return vbox({
        text(""),
        text("╔══════════════════════════════════════════════════════════╗") | color(Color::Magenta) | bold,
        hbox({
            text("║ ") | color(Color::Magenta) | bold,
            text(group.display_name) | bold,
        }),
        hbox({
            text("║ ") | color(Color::Magenta) | bold,
            text(stats) | color(Color::Cyan),
        }),
        text("╚══════════════════════════════════════════════════════════╝") | color(Color::Magenta) | bold,
    });
}

inline Component nodedetails(const api::DetailedJob& job, int width) {
    using namespace ftxui;

    return Renderer([job, width] {
        // Group nodes by APU prefix
        std::map<std::string, ApuGroup> groups;

        for (const auto& node : job.node_allocations) {
            std::string prefix = extractApuPrefix(node.node_name);

            if (groups.find(prefix) == groups.end()) {
                groups[prefix] = ApuGroup{
                    prefix,
                    getApuDisplayName(prefix),
                    {},
                    0, 0, 0, 0
                };
            }

            auto& group = groups[prefix];
            group.nodes.push_back(&node);
            group.total_allocated_cores += node.allocated_cores.size();
            group.total_cores += node.total_cores;
            group.total_allocated_gpus += node.allocated_gpus;
            group.total_gpus += node.total_gpus;
        }

        // Build the display
        std::vector<Element> all_elements;
        const int cell_width = 44;
        int nodes_per_row = std::max(1, width / cell_width);

        for (auto& [prefix, group] : groups) {
            // Add APU header
            all_elements.push_back(renderApuHeader(group));

            // Add nodes in grid layout
            std::vector<std::vector<Element>> rows;
            std::vector<Element> row;
            int count = 0;

            for (const auto* node : group.nodes) {
                row.push_back(renderNodeCell(*node));
                count++;

                if (count % nodes_per_row == 0) {
                    rows.push_back(row);
                    row.clear();
                }
            }

            if (!row.empty()) rows.push_back(row);

            if (!rows.empty()) {
                all_elements.push_back(gridbox(rows));
            }
        }

        return vbox(all_elements);
    });
}

}