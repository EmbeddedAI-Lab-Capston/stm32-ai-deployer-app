#pragma once
#include <QWidget>
#include "core/AppState.h"

class SerialManager;
class QComboBox;
class QLabel;
class QPushButton;
class QProcess;

// ── Sidebar ───────────────────────────────────────────────────────────────
// Left-hand panel widget.  Owns board selection, port/baud selection and
// the Connect button.  Writes to AppState; the rest of the app reacts to
// AppState signals rather than Sidebar signals directly.
class Sidebar : public QWidget
{
    Q_OBJECT

public:
    explicit Sidebar(AppState     *state,
                     SerialManager *serial,
                     QWidget       *parent = nullptr);

    // Re-scan serial ports and repopulate the port combo.
    void refreshPorts();

private slots:
    void onBoardComboChanged(int index);
    void onPortComboChanged(int index);
    void onBaudComboChanged(int index);
    void onConnectClicked();
    void onRefreshClicked();
    void onAddCustomBoardClicked();

    // AppState reactions
    void onConnectionChanged(bool connected, const QString &info);
    void onBoardStateChanged(const BoardInfo &board);
    void onLastModelChanged(const QString &name, double infMs, quint8 acc);
    void onStLinkProbeFinished(int exitCode);

private:
    void setupUi();
    void setupConnections();
    void populateBoards();
    void populatePorts();
    void populateBauds();
    void ensureBoardVisible(const BoardInfo &board);
    void startStLinkBoardProbe();
    void applyDetectedStLinkBoard(const QString &probeOutput);

    AppState      *m_state  = nullptr;
    SerialManager *m_serial = nullptr;

    // ── Board section ─────────────────────────────────────────────────
    QComboBox *m_boardCombo  = nullptr;
    QLabel    *m_boardNameLabel = nullptr;
    QLabel    *m_flashLabel  = nullptr;
    QLabel    *m_ramLabel    = nullptr;
    QLabel    *m_clockLabel  = nullptr;

    // ── Connection section ────────────────────────────────────────────
    QComboBox   *m_portCombo   = nullptr;
    QComboBox   *m_baudCombo   = nullptr;
    QPushButton *m_connectBtn  = nullptr;
    QPushButton *m_refreshBtn  = nullptr;
    QLabel      *m_statusLabel = nullptr;
    QProcess    *m_stlinkProbe = nullptr;
    QString      m_stlinkPortName;

    // ── Last model section ────────────────────────────────────────────
    QLabel *m_modelNameLabel = nullptr;
    QLabel *m_modelMetaLabel = nullptr;
};
