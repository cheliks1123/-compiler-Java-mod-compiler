// LibGet GUI — загрузчик библиотек на Qt 6 (Widgets + Network)
//
// Сборка:
//   cmake -B build -DCMAKE_PREFIX_PATH=/путь/к/Qt/6.x/gcc_64
//   cmake --build build
//   ./build/libget_gui

#include <QApplication>
#include <QCheckBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHeaderView>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QTabWidget>
#include <QTreeWidget>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QPalette>
#include <QSettings>
#include <QSplitter>
#include <QStyle>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

struct Lib {
    QString name;
    QString url;
    QString description;
};

// Реестр известных библиотек. Добавляйте свои записи сюда.
static const QList<Lib> REGISTRY = {
    {"Qt",     "https://download.qt.io/official_releases/qt/6.8/6.8.0/single/qt-everywhere-src-6.8.0.tar.xz", "Qt 6.8.0 (исходники, большой архив)"},
    {"Boost",  "https://archives.boost.io/release/1.86.0/source/boost_1_86_0.tar.gz", "Boost 1.86.0"},
    {"fmt",    "https://github.com/fmtlib/fmt/archive/refs/tags/11.0.2.tar.gz", "fmt 11.0.2"},
    {"json",   "https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz", "nlohmann/json 3.11.3"},
    {"spdlog", "https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz", "spdlog 1.14.1"},
    {"SDL2",   "https://github.com/libsdl-org/SDL/releases/download/release-2.30.8/SDL2-2.30.8.tar.gz", "SDL 2.30.8"},
    {"OpenCV", "https://github.com/opencv/opencv/archive/refs/tags/4.10.0.tar.gz", "OpenCV 4.10.0"},
    {"Eigen",  "https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz", "Eigen 3.4.0"},
    {"Catch2", "https://github.com/catchorg/Catch2/archive/refs/tags/v3.7.0.tar.gz", "Catch2 3.7.0"},
    {"GTest",  "https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz", "GoogleTest 1.15.2"},
    {"ImGui",  "https://github.com/ocornut/imgui/archive/refs/tags/v1.91.2.tar.gz", "Dear ImGui 1.91.2"},
    {"GLFW",   "https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip", "GLFW 3.4"},
    {"GLM",    "https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz", "GLM 1.0.1 (математика для графики)"},
    {"SFML",   "https://github.com/SFML/SFML/archive/refs/tags/2.6.1.tar.gz", "SFML 2.6.1"},
    {"zlib",   "https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz", "zlib 1.3.1"},
    {"curl",   "https://curl.se/download/curl-8.10.1.tar.gz", "libcurl 8.10.1"},
    {"OpenSSL","https://github.com/openssl/openssl/releases/download/openssl-3.3.2/openssl-3.3.2.tar.gz", "OpenSSL 3.3.2"},
    {"yaml-cpp","https://github.com/jbeder/yaml-cpp/archive/refs/tags/0.8.0.tar.gz", "yaml-cpp 0.8.0"},
    {"pugixml","https://github.com/zeux/pugixml/releases/download/v1.14/pugixml-1.14.tar.gz", "pugixml 1.14"},
    {"Abseil", "https://github.com/abseil/abseil-cpp/archive/refs/tags/20240722.0.tar.gz", "Abseil 20240722.0"},
    {"SQLite", "https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip", "SQLite 3.46.1 (amalgamation)"},
    {"stb",    "https://github.com/nothings/stb/archive/refs/heads/master.tar.gz", "stb (master)"},

    // Записи "github:владелец/репо" — программа сама берёт последний релиз.
    {"Poco",         "github:pocoproject/poco",          "Poco C++ Libraries (сеть, HTTP, JSON)"},
    {"cpr",          "github:libcpr/cpr",                "cpr — HTTP-клиент (обёртка над curl)"},
    {"Crow",         "github:CrowCpp/Crow",              "Crow — микро веб-фреймворк"},
    {"cxxopts",      "github:jarro2783/cxxopts",         "cxxopts — разбор аргументов командной строки"},
    {"CLI11",        "github:CLIUtils/CLI11",            "CLI11 — разбор аргументов командной строки"},
    {"RapidJSON",    "github:Tencent/rapidjson",         "RapidJSON — быстрый JSON"},
    {"Protobuf",     "github:protocolbuffers/protobuf",  "Protocol Buffers"},
    {"Assimp",       "github:assimp/assimp",             "Assimp — загрузка 3D-моделей"},
    {"Bullet",       "github:bulletphysics/bullet3",     "Bullet Physics"},
    {"Box2D",        "github:erincatto/box2d",           "Box2D — 2D-физика"},
    {"raylib",       "github:raysan5/raylib",            "raylib — простая библиотека для игр"},
    {"SDL3",         "github:libsdl-org/SDL",            "SDL (последний релиз)"},
    {"Taskflow",     "github:taskflow/taskflow",         "Taskflow — параллельные задачи"},
    {"benchmark",    "github:google/benchmark",          "Google Benchmark"},
    {"doctest",      "github:doctest/doctest",           "doctest — лёгкий фреймворк тестов"},
    {"magic_enum",   "github:Neargye/magic_enum",        "magic_enum — рефлексия enum"},
    {"range-v3",     "github:ericniebler/range-v3",      "range-v3 — диапазоны"},
    {"tinyxml2",     "github:leethomason/tinyxml2",      "tinyxml2 — XML"},
    {"ZeroMQ",       "github:zeromq/libzmq",             "ZeroMQ — очереди сообщений"},
    {"Asio",         "github:chriskohlhoff/asio",        "Asio — асинхронная сеть"},
    {"libuv",        "github:libuv/libuv",               "libuv — асинхронный ввод-вывод"},
    {"pybind11",     "github:pybind/pybind11",           "pybind11 — привязки C++ к Python"},
    {"toml++",       "github:marzer/tomlplusplus",       "toml++ — парсер TOML"},
    {"date",         "github:HowardHinnant/date",        "date — даты и время"},
    {"FTXUI",        "github:ArthurSonzogni/FTXUI",      "FTXUI — интерфейс в терминале"},
    {"OpenAL",       "github:kcat/openal-soft",          "OpenAL Soft — 3D-звук"},
    {"mimalloc",     "github:microsoft/mimalloc",        "mimalloc — быстрый аллокатор памяти"},
    {"Vulkan-Headers","github:KhronosGroup/Vulkan-Headers","Vulkan — заголовки"},
};

// ======================================================================
//  Страница «Сборка»: анализ проекта, поиск библиотек, CMake / прямая
//  компиляция, внизу панель Output и Problems.
// ======================================================================

struct KnownHeader {
    const char* prefix;
    const char* lib;  // имя совпадает с записью в REGISTRY
};

static const KnownHeader KNOWN_HEADERS[] = {
    {"boost/", "Boost"},        {"curl/", "curl"},           {"nlohmann/", "json"},
    {"fmt/", "fmt"},            {"spdlog/", "spdlog"},       {"SDL2/", "SDL2"},
    {"SDL.h", "SDL2"},          {"SDL3/", "SDL3"},           {"GLFW/", "GLFW"},
    {"opencv2/", "OpenCV"},     {"Eigen/", "Eigen"},         {"gtest/", "GTest"},
    {"gmock/", "GTest"},        {"catch2/", "Catch2"},       {"imgui.h", "ImGui"},
    {"imgui/", "ImGui"},        {"glm/", "GLM"},             {"zlib.h", "zlib"},
    {"openssl/", "OpenSSL"},    {"yaml-cpp/", "yaml-cpp"},   {"pugixml.hpp", "pugixml"},
    {"sqlite3.h", "SQLite"},    {"absl/", "Abseil"},         {"SFML/", "SFML"},
    {"google/protobuf/", "Protobuf"}, {"asio.hpp", "Asio"},  {"asio/", "Asio"},
    {"zmq.h", "ZeroMQ"},        {"uv.h", "libuv"},           {"pybind11/", "pybind11"},
    {"raylib.h", "raylib"},     {"box2d/", "Box2D"},         {"assimp/", "Assimp"},
    {"AL/", "OpenAL"},          {"vulkan/", "Vulkan-Headers"}, {"doctest/", "doctest"},
    {"benchmark/", "benchmark"}, {"tinyxml2.h", "tinyxml2"}, {"toml++/", "toml++"},
    {"date/", "date"},          {"ftxui/", "FTXUI"},         {"mimalloc.h", "mimalloc"},
    {"cpr/", "cpr"},            {"crow.h", "Crow"},          {"crow/", "Crow"},
    {"cxxopts.hpp", "cxxopts"}, {"CLI/", "CLI11"},           {"rapidjson/", "RapidJSON"},
    {"Poco/", "Poco"},          {"stb_", "stb"},             {"taskflow/", "Taskflow"},
    {"magic_enum", "magic_enum"}, {"range/v3/", "range-v3"}, {"btBulletDynamicsCommon.h", "Bullet"},
};

