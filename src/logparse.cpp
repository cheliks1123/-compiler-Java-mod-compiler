#include "logparse.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <set>
#include <sstream>
#include <tuple>

namespace {

std::string stripAnsi(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\x1B' && i + 1 < s.size() && s[i + 1] == '[') {
            i += 2;
            while (i < s.size() && !std::isalpha(static_cast<unsigned char>(s[i]))) ++i;
            continue;
        }
        out += s[i];
    }
    return out;
}

// "2026-10-11T01:00:00.1234567Z текст" -> "текст"
std::string stripTimestamp(const std::string& s) {
    if (s.size() > 21 && s[4] == '-' && s[7] == '-' && s[10] == 'T') {
        size_t p = s.find("Z ");
        if (p != std::string::npos && p <= 32) return s.substr(p + 2);
    }
    return s;
}

std::string rtrim(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    return s;
}

bool startsWith(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }

std::string trimLeft(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

// Убирает префикс рабочей папки раннера: /home/runner/work/repo/repo/src/... -> src/...
std::string shortenPath(std::string f) {
    std::replace(f.begin(), f.end(), '\\', '/');
    size_t p = f.find("/work/");
    if (p == std::string::npos) {
        // D:/a/repo/repo/src/...
        p = f.find(":/a/");
        if (p != std::string::npos) p += 2;  // указывает на "/a/"
        else return f;
        std::string rest = f.substr(p + 3);
        size_t s1 = rest.find('/');
        if (s1 == std::string::npos) return f;
        size_t s2 = rest.find('/', s1 + 1);
        if (s2 == std::string::npos) return f;
        return rest.substr(s2 + 1);
    }
    std::string rest = f.substr(p + 6);
    size_t s1 = rest.find('/');
    if (s1 == std::string::npos) return f;
    size_t s2 = rest.find('/', s1 + 1);
    if (s2 == std::string::npos) return f;
    return rest.substr(s2 + 1);
}

// Разбор "path/File.java:12: error: message" или "...:12:5: error: ..."
bool parseCompilerLine(std::string line, Problem& out) {
    if (startsWith(line, "[ant:javac] ")) line = line.substr(12);
    const char* markers[] = {": error: ", ": warning: ", ": fatal error: "};
    const char* sev[] = {"error", "warning", "error"};
    for (int m = 0; m < 3; ++m) {
        size_t p = line.find(markers[m]);
        if (p == std::string::npos) continue;
        std::string prefix = line.substr(0, p);
        // убрать необязательную колонку
        auto lastDigits = [&](std::string& pre, int& num) -> bool {
            size_t c = pre.find_last_of(':');
            if (c == std::string::npos || c + 1 >= pre.size()) return false;
            for (size_t i = c + 1; i < pre.size(); ++i)
                if (!std::isdigit(static_cast<unsigned char>(pre[i]))) return false;
            num = std::atoi(pre.c_str() + c + 1);
            pre = pre.substr(0, c);
            return true;
        };
        int n1 = 0;
        if (!lastDigits(prefix, n1)) continue;
        int lineNo = n1;
        int n2 = 0;
        std::string tmp = prefix;
        if (lastDigits(tmp, n2)) {  // было file:line:col
            prefix = tmp;
            lineNo = n2;
        }
        if (prefix.find('.') == std::string::npos) continue;
        out.severity = sev[m];
        out.file = shortenPath(prefix);
        out.line = lineNo;
        out.message = line.substr(p + std::strlen(markers[m]));
        return true;
    }
    return false;
}

// Убирает хвост MSBuild " [D:\a\...\project.vcxproj]"
std::string stripProjectTag(std::string msg) {
    if (!msg.empty() && msg.back() == ']') {
        size_t b = msg.rfind(" [");
        if (b != std::string::npos && msg.find(".vcxproj", b) != std::string::npos) msg = msg.substr(0, b);
    }
    return msg;
}

// MSVC: "D:\a\x\x\src\main.cpp(12,5): error C2065: 'x': undeclared identifier [proj.vcxproj]"
bool parseMsvcLine(const std::string& line, Problem& out) {
    const char* markers[] = {"): error ", "): fatal error ", "): warning "};
    const char* sev[] = {"error", "error", "warning"};
    for (int m = 0; m < 3; ++m) {
        size_t p = line.find(markers[m]);
        if (p == std::string::npos) continue;
        size_t open = line.rfind('(', p);
        if (open == std::string::npos || open == 0) continue;
        std::string inside = line.substr(open + 1, p - open - 1);
        if (inside.empty() || !std::isdigit(static_cast<unsigned char>(inside[0]))) continue;
        out.severity = sev[m];
        out.line = std::atoi(inside.c_str());
        out.file = shortenPath(trimLeft(line.substr(0, open)));
        out.message = stripProjectTag(line.substr(p + std::strlen(markers[m])));
        return true;
    }
    return false;
}

// Линкер MSVC: "main.obj : error LNK2019: unresolved external symbol ..."  /  "LINK : fatal error LNK1104: ..."
// Линкер GNU:  "main.cpp:(.text+0x9): undefined reference to `foo()'"  /  "collect2: error: ld returned 1 exit status"
bool parseLinkerLine(const std::string& line, Problem& out) {
    size_t q = line.find(": fatal error LNK");
    if (q == std::string::npos) q = line.find(": error LNK");
    if (q != std::string::npos) {
        size_t lnk = line.find("LNK", q);
        out.severity = "error";
        out.message = stripProjectTag(line.substr(lnk));
        std::string who = trimLeft(line.substr(0, q));
        while (!who.empty() && who.back() == ' ') who.pop_back();
        if (!who.empty() && who != "LINK") out.context = "в " + shortenPath(who);
        return true;
    }
    for (const char* key : {"undefined reference to", "multiple definition of"}) {
        size_t p = line.find(key);
        if (p == std::string::npos) continue;
        out.severity = "error";
        out.message = line.substr(p);
        size_t c = line.find(":(");
        if (c != std::string::npos && c < p) out.file = shortenPath(line.substr(0, c));
        return true;
    }
    if (startsWith(line, "collect2: error:") || line.find("ld: cannot find -l") != std::string::npos) {
        out.severity = "error";
        size_t c = line.find("error: ");
        out.message = c != std::string::npos ? line.substr(c + 7) : line.substr(line.find("ld: ") + 4);
        return true;
    }
    return false;
}

// "CMake Error at CMakeLists.txt:5 (add_subdirectory):" + строки сообщения с отступом
bool parseCMakeBlock(const std::vector<std::string>& lines, size_t i, Problem& out) {
    const std::string& l = lines[i];
    bool err = startsWith(l, "CMake Error");
    bool warn = startsWith(l, "CMake Warning");
    if (!err && !warn) return false;
    out.severity = err ? "error" : "warning";
    size_t at = l.find(" at ");
    std::string head = at != std::string::npos ? l.substr(at + 4) : "";
    if (!head.empty()) {
        size_t sp = head.find(' ');
        std::string loc = head.substr(0, sp);
        size_t c = loc.rfind(':');
        if (c != std::string::npos) {
            out.file = shortenPath(loc.substr(0, c));
            out.line = std::atoi(loc.c_str() + c + 1);
        }
    } else {
        size_t c = l.find(": ");
        if (c != std::string::npos) out.message = l.substr(c + 2);
    }
    std::string body;
    int taken = 0;
    for (size_t j = i + 1; j < lines.size() && taken < 8; ++j) {
        const std::string& n = lines[j];
        if (n.empty()) { if (taken == 0) continue; break; }
        if (n[0] != ' ' && n[0] != '\t') break;
        std::string t = trimLeft(n);
        if (out.message.empty()) out.message = t;
        else body += (body.empty() ? "" : "\n") + t;
        ++taken;
    }
    out.context = body;
    return !out.message.empty();
}

}  // namespace

