#include "AnalysisManager.h"

#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QVariant>

AnalysisManager::AnalysisManager(QObject *parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("analysis_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    openDatabase();
    migrate();
}

AnalysisManager::~AnalysisManager()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName);
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool AnalysisManager::isOpen() const
{
    return QSqlDatabase::contains(m_connectionName)
        && QSqlDatabase::database(m_connectionName).isOpen();
}

void AnalysisManager::openDatabase()
{
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(baseDir);

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(QDir(baseDir).filePath(QStringLiteral("analysis.sqlite")));
    db.open();
}

void AnalysisManager::migrate()
{
    if (!isOpen())
        return;

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS analysis_records ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "kind TEXT NOT NULL,"
        "created_at TEXT NOT NULL,"
        "c0 TEXT, c1 TEXT, c2 TEXT, c3 TEXT, c4 TEXT, c5 TEXT,"
        "c6 TEXT, c7 TEXT, c8 TEXT, c9 TEXT, c10 TEXT, c11 TEXT,"
        "c12 TEXT, c13 TEXT, c14 TEXT"
        ")"));
    q.exec(QStringLiteral("ALTER TABLE analysis_records ADD COLUMN c12 TEXT"));
    q.exec(QStringLiteral("ALTER TABLE analysis_records ADD COLUMN c13 TEXT"));
    q.exec(QStringLiteral("ALTER TABLE analysis_records ADD COLUMN c14 TEXT"));
}

int AnalysisManager::addRecord(const QString &kind, const QStringList &cells)
{
    if (!isOpen())
        return -1;

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "INSERT INTO analysis_records "
        "(kind, created_at, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14) "
        "VALUES (:kind, datetime('now'), :c0, :c1, :c2, :c3, :c4, :c5, :c6, :c7, :c8, :c9, :c10, :c11, :c12, :c13, :c14)"));
    q.bindValue(QStringLiteral(":kind"), kind);
    for (int i = 0; i < 15; ++i)
        q.bindValue(QStringLiteral(":c%1").arg(i), cells.value(i));

    if (!q.exec())
        return -1;
    return q.lastInsertId().toInt();
}

QVector<AnalysisRecord> AnalysisManager::records(const QString &kind) const
{
    QVector<AnalysisRecord> out;
    if (!isOpen())
        return out;

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "SELECT id, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14 "
        "FROM analysis_records WHERE kind = :kind ORDER BY id DESC"));
    q.bindValue(QStringLiteral(":kind"), kind);
    if (!q.exec())
        return out;

    while (q.next()) {
        AnalysisRecord record;
        record.id = q.value(0).toInt();
        record.kind = kind;
        for (int i = 0; i < 15; ++i)
            record.cells << q.value(i + 1).toString();
        out.append(record);
    }
    return out;
}

void AnalysisManager::deleteRecord(int id)
{
    if (!isOpen() || id < 0)
        return;

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("DELETE FROM analysis_records WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    q.exec();
}