static QString normName(const QString& s) {
    QString r;
    for (QChar c : s.toLower())
        if (c.isLetterOrNumber()) r += c;
    return r;
}

// Имя из CMake / vcpkg / conan -> имя записи в реестре (если есть)
static QString resolveLibName(const QString& raw) {
    static const QMap<QString, QString> alias = {
        {"qt5", "Qt"}, {"qt6", "Qt"}, {"qt", "Qt"}, {"eigen3", "Eigen"}, {"glfw3", "GLFW"},
        {"nlohmann_json", "json"}, {"nlohmann-json", "json"}, {"googletest", "GTest"},
        {"libcurl", "curl"}, {"sqlite3", "SQLite"}, {"absl", "Abseil"},
        {"tomlplusplus", "toml++"}, {"libzmq", "ZeroMQ"}, {"cppzmq", "ZeroMQ"},
        {"openal-soft", "OpenAL"}, {"vulkan", "Vulkan-Headers"}, {"vulkanheaders", "Vulkan-Headers"},
        {"opengl", ""}, {"threads", ""}, {"pkgconfig", ""}, {"git", ""}, {"python", ""},
        {"python3", ""}, {"doxygen", ""}, {"perl", ""}, {"interpreter", ""},
    };
    const QString low = raw.toLower();
    if (alias.contains(low)) return alias.value(low);
    const QString n = normName(raw);
    for (const Lib& l : REGISTRY)
        if (normName(l.name) == n) return l.name;
    return raw;
}

