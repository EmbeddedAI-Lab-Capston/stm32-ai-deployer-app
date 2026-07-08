#include "RegisterAdvisor.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

namespace {

// System prompt: asks for hypotheses (peripheral/field/suggested symbolic
// value/reasoning), explicitly not a "fix", and explicitly field/enum level
// — never asked to interpret raw hex itself (we don't send raw hex either).
const char *kSystemPrompt =
    "Sen bir STM32 gomulu sistem hata ayiklama uzmanisin. Sana bir register "
    "anlik goruntusunun (snapshot) alan (field) seviyesinde ozeti, iki "
    "anlik goruntu arasindaki farklar ve deterministik kural motorunun "
    "yakaladigi tutarsizliklar verilecek. Gorevin KESIN COZUM sunmak degil, "
    "olasi kok nedenler icin HIPOTEZ uretmek. Yanitini SADECE su JSON "
    "dizisi formatinda ver, baska metin ekleme:\n"
    "[{\"peripheral\":\"...\",\"field\":\"...\",\"hypothesis\":\"...\","
    "\"suggestedValue\":\"...\",\"reasoning\":\"...\"}]\n"
    "Hex deger yorumlama; sadece alan adlari, sembolik enum degerleri ve "
    "aciklamalar uzerinden akil yurut.";

} // namespace

RegisterAdvisor::RegisterAdvisor(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

QString RegisterAdvisor::buildPrompt(const RegisterSnapshot &snap, const SnapshotDiff &diff,
                                     const QList<RuleViolation> &violations) const
{
    QJsonObject ctx;
    ctx[QStringLiteral("board")]  = snap.boardName;
    ctx[QStringLiteral("device")] = snap.deviceName;

    QJsonArray violationsJson;
    for (const RuleViolation &v : violations) {
        QJsonObject o;
        o[QStringLiteral("severity")]   = v.severity;
        o[QStringLiteral("peripheral")] = v.peripheralName;
        o[QStringLiteral("register")]   = v.registerName;
        o[QStringLiteral("field")]      = v.fieldName;
        o[QStringLiteral("message")]    = v.message;
        violationsJson.append(o);
    }
    ctx[QStringLiteral("ruleViolations")] = violationsJson;

    QJsonArray diffJson;
    if (diff.comparable) {
        for (const RegisterDiff &rd : diff.changedRegisters) {
            QJsonObject ro;
            ro[QStringLiteral("peripheral")] = rd.peripheralName;
            ro[QStringLiteral("register")]   = rd.registerName;
            QJsonArray fields;
            for (const FieldDiff &fd : rd.changedFields) {
                QJsonObject fo;
                fo[QStringLiteral("field")]       = fd.name;
                fo[QStringLiteral("description")] = fd.description;
                // Symbolic values only — never raw hex — per the system prompt's contract.
                fo[QStringLiteral("before")] = fd.enumNameA.isEmpty()
                    ? QJsonValue(double(fd.valueA)) : QJsonValue(fd.enumNameA);
                fo[QStringLiteral("after")] = fd.enumNameB.isEmpty()
                    ? QJsonValue(double(fd.valueB)) : QJsonValue(fd.enumNameB);
                fields.append(fo);
            }
            ro[QStringLiteral("changedFields")] = fields;
            diffJson.append(ro);
        }
    }
    ctx[QStringLiteral("diff")] = diffJson;

    return QString::fromUtf8(QJsonDocument(ctx).toJson(QJsonDocument::Compact));
}

void RegisterAdvisor::requestDiagnosis(const RegisterSnapshot &snap, const SnapshotDiff &diff,
                                       const QList<RuleViolation> &violations)
{
    if (!isConfigured()) {
        emit diagnosisFailed(QStringLiteral("LLM yapılandırılmamış (Ayarlar'dan base URL + API key girin)"));
        return;
    }
    if (m_busy) {
        emit diagnosisFailed(QStringLiteral("Bir tanılama isteği zaten sürüyor"));
        return;
    }
    if (violations.isEmpty() && !diff.comparable) {
        emit diagnosisFailed(QStringLiteral("Gönderilecek fark veya kural ihlali yok"));
        return;
    }

    QJsonObject body;
    body[QStringLiteral("model")] = m_model.isEmpty() ? QStringLiteral("gpt-4o-mini") : m_model;
    QJsonArray messages;
    QJsonObject sys; sys[QStringLiteral("role")] = QStringLiteral("system");
    sys[QStringLiteral("content")] = QString::fromUtf8(kSystemPrompt);
    QJsonObject usr; usr[QStringLiteral("role")] = QStringLiteral("user");
    usr[QStringLiteral("content")] = buildPrompt(snap, diff, violations);
    messages.append(sys);
    messages.append(usr);
    body[QStringLiteral("messages")] = messages;

    QString url = m_baseUrl;
    if (url.endsWith(QLatin1Char('/')))
        url.chop(1);
    if (!url.endsWith(QStringLiteral("/chat/completions")))
        url += QStringLiteral("/chat/completions");

    QNetworkRequest req{ QUrl(url) };
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());

    m_busy = true;
    QNetworkReply *reply = m_net->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onReplyFinished(reply); });
}

void RegisterAdvisor::onReplyFinished(QNetworkReply *reply)
{
    m_busy = false;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        emit diagnosisFailed(QStringLiteral("LLM isteği başarısız (HTTP %1): %2")
                                  .arg(status).arg(reply->errorString()));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    const QJsonArray choices = doc.object().value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        emit diagnosisFailed(QStringLiteral("LLM yanıtı beklenmedik biçimde geldi (choices boş)"));
        return;
    }
    const QString content = choices.first().toObject()
                                 .value(QStringLiteral("message")).toObject()
                                 .value(QStringLiteral("content")).toString();
    emit diagnosisReady(parseResponseContent(content));
}

QVariantList RegisterAdvisor::parseResponseContent(const QString &content) const
{
    // Tolerant parse: extract the first [...] block regardless of surrounding
    // prose, since not every provider/model reliably returns bare JSON despite
    // the system prompt's instruction.
    static const QRegularExpression arrayRe(QStringLiteral("\\[.*\\]"),
                                             QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch m = arrayRe.match(content);
    if (m.hasMatch()) {
        const QJsonDocument doc = QJsonDocument::fromJson(m.captured(0).toUtf8());
        if (doc.isArray()) {
            QVariantList out;
            for (const QJsonValue &v : doc.array())
                if (v.isObject())
                    out.append(v.toObject().toVariantMap());
            if (!out.isEmpty())
                return out;
        }
    }

    // Fall back to a single free-text hypothesis rather than discarding the
    // response — a provider that ignores the JSON instruction still gives
    // the user something to read.
    QVariantMap freeText;
    freeText[QStringLiteral("hypothesis")] = content.trimmed();
    return { freeText };
}
