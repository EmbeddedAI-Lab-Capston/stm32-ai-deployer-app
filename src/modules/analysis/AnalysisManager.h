#pragma once

#include <QObject>
#include <QString>

// Manages session recording to SQLite and provides data for comparison charts.
class AnalysisManager : public QObject
{
    Q_OBJECT

public:
    explicit AnalysisManager(QObject *parent = nullptr);
    ~AnalysisManager() override;

    // TODO: Open (or create) the SQLite database at the given path
    // bool openDatabase(const QString &dbPath);

    // TODO: Begin a new recording session; returns session ID
    // int startSession(const QString &boardName, const QString &modelName);

    // TODO: End the current recording session
    // void endSession();

    // TODO: Insert an inference metric record into the current session
    // void recordMetric(const QJsonObject &packet);

    // TODO: Return all session records as a list of structs for the comparison table
    // QList<SessionRecord> allSessions() const;

    // TODO: Return all metric rows for the given session ID (for chart data)
    // QList<MetricRecord> metricsForSession(int sessionId) const;

    // TODO: Delete a session and all its metric rows by session ID
    // void deleteSession(int sessionId);

signals:
    // TODO: Emitted when a new metric is inserted (for live chart updates)
    // void metricRecorded(const QJsonObject &packet);

    // TODO: Emitted when the session list changes (add/delete)
    // void sessionsChanged();
};
