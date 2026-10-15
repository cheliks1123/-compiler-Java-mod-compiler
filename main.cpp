// LibGet GUI — загрузчик библиотек на Qt 6 (Widgets + Network)
//
// Сборка:
//   cmake -B build -DCMAKE_PREFIX_PATH=/путь/к/Qt/6.x/gcc_64
//   cmake --build build
//   ./build/libget_gui

#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QStandardPaths>
#include <QUrl>
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
};

class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow() {
        setWindowTitle("LibGet — загрузчик библиотек");
        resize(900, 560);

        // --- Левая панель: реестр ---
        list_ = new QListWidget;
        for (const Lib& l : REGISTRY) {
            auto* item = new QListWidgetItem(l.name + "  —  " + l.description);
            item->setData(Qt::UserRole, l.url);
            list_->addItem(item);
        }
        auto* leftBox = new QGroupBox("Известные библиотеки");
        auto* leftLay = new QVBoxLayout(leftBox);
        leftLay->addWidget(list_);

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
        auto* dirLay = new QHBoxLayout(dirBox);
        dirLay->addWidget(dirEdit_, 1);
        dirLay->addWidget(browseBtn);
        dirLay->addWidget(openBtn);

        auto* right = new QWidget;
        auto* rightLay = new QVBoxLayout(right);
        rightLay->setContentsMargins(0, 0, 0, 0);
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
        auto* main = new QVBoxLayout(this);
        main->addWidget(split);

        // --- Сигналы ---
        connect(list_, &QListWidget::currentItemChanged, this,
                [this](QListWidgetItem* it) {
                    if (it) urlEdit_->setText(it->data(Qt::UserRole).toString());
                });
        connect(list_, &QListWidget::itemDoubleClicked, this,
                [this](QListWidgetItem* it) {
                    urlEdit_->setText(it->data(Qt::UserRole).toString());
                    startFromUrl();
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
    // ---- Запуск ----
    void startFromUrl() {
        QUrl url = QUrl::fromUserInput(urlEdit_->text().trimmed());
        if (!url.isValid() || url.host().isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Введите корректную ссылку.");
            return;
        }
        QString name = QFileInfo(url.path()).fileName();
        if (name.isEmpty()) name = "download.bin";
        begin({{url, name}});
    }

    void startFromGitHub() {
        QString repo = ghEdit_->text().trimmed();
        QString ref = refEdit_->text().trimmed();
        if (repo.count('/') != 1 || repo.startsWith('/') || repo.endsWith('/')) {
            QMessageBox::warning(this, "Ошибка", "Укажите репозиторий в виде владелец/репо.");
            return;
        }
        QString base = "https://github.com/" + repo + "/archive/refs/";
        QString name = repo.section('/', 1, 1);
        QList<Candidate> c;
        if (!ref.isEmpty()) {
            c << Candidate{QUrl(base + "tags/" + ref + ".tar.gz"), name + "-" + ref + ".tar.gz"}
              << Candidate{QUrl(base + "heads/" + ref + ".tar.gz"), name + "-" + ref + ".tar.gz"};
        } else {
            c << Candidate{QUrl(base + "heads/main.tar.gz"), name + "-main.tar.gz"}
              << Candidate{QUrl(base + "heads/master.tar.gz"), name + "-master.tar.gz"};
        }
        begin(c);
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
            return;
        }
        queue_ = candidates;
        tryNext();
    }

    void tryNext() {
        if (queue_.isEmpty()) {
            finish("Ошибка: не удалось скачать");
            return;
        }
        Candidate c = queue_.takeFirst();
        path_ = QDir(dirEdit_->text().trimmed()).filePath(c.fileName);
        file_ = new QFile(path_, this);
        if (!file_->open(QIODevice::WriteOnly)) {
            log("Не удалось открыть файл: " + path_);
            delete file_;
            file_ = nullptr;
            finish("Ошибка записи файла");
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
            finish("Готово: " + path_);
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

    void cancel() {
        queue_.clear();
        if (reply_) reply_->abort();
    }

    void finish(const QString& text) {
        statusLabel_->setText(text);
        setBusy(false);
    }

    void setBusy(bool busy) {
        downloadBtn_->setEnabled(!busy);
        cancelBtn_->setEnabled(busy);
    }

    void log(const QString& s) { log_->appendPlainText(s); }

    QListWidget* list_;
    QLineEdit *urlEdit_, *ghEdit_, *refEdit_, *dirEdit_;
    QPushButton *downloadBtn_, *cancelBtn_;
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
    MainWindow w;
    w.show();
    return app.exec();
}

#include "main.moc"