static bool inRegistry(const QString& name) {
    for (const Lib& l : REGISTRY)
        if (l.name.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

class BuildPage : public QWidget {
    Q_OBJECT
public:
    explicit BuildPage(QWidget* parent = nullptr) : QWidget(parent) {
        // ---- Путь к проекту ----
        pathEdit_ = new QLineEdit(QSettings().value("build/project").toString());
        pathEdit_->setPlaceholderText("Путь к папке проекта (или к одному .cpp файлу)");
        pathEdit_->setClearButtonEnabled(true);
        auto* dirBtn = new QPushButton("Папка...");
        auto* fileBtn = new QPushButton("Файл...");
        analyzeBtn_ = new QPushButton("Анализировать");
        analyzeBtn_->setDefault(true);
        modeLabel_ = new QLabel("Режим: —");

        auto* pathRow = new QHBoxLayout;
        pathRow->addWidget(new QLabel("Проект:"));
        pathRow->addWidget(pathEdit_, 1);
        pathRow->addWidget(dirBtn);
        pathRow->addWidget(fileBtn);
        pathRow->addWidget(analyzeBtn_);

        // ---- Найденные библиотеки ----
        libs_ = new QTreeWidget;
        libs_->setHeaderLabels({"Библиотека", "Где найдена", "В реестре LibGet"});
        libs_->setRootIsDecorated(false);
        libs_->setSelectionMode(QAbstractItemView::ExtendedSelection);
        libs_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        downloadBtn_ = new QPushButton("Скачать выбранные библиотеки →");
        downloadBtn_->setToolTip("Переходит на страницу загрузки и выделяет эти библиотеки");
        auto* libBox = new QGroupBox("Найденные библиотеки");
        auto* libLay = new QVBoxLayout(libBox);
        libLay->addWidget(libs_, 1);
        libLay->addWidget(downloadBtn_);

        // ---- Параметры сборки ----
        buildDirEdit_ = new QLineEdit;
        auto* buildBrowse = new QPushButton("Обзор...");
        auto* buildDirRow = new QHBoxLayout;
        buildDirRow->addWidget(buildDirEdit_, 1);
        buildDirRow->addWidget(buildBrowse);

        typeCb_ = new QComboBox;
        typeCb_->addItems({"Release", "Debug", "RelWithDebInfo"});
        compilerCb_ = new QComboBox;
        for (const QString& c : QStringList{"g++", "clang++", "c++", "cl"}) {
            QString p = QStandardPaths::findExecutable(c);
            if (!p.isEmpty()) compilerCb_->addItem(c + "   (" + p + ")", p);
        }
        if (compilerCb_->count() == 0) compilerCb_->addItem("не найден (g++, clang++, cl)", QString());
        stdCb_ = new QComboBox;
        stdCb_->addItems({"c++17", "c++20", "c++23", "c++14"});
        extraEdit_ = new QLineEdit;
        extraEdit_->setPlaceholderText(
            "CMake: -DCMAKE_PREFIX_PATH=/путь/к/Qt   |   компиляция: -I/inc -L/lib -lcurl");

        configureBtn_ = new QPushButton("CMake: настроить");
        buildBtn_ = new QPushButton("Собрать");
        buildBtn_->setDefault(true);
        cleanBtn_ = new QPushButton("Очистить");
        stopBtn_ = new QPushButton("Стоп");
        stopBtn_->setEnabled(false);
        auto* btnRow = new QHBoxLayout;
        btnRow->addWidget(configureBtn_);
        btnRow->addWidget(buildBtn_);
        btnRow->addWidget(cleanBtn_);
        btnRow->addWidget(stopBtn_);
        btnRow->addStretch(1);

        statusLabel_ = new QLabel("Готов");

        auto* optBox = new QGroupBox("Сборка и компиляция");
        auto* form = new QFormLayout(optBox);
        form->addRow("Папка сборки:", buildDirRow);
        form->addRow("Конфигурация:", typeCb_);
        form->addRow("Компилятор:", compilerCb_);
        form->addRow("Стандарт C++:", stdCb_);
        form->addRow("Доп. аргументы:", extraEdit_);
        form->addRow(btnRow);
        form->addRow(statusLabel_);

        auto* topSplit = new QSplitter;
        topSplit->addWidget(libBox);
        topSplit->addWidget(optBox);
        topSplit->setStretchFactor(0, 1);
        topSplit->setStretchFactor(1, 1);

        auto* topWidget = new QWidget;
        auto* topLay = new QVBoxLayout(topWidget);
        topLay->setContentsMargins(0, 0, 0, 0);
        topLay->addLayout(pathRow);
        topLay->addWidget(modeLabel_);
        topLay->addWidget(topSplit, 1);

        // ---- Нижняя панель: Output / Problems ----
        output_ = new QPlainTextEdit;
        output_->setReadOnly(true);
        output_->setLineWrapMode(QPlainTextEdit::NoWrap);
        output_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

        problems_ = new QTreeWidget;
        problems_->setHeaderLabels({"", "Сообщение", "Файл", "Строка"});
        problems_->setRootIsDecorated(false);
        problems_->setUniformRowHeights(true);
        problems_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        problems_->setColumnWidth(0, 28);
        problems_->setColumnWidth(2, 260);
        problems_->setColumnWidth(3, 60);

        bottom_ = new QTabWidget;
        bottom_->addTab(output_, "Output");
        bottom_->addTab(problems_, "Problems");
        auto* clearBtn = new QPushButton("Очистить");
        clearBtn->setFlat(true);
        bottom_->setCornerWidget(clearBtn, Qt::TopRightCorner);

        auto* vsplit = new QSplitter(Qt::Vertical);
        vsplit->addWidget(topWidget);
        vsplit->addWidget(bottom_);
        vsplit->setStretchFactor(0, 3);
        vsplit->setStretchFactor(1, 2);

        auto* lay = new QVBoxLayout(this);
        lay->addWidget(vsplit);

        // ---- Сигналы ----
        connect(dirBtn, &QPushButton::clicked, this, [this] {
            QString d = QFileDialog::getExistingDirectory(this, "Папка проекта", pathEdit_->text());
            if (!d.isEmpty()) { pathEdit_->setText(d); analyze(); }
        });
        connect(fileBtn, &QPushButton::clicked, this, [this] {
            QString f = QFileDialog::getOpenFileName(this, "Файл C++", pathEdit_->text(),
                                                     "C++ (*.cpp *.cc *.cxx)");
            if (!f.isEmpty()) { pathEdit_->setText(f); analyze(); }
        });
        connect(buildBrowse, &QPushButton::clicked, this, [this] {
            QString d = QFileDialog::getExistingDirectory(this, "Папка сборки", buildDirEdit_->text());
            if (!d.isEmpty()) { buildDirEdit_->setText(d); buildDirManual_ = true; }
        });
        connect(buildDirEdit_, &QLineEdit::textEdited, this, [this] { buildDirManual_ = true; });
        connect(analyzeBtn_, &QPushButton::clicked, this, &BuildPage::analyze);
        connect(pathEdit_, &QLineEdit::returnPressed, this, &BuildPage::analyze);
        connect(configureBtn_, &QPushButton::clicked, this, [this] { startBuild(true); });
        connect(buildBtn_, &QPushButton::clicked, this, [this] { startBuild(false); });
        connect(cleanBtn_, &QPushButton::clicked, this, &BuildPage::cleanBuild);
        connect(stopBtn_, &QPushButton::clicked, this, &BuildPage::stop);
        connect(clearBtn, &QPushButton::clicked, this, [this] {
            output_->clear();
            clearProblems();
        });
        connect(downloadBtn_, &QPushButton::clicked, this, [this] {
            QStringList names;
            auto items = libs_->selectedItems();
            if (items.isEmpty())
                for (int i = 0; i < libs_->topLevelItemCount(); ++i) items << libs_->topLevelItem(i);
            for (QTreeWidgetItem* it : items)
                if (it->text(2) == "Да") names << it->text(0);
            if (names.isEmpty()) {
                QMessageBox::information(this, "Нет библиотек",
                    "Среди выбранных нет библиотек из реестра LibGet. "
                    "Найдите их через поиск GitHub на странице загрузки.");
                return;
            }
            emit requestDownload(names);
        });
        connect(problems_, &QTreeWidget::itemDoubleClicked, this,
                [this](QTreeWidgetItem* it) { openProblemFile(it); });

        if (!pathEdit_->text().trimmed().isEmpty() && QFileInfo::exists(pathEdit_->text().trimmed()))
            analyze();
    }

signals:
    void requestDownload(const QStringList& names);

private:
    struct Step {
        QString program;
        QStringList args;
        QString workDir;
        QString title;
    };

    // ---------- Пути ----------
    bool singleFile() const { return QFileInfo(pathEdit_->text().trimmed()).isFile(); }

    QString rootDir() const {
        QFileInfo fi(pathEdit_->text().trimmed());
        return fi.isFile() ? fi.absolutePath() : fi.absoluteFilePath();
    }

    QString projectName() const {
        QFileInfo fi(pathEdit_->text().trimmed());
        QString n = fi.isFile() ? fi.completeBaseName() : fi.fileName();
        return n.isEmpty() ? QString("app") : n;
    }

    QString buildDir() const {
        QString b = buildDirEdit_->text().trimmed();
        return b.isEmpty() ? rootDir() + "/build" : QDir(b).absolutePath();
    }

    QString directOutput() const {
#ifdef Q_OS_WIN
        return buildDir() + "/" + projectName() + ".exe";
#else
        return buildDir() + "/" + projectName();
#endif
    }

    // ---------- Анализ проекта ----------
    void analyze() {
        const QString p = pathEdit_->text().trimmed();
        if (p.isEmpty() || !QFileInfo::exists(p)) {
            QMessageBox::warning(this, "Ошибка", "Укажите существующую папку или файл проекта.");
            return;
        }
        QSettings().setValue("build/project", p);
        if (!buildDirManual_ || buildDirEdit_->text().trimmed().isEmpty()) {
            buildDirEdit_->setText(rootDir() + "/build");
            buildDirManual_ = false;
        }

        libs_->clear();
        sources_.clear();
        includeDirs_.clear();
        found_.clear();
        includeCount_.clear();
        scanned_ = 0;
        hasCMake_ = false;

        QApplication::setOverrideCursor(Qt::WaitCursor);
        QStringList cmakeFiles;
        if (singleFile()) {
            sources_ << QFileInfo(p).absoluteFilePath();
            scanIncludes(sources_.first());
        } else {
            scanDir(QDir(QFileInfo(p).absoluteFilePath()), 0, cmakeFiles);
            hasCMake_ = QFileInfo::exists(rootDir() + "/CMakeLists.txt");
        }
        for (const QString& cf : cmakeFiles) parseCMake(cf);
        for (auto it = includeCount_.constBegin(); it != includeCount_.constEnd(); ++it)
            addFound(it.key(), QString("#include в файлах: %1").arg(it.value()));

        for (auto it = found_.constBegin(); it != found_.constEnd(); ++it) {
            auto* item = new QTreeWidgetItem(libs_);
            item->setText(0, it.key());
            item->setText(1, it.value().join("; "));
            item->setText(2, inRegistry(it.key()) ? "Да" : "Нет");
        }
        libs_->sortItems(0, Qt::AscendingOrder);
        QApplication::restoreOverrideCursor();

        QString mode;
        if (singleFile()) mode = "компиляция одного файла";
        else if (hasCMake_) mode = "CMake (найден CMakeLists.txt)";
        else mode = QString("прямая компиляция (файлов .cpp: %1)").arg(sources_.size());
        modeLabel_->setText("Режим: " + mode);
        appendOutput(QString("Анализ: просмотрено файлов %1, найдено библиотек %2. Режим: %3\n")
                         .arg(scanned_).arg(found_.size()).arg(mode));
    }

    void addFound(const QString& raw, const QString& source) {
        const QString name = resolveLibName(raw);
        if (name.isEmpty()) return;
        QStringList& l = found_[name];
        if (!l.contains(source)) l << source;
    }

    static QString readText(const QString& path, qint64 maxSize = 2 * 1024 * 1024) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly) || f.size() > maxSize) return {};
        return QString::fromUtf8(f.readAll());
    }

    void scanDir(const QDir& dir, int depth, QStringList& cmakeFiles) {
        static const QSet<QString> skip = {
            ".git", "build", "node_modules", "_deps", "cmakefiles", "out", ".vs", ".idea",
            "third_party", "thirdparty", "3rdparty", "external", "vendor", "libs", "downloads",
            "cmake-build-debug", "cmake-build-release"};
        static const QSet<QString> srcExt = {"cpp", "cc", "cxx"};
        static const QSet<QString> hdrExt = {"h", "hpp", "hh", "hxx", "c", "inl"};
        if (depth > 12 || scanned_ > 5000) return;

        const QString bdir = QFileInfo(buildDir()).absoluteFilePath();
        const auto entries =
            dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
        for (const QFileInfo& e : entries) {
            const QString path = e.absoluteFilePath();
            if (e.isDir()) {
                const QString low = e.fileName().toLower();
                if (skip.contains(low) || path == bdir) continue;
                if (low == "include" || low == "src") includeDirs_ << path;
                scanDir(QDir(path), depth + 1, cmakeFiles);
                continue;
            }
            const QString name = e.fileName();
            const QString suf = e.suffix().toLower();
            if (name == "CMakeLists.txt") {
                cmakeFiles << path;
            } else if (name == "vcpkg.json") {
                parseVcpkg(path);
            } else if (name == "conanfile.txt") {
                parseConan(path);
            } else if (srcExt.contains(suf)) {
                sources_ << path;
                scanIncludes(path);
            } else if (hdrExt.contains(suf)) {
                scanIncludes(path);
            }
        }
    }

    static QString libForHeader(const QString& h) {
        if (h.startsWith("Qt") ||
            (h.size() > 1 && h[0] == QLatin1Char('Q') && h[1].isUpper() && !h.contains('/')))
            return "Qt";
        for (const KnownHeader& k : KNOWN_HEADERS)
            if (h.startsWith(QLatin1String(k.prefix), Qt::CaseInsensitive)) return k.lib;
        return {};
    }

    void scanIncludes(const QString& path) {
        const QString text = readText(path);
        if (text.isEmpty()) return;
        ++scanned_;
        static const QRegularExpression re(R"(^\s*#\s*include\s*[<"]([^>"]+)[>"])",
                                           QRegularExpression::MultilineOption);
        QSet<QString> inFile;
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            const QString lib = libForHeader(it.next().captured(1));
            if (!lib.isEmpty()) inFile.insert(lib);
        }
        for (const QString& l : inFile) includeCount_[l]++;
    }

    void parseCMake(const QString& path) {
        QString text = readText(path);
        static const QRegularExpression comment("#[^\n]*");
        text.remove(comment);
        const QString file = QFileInfo(path).fileName();
        struct Rule { QRegularExpression re; QString label; };
        static const QList<Rule> rules = {
            {QRegularExpression(R"(find_package\s*\(\s*([A-Za-z0-9_+\-]+))",
                                QRegularExpression::CaseInsensitiveOption), "find_package"},
            {QRegularExpression(R"(FetchContent_Declare\s*\(\s*([A-Za-z0-9_+\-]+))",
                                QRegularExpression::CaseInsensitiveOption), "FetchContent"},
            {QRegularExpression(R"(pkg_check_modules\s*\(\s*\w+\s+(?:REQUIRED\s+|QUIET\s+|IMPORTED_TARGET\s+)*([A-Za-z0-9_+\-.]+))",
                                QRegularExpression::CaseInsensitiveOption), "pkg_check_modules"},
        };
        for (const Rule& r : rules) {
            auto it = r.re.globalMatch(text);
            while (it.hasNext())
                addFound(it.next().captured(1), file + ": " + r.label);
        }
    }

    void parseVcpkg(const QString& path) {
        const QJsonArray deps =
            QJsonDocument::fromJson(readText(path).toUtf8()).object().value("dependencies").toArray();
        for (const QJsonValue& v : deps) {
            QString n = v.isString() ? v.toString() : v.toObject().value("name").toString();
            if (!n.isEmpty()) addFound(n, "vcpkg.json");
        }
    }

    void parseConan(const QString& path) {
        bool inReq = false;
        for (QString line : readText(path).split('\n')) {
            line = line.trimmed();
            if (line.startsWith('[')) { inReq = (line == "[requires]"); continue; }
            if (inReq && !line.isEmpty() && !line.startsWith('#'))
                addFound(line.section('/', 0, 0), "conanfile.txt");
        }
    }

    // ---------- Запуск сборки ----------
    void startBuild(bool configureOnly) {
        if (proc_) return;
        if (!QFileInfo::exists(pathEdit_->text().trimmed())) {
            QMessageBox::warning(this, "Ошибка", "Укажите существующую папку или файл проекта.");
            return;
        }
        if (sources_.isEmpty() && !hasCMake_) analyze();

        output_->clear();
        clearProblems();
        stopped_ = false;

        const QString root = rootDir();
        const QString bdir = buildDir();
        const QString type = typeCb_->currentText();
        QList<Step> steps;

        if (hasCMake_ && !singleFile()) {
            const QString cmake = QStandardPaths::findExecutable("cmake");
            if (cmake.isEmpty()) {
                addProblem("error", "CMake не найден в PATH. Установите CMake и перезапустите программу.", "", 0);
                bottom_->setCurrentIndex(1);
                return;
            }
            QStringList cfg{"-S", root, "-B", bdir, "-DCMAKE_BUILD_TYPE=" + type};
            cfg += QProcess::splitCommand(extraEdit_->text());
            if (configureOnly || !QFileInfo::exists(bdir + "/CMakeCache.txt"))
                steps << Step{cmake, cfg, root, "CMake: настройка"};
            if (!configureOnly)
                steps << Step{cmake, {"--build", bdir, "--config", type, "--parallel"}, root, "Сборка"};
        } else {
            if (configureOnly) {
                appendOutput("Настройка нужна только для CMake-проектов. Нажмите «Собрать».\n");
                return;
            }
            const QString cxx = compilerCb_->currentData().toString();
            if (cxx.isEmpty()) {
                addProblem("error",
                           "Компилятор не найден (g++, clang++ или cl). Установите его "
                           "(на Windows запускайте программу из Developer Command Prompt для MSVC).",
                           "", 0);
                bottom_->setCurrentIndex(1);
                return;
            }
            if (sources_.isEmpty()) {
                addProblem("error", "В проекте не найдено файлов .cpp / .cc / .cxx.", "", 0);
                bottom_->setCurrentIndex(1);
                return;
            }
            QDir().mkpath(bdir);
            const QString out = directOutput();
            const bool msvc = QFileInfo(cxx).completeBaseName().compare("cl", Qt::CaseInsensitive) == 0;
            const QString std_ = stdCb_->currentText();
            QStringList incs{root};
            incs += includeDirs_;
            incs.removeDuplicates();

            QStringList args;
            if (msvc) {
                args << "/nologo" << "/EHsc" << "/std:" + std_
                     << (type == "Release" ? "/O2" : "/Zi")
                     << "/Fe:" + QDir::toNativeSeparators(out)
                     << "/Fo:" + QDir::toNativeSeparators(bdir) + "\\";
                for (const QString& i : incs) args << "/I" + QDir::toNativeSeparators(i);
                args += sources_;
                args += QProcess::splitCommand(extraEdit_->text());
            } else {
                args << "-std=" + std_ << (type == "Release" ? "-O2" : "-g") << "-Wall";
                for (const QString& i : incs) args << "-I" + i;
                args += sources_;
                args += QProcess::splitCommand(extraEdit_->text());
                args << "-o" << out;
            }
            steps << Step{cxx, args, root, "Компиляция"};
        }
        runSteps(steps);
    }

    void cleanBuild() {
        if (proc_) return;
        if (hasCMake_ && !singleFile()) {
            const QString bdir = buildDir();
            const QString cmake = QStandardPaths::findExecutable("cmake");
            if (cmake.isEmpty() || !QFileInfo::exists(bdir + "/CMakeCache.txt")) {
                appendOutput("Нечего очищать: проект ещё не настроен.\n");
                return;
            }
            output_->clear();
            clearProblems();
            stopped_ = false;
            runSteps({Step{cmake, {"--build", bdir, "--target", "clean", "--config", typeCb_->currentText()},
                           rootDir(), "Очистка"}});
        } else {
            const QString out = directOutput();
            if (QFile::remove(out)) appendOutput("Удалено: " + out + "\n");
            else appendOutput("Нечего очищать.\n");
        }
    }

    void stop() {
        stopped_ = true;
        steps_.clear();
        if (proc_) proc_->kill();
    }

    void runSteps(const QList<Step>& steps) {
        steps_ = steps;
        setRunning(true);
        statusLabel_->setText("Выполняется...");
        nextStep();
    }

    void nextStep() {
        if (steps_.isEmpty()) {
            finishAll(true);
            return;
        }
        const Step s = steps_.takeFirst();
        appendOutput(QString("=== %1 ===\n> %2 %3\n").arg(s.title, s.program, s.args.join(' ')));
        statusLabel_->setText(s.title + "...");

        QProcess* p = new QProcess(this);
        proc_ = p;
        lineBuf_.clear();
        pendingCMake_ = nullptr;
        p->setProcessChannelMode(QProcess::MergedChannels);
        p->setWorkingDirectory(s.workDir);
        connect(p, &QProcess::readyReadStandardOutput, this,
                [this, p] { onData(p->readAll()); });
        connect(p, &QProcess::errorOccurred, this, [this, p, s](QProcess::ProcessError err) {
            if (err != QProcess::FailedToStart) return;
            proc_ = nullptr;
            p->deleteLater();
            addProblem("error", "Не удалось запустить: " + s.program, "", 0);
            finishAll(false);
        });
        connect(p, &QProcess::finished, this, [this, p](int code, QProcess::ExitStatus st) {
            onData(p->readAll());
            flushLine();
            proc_ = nullptr;
            p->deleteLater();
            if (st == QProcess::NormalExit && code == 0 && !stopped_) {
                nextStep();
            } else {
                appendOutput(stopped_ ? "\nОстановлено пользователем.\n"
                                      : QString("\nЗавершено с кодом %1.\n").arg(code));
                finishAll(false);
            }
        });
        p->start(s.program, s.args);
    }

    void finishAll(bool ok) {
        steps_.clear();
        setRunning(false);
        if (ok) {
            QString msg = "Готово";
            if (!(hasCMake_ && !singleFile()) && QFileInfo::exists(directOutput()))
                msg += ": " + directOutput();
            statusLabel_->setText(msg);
            appendOutput("\n=== " + msg + " ===\n");
        } else {
            statusLabel_->setText(stopped_ ? "Остановлено" : "Ошибка");
        }
        if (errorCount_ > 0) bottom_->setCurrentIndex(1);
    }

    void setRunning(bool r) {
        analyzeBtn_->setEnabled(!r);
        configureBtn_->setEnabled(!r);
        buildBtn_->setEnabled(!r);
        cleanBtn_->setEnabled(!r);
        stopBtn_->setEnabled(r);
    }

    // ---------- Вывод и разбор ошибок ----------
    void appendOutput(const QString& s) {
        output_->moveCursor(QTextCursor::End);
        output_->insertPlainText(s);
        output_->moveCursor(QTextCursor::End);
    }

    void onData(const QByteArray& data) {
        if (data.isEmpty()) return;
        const QString s = QString::fromLocal8Bit(data);
        appendOutput(s);
        lineBuf_ += s;
        int i;
        while ((i = lineBuf_.indexOf('\n')) >= 0) {
            parseLine(lineBuf_.left(i));
            lineBuf_.remove(0, i + 1);
        }
    }

    void flushLine() {
        if (!lineBuf_.isEmpty()) parseLine(lineBuf_);
        lineBuf_.clear();
    }

    void parseLine(QString line) {
        while (line.endsWith('\r')) line.chop(1);
        if (line.trimmed().isEmpty()) return;

        // Текст ошибки CMake идёт на следующей строке
        if (pendingCMake_) {
            pendingCMake_->setText(1, pendingCMake_->text(1) + " — " + line.trimmed());
            pendingCMake_ = nullptr;
            return;
        }

        static const QRegularExpression cmakeAt(
            R"(^CMake (Error|Warning)(?: \(dev\))? at (.+?):(\d+) \((.*?)\):?\s*(.*)$)");
        static const QRegularExpression cmakeGen(R"(^CMake (Error|Warning)(?: \(dev\))?:\s*(.*)$)");
        static const QRegularExpression gcc(
            R"(^(.+?):(\d+):(?:(\d+):)?\s*(warning|error|fatal error):\s*(.*)$)");
        static const QRegularExpression msvc(
            R"(^\s*(.+?)\((\d+)(?:,(\d+))?\)\s*:\s*(warning|error|fatal error)\s*([A-Z]+\d+)?\s*:\s*(.*)$)");
        static const QRegularExpression lnk(
            R"(^\s*(.+?)\s*:\s*(fatal error|error|warning)\s+(LNK\d+)\s*:\s*(.*)$)");
        static const QRegularExpression generic(
            R"(^(?:\S+:\s*)?(fatal error|error|warning):\s*(.*)$)");

        QRegularExpressionMatch m;
        if ((m = cmakeAt.match(line)).hasMatch()) {
            QString msg = "(" + m.captured(4) + ")";
            if (!m.captured(5).isEmpty()) msg += " " + m.captured(5);
            auto* it = addProblem(m.captured(1) == "Error" ? "error" : "warning",
                                  "CMake: " + msg, m.captured(2), m.captured(3).toInt());
            if (m.captured(5).isEmpty()) pendingCMake_ = it;
        } else if ((m = cmakeGen.match(line)).hasMatch()) {
            addProblem(m.captured(1) == "Error" ? "error" : "warning", "CMake: " + m.captured(2), "", 0);
        } else if ((m = gcc.match(line)).hasMatch()) {
            addProblem(m.captured(4) == "warning" ? "warning" : "error", m.captured(5),
                       m.captured(1), m.captured(2).toInt());
        } else if ((m = msvc.match(line)).hasMatch()) {
            QString msg = m.captured(6);
            if (!m.captured(5).isEmpty()) msg = m.captured(5) + ": " + msg;
            addProblem(m.captured(4) == "warning" ? "warning" : "error", msg, m.captured(1),
                       m.captured(2).toInt());
        } else if ((m = lnk.match(line)).hasMatch()) {
            addProblem(m.captured(2) == "warning" ? "warning" : "error",
                       m.captured(3) + ": " + m.captured(4), m.captured(1), 0);
        } else if (line.contains("undefined reference to")) {
            addProblem("error", line.trimmed(), "", 0);
        } else if ((m = generic.match(line)).hasMatch()) {
            addProblem(m.captured(1) == "warning" ? "warning" : "error", m.captured(2), "", 0);
        }
    }

    QTreeWidgetItem* addProblem(const QString& sev, const QString& msg, const QString& file, int ln) {
        QString shown = file;
        const QString root = rootDir();
        if (!file.isEmpty() && QFileInfo(file).isAbsolute() && !root.isEmpty()) {
            const QString rel = QDir(root).relativeFilePath(file);
            if (!rel.startsWith("..")) shown = rel;
        }
        auto* it = new QTreeWidgetItem(problems_);
        it->setIcon(0, style()->standardIcon(sev == "error" ? QStyle::SP_MessageBoxCritical
                                                            : QStyle::SP_MessageBoxWarning));
        it->setText(1, msg);
        it->setToolTip(1, msg);
        it->setText(2, shown);
        it->setToolTip(2, file);
        it->setText(3, ln > 0 ? QString::number(ln) : QString());
        it->setData(2, Qt::UserRole, file);
        if (sev == "error") ++errorCount_;
        else ++warningCount_;
        updateProblemsTitle();
        return it;
    }

    void clearProblems() {
        problems_->clear();
        pendingCMake_ = nullptr;
        errorCount_ = warningCount_ = 0;
        updateProblemsTitle();
    }

    void updateProblemsTitle() {
        bottom_->setTabText(1, QString("Problems (E: %1, W: %2)").arg(errorCount_).arg(warningCount_));
    }

    void openProblemFile(QTreeWidgetItem* it) {
        const QString f = it->data(2, Qt::UserRole).toString();
        if (f.isEmpty()) return;
        QStringList candidates{f, rootDir() + "/" + f, buildDir() + "/" + f};
        for (const QString& c : candidates) {
            if (QFileInfo::exists(c) && QFileInfo(c).isFile()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(c));
                return;
            }
        }
        appendOutput("Файл не найден: " + f + "\n");
    }

    QLineEdit *pathEdit_, *buildDirEdit_, *extraEdit_;
    QPushButton *analyzeBtn_, *downloadBtn_, *configureBtn_, *buildBtn_, *cleanBtn_, *stopBtn_;
    QLabel *modeLabel_, *statusLabel_;
    QTreeWidget *libs_, *problems_;
    QComboBox *typeCb_, *compilerCb_, *stdCb_;
    QPlainTextEdit* output_;
    QTabWidget* bottom_;

    QStringList sources_, includeDirs_;
    QMap<QString, QStringList> found_;
    QMap<QString, int> includeCount_;
    int scanned_ = 0;
    bool hasCMake_ = false;
    bool buildDirManual_ = false;

    QList<Step> steps_;
    QProcess* proc_ = nullptr;
    QString lineBuf_;
    QTreeWidgetItem* pendingCMake_ = nullptr;
    bool stopped_ = false;
    int errorCount_ = 0, warningCount_ = 0;
};

