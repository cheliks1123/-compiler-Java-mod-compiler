#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <miniz.h>

#include "logparse.h"
#include "zip_util.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

int main() {
    const std::string log =
        "\xEF\xBB\xBF" "2026-10-11T01:00:00.1234567Z \x1B[32m> Task :compileJava\x1B[0m\n"
        "2026-10-11T01:00:01.0000000Z /home/runner/work/mod/mod/src/main/java/com/x/Foo.java:12: error: cannot find symbol\n"
        "2026-10-11T01:00:01.0000000Z         Bar b = new Bar();\n"
        "2026-10-11T01:00:01.0000000Z         ^\n"
        "2026-10-11T01:00:01.0000000Z   symbol:   class Bar\n"
        "2026-10-11T01:00:01.0000000Z   location: class Foo\n"
        "2026-10-11T01:00:02.0000000Z /home/runner/work/mod/mod/src/main/java/com/x/Baz.java:7:9: warning: [deprecation] old() is deprecated\n"
        "2026-10-11T01:00:03.0000000Z \n"
        "2026-10-11T01:00:03.0000000Z FAILURE: Build failed with an exception.\n"
        "2026-10-11T01:00:03.0000000Z * What went wrong:\n"
        "2026-10-11T01:00:03.0000000Z Execution failed for task ':compileJava'.\n"
        "2026-10-11T01:00:03.0000000Z > Compilation failed; see the compiler error output for details.\n"
        "2026-10-11T01:00:03.0000000Z \n"
        "2026-10-11T01:00:03.0000000Z * Try:\n"
        "2026-10-11T01:00:04.0000000Z ##[error]Process completed with exit code 1.\n"
        "2026-10-11T01:00:05.0000000Z D:\\a\\mod\\mod\\src\\main\\java\\Win.java:3: error: ';' expected\n";

    auto lines = cleanLines(log);
    CHECK(lines[0] == "> Task :compileJava");
    auto pr = parseProblems(lines);
    for (auto& p : pr)
        std::printf("%s | %s:%d | %s | ctx=%zu\n", p.severity.c_str(), p.file.c_str(), p.line, p.message.c_str(), p.context.size());

    CHECK(pr.size() == 5);
    CHECK(pr[0].file == "src/main/java/com/x/Foo.java" && pr[0].line == 12 && pr[0].severity == "error");
    CHECK(pr[0].message == "cannot find symbol");
    CHECK(pr[0].context.find("symbol:   class Bar") != std::string::npos);
    CHECK(pr[1].severity == "warning" && pr[1].line == 7);
    CHECK(pr[2].file == "src/main/java/Win.java" && pr[2].line == 3);
    CHECK(pr[3].message == "Execution failed for task ':compileJava'.");
    CHECK(pr[4].message == "Process completed with exit code 1.");

    // ---- C++ ----
    const std::string cpplog =
        "2026-10-11T02:00:00.0000000Z CMake Error at CMakeLists.txt:12 (add_subdirectory):\n"
        "2026-10-11T02:00:00.0000000Z   add_subdirectory given source \"libs/foo\" which is not an existing directory.\n"
        "2026-10-11T02:00:00.0000000Z \n"
        "2026-10-11T02:00:01.0000000Z /home/runner/work/app/app/src/main.cpp:10:5: error: 'x' was not declared in this scope\n"
        "2026-10-11T02:00:01.0000000Z    10 |     x = 5;\n"
        "2026-10-11T02:00:01.0000000Z       |     ^\n"
        "2026-10-11T02:00:01.0000000Z /home/runner/work/app/app/src/main.cpp:4:7: note: 'y' declared here\n"
        "2026-10-11T02:00:02.0000000Z /home/runner/work/app/app/src/util.cpp:3:10: fatal error: missing.h: No such file or directory\n"
        "2026-10-11T02:00:03.0000000Z /home/runner/work/app/app/src/a.cpp:8:3: warning: unused variable 'k' [-Wunused-variable]\n"
        "2026-10-11T02:00:04.0000000Z /usr/bin/ld: main.o: in function `main':\n"
        "2026-10-11T02:00:04.0000000Z main.cpp:(.text+0x9): undefined reference to `foo()'\n"
        "2026-10-11T02:00:04.0000000Z collect2: error: ld returned 1 exit status\n"
        "2026-10-11T02:00:05.0000000Z D:\\a\\app\\app\\src\\win.cpp(21,9): error C2065: 'q': undeclared identifier [D:\\a\\app\\app\\build\\app.vcxproj]\n"
        "2026-10-11T02:00:05.0000000Z D:\\a\\app\\app\\src\\win.cpp(30): warning C4996: 'strcpy': deprecated [D:\\a\\app\\app\\build\\app.vcxproj]\n"
        "2026-10-11T02:00:06.0000000Z win.obj : error LNK2019: unresolved external symbol bar referenced in function main [D:\\a\\app\\app\\build\\app.vcxproj]\n"
        "2026-10-11T02:00:06.0000000Z LINK : fatal error LNK1104: cannot open file 'foo.lib' [D:\\a\\app\\app\\build\\app.vcxproj]\n";
    auto cp = parseProblems(cleanLines(cpplog), Lang::Cpp);
    for (auto& p : cp)
        std::printf("%s | %s:%d | %s | ctx=%zu\n", p.severity.c_str(), p.file.c_str(), p.line, p.message.c_str(), p.context.size());
    CHECK(cp.size() == 10);
    CHECK(cp[0].file == "CMakeLists.txt" && cp[0].line == 12 && cp[0].message.find("add_subdirectory given") == 0);
    CHECK(cp[1].file == "src/main.cpp" && cp[1].line == 10 && cp[1].message == "'x' was not declared in this scope");
    CHECK(cp[1].context.find("note: 'y' declared here") != std::string::npos);
    CHECK(cp[2].message.find("missing.h") == 0 && cp[2].severity == "error");
    CHECK(cp[3].severity == "warning");
    CHECK(cp[4].file == "main.cpp" && cp[4].message == "undefined reference to `foo()'");
    CHECK(cp[5].message == "ld returned 1 exit status");
    CHECK(cp[6].file == "src/win.cpp" && cp[6].line == 21 && cp[6].message == "C2065: 'q': undeclared identifier");
    CHECK(cp[7].severity == "warning" && cp[7].line == 30);
    CHECK(cp[8].message.rfind("LNK2019", 0) == 0 && cp[8].context == "в win.obj");
    CHECK(cp[9].message.rfind("LNK1104", 0) == 0);

    // zip: корневой файл с логом + дубликат в папке шага
    mz_zip_archive w{};
    CHECK(mz_zip_writer_init_heap(&w, 0, 0));
    const char* a = "root log";
    const char* b = "step log";
    mz_zip_writer_add_mem(&w, "0_build.txt", a, 8, MZ_DEFAULT_COMPRESSION);
    mz_zip_writer_add_mem(&w, "build/1_step.txt", b, 8, MZ_DEFAULT_COMPRESSION);
    void* buf = nullptr; size_t sz = 0;
    CHECK(mz_zip_writer_finalize_heap_archive(&w, &buf, &sz));
    std::string zip(static_cast<char*>(buf), sz);
    mz_zip_writer_end(&w);

    std::string text, err;
    CHECK(zipReadLogs(zip, text, err));
    CHECK(text.find("root log") != std::string::npos && text.find("step log") == std::string::npos);

    std::vector<std::string> files;
    CHECK(zipExtract(zip, "zip_out_test", files, err));
    CHECK(files.size() == 2);

    std::printf(fails ? "FAILED\n" : "ALL OK\n");
    return fails ? 1 : 0;
}
