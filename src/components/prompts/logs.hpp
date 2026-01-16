#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>

#include "../../api/slurmjobs.hpp"

namespace ui {
using namespace ftxui;

inline std::vector<std::string> readLogFileLines(const std::string& path, int max_lines = 500) {
    std::vector<std::string> lines;

    if (path.empty() || path == "(null)") {
        lines.push_back("[File not specified]");
        return lines;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        lines.push_back("[Cannot open: " + path + "]");
        return lines;
    }

    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    if (lines.empty()) {
        lines.push_back("[Empty file]");
        return lines;
    }

    if (lines.size() > (size_t)max_lines) {
        lines.erase(lines.begin(), lines.end() - max_lines);
    }

    return lines;
}

inline Component logModal(const api::DetailedJob& job, std::shared_ptr<bool> show_stderr, std::shared_ptr<float> scroll_y, std::function<void()> on_close) {
    auto paths = api::slurm::getJobLogPaths(job.id);

    auto stdout_path = std::make_shared<std::string>(paths.first);
    auto stderr_path = std::make_shared<std::string>(paths.second);
    auto stdout_lines = std::make_shared<std::vector<std::string>>(readLogFileLines(*stdout_path));
    auto stderr_lines = std::make_shared<std::vector<std::string>>(readLogFileLines(*stderr_path));

    auto log_content = Renderer([=] {
        std::vector<Element> log_elements;
        auto& lines = *show_stderr ? *stderr_lines : *stdout_lines;

        int line_num = 1;
        for (const auto& line : lines) {
            std::string display_line = line;
            if (display_line.length() > 120) {
                display_line = display_line.substr(0, 117) + "...";
            }
            log_elements.push_back(hbox({
                text(std::to_string(line_num)) | dim | size(WIDTH, EQUAL, 5),
                text(" "),
                text(display_line),
            }));
            line_num++;
        }

        return vbox(log_elements);
    });

    auto scrollable_content = Renderer(log_content, [=] {
        return log_content->Render()
               | focusPositionRelative(0.f, *scroll_y)
               | frame
               | size(HEIGHT, EQUAL, 25) | size(WIDTH, EQUAL, 120);
    });

    auto full_view = Renderer(scrollable_content, [=] {
        std::string current_path = *show_stderr ? *stderr_path : *stdout_path;
        auto& lines = *show_stderr ? *stderr_lines : *stdout_lines;
        int total_lines = lines.size();
        int scroll_percent = (int)(*scroll_y * 100);

        Element title = hbox({
            text("LOGS: ") | bold,
            text(job.name + " (") | bold,
            text(job.id) | bold | color(Color::Magenta),
            text(")") | bold
        }) | center;

        Element selector = hbox({
            (*show_stderr ? text("[stdout]") | dim : text("[stdout]") | bold | color(Color::Blue)),
            text("  "),
            separator(),
            text("  "),
            (*show_stderr ? text("[stderr]") | bold | color(Color::Blue) : text("[stderr]") | dim),
            filler(),
            text(std::to_string(total_lines) + " lines") | dim,
            text("  "),
            text(std::to_string(scroll_percent) + "%") | color(Color::Blue),
        });

        Element path = hbox({
            text("File: ") | dim,
            text(current_path),
        });

        Element footer = hbox({
            text("Arrows/Wheel") | bold | color(Color::Blue),
            text(":scroll  ") | dim,
            text("Tab") | bold | color(Color::Blue),
            text(":stdout/stderr  ") | dim,
            text("Any") | bold | color(Color::Blue),
            text(":close") | dim,
        }) | center;

        return hbox({
            text("  "),
            vbox({
                title,
                text(""),
                selector,
                text(""),
                separator(),
                text(""),
                path,
                text(""),
                scrollable_content->Render() | flex,
                text(""),
                footer
                
            }),
            text("  ")
        }) | border | size(WIDTH, LESS_THAN, 130);
    });

    return CatchEvent(full_view, [=](Event e) {
        constexpr float scroll_step = 0.05f;

        if (e.is_mouse()) {
            if (e.mouse().button == Mouse::WheelDown) {
                *scroll_y = std::min(*scroll_y + scroll_step, 1.f);
                return true;
            }
            if (e.mouse().button == Mouse::WheelUp) {
                *scroll_y = std::max(*scroll_y - scroll_step, 0.f);
                return true;
            }
        }

        else if (e == Event::ArrowDown) {
            *scroll_y = std::min(*scroll_y + scroll_step, 1.f);
            return true;
        }

        else if (e == Event::ArrowUp) {
            *scroll_y = std::max(*scroll_y - scroll_step, 0.f);
            return true;
        }

        else if (e == Event::Tab || e == Event::TabReverse) {
            *show_stderr = !*show_stderr;
            *scroll_y = 0.f; 
            return true;
        }

        else if (e.is_character() || e == Event::Escape || e == Event::Return) {
            on_close();
            return true;
        }
        
        return false;
    });
}

}