class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow() {
        setWindowTitle("LibGet — загрузчик библиотек");
        resize(1200, 760);

        // --- Левая панель: реестр ---
        list_ = new QListWidget;
        for (const Lib& l : REGISTRY) {
            auto* item = new QListWidgetItem(l.name + "  —  " + l.description);
            item->setData(Qt::UserRole, l.url);
            list_->addItem(item);
        }
        searchEdit_ = new QLineEdit;
        searchEdit_->setPlaceholderText("Поиск на GitHub: например json, qt, networking...");
        searchEdit_->setClearButtonEnabled(true);
        searchBtn_ = new QPushButton("Найти");
        results_ = new QListWidget;
        list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
        results_->setSelectionMode(QAbstractItemView::ExtendedSelection);
        batchBtn_ = new QPushButton("Скачать выбранные (Ctrl/Shift — несколько)");

        auto* leftBox = new QGroupBox("Библиотеки");
        auto* leftLay = new QVBoxLayout(leftBox);
        leftLay->addWidget(new QLabel("Готовый список:"));
        leftLay->addWidget(list_, 1);
        leftLay->addWidget(new QLabel("Поиск по GitHub (любые библиотеки):"));
        auto* searchLay = new QHBoxLayout;
        searchLay->addWidget(searchEdit_, 1);
        searchLay->addWidget(searchBtn_);
        leftLay->addLayout(searchLay);
        leftLay->addWidget(results_, 1);
        leftLay->addWidget(batchBtn_);

