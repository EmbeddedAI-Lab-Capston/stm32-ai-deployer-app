#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

// Manages board presets and custom board configurations.
class BoardManager : public QObject
{
    Q_OBJECT

public:
    explicit BoardManager(QObject *parent = nullptr);
    ~BoardManager() override;

    // TODO: Return list of built-in board preset names (STM32F4, STM32H7, STM32N6)
    // QStringList availablePresets() const;

    // TODO: Load a board preset by name and return its configuration as a struct
    // BoardConfig loadPreset(const QString &presetName) const;

    // TODO: Load custom board configuration from a JSON file
    // BoardConfig loadFromJson(const QString &filePath) const;

    // TODO: Save a custom board configuration to a JSON file
    // void saveToJson(const BoardConfig &config, const QString &filePath) const;

    // TODO: Return the currently active board configuration
    // BoardConfig activeBoard() const;

    // TODO: Set the active board and emit boardChanged signal
    // void setActiveBoard(const BoardConfig &config);

signals:
    // TODO: Emitted when the active board changes
    // void boardChanged(const BoardConfig &config);
};
