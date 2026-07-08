#pragma once
#include "RegisterDiff.h"
#include "RegisterRuleModel.h"
#include "RegisterSnapshot.h"

#include <QObject>
#include <QString>
#include <QVariantList>

class QNetworkAccessManager;
class QNetworkReply;

// ── RegisterAdvisor ─────────────────────────────────────────────────────────
// Optional, provider-agnostic LLM diagnosis layer (plan Bolum 1c). Sends the
// decoded diff + rule violations (field/enum level, never raw hex) to any
// OpenAI-compatible chat/completions endpoint and asks for hypotheses, not
// fixes. If baseUrl/apiKey are not configured, requestDiagnosis() fails
// immediately and harmlessly — the rest of the tool (diff, rule engine, JSON
// export) works fully without this class ever succeeding. No feature in this
// app depends on network access except this one, by design.
class RegisterAdvisor : public QObject
{
    Q_OBJECT
public:
    explicit RegisterAdvisor(QObject *parent = nullptr);

    void setBaseUrl(const QString &url) { m_baseUrl = url; }
    void setApiKey(const QString &key)  { m_apiKey = key; }
    void setModel(const QString &model) { m_model = model; }
    bool isConfigured() const { return !m_baseUrl.trimmed().isEmpty() && !m_apiKey.trimmed().isEmpty(); }

    bool isBusy() const { return m_busy; }

    // snap = the slot the violations were computed against (for board/device
    // context in the prompt); diff may be a default-constructed SnapshotDiff
    // (comparable=false) if only one slot is available — the prompt adapts.
    void requestDiagnosis(const RegisterSnapshot &snap, const SnapshotDiff &diff,
                          const QList<RuleViolation> &violations);

signals:
    void diagnosisReady(const QVariantList &hypotheses);
    void diagnosisFailed(const QString &message);

private:
    QString buildPrompt(const RegisterSnapshot &snap, const SnapshotDiff &diff,
                        const QList<RuleViolation> &violations) const;
    void onReplyFinished(QNetworkReply *reply);
    QVariantList parseResponseContent(const QString &content) const;

    QNetworkAccessManager *m_net = nullptr;
    QString m_baseUrl;
    QString m_apiKey;
    QString m_model;
    bool    m_busy = false;
};
