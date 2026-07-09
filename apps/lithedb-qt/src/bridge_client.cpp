#include "bridge_client.h"
#include "bridge_utils.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

BridgeClient* BridgeClient::instance()
{
    static BridgeClient* s_instance = new BridgeClient(qApp);
    return s_instance;
}

BridgeClient::BridgeClient(QObject* parent)
    : QObject(parent)
{
}

BridgeClient::~BridgeClient()
{
    stop();
}

bool BridgeClient::is_running() const
{
    return process_ && process_->state() == QProcess::Running;
}

void BridgeClient::start()
{
    ensure_started();
}

void BridgeClient::stop()
{
    if (!process_) {
        fail_pending_commands(QStringLiteral("Bridge process stopped"));
        return;
    }

    shutting_down_ = true;
    if (process_->state() == QProcess::Running) {
        const QJsonObject cmd{
            {QStringLiteral("id"), QStringLiteral("shutdown")},
            {QStringLiteral("command"), QStringLiteral("exit")},
            {QStringLiteral("args"), QJsonArray()}
        };
        const auto line = QJsonDocument(cmd).toJson(QJsonDocument::Compact) + '\n';
        process_->write(line);
        process_->waitForBytesWritten(500);
        process_->waitForFinished(2000);
    }
    teardown_process(QStringLiteral("Bridge process stopped"), true);
    shutting_down_ = false;
}

void BridgeClient::restart(const QString& reason)
{
    shutting_down_ = true;
    teardown_process(reason, true);
    shutting_down_ = false;
    ensure_started();
}

void BridgeClient::teardown_process(const QString& pendingError, bool killImmediately)
{
    if (process_) {
        process_->disconnect(this);
        if (process_->state() != QProcess::NotRunning) {
            if (killImmediately) {
                process_->kill();
            }
            process_->waitForFinished(1000);
        }
        process_->deleteLater();
        process_ = nullptr;
    }
    read_buffer_.clear();
    fail_pending_commands(pendingError);
}

bool BridgeClient::ensure_started()
{
    if (process_ && process_->state() == QProcess::Running) {
        return true;
    }

    if (process_) {
        process_->disconnect(this);
        process_->deleteLater();
        process_ = nullptr;
    }

    read_buffer_.clear();

    process_ = new QProcess(this);
    process_->setProcessChannelMode(QProcess::SeparateChannels);

    connect(process_, &QProcess::readyReadStandardOutput, this, &BridgeClient::on_ready_read);
    connect(process_, &QProcess::readyReadStandardError, this, &BridgeClient::on_ready_read_stderr);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &BridgeClient::on_process_finished);

    const QString bridgePath = lith_bridge::resolve_bridge_binary_path();
    process_->start(bridgePath, QStringList{QStringLiteral("--daemon")});
    if (!process_->waitForStarted(5000)) {
        const QString error = process_->errorString().isEmpty()
            ? QStringLiteral("Failed to start bridge process")
            : process_->errorString();
        qWarning() << "BridgeClient:" << error << "path:" << bridgePath;
        process_->disconnect(this);
        process_->deleteLater();
        process_ = nullptr;
        return false;
    }
    return true;
}

void BridgeClient::send_command(const QString& command, const QJsonArray& args, ResponseCallback callback)
{
    if (!ensure_started()) {
        QJsonObject errorResp;
        errorResp[QStringLiteral("error")] = QStringLiteral("Bridge process not running");
        if (callback) {
            callback(errorResp);
        }
        return;
    }

    const QString id = QString::number(next_id_++);

    QJsonObject request;
    request[QStringLiteral("id")] = id;
    request[QStringLiteral("command")] = command;
    request[QStringLiteral("args")] = args;

    if (callback) {
        pending_commands_.insert(id, std::move(callback));
    }

    const auto line = QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';
    if (process_->write(line) == -1) {
        pending_commands_.remove(id);
        QJsonObject errorResp;
        errorResp[QStringLiteral("error")] = QStringLiteral("Failed to write to bridge process");
        if (callback) {
            callback(errorResp);
        }
    }
}

void BridgeClient::on_ready_read()
{
    if (!process_) {
        return;
    }
    read_buffer_.append(process_->readAllStandardOutput());

    int newlinePos;
    while ((newlinePos = read_buffer_.indexOf('\n')) != -1) {
        const QByteArray line = read_buffer_.left(newlinePos).trimmed();
        read_buffer_.remove(0, newlinePos + 1);

        if (line.isEmpty()) {
            continue;
        }

        const auto doc = QJsonDocument::fromJson(line);
        if (!doc.isObject()) {
            qWarning() << "BridgeClient: non-JSON response line:" << line;
            continue;
        }

        const auto response = doc.object();
        const QString id = response.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            continue;
        }

        auto it = pending_commands_.find(id);
        if (it != pending_commands_.end()) {
            auto callback = std::move(it.value());
            pending_commands_.erase(it);
            if (callback) {
                callback(response);
            }
        }
    }
}

void BridgeClient::on_ready_read_stderr()
{
    if (!process_) {
        return;
    }
    const auto err = process_->readAllStandardError().trimmed();
    if (!err.isEmpty()) {
        qWarning() << "Bridge stderr:" << err;
    }
}

void BridgeClient::on_process_finished(int /*exitCode*/, QProcess::ExitStatus /*exitStatus*/)
{
    if (shutting_down_) {
        return;
    }
    fail_pending_commands(QStringLiteral("Bridge process exited unexpectedly"));
    read_buffer_.clear();
    if (process_) {
        process_->disconnect(this);
        process_->deleteLater();
        process_ = nullptr;
    }
}

void BridgeClient::fail_pending_commands(const QString& error)
{
    QJsonObject errorResp;
    errorResp[QStringLiteral("error")] = error;

    auto pending = std::move(pending_commands_);
    pending_commands_.clear();

    for (auto it = pending.begin(); it != pending.end(); ++it) {
        if (it.value()) {
            it.value()(errorResp);
        }
    }
}
