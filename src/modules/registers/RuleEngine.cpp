#include "RuleEngine.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

RuleConditionType parseConditionType(const QString &s)
{
    if (s == QStringLiteral("fieldNonZeroImpliesFieldNonZero"))
        return RuleConditionType::FieldNonZeroImpliesFieldNonZero;
    if (s == QStringLiteral("fieldNonZeroImpliesFieldZero"))
        return RuleConditionType::FieldNonZeroImpliesFieldZero;
    return RuleConditionType::FieldNonZeroImpliesRegisterNonZero;   // default
}

} // namespace

bool RuleEngine::loadRules(const QString &path)
{
    m_error.clear();
    m_rules.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_error = QStringLiteral("Cannot open rules file: %1").arg(path);
        return false;
    }

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &perr);
    if (perr.error != QJsonParseError::NoError) {
        m_error = QStringLiteral("rules.json parse error: %1").arg(perr.errorString());
        return false;
    }

    const QJsonArray rules = doc.object().value(QStringLiteral("rules")).toArray();
    for (const QJsonValue &v : rules) {
        const QJsonObject o = v.toObject();
        Rule rule;
        rule.id             = o.value(QStringLiteral("id")).toString();
        rule.severity       = o.value(QStringLiteral("severity")).toString(QStringLiteral("warning"));
        rule.message        = o.value(QStringLiteral("message")).toString();
        rule.groupContains  = o.value(QStringLiteral("groupContains")).toString();

        const QJsonObject c = o.value(QStringLiteral("condition")).toObject();
        rule.condition.type             = parseConditionType(c.value(QStringLiteral("type")).toString());
        rule.condition.ifRegisterName   = c.value(QStringLiteral("ifRegisterName")).toString();
        rule.condition.ifFieldName      = c.value(QStringLiteral("ifFieldName")).toString();
        rule.condition.thenRegisterName = c.value(QStringLiteral("thenRegisterName")).toString();
        rule.condition.thenFieldName    = c.value(QStringLiteral("thenFieldName")).toString();

        if (!rule.id.isEmpty() && !rule.condition.ifRegisterName.isEmpty())
            m_rules.append(rule);
    }

    if (m_rules.isEmpty()) {
        m_error = QStringLiteral("rules.json contains no valid rules");
        return false;
    }
    return true;
}

const DecodedRegister *RuleEngine::findRegister(const DecodedPeripheral &p, const QString &name) const
{
    for (const DecodedRegister &r : p.registers)
        if (r.name.compare(name, Qt::CaseInsensitive) == 0)
            return &r;
    return nullptr;
}

const DecodedField *RuleEngine::findField(const DecodedRegister &r, const QString &name) const
{
    for (const DecodedField &f : r.fields)
        if (f.name.compare(name, Qt::CaseInsensitive) == 0)
            return &f;
    return nullptr;
}

QString RuleEngine::formatMessage(const Rule &rule, const QString &peripheralName) const
{
    QString msg = rule.message;
    msg.replace(QStringLiteral("{peripheral}"), peripheralName);
    msg.replace(QStringLiteral("{ifRegister}"), rule.condition.ifRegisterName);
    msg.replace(QStringLiteral("{ifField}"), rule.condition.ifFieldName);
    msg.replace(QStringLiteral("{thenRegister}"), rule.condition.thenRegisterName);
    msg.replace(QStringLiteral("{thenField}"), rule.condition.thenFieldName);
    return msg;
}

QList<RuleViolation> RuleEngine::evaluate(const QList<DecodedPeripheral> &peripherals) const
{
    QList<RuleViolation> violations;

    for (const DecodedPeripheral &p : peripherals) {
        for (const Rule &rule : m_rules) {
            if (!rule.groupContains.isEmpty()
                && !p.groupName.contains(rule.groupContains, Qt::CaseInsensitive))
                continue;

            const DecodedRegister *ifReg = findRegister(p, rule.condition.ifRegisterName);
            if (!ifReg || ifReg->status != RegStatus::Ok)
                continue;   // register not present / not readable — cannot judge
            const DecodedField *ifField = findField(*ifReg, rule.condition.ifFieldName);
            if (!ifField || ifField->value == 0)
                continue;   // trigger condition not met

            bool violated = false;
            switch (rule.condition.type) {
            case RuleConditionType::FieldNonZeroImpliesRegisterNonZero: {
                const DecodedRegister *thenReg = findRegister(p, rule.condition.thenRegisterName);
                if (!thenReg || thenReg->status != RegStatus::Ok)
                    continue;   // can't confirm either way
                violated = (thenReg->rawValue == 0);
                break;
            }
            case RuleConditionType::FieldNonZeroImpliesFieldNonZero:
            case RuleConditionType::FieldNonZeroImpliesFieldZero: {
                const DecodedRegister *thenReg = findRegister(p, rule.condition.thenRegisterName);
                if (!thenReg || thenReg->status != RegStatus::Ok)
                    continue;
                const DecodedField *thenField = findField(*thenReg, rule.condition.thenFieldName);
                if (!thenField)
                    continue;
                violated = (rule.condition.type == RuleConditionType::FieldNonZeroImpliesFieldNonZero)
                               ? (thenField->value == 0)
                               : (thenField->value != 0);
                break;
            }
            }

            if (!violated)
                continue;

            RuleViolation rv;
            rv.ruleId         = rule.id;
            rv.severity       = rule.severity;
            rv.peripheralName = p.name;
            rv.registerName   = ifReg->name;
            rv.fieldName      = ifField->name;
            rv.message        = formatMessage(rule, p.name);
            violations.append(rv);
        }
    }

    return violations;
}
