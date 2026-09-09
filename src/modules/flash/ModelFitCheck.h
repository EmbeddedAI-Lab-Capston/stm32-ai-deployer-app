#pragma once
#include "XCubeAIRunner.h"
#include "modules/board/BoardPresets.h"

#include <QStringList>

// ── ModelFitCheck ─────────────────────────────────────────────────────────
// Pure (no process/QObject): the "does this model fit" arithmetic used by
// PipelineRunner::onXCubeAnalyzeFinished() (Faz 10.5, plan
// docs/memory_telemetry_plan.md Bolum 7), pulled out so it is unit-testable
// on its own. Returns human-readable Turkish warning strings matching the
// pipelineLines convention; an EMPTY list means "no concerns found" — never
// a hard failure, this is advisory only (the user may still proceed).
//
// kEstFirmwareRamBytes/kEstFirmwareFlashBytes are ROUGH ESTIMATES of what
// the non-AI parts of the firmware need beyond the model itself (HAL,
// stack, sensor/UART buffers) — measured once against a real build (F4 +
// anomaly_mlp_int8: ~68 KB static RAM beyond the model's own 716 B
// activations buffer). NOT a per-board/per-sensor guarantee. The exact,
// authoritative check is the POST-BUILD RAM budget bar (Faz 10.2,
// Backend::ramBudget()) — this one exists only to catch an obviously
// oversized model before spending minutes compiling and flashing.
QStringList checkModelFitsBoard(const ModelFootprint &footprint, const BoardInfo &board);