        // --- Правая панель ---
        urlEdit_ = new QLineEdit;
        urlEdit_->setPlaceholderText("Прямая ссылка на архив...");
        urlEdit_->setClearButtonEnabled(true);

        ghEdit_ = new QLineEdit;
        ghEdit_->setPlaceholderText("владелец/репозиторий, например fmtlib/fmt");
        refEdit_ = new QLineEdit;
        refEdit_->setPlaceholderText("тег/ветка (необязательно)");
        refEdit_->setMaximumWidth(200);
        auto* ghBtn = new QPushButton("Скачать с GitHub");

        dirEdit_ = new QLineEdit(
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
        auto* browseBtn = new QPushButton("Обзор...");

        downloadBtn_ = new QPushButton("Скачать по ссылке");
        downloadBtn_->setDefault(true);
        cancelBtn_ = new QPushButton("Отмена");
        cancelBtn_->setEnabled(false);
        auto* openBtn = new QPushButton("Открыть папку");
        extractCb_ = new QCheckBox("Распаковать после загрузки");
        extractCb_->setChecked(true);
        extractDirEdit_ = new QLineEdit(dirEdit_->text());
        extractDirEdit_->setPlaceholderText("Папка для распаковки");
        auto* extractBrowseBtn = new QPushButton("Обзор...");
        connect(extractCb_, &QCheckBox::toggled, extractDirEdit_, &QWidget::setEnabled);
        connect(extractCb_, &QCheckBox::toggled, extractBrowseBtn, &QWidget::setEnabled);
        connect(extractBrowseBtn, &QPushButton::clicked, this, [this] {
            QString d = QFileDialog::getExistingDirectory(this, "Папка для распаковки",
                                                          extractDirEdit_->text());
            if (!d.isEmpty()) extractDirEdit_->setText(d);
        });

        bar_ = new QProgressBar;
        bar_->setRange(0, 100);
        statusLabel_ = new QLabel("Готов");
        log_ = new QPlainTextEdit;
        log_->setReadOnly(true);

        auto* urlBox = new QGroupBox("Прямая ссылка");
        auto* urlLay = new QHBoxLayout(urlBox);
        urlLay->addWidget(urlEdit_);
        urlLay->addWidget(downloadBtn_);

        auto* ghBox = new QGroupBox("Репозиторий GitHub");
        auto* ghLay = new QHBoxLayout(ghBox);
        ghLay->addWidget(ghEdit_, 1);
        ghLay->addWidget(refEdit_);
        ghLay->addWidget(ghBtn);

        auto* dirBox = new QGroupBox("Папка сохранения");
        auto* dirLay = new QVBoxLayout(dirBox);
        auto* dirRow1 = new QHBoxLayout;
        dirRow1->addWidget(new QLabel("Скачать в:"));
        dirRow1->addWidget(dirEdit_, 1);
        dirRow1->addWidget(browseBtn);
        dirRow1->addWidget(openBtn);
        auto* dirRow2 = new QHBoxLayout;
        dirRow2->addWidget(extractCb_);
        dirRow2->addWidget(extractDirEdit_, 1);
        dirRow2->addWidget(extractBrowseBtn);
        dirLay->addLayout(dirRow1);
        dirLay->addLayout(dirRow2);

        auto* right = new QWidget;
        auto* rightLay = new QVBoxLayout(right);
        rightLay->setContentsMargins(0, 0, 0, 0);
        themeCb_ = new QComboBox;
        themeCb_->addItems({"Светлая", "Тёмная", "Системная"});
        auto* themeRow = new QHBoxLayout;
        themeRow->addStretch(1);
        themeRow->addWidget(new QLabel("Тема:"));
        themeRow->addWidget(themeCb_);
        rightLay->addLayout(themeRow);
        rightLay->addWidget(urlBox);
        rightLay->addWidget(ghBox);
        rightLay->addWidget(dirBox);
        auto* ctrlLay = new QHBoxLayout;
        ctrlLay->addWidget(bar_, 1);
        ctrlLay->addWidget(cancelBtn_);
        rightLay->addLayout(ctrlLay);
        rightLay->addWidget(statusLabel_);
        rightLay->addWidget(log_, 1);

        auto* split = new QSplitter;
        split->addWidget(leftBox);
        split->addWidget(right);
        split->setStretchFactor(1, 1);
        build_ = new BuildPage;
        tabs_ = new QTabWidget;
        tabs_->addTab(split, "Загрузка библиотек");
        tabs_->addTab(build_, "Сборка");
        auto* main = new QVBoxLayout(this);
        main->addWidget(tabs_);
        connect(build_, &BuildPage::requestDownload, this, &MainWindow::selectInRegistryByNames);

        // --- Сигналы ---
        connect(list_, &QListWidget::currentItemChanged, this,
                [this](QListWidgetItem* it) {
                    if (it) selectRegistryEntry(it->data(Qt::UserRole).toString());
                });
        connect(list_, &QListWidget::itemDoubleClicked, this,
                [this](QListWidgetItem* it) {
                    QString v = it->data(Qt::UserRole).toString();
                    selectRegistryEntry(v);
                    if (v.startsWith("github:")) startFromGitHub();
                    else startFromUrl();
                });
        {
            int th = QSettings().value("theme", 2).toInt();
            if (th < 0 || th > 2) th = 2;
            themeCb_->setCurrentIndex(th);
            applyTheme(th);
            connect(themeCb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                    [this](int i) {
                        applyTheme(i);
                        QSettings().setValue("theme", i);
                    });
        }
        connect(batchBtn_, &QPushButton::clicked, this, &MainWindow::startBatch);
        connect(searchBtn_, &QPushButton::clicked, this, &MainWindow::searchGitHub);
        connect(searchEdit_, &QLineEdit::returnPressed, this, &MainWindow::searchGitHub);
        connect(results_, &QListWidget::currentItemChanged, this,
                [this](QListWidgetItem* it) {
                    if (it) {
                        ghEdit_->setText(it->data(Qt::UserRole).toString());
                        refEdit_->clear();
                    }
                });
        connect(results_, &QListWidget::itemDoubleClicked, this,
                [this](QListWidgetItem* it) {
                    ghEdit_->setText(it->data(Qt::UserRole).toString());
                    refEdit_->clear();
                    startFromGitHub();
                });
        connect(browseBtn, &QPushButton::clicked, this, [this] {
            QString d = QFileDialog::getExistingDirectory(this, "Папка сохранения", dirEdit_->text());
            if (!d.isEmpty()) dirEdit_->setText(d);
        });
        connect(openBtn, &QPushButton::clicked, this, [this] {
            QDir().mkpath(dirEdit_->text());
            QDesktopServices::openUrl(QUrl::fromLocalFile(dirEdit_->text()));
        });
        connect(downloadBtn_, &QPushButton::clicked, this, &MainWindow::startFromUrl);
        connect(urlEdit_, &QLineEdit::returnPressed, this, &MainWindow::startFromUrl);
        connect(ghBtn, &QPushButton::clicked, this, &MainWindow::startFromGitHub);
        connect(cancelBtn_, &QPushButton::clicked, this, &MainWindow::cancel);
    }

private:
    // ---- Поиск по GitHub ----
    void searchGitHub() {
        QString q = searchEdit_->text().trimmed();
        if (q.isEmpty()) return;

        QUrl url("https://api.github.com/search/repositories");
        QUrlQuery uq;
        uq.addQueryItem("q", q);
        uq.addQueryItem("sort", "stars");
        uq.addQueryItem("order", "desc");
        uq.addQueryItem("per_page", "30");
        url.setQuery(uq);

        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader, "LibGetGui/1.0");
        req.setRawHeader("Accept", "application/vnd.github+json");

