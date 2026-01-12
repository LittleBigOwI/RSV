#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <array>
#include <sstream>
#include <cstdlib>
#include <regex>

namespace api {

struct Job {
    std::string id;
    std::string name;
    std::string entry_name;
};

struct NodeAllocation {
    std::string node_name;
    std::vector<int> allocated_cores;
    int allocated_gpus;
    int total_cores;
    int total_gpus;
};

struct DetailedJob {
    int nodes = 0;

    std::string id;
    std::string name;
    std::string entry_name;
    std::string submitTime;
    std::string maxTime;
    std::string partition;
    std::string status;
    std::string constraints;

    std::vector<NodeAllocation> node_allocations;
};

class slurm {
private:
    static inline std::string exec(const std::string& cmd) {
        std::array<char, 128> buffer;
        std::string result;
        result.reserve(8192);

        std::unique_ptr<FILE, int(*)(FILE*)> pipe(
            popen(cmd.c_str(), "r"),
            static_cast<int(*)(FILE*)>(pclose)
        );
        if (!pipe) return "";
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    }

    static inline std::vector<int> parseCpuIds(const std::string& cpu_ids_str) {
        std::vector<int> cpu_ids;
        static std::regex re(R"((\d+)-(\d+)|(\d+))");
        
        auto begin = std::sregex_iterator(cpu_ids_str.begin(), cpu_ids_str.end(), re);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            if ((*it)[1].matched && (*it)[2].matched) {
                int start = std::stoi((*it)[1].str());
                int stop  = std::stoi((*it)[2].str());
                for (int i = start; i <= stop; ++i)
                    cpu_ids.push_back(i);
            } else if ((*it)[3].matched) {
                cpu_ids.push_back(std::stoi((*it)[3].str()));
            }
        }
        return cpu_ids;
    }

    static inline std::pair<int,int> getNodeInfo(const std::string& node_name) {
        std::string out = exec("scontrol show node " + node_name);
        static std::regex cpu_re("CPUTot=(\\d+)");
        static std::regex gpu_re("Gres=gpu:[^:]*:(\\d+)");

        int total_cores = 0;
        int total_gpus = 0;

        std::smatch m;
        if (std::regex_search(out, m, cpu_re)) total_cores = std::stoi(m[1]);
        if (std::regex_search(out, m, gpu_re)) total_gpus = std::stoi(m[1]);

        return {total_cores, total_gpus};
    }

    static inline std::vector<std::string> expandNodelist(const std::string& nodelist) {
        std::string cmd = "scontrol show hostnames " + nodelist + " 2>/dev/null";
        std::string out = exec(cmd);
        std::vector<std::string> nodes;
        std::istringstream iss(out);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty()) nodes.push_back(line);
        }
        return nodes;
    }

public:
    static std::vector<Job> getUserJobs() {
        std::vector<Job> jobs;

        const char* user = std::getenv("USER");
        if (!user) user = "unknown";

        std::string cmd = "squeue -u " + std::string(user) + " -o \"%i %j\" --noheader";

        std::array<char, 256> buffer;
        std::unique_ptr<FILE, int(*)(FILE*)> pipe(
            popen(cmd.c_str(), "r"),
            static_cast<int(*)(FILE*)>(pclose)
        );

        if (!pipe) {
            std::cerr << "Failed to run squeue command\n";
            return jobs;
        }

        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            std::string line = buffer.data();
            if (line.empty()) continue;

            std::istringstream iss(line);
            Job job;
            iss >> job.id >> job.name;
            job.entry_name = (job.name + " (" + job.id + ")");

            if (!job.id.empty() && !job.name.empty()) {
                jobs.push_back(job);
            }
        }

        return jobs;
    }

    static DetailedJob getJobDetails(const std::string& job_id) {
        DetailedJob job;

        std::string sctrl = exec("scontrol show jobid -dd " + job_id);
        if (sctrl.empty()) return job;

        std::regex field_re(R"((\w+)=([^\s]+))");
        for (std::sregex_iterator it(sctrl.begin(), sctrl.end(), field_re), end; it != end; ++it) {
            std::string key = (*it)[1];
            std::string val = (*it)[2];
            if (key == "JobId") job.id = val;
            else if (key == "JobName") job.name = val;
            else if (key == "SubmitTime") job.submitTime = val;
            else if (key == "NumNodes") job.nodes = std::stoi(val);
            else if (key == "TimeLimit") job.maxTime = val;
            else if (key == "Partition") job.partition = val;
            else if (key == "JobState") job.status = val;
            else if (key == "Features") job.constraints = val;
        }

        static std::regex alloc_re(
            R"(^\s*Nodes=([^\s]+)\s+CPU_IDs=([^\s]+).*?(?:GRES=([^\s]+))?)",
            std::regex_constants::multiline
        );

        std::sregex_iterator it(sctrl.begin(), sctrl.end(), alloc_re);
        std::sregex_iterator end;

        for (; it != end; ++it) {
            std::string node_str = (*it)[1].str();
            std::string cpu_str  = (*it)[2].str();
            std::string gres_str = it->size() > 3 ? (*it)[3].str() : "";

            auto nodes = expandNodelist(node_str);
            auto cpu_ids = parseCpuIds(cpu_str);

            size_t missing_cpus = cpu_ids.size() % nodes.size();
            int cpus_per_node = cpu_ids.size() / nodes.size();

            int allocated_gpus = 0;
            if (!gres_str.empty()) {
                static std::regex gpunum(R"(gpu:[^:]*:(\d+))");
                std::smatch gm;
                if (std::regex_search(gres_str, gm, gpunum))
                    allocated_gpus = std::stoi(gm[1].str());
            }

            int cpu_index = 0;
            for (size_t group_index = 0; group_index < nodes.size(); ++group_index) {
                NodeAllocation na;
                na.node_name = nodes[group_index];

                auto [total_cores, total_gpus] = getNodeInfo(na.node_name);
                na.total_cores = total_cores;
                na.total_gpus  = total_gpus;

                na.allocated_gpus = allocated_gpus / nodes.size();

                for (int i = 0; i < cpus_per_node; ++i) {
                    na.allocated_cores.push_back(cpu_ids[cpu_index++]);
                }

                if (group_index < missing_cpus) {
                    na.allocated_cores.push_back(cpu_ids[cpu_index++]);
                }

                job.node_allocations.push_back(std::move(na));
            }
        }


        return job;
    }

};

}
