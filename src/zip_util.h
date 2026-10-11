#pragma once
#include <string>
#include <vector>

// Склеивает текстовые логи из zip, который отдаёт GitHub (run logs).
bool zipReadLogs(const std::string& zipData, std::string& text, std::string& err);

// Распаковывает zip в папку dir. files получает список созданных файлов.
bool zipExtract(const std::string& zipData, const std::string& dir,
                std::vector<std::string>& files, std::string& err);
