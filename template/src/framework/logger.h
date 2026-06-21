#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <QStandardPaths>
#include <QDir>

// Initialize application-wide logging.
// Call once at startup. Creates a console sink + rotating file sink.
inline void initLogger(const QString& appName = "yz-ui-template") {
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::info);

    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(logDir);
    QString logPath = logDir + "/" + appName + ".log";

    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logPath.toStdString(), 1024 * 1024 * 5, 3);  // 5MB × 3 files
    fileSink->set_level(spdlog::level::trace);

    auto logger = std::make_shared<spdlog::logger>(appName.toStdString(),
        spdlog::sinks_init_list{consoleSink, fileSink});
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::warn);
    spdlog::set_default_logger(logger);

    spdlog::info("Logger initialized. Log file: {}", logPath.toStdString());
}
