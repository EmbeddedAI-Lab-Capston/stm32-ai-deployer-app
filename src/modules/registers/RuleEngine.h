#pragma once
#include "RegisterRuleModel.h"
#include "RegisterSnapshot.h"

#include <QList>
#include <QString>

// ── RuleEngine ──────────────────────────────────────────────────────────────
// Pure logic (no QObject): loads data-driven rules (svd/rules.json, same
// distribution/JSON-parsing style as SvdCatalog's boards.json) and evaluates
// them against a decoded snapshot. No LLM involved — deterministic field
// comparisons only. A rule whose referenced register/field does not exist on
// a given peripheral (e.g. an I2Cv1-style rule tested against an I2Cv2
// peripheral) simply does not fire for that peripheral — never treated as a
// false "0" value. See plan Bolum 1b.
class RuleEngine
{
public:
    bool loadRules(const QString &path);   // false + errorString() on failure
    QString errorString() const { return m_error; }

    QList<RuleViolation> evaluate(const QList<DecodedPeripheral> &peripherals) const;

private:
    const DecodedRegister *findRegister(const DecodedPeripheral &p, const QString &name) const;
    const DecodedField    *findField(const DecodedRegister &r, const QString &name) const;
    QString formatMessage(const Rule &rule, const QString &peripheralName) const;

    QList<Rule> m_rules;
    QString     m_error;
};