std::vector<std::string> cleanLines(const std::string& rawLog) {
    std::vector<std::string> lines;
    std::string cur;
    std::string text = rawLog;
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF)
        text = text.substr(3);
    std::istringstream in(text);
    while (std::getline(in, cur)) {
        cur = rtrim(cur);
        if (cur.size() >= 3 && static_cast<unsigned char>(cur[0]) == 0xEF &&
            static_cast<unsigned char>(cur[1]) == 0xBB && static_cast<unsigned char>(cur[2]) == 0xBF)
            cur = cur.substr(3);
        lines.push_back(rtrim(stripAnsi(stripTimestamp(cur))));
    }
    return lines;
}

std::vector<Problem> parseProblems(const std::vector<std::string>& lines, Lang lang) {
    const bool cpp = lang == Lang::Cpp;
    std::vector<Problem> result;
    std::set<std::tuple<std::string, int, std::string>> seen;
    std::vector<Problem> tail;  // ##[error] и прочее — в конец списка

    auto add = [&](std::vector<Problem>& dst, const Problem& p) {
        auto key = std::make_tuple(p.file, p.line, p.message);
        if (seen.insert(key).second) dst.push_back(p);
    };

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& l = lines[i];

        Problem p;
        if (parseCompilerLine(l, p) || (cpp && parseMsvcLine(l, p))) {
            for (size_t j = i + 1; j < lines.size() && p.context.size() < 600; ++j) {
                const std::string& n = lines[j];
                bool note = cpp && n.find(": note: ") != std::string::npos;  // gcc/clang: "file.cpp:5:3: note: ..."
                if (n.empty() || ((n[0] != ' ' && n[0] != '\t') && !note)) break;
                Problem dummy;
                if (!note && (parseCompilerLine(n, dummy) || (cpp && parseMsvcLine(n, dummy)))) break;
                if (!p.context.empty()) p.context += '\n';
                p.context += n;
            }
            add(result, p);
            continue;
        }

        if (cpp) {
            Problem q;
            if (parseLinkerLine(l, q) || parseCMakeBlock(lines, i, q)) {
                add(result, q);
                continue;
            }
        }

        if (l == "* What went wrong:") {
            Problem g;
            g.severity = "error";
            std::string body;
            for (size_t j = i + 1; j < lines.size(); ++j) {
                const std::string& n = lines[j];
                if (n.empty() || startsWith(n, "* Try:") || startsWith(n, "* Exception")) break;
                if (g.message.empty()) g.message = n;
                else body += (body.empty() ? "" : "\n") + n;
            }
            g.context = body;
            if (!g.message.empty()) add(tail, g);
            continue;
        }

        if (startsWith(l, "##[error]")) {
            Problem e;
            e.severity = "error";
            e.message = trimLeft(l.substr(9));
            if (!e.message.empty()) add(tail, e);
        }
    }

    result.insert(result.end(), tail.begin(), tail.end());
    return result;
}

std::string problemsToText(const std::vector<Problem>& problems) {
    std::string out;
    for (const auto& p : problems) {
        out += (p.severity == "error" ? "[ERROR] " : "[WARN] ");
        if (!p.file.empty()) out += p.file + ":" + std::to_string(p.line) + " ";
        out += p.message + "\n";
        if (!p.context.empty()) out += p.context + "\n";
        out += "\n";
    }
    return out;
}
