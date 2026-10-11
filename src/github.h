#pragma once
#include <string>
#include <vector>

struct GhConfig {
    std::string owner;
    std::string repo;
    std::string workflow = "build.yml";
    std::string ref = "main";
    std::string token;
};

struct RunInfo {
    long long id = 0;
    std::string status;      // queued | in_progress | completed
    std::string conclusion;  // success | failure | cancelled | ...
    std::string htmlUrl;
};

struct ArtifactInfo {
    long long id = 0;
    std::string name;
    long long size = 0;
    bool expired = false;
    std::string downloadUrl;
};

class GitHub {
public:
    explicit GitHub(GhConfig cfg);
    ~GitHub();
    GitHub(const GitHub&) = delete;
    GitHub& operator=(const GitHub&) = delete;

    bool latestRun(RunInfo& out, std::string& err);   // последний запуск workflow (может не существовать)
    bool dispatch(std::string& err);                  // запустить сборку
    bool getRun(long long id, RunInfo& out, std::string& err);
    bool downloadLogs(long long runId, std::string& zipData, std::string& err);
    bool listArtifacts(long long runId, std::vector<ArtifactInfo>& out, std::string& err);
    bool downloadArtifact(const ArtifactInfo& a, std::string& zipData, std::string& err);

private:
    struct Impl;
    Impl* d;
    GhConfig cfg_;
    std::string base() const;
};
