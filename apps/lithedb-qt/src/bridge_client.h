#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QProcess>
#include <QString>
#include <functional>

class BridgeClient : public QObject
{
    Q_OBJECT

public:
    static BridgeClient* instance();

    using ResponseCallback = std::function<void(const QJsonObject& response)>;

    void send_command(const QString& command, const QJsonArray& args, ResponseCallback callback);
    bool is_running() const;
    void start();
    void stop();
    /// Kill the bridge process (aborting any in-flight DB op) and start a fresh daemon.
    void restart(const QString& reason = QStringLiteral("Operation cancelled"));

private:
    explicit BridgeClient(QObject* parent = nullptr);
    ~BridgeClient() override;

    bool ensure_started();
    void teardown_process(const QString& pendingError, bool killImmediately);
    void on_ready_read();
    void on_ready_read_stderr();
    void on_process_finished(int exitCode, QProcess::ExitStatus exitStatus);
    void fail_pending_commands(const QString& error);

    QProcess* process_ = nullptr;
    QByteArray read_buffer_;
    QMap<QString, ResponseCallback> pending_commands_;
    int next_id_ = 1;
    bool shutting_down_ = false;
};
