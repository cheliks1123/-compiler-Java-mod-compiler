#include "zip_util.h"

#include <miniz.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {
struct Zip {
    mz_zip_archive za{};
    bool ok = false;
    explicit Zip(const std::string& d) { ok = mz_zip_reader_init_mem(&za, d.data(), d.size(), 0) != 0; }
    ~Zip() { if (ok) mz_zip_reader_end(&za); }
};
}  // namespace

bool zipReadLogs(const std::string& zipData, std::string& text, std::string& err) {
    Zip z(zipData);
    if (!z.ok) { err = "Не удалось открыть zip с логами"; return false; }

    struct Entry { int order; std::string name; std::string data; };
    std::vector<Entry> root, all;
    mz_uint n = mz_zip_reader_get_num_files(&z.za);
    for (mz_uint i = 0; i < n; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&z.za, i, &st) || st.m_is_directory) continue;
        std::string name = st.m_filename;
        if (name.size() < 4 || name.substr(name.size() - 4) != ".txt") continue;
        size_t sz = 0;
        void* p = mz_zip_reader_extract_to_heap(&z.za, i, &sz, 0);
        if (!p) continue;
        Entry e{std::atoi(name.c_str()), name, std::string(static_cast<char*>(p), sz)};
        mz_free(p);
        all.push_back(e);
        if (name.find('/') == std::string::npos) root.push_back(e);
    }
    auto& use = root.empty() ? all : root;
    std::sort(use.begin(), use.end(), [](const Entry& a, const Entry& b) {
        return a.order != b.order ? a.order < b.order : a.name < b.name;
    });
    text.clear();
    for (auto& e : use) { text += e.data; text += '\n'; }
    if (text.empty()) { err = "В архиве нет текстовых логов"; return false; }
    return true;
}

bool zipExtract(const std::string& zipData, const std::string& dir,
                std::vector<std::string>& files, std::string& err) {
    Zip z(zipData);
    if (!z.ok) { err = "Не удалось открыть zip"; return false; }
    std::error_code ec;
    fs::create_directories(dir, ec);
    mz_uint n = mz_zip_reader_get_num_files(&z.za);
    for (mz_uint i = 0; i < n; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&z.za, i, &st) || st.m_is_directory) continue;
        std::string name = st.m_filename;
        if (name.empty() || name[0] == '/' || name[0] == '\\' || name.find("..") != std::string::npos ||
            name.find(':') != std::string::npos)
            continue;  // защита от выхода из папки
        size_t sz = 0;
        void* p = mz_zip_reader_extract_to_heap(&z.za, i, &sz, 0);
        if (!p) { err = "Ошибка распаковки: " + name; return false; }
        fs::path out = fs::path(dir) / fs::u8path(name);
        fs::create_directories(out.parent_path(), ec);
        std::ofstream f(out, std::ios::binary);
        f.write(static_cast<char*>(p), static_cast<std::streamsize>(sz));
        mz_free(p);
        if (!f) { err = "Не удалось записать " + out.string(); return false; }
        files.push_back(out.string());
    }
    return true;
}
