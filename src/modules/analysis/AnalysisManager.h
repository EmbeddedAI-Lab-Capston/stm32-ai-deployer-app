#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct AnalysisRecord {
    int id = -1;
    QString kind;
    QStringList cells;
};

class AnalysisManager : public QObject
{
    Q_OBJECT

public:
    explicit AnalysisManager(QObject *parent = nullptr);
    ~AnalysisManager() override;

    bool isOpen() const;
    int addRecord(const QString &kind, const QStringList &cells);
    QVector<AnalysisRecord> records(const QString &kind) const;
    void deleteRecord(int id);

private:
    void openDatabase();
    void migrate();

    QString m_connectionName;
};
