#pragma once

#include <QWizard>
#include "modules/flash/PipelineConfig.h"

class AppState;

// 4-step wizard that guides the user from a .tflite model file to a
// compiled and flashed firmware image.
//
//   Page 1 — Model      : select model file + quantization
//   Page 2 — Sensor     : sensor type + pin configuration
//   Page 3 — Summary    : validate tools + choose output directory
//   Page 4 — Build      : run pipeline, show live progress & log
class PipelineWizard : public QWizard
{
    Q_OBJECT

public:
    explicit PipelineWizard(AppState *state, QWidget *parent = nullptr);

    // Final configuration assembled across the wizard pages.
    // Valid only after the wizard has been accepted.
    const PipelineConfig &config() const { return m_config; }

private:
    void setupPages();

    AppState      *m_appState = nullptr;
    PipelineConfig m_config;
};
