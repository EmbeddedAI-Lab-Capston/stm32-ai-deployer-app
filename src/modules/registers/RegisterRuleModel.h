#pragma once
#include <QString>
#include <QList>

// ── Rule model ──────────────────────────────────────────────────────────────
// Data-driven consistency rules (plan Bolum 1b). Rules are loaded from
// svd/rules.json, not hardcoded — the same "everything family-specific is
// data, not code" principle ReadPlanBuilder's RCC-gating already follows
// (docs/register_inspector_plan.md Bolum 3.3). Field/register names are
// matched against SVD names, so a rule that only makes sense for one register
// name (e.g. exact "S0CR") only ever fires for peripherals that actually have
// that register — no per-family branching in code.

enum class RuleConditionType
{
    // ifRegister.ifField != 0  =>  thenRegister's raw value != 0
    FieldNonZeroImpliesRegisterNonZero,
    // ifRegister.ifField != 0  =>  thenRegister.thenField != 0
    FieldNonZeroImpliesFieldNonZero,
    // ifRegister.ifField != 0  =>  thenRegister.thenField == 0
    FieldNonZeroImpliesFieldZero,
};

struct RuleCondition
{
    RuleConditionType type = RuleConditionType::FieldNonZeroImpliesRegisterNonZero;
    QString ifRegisterName;
    QString ifFieldName;
    QString thenRegisterName;
    QString thenFieldName;   // unused for FieldNonZeroImpliesRegisterNonZero
};

struct Rule
{
    QString id;
    QString severity;         // "warning" | "info" | "error"
    QString message;          // may reference {peripheral}, {ifRegister}, {ifField},
                               // {thenRegister}, {thenField} placeholders
    QString groupContains;    // matched against SvdPeripheral.groupName, case-insensitive
                               // substring; empty = applies to every peripheral
    RuleCondition condition;
};

struct RuleViolation
{
    QString ruleId;
    QString severity;
    QString peripheralName;
    QString registerName;   // the "if" register — primary UI anchor
    QString fieldName;      // the "if" field
    QString message;
};
