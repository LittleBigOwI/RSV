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

struct PartitionInfo {
    std::string name;
    int nodes_total = 0;
    int nodes_idle = 0;
    int nodes_alloc = 0;
    int nodes_mix = 0;
    int nodes_down = 0;
    std::string timelimit;
    std::string state;
};

struct DetailedJob {
    int cpus = 0;
    int gpus = 0;
    int nodes = 0;

    std::string id;
    std::string name;
    std::string entry_name;
    std::string submitTime;
    std::string maxTime;
    std::string elapsedTime;
    std::string partition;
    std::string status;
    std::string constraints;
    std::string reason;

    std::vector<NodeAllocation> node_allocations;
};

struct JobHistory {
    std::string id;
    std::string name;
    std::string state;
    std::string start;
    std::string end;
    std::string elapsed;
    std::string exit_code;
    std::string max_rss;
    std::string cpu_time;
    std::string ncpus;
    std::string nnodes;
    std::string partition;
    std::string account;
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

    static inline std::unordered_map<std::string, std::pair<int,int>> getAllNodeInfo(const std::string& node_str) {
        std::unordered_map<std::string, std::pair<int,int>> info;
        if (node_str.empty()) return info;

        std::string out = exec("scontrol show node " + node_str);
        if (out.empty()) return info;

        std::istringstream iss(out);
        std::string line;

        static std::regex name_re(R"(NodeName=([^\s]+))");
        static std::regex cpu_re(R"(CPUTot=(\d+))");
        static std::regex gpu_re(R"(Gres=gpu:[^:]*:(\d+))");

        std::string current_node;
        int total_cores = 0;
        int total_gpus = 0;

        for (std::string line; std::getline(iss, line); ) {
            std::smatch m;

            if (std::regex_search(line, m, name_re)) {
                if (!current_node.empty()) {
                    info[current_node] = {total_cores, total_gpus};
                }
                current_node = m[1];
                total_cores = 0;
                total_gpus = 0;
            }

            if (std::regex_search(line, m, cpu_re)) {
                total_cores = std::stoi(m[1]);
            }
            if (std::regex_search(line, m, gpu_re)) {
                total_gpus = std::stoi(m[1]);
            }
        }

        if (!current_node.empty()) {
            info[current_node] = {total_cores, total_gpus};
        }

        return info;
    }


