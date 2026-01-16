#include <ftxui/component/screen_interactive.hpp>
#include <algorithm>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>

#include "api/slurmjobs.hpp"

#include "components/nodedetails.hpp"
#include "components/apudetails.hpp"
#include "components/jobdetails.hpp"
#include "components/footer.hpp"
#include "components/title.hpp"

#include "components/prompts/partitions.hpp"
#include "components/prompts/cancel.hpp"
#include "components/prompts/help.hpp"
#include "components/prompts/logs.hpp"

using namespace ftxui;

int main() {
    auto jobs = std::make_shared<std::vector<api::Job>>(api::slurm::getUserJobs());
    if (jobs->empty()) {
        std::cout << "No jobs found for current user\n";
        return 0;
    }

    auto entries = std::make_shared<std::vector<std::string>>();
    for (const auto& job : *jobs)
        entries->push_back(job.name + " (" + job.id + ")");

    int selected = 0;
    
    bool show_help = false;
    bool show_logs = false;
    bool show_partitions = false;
    bool show_cancel_confirm = false;

    auto log_show_stderr = std::make_shared<bool>(false);
    auto log_scroll_y = std::make_shared<float>(0.f);

    std::string status_message;

    std::string cancel_job_id;
    std::string cancel_job_name;

    auto last_refresh = std::chrono::steady_clock::now();
    constexpr int AUTO_REFRESH_SECONDS = 30;

    auto current_job = std::make_shared<api::DetailedJob>(api::slurm::getJobDetails((*jobs)[selected].id));

    ScreenInteractive screen = ScreenInteractive::Fullscreen();

    auto refresh_jobs = [&]() {
        *jobs = api::slurm::getUserJobs();
        entries->clear();
        
        for (const auto& job : *jobs)
            entries->push_back(job.name + " (" + job.id + ")");

        if (jobs->empty()) {
            status_message = "No jobs";
            return;
        }

        if (selected >= (int)jobs->size()) {
            selected = jobs->size() - 1;
        }
        *current_job = api::slurm::getJobDetails((*jobs)[selected].id);
        
        last_refresh = std::chrono::steady_clock::now();
        status_message = "Refreshed!";
    };

    Component job_info = Renderer([&] {
        return hbox({
            ui::jobdetails(*current_job)->Render(),
            text("  "),
            ui::apudetails(*current_job)->Render()
        });
    });

    Component job_nodes_content = Renderer([&] {
        return ui::nodedetails(*current_job, screen.dimx())->Render();
    });

    float scroll_y = 0.f;

    Component job_nodes_scrollable = Renderer(job_nodes_content, [&] {
        return job_nodes_content->Render()
               | focusPositionRelative(0.f, scroll_y)
               | frame
               | flex;
    });

    job_nodes_scrollable =
        CatchEvent(job_nodes_scrollable, [&](Event e) {
            constexpr float wheel_step = 0.05f;
            bool handled = false;

            if (e.is_mouse()) {
                if (e.mouse().button == Mouse::WheelDown) {
                    scroll_y += wheel_step;
                    handled = true;
                }
                if (e.mouse().button == Mouse::WheelUp) {
                    scroll_y -= wheel_step;
                    handled = true;
                }
            }

            if (handled) {
                scroll_y = std::clamp(scroll_y, 0.f, 1.f);
                return true;
            }

            return false;
        });

    Component job_nodes = Container::Horizontal({
        job_nodes_scrollable | flex,
    });

    MenuOption menu_opt;
    menu_opt.on_change = [&] {
        if (!jobs->empty() && selected < (int)jobs->size()) {
            *current_job = api::slurm::getJobDetails((*jobs)[selected].id);
            scroll_y = 0.f;
        }
    };

    Component sidebar =
        Menu(entries.get(), &selected, menu_opt)
        | size(WIDTH, EQUAL, 30);

    Component interface_job = Container::Vertical({
        job_info,
        job_nodes | flex,
    });

    Component interface_jobs = Container::Horizontal({
        sidebar,
        Renderer([] { return hbox({text("  "), separator(), text("  ")}); }),
        interface_job | flex,
    });

    Component footer = Renderer([&] { 
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_refresh).count();
        int next_refresh = AUTO_REFRESH_SECONDS - elapsed;
        
        Element footer_status = hbox({
            text(" Auto-refresh: " + std::to_string(next_refresh) + "s" ) | dim,
        });

        return vbox({
            separator(), 
            hbox({ui::footer(), filler(), footer_status})
        });
    });

    Component main_content = Container::Vertical({
        ui::title("Romeo Slurm Viewer (RSV) v1.0.0"),
        interface_jobs | flex,
        footer
    });

    Component help = ui::helpModal([&] { show_help = false; });

    Component partition_view = Renderer([&] {
        return ui::paritionsModal()->Render();
    });

    partition_view = CatchEvent(partition_view, [&](Event e) {
        if (e.is_character() || e == Event::Escape || e == Event::Return) {
            show_partitions = false;
            return true;
        }
        return false;
    });

    auto log_component = std::make_shared<Component>(
        ui::logModal(*current_job, log_show_stderr, log_scroll_y, [&] { show_logs = false; })
    );

    Component interface = Container::Tab({main_content, help, partition_view}, nullptr);

    interface = Renderer(interface, [&] {
        Element base = main_content->Render();

        if (show_help) {
            return dbox({
                base,
                help->Render() | clear_under | center,
            });
        }

        if (show_partitions) {
            return dbox({
                base,
                partition_view->Render() | clear_under | center,
            });
        }

        if (show_logs) {
            return dbox({
                base,
                (*log_component)->Render() | clear_under | center,
            });
        }

        if (show_cancel_confirm) {
            return ui::cancelModal(base, *current_job);
        }

        return base;
    });

    interface = CatchEvent(interface, [&](Event e) {
        if (show_help) {
            if (e.is_character() || e == Event::Escape || e == Event::Return) {
                show_help = false;
                return true;
            }
            return false;
        }
        if (show_partitions) {
            if (e.is_character() || e == Event::Escape || e == Event::Return) {
                show_partitions = false;
                return true;
            }
            return false;
        }
        
        if (show_logs) {
            return (*log_component)->OnEvent(e);
        }

    
        if (show_cancel_confirm) {
            if (e == Event::Character('y') || e == Event::Character('Y')) {

                if (api::slurm::cancelJob(cancel_job_id)) {
                    status_message = "Job " + cancel_job_id + " cancelled";
                    refresh_jobs();
                } else {
                    status_message = "Cancel failed";
                }
                show_cancel_confirm = false;
                return true;
            }
            if (e == Event::Character('n') || e == Event::Character('N') ||
                e == Event::Escape || e == Event::Return) {

                    status_message = "Cancel aborted";
                show_cancel_confirm = false;
                return true;
            }
            return true;
        }

        if (e == Event::Character('q') || e == Event::Character('Q') ||
            e == Event::Escape || e == Event::Character('\x03')) {
            screen.Exit();
            return true;
        }

        if (e == Event::Character('r') || e == Event::Character('R')) {
            refresh_jobs();
            return true;
        }

        if (e == Event::Character('h') || e == Event::Character('H') ||
            e == Event::Character('?')) {
            show_help = true;
            return true;
        }

        if (e == Event::Character('c') || e == Event::Character('C') ||
            e == Event::Delete) {
            if (!jobs->empty()) {
                cancel_job_id = (*jobs)[selected].id;
                cancel_job_name = (*jobs)[selected].name;
                show_cancel_confirm = true;
            }
            return true;
        }

        if (e == Event::Character('p') || e == Event::Character('P')) {
            show_partitions = true;
            return true;
        }

        if (e == Event::Character('l') || e == Event::Character('L')) {
            if (!jobs->empty()) {
                *log_show_stderr = false;
                *log_scroll_y = 0.f;
                *log_component = ui::logModal(*current_job, log_show_stderr, log_scroll_y, [&] { show_logs = false; });
                show_logs = true;
            }
            return true;
        }

        return false;
    });

    std::atomic<bool> running{true};
    
    std::mutex m;
    std::condition_variable cv;

    std::thread refresh_thread([&] {
        using namespace std::chrono;

        std::unique_lock<std::mutex> lock(m);
        while (running) {
            cv.wait_for(lock, seconds(AUTO_REFRESH_SECONDS), [&] {
                return !running.load();
            });

            if (!running)
                break;

            screen.Post([&] { refresh_jobs(); });
            screen.Post(Event::Custom);
        }
    });

    screen.Loop(interface);

    running = false;
    cv.notify_all();
    refresh_thread.join();

    return 0;
}
