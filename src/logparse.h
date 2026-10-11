#pragma once
#include <string>
#include <vector>

struct Problem {
    std::string severity;  // "error" | "warning"
    std::string file;      // относительный путь, может быть пустым
    int line = 0;
    std::string message;
    std::string context;   // строки кода, символ, расположение и т.п.
};

// Разбивает лог на строки, убирает BOM, метки времени GitHub и ANSI-коды.
std::vector<std::string> cleanLines(const std::string& rawLog);

enum class Lang { Java, Cpp };

// Достаёт ошибки из очищенных строк.
//   Java: javac, Gradle ("What went wrong")
//   Cpp:  gcc/clang, MSVC (Cxxxx), линкеры (ld, LNKxxxx), CMake
// В обоих случаях добавляются строки ##[error] из GitHub Actions.
std::vector<Problem> parseProblems(const std::vector<std::string>& lines, Lang lang = Lang::Java);

// Человекочитаемый список для копирования в буфер.
std::string problemsToText(const std::vector<Problem>& problems);