    static inline std::vector<std::string> expandNodelist(const std::string& node_list) {
        std::string cmd = "scontrol show hostnames " + node_list + " 2>/dev/null";
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
            else if (key == "RunTime") job.elapsedTime = val;
            else if (key == "Reason") job.reason = val;
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

            job.cpus = cpu_ids.size();
            job.gpus = allocated_gpus;

            std::unordered_map<std::string, std::pair<int,int>> nodes_info = getAllNodeInfo(node_str);         

            int cpu_index = 0;
            for (size_t group_index = 0; group_index < nodes.size(); ++group_index) {
                NodeAllocation na;
                na.node_name = nodes[group_index];

                auto it = nodes_info.find(na.node_name);

                if (it != nodes_info.end()) {
                    na.total_cores = it->second.first;
                    na.total_gpus  = it->second.second;
                } else {
                    na.total_cores = 0;
                    na.total_gpus  = 0;
                }

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

    static bool cancelJob(const std::string& job_id) {
        std::string cmd = "scancel " + job_id + " 2>&1";
        std::string result = exec(cmd);
        return result.empty() || result.find("error") == std::string::npos;
    }

    static std::vector<PartitionInfo> getPartitions() {
        std::vector<PartitionInfo> partitions;

        std::string cmd = "sinfo -o \"%P %a %l %D %T\" --noheader 2>/dev/null";
        std::string out = exec(cmd);

        std::map<std::string, PartitionInfo> part_map;

        std::istringstream iss(out);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;

            std::istringstream lss(line);
            std::string name, avail, timelimit, nodes_str, state;
            lss >> name >> avail >> timelimit >> nodes_str >> state;

            if (!name.empty() && name.back() == '*') {
                name.pop_back();
            }

            int nodes = 0;
            try { nodes = std::stoi(nodes_str); } catch (...) {}

            if (part_map.find(name) == part_map.end()) {
                part_map[name] = PartitionInfo{name, 0, 0, 0, 0, 0, timelimit, avail};
            }

            auto& p = part_map[name];
            p.nodes_total += nodes;

            if (state.find("idle") != std::string::npos) p.nodes_idle += nodes;
            else if (state.find("mix") != std::string::npos) p.nodes_mix += nodes;
            else if (state.find("alloc") != std::string::npos) p.nodes_alloc += nodes;
            else if (state.find("down") != std::string::npos ||
                     state.find("drain") != std::string::npos) p.nodes_down += nodes;
        }

        for (auto& [name, p] : part_map) {
            partitions.push_back(p);
        }

        return partitions;
    }

    static std::string getRawJobDetails(const std::string& job_id) {
        return exec("scontrol show job " + job_id + " 2>&1");
    }

    static std::string expandSlurmPath(const std::string& path, const std::string& job_id, const std::string& job_name) {
        std::string result = path;

        std::string base_job_id = job_id;
        size_t dot_pos = job_id.find('.');
        if (dot_pos != std::string::npos) {
            base_job_id = job_id.substr(0, dot_pos);
        }

        size_t pos;
        while ((pos = result.find("%J")) != std::string::npos) {
            result.replace(pos, 2, job_id);
        }
        while ((pos = result.find("%j")) != std::string::npos) {
            result.replace(pos, 2, base_job_id);
        }
        while ((pos = result.find("%x")) != std::string::npos) {
            result.replace(pos, 2, job_name);
        }

        return result;
    }

    static std::pair<std::string, std::string> getJobLogPaths(const std::string& job_id) {
        std::string raw = exec("scontrol show job " + job_id + " 2>/dev/null");

        std::string stdout_path, stderr_path, job_name;

        std::regex stdout_re(R"(StdOut=([^\s]+))");
        std::regex stderr_re(R"(StdErr=([^\s]+))");
        std::regex name_re(R"(JobName=([^\s]+))");

        std::smatch m;
        if (std::regex_search(raw, m, name_re)) job_name = m[1].str();
        if (std::regex_search(raw, m, stdout_re)) stdout_path = expandSlurmPath(m[1].str(), job_id, job_name);
        if (std::regex_search(raw, m, stderr_re)) stderr_path = expandSlurmPath(m[1].str(), job_id, job_name);

        return {stdout_path, stderr_path};
    }

    static std::vector<JobHistory> getJobHistory(const std::string& filter = "") {
        std::vector<JobHistory> history;

        const char* user = std::getenv("USER");
        if (!user) user = "unknown";

        std::string cmd = "sacct -u " + std::string(user) +
                        " --starttime=now-7days"
                        " --format=JobID,JobName%30,State,Start,End,Elapsed,ExitCode,MaxRSS,CPUTime,NCPUs,NNodes,Partition,Account"
                        " --noheader -P 2>/dev/null";

        if (!filter.empty()) {
            cmd = "sacct -u " + std::string(user) +
                " --starttime=now-7days -s " + filter +
                " --format=JobID,JobName%30,State,Start,End,Elapsed,ExitCode,MaxRSS,CPUTime,NCPUs,NNodes,Partition,Account"
                " --noheader -P 2>/dev/null";
        }

        std::array<char, 512> buffer;
        std::unique_ptr<FILE, int(*)(FILE*)> pipe(
            popen(cmd.c_str(), "r"),
            static_cast<int(*)(FILE*)>(pclose)
        );

        if (!pipe) return history;

        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            std::string line = buffer.data();
            if (line.empty() || line[0] == '\n') continue;

            if (!line.empty() && line.back() == '\n') line.pop_back();

            std::istringstream iss(line);
            std::string field;
            std::vector<std::string> fields;

            while (std::getline(iss, field, '|')) {
                fields.push_back(field);
            }

            if (fields.size() >= 7) {
                // Skip step entries (those with . in JobID like 12345.batch)
                if (fields[0].find('.') != std::string::npos) continue;

                api::JobHistory job;
                job.id = fields[0];
                job.name = fields[1];
                job.state = fields[2];
                job.start = fields[3];
                job.end = fields[4];
                job.elapsed = fields[5];
                job.exit_code = fields[6];
                if (fields.size() > 7) job.max_rss = fields[7];
                if (fields.size() > 8) job.cpu_time = fields[8];
                if (fields.size() > 9) job.ncpus = fields[9];
                if (fields.size() > 10) job.nnodes = fields[10];
                if (fields.size() > 11) job.partition = fields[11];
                if (fields.size() > 12) job.account = fields[12];
                history.push_back(job);
            }
        }

        std::reverse(history.begin(), history.end());
        return history;
    }

};

}