        searchBtn_->setEnabled(false);
        results_->clear();
        log("Поиск на GitHub: " + q);
        QNetworkReply* r = nam_.get(req);
        connect(r, &QNetworkReply::finished, this, [this, r] {
            searchBtn_->setEnabled(true);
            r->deleteLater();
            if (r->error() != QNetworkReply::NoError) {
                log("Ошибка поиска: " + r->errorString() +
                    " (у GitHub лимит запросов без авторизации, попробуйте позже)");
                return;
            }
            const QJsonArray items =
                QJsonDocument::fromJson(r->readAll()).object().value("items").toArray();
            if (items.isEmpty()) log("Ничего не найдено");
            for (const QJsonValue& v : items) {
                QJsonObject o = v.toObject();
                QString full = o.value("full_name").toString();
                QString desc = o.value("description").toString();
                int stars = o.value("stargazers_count").toInt();
                auto* item = new QListWidgetItem(
                    QString("%1  ★%2\n%3").arg(full).arg(stars).arg(desc.left(100)));
                item->setData(Qt::UserRole, full);
                results_->addItem(item);
            }
        });
    }

    // ---- Запуск ----
    // Библиотеки, найденные на странице «Сборка» -> выделяем в списке загрузки
    void selectInRegistryByNames(const QStringList& names) {
        list_->clearSelection();
        int n = 0;
        QListWidgetItem* first = nullptr;
        for (int i = 0; i < list_->count(); ++i) {
            QListWidgetItem* it = list_->item(i);
            const QString nm = it->text().section("  —  ", 0, 0).trimmed();
            for (const QString& want : names) {
                if (nm.compare(want, Qt::CaseInsensitive) == 0) {
                    it->setSelected(true);
                    if (!first) first = it;
                    ++n;
                    break;
                }
            }
        }
        if (first) list_->scrollToItem(first);
        tabs_->setCurrentIndex(0);
        log(QString("Из анализа проекта выбрано библиотек: %1. Нажмите «Скачать выбранные».").arg(n));
    }

    // ---- Темы: 0 — светлая, 1 — тёмная, 2 — системная ----
    void applyTheme(int idx) {
        static QString origStyle = QApplication::style()->objectName();
        if (origStyle.isEmpty()) origStyle = "Fusion";

        if (idx == 2) {
            QApplication::setStyle(origStyle);
            QApplication::setPalette(QApplication::style()->standardPalette());
            return;
        }
        QApplication::setStyle("Fusion");
        if (idx == 0) {
            QApplication::setPalette(QApplication::style()->standardPalette());
            return;
        }
        QPalette p;
        const QColor base(35, 35, 35), window(53, 53, 53), text(Qt::white);
        const QColor accent(42, 130, 218), dim(127, 127, 127);
        p.setColor(QPalette::Window, window);
        p.setColor(QPalette::WindowText, text);
        p.setColor(QPalette::Base, base);
        p.setColor(QPalette::AlternateBase, window);
        p.setColor(QPalette::ToolTipBase, window);
        p.setColor(QPalette::ToolTipText, text);
        p.setColor(QPalette::Text, text);
        p.setColor(QPalette::Button, window);
        p.setColor(QPalette::ButtonText, text);
        p.setColor(QPalette::BrightText, Qt::red);
        p.setColor(QPalette::Link, accent);
        p.setColor(QPalette::Highlight, accent);
        p.setColor(QPalette::HighlightedText, Qt::black);
        p.setColor(QPalette::PlaceholderText, dim);
        p.setColor(QPalette::Disabled, QPalette::Text, dim);
        p.setColor(QPalette::Disabled, QPalette::ButtonText, dim);
        p.setColor(QPalette::Disabled, QPalette::WindowText, dim);
        QApplication::setPalette(p);
    }

    bool busy() const { return cancelBtn_->isEnabled(); }

    void startFromUrl() {
        if (busy()) return;
        QUrl url = QUrl::fromUserInput(urlEdit_->text().trimmed());
        if (!url.isValid() || url.host().isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Введите корректную ссылку.");
            return;
        }
        startUrl(url);
    }

    void startUrl(const QUrl& url) {
        QString name = QFileInfo(url.path()).fileName();
        if (name.isEmpty()) name = "download.bin";
        begin({{url, name}});
    }

    void startFromGitHub() {
        if (busy()) return;
        QString repo = ghEdit_->text().trimmed();
        QString ref = refEdit_->text().trimmed();
        if (repo.count('/') != 1 || repo.startsWith('/') || repo.endsWith('/')) {
            QMessageBox::warning(this, "Ошибка", "Укажите репозиторий в виде владелец/репо.");
            return;
        }
        startGitHubRepo(repo, ref);
    }

    // ---- Пакетная загрузка: несколько библиотек подряд ----
    void startBatch() {
        if (busy()) return;
        QStringList specs;
        for (QListWidgetItem* it : list_->selectedItems())
            specs << it->data(Qt::UserRole).toString();
        for (QListWidgetItem* it : results_->selectedItems())
            specs << "github:" + it->data(Qt::UserRole).toString();
        specs.removeDuplicates();
        if (specs.isEmpty()) {
            QMessageBox::information(this, "Нет выбора",
                "Выделите библиотеки в списках слева (Ctrl или Shift для нескольких).");
            return;
        }
        log(QString("Очередь загрузки: %1 шт.").arg(specs.size()));
        jobs_ = specs;
        runNextJob();
    }

    void runNextJob() {
        if (jobs_.isEmpty()) {
            finish("Все загрузки завершены");
            return;
        }
        QString spec = jobs_.takeFirst();
        log(QString("В очереди осталось: %1").arg(jobs_.size()));
        if (spec.startsWith("github:")) startGitHubRepo(spec.mid(7), QString());
        else startUrl(QUrl(spec));
    }

    // Ошибка текущей загрузки: идём дальше по очереди, если она есть
    void jobFailed(const QString& msg) {
        log(msg);
        if (!jobs_.isEmpty()) runNextJob();
        else finish(msg);
    }

    void startGitHubRepo(const QString& repo, const QString& ref) {
        QString base ="https://github.com/" + repo + "/archive/refs/";
        QString name = repo.section('/', 1, 1);
        if (!ref.isEmpty()) {
            begin({Candidate{QUrl(base + "tags/" + ref + ".tar.gz"), name + "-" + ref + ".tar.gz"},
                   Candidate{QUrl(base + "heads/" + ref + ".tar.gz"), name + "-" + ref + ".tar.gz"}});
            return;
        }

        // Версия не указана: узнаём последний релиз, иначе берём main/master.
        QNetworkRequest req(QUrl("https://api.github.com/repos/" + repo + "/releases/latest"));
        req.setHeader(QNetworkRequest::UserAgentHeader, "LibGetGui/1.0");
        req.setRawHeader("Accept", "application/vnd.github+json");
        setBusy(true);
        statusLabel_->setText("Поиск последнего релиза: " + name);
        QNetworkReply* r = nam_.get(req);
        resolveReply_ = r;
        connect(r, &QNetworkReply::finished, this, [this, r, base, name] {
            r->deleteLater();
            resolveReply_ = nullptr;
            if (r->error() == QNetworkReply::OperationCanceledError) {
                log("Отменено");
                finish("Отменено");
                return;
            }
            QString tag;
            if (r->error() == QNetworkReply::NoError)
                tag = QJsonDocument::fromJson(r->readAll()).object().value("tag_name").toString();
            QList<Candidate> c;
            if (!tag.isEmpty()) {
                log("Последний релиз: " + tag);
                c << Candidate{QUrl(base + "tags/" + tag + ".tar.gz"), name + "-" + tag + ".tar.gz"};
            } else {
                log("Релиз не найден, берём ветку по умолчанию");
            }
            c << Candidate{QUrl(base + "heads/main.tar.gz"), name + "-main.tar.gz"}
              << Candidate{QUrl(base + "heads/master.tar.gz"), name + "-master.tar.gz"};
            begin(c);
        });
    }

    // Клик по записи реестра: обычная ссылка или "github:владелец/репо"
    void selectRegistryEntry(const QString& v) {
        if (v.startsWith("github:")) {
            ghEdit_->setText(v.mid(7));
            refEdit_->clear();
            urlEdit_->clear();
        } else {
            urlEdit_->setText(v);
        }
    }

    struct Candidate {
        QUrl url;
        QString fileName;
    };

    void begin(const QList<Candidate>& candidates) {
        if (reply_) return;
        QString dir = dirEdit_->text().trimmed();
        if (dir.isEmpty() || !QDir().mkpath(dir)) {
            QMessageBox::warning(this, "Ошибка", "Не удалось создать папку сохранения.");
            jobs_.clear();
            setBusy(false);
            return;
        }
        queue_ = candidates;
        tryNext();
    }

    void tryNext() {
        if (queue_.isEmpty()) {
            jobFailed("Ошибка: не удалось скачать");
            return;
        }
        Candidate c = queue_.takeFirst();
        path_ = QDir(dirEdit_->text().trimmed()).filePath(c.fileName);
        file_ = new QFile(path_, this);
        if (!file_->open(QIODevice::WriteOnly)) {
            log("Не удалось открыть файл: " + path_);
            delete file_;
            file_ = nullptr;
            jobFailed("Ошибка записи файла");
            return;
        }

        log("Загрузка: " + c.url.toString());
        QNetworkRequest req(c.url);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setHeader(QNetworkRequest::UserAgentHeader, "LibGetGui/1.0");
        reply_ = nam_.get(req);
        setBusy(true);
        bar_->setValue(0);
        statusLabel_->setText("Загрузка: " + c.fileName);

        connect(reply_, &QNetworkReply::readyRead, this, [this] {
            if (reply_ && file_) file_->write(reply_->readAll());
        });
        connect(reply_, &QNetworkReply::downloadProgress, this,
                [this](qint64 got, qint64 total) {
                    if (total > 0) {
                        bar_->setRange(0, 100);
                        bar_->setValue(int(got * 100 / total));
                        statusLabel_->setText(QString("%1 / %2 МБ")
                                                  .arg(got / 1048576.0, 0, 'f', 1)
                                                  .arg(total / 1048576.0, 0, 'f', 1));
                    } else {
                        bar_->setRange(0, 0);  // неопределённый прогресс
                        statusLabel_->setText(QString("%1 МБ").arg(got / 1048576.0, 0, 'f', 1));
                    }
                });
        connect(reply_, &QNetworkReply::finished, this, &MainWindow::onFinished);
    }

    void onFinished() {
        QNetworkReply* r = reply_;
        reply_ = nullptr;
        if (file_) {
            file_->write(r->readAll());
            file_->close();
        }
        QNetworkReply::NetworkError err = r->error();
        QString errStr = r->errorString();
        r->deleteLater();

        if (err == QNetworkReply::NoError) {
            log("Готово: " + path_);
            delete file_;
            file_ = nullptr;
            bar_->setRange(0, 100);
            bar_->setValue(100);
            if (!jobs_.isEmpty()) {  // в очереди ещё есть библиотеки
                if (extractCb_->isChecked()) extract(path_);
                runNextJob();
                return;
            }
            finish("Готово: " + path_);
            if (extractCb_->isChecked()) extract(path_);
            return;
        }

        QFile::remove(path_);
        delete file_;
        file_ = nullptr;
        bar_->setRange(0, 100);
        bar_->setValue(0);

        if (err == QNetworkReply::OperationCanceledError) {
            queue_.clear();
            log("Отменено");
            finish("Отменено");
            return;
        }
        log("Ошибка: " + errStr);
        tryNext();  // следующий вариант (для GitHub) или завершение
    }

    // Распаковка через системные tar / unzip (tar есть в Windows 10+, macOS, Linux)
    void extract(const QString& archive) {
        QString outDir = extractDirEdit_->text().trimmed();
        if (outDir.isEmpty()) outDir = QFileInfo(archive).absolutePath();
        if (!QDir().mkpath(outDir)) {
            log("Не удалось создать папку для распаковки: " + outDir);
            statusLabel_->setText("Скачано, но не распаковано");
            return;
        }
        QString program = "tar";
        QStringList args;
#ifndef Q_OS_WIN
        if (archive.endsWith(".zip", Qt::CaseInsensitive)) {
            program = "unzip";
            args << "-o" << "-q" << archive << "-d" << outDir;
        } else
#endif
        {
            args << "-xf" << archive << "-C" << outDir;
        }

        log("Распаковка: " + archive);
        statusLabel_->setText("Распаковка...");
        auto* p = new QProcess(this);
        connect(p, &QProcess::errorOccurred, this, [this, p, program](QProcess::ProcessError) {
            log("Не удалось запустить '" + program + "': установите его или распакуйте вручную");
            statusLabel_->setText("Скачано, но не распаковано");
            p->deleteLater();
        });
        connect(p, &QProcess::finished, this,
                [this, p, outDir](int code, QProcess::ExitStatus st) {
                    if (st == QProcess::NormalExit && code == 0) {
                        log("Распаковано в: " + outDir);
                        statusLabel_->setText("Готово, распаковано в: " + outDir);
                    } else {
                        log("Ошибка распаковки: " + QString::fromLocal8Bit(p->readAllStandardError()));
                        statusLabel_->setText("Скачано, но не распаковано");
                    }
                    p->deleteLater();
                });
        p->start(program, args);
    }

    void cancel() {
        queue_.clear();
        jobs_.clear();
        if (resolveReply_) resolveReply_->abort();
        if (reply_) reply_->abort();
    }

    void finish(const QString& text) {
        statusLabel_->setText(text);
        setBusy(false);
    }

    void setBusy(bool busy) {
        downloadBtn_->setEnabled(!busy);
        batchBtn_->setEnabled(!busy);
        cancelBtn_->setEnabled(busy);
    }

    void log(const QString& s) { log_->appendPlainText(s); }

    QListWidget *list_, *results_;
    QLineEdit *urlEdit_, *ghEdit_, *refEdit_, *dirEdit_, *searchEdit_;
    QPushButton *downloadBtn_, *cancelBtn_, *searchBtn_, *batchBtn_;
    QStringList jobs_;                  // очередь: "github:владелец/репо" или ссылка
    QNetworkReply* resolveReply_ = nullptr;
    QCheckBox* extractCb_;
    QComboBox* themeCb_;
    BuildPage* build_;
    QTabWidget* tabs_;
    QLineEdit* extractDirEdit_;
    QProgressBar* bar_;
    QLabel* statusLabel_;
    QPlainTextEdit* log_;

    QNetworkAccessManager nam_;
    QNetworkReply* reply_ = nullptr;
    QFile* file_ = nullptr;
    QString path_;
    QList<Candidate> queue_;
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("LibGet");
    QCoreApplication::setApplicationName("LibGetGui");
    MainWindow w;
    w.show();
    return app.exec();
}

#include "main.moc"
