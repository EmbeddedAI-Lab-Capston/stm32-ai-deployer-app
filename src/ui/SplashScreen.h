#pragma once

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QScreen>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QFont>

class SplashScreen : public QWidget
{
    Q_OBJECT

public:
    explicit SplashScreen(QWidget *parent = nullptr)
        : QWidget(parent, Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setFixedSize(700, 420);

        // Center on screen
        QScreen *screen = QApplication::primaryScreen();
        QRect geo = screen->geometry();
        move((geo.width() - width()) / 2, (geo.height() - height()) / 2);

        setupUI();
        startAnimation();
    }

    void startClosingSequence(int delayMs = 3500)
    {
        QTimer::singleShot(delayMs, this, [this]() {
            auto *fadeOut = new QPropertyAnimation(m_opacityEffect, "opacity", this);
            fadeOut->setDuration(600);
            fadeOut->setStartValue(1.0);
            fadeOut->setEndValue(0.0);
            connect(fadeOut, &QPropertyAnimation::finished, this, &SplashScreen::done);
            fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
        });
    }

signals:
    void done();

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Rounded rect background
        QPainterPath path;
        path.addRoundedRect(rect(), 18, 18);

        // Dark gradient
        QLinearGradient grad(0, 0, width(), height());
        grad.setColorAt(0.0, QColor("#0d1b2a"));
        grad.setColorAt(1.0, QColor("#1b3358"));
        p.fillPath(path, grad);

        // Cyan accent line near bottom
        p.setPen(Qt::NoPen);
        QLinearGradient lineGrad(0, 0, width(), 0);
        lineGrad.setColorAt(0.0,  QColor(0, 0, 0, 0));
        lineGrad.setColorAt(0.3,  QColor("#00d4ff"));
        lineGrad.setColorAt(0.7,  QColor("#00d4ff"));
        lineGrad.setColorAt(1.0,  QColor(0, 0, 0, 0));
        p.setBrush(lineGrad);
        p.drawRect(0, height() - 3, width(), 3);
    }

private:
    void setupUI()
    {
        m_opacityEffect = new QGraphicsOpacityEffect(this);
        m_opacityEffect->setOpacity(0.0);
        setGraphicsEffect(m_opacityEffect);

        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(40, 35, 40, 30);
        mainLayout->setSpacing(0);

        // Logo
        m_logoLabel = new QLabel(this);
        m_logoLabel->setAttribute(Qt::WA_TranslucentBackground);
        QPixmap logo(":/app_icon.png");
        if (!logo.isNull())
            m_logoLabel->setPixmap(logo.scaled(90, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_logoLabel->setAlignment(Qt::AlignCenter);
        m_logoLabel->setStyleSheet("background: transparent;");
        mainLayout->addWidget(m_logoLabel);

        mainLayout->addSpacing(18);

        // App name
        m_titleLabel = new QLabel("STM32 AI Deployer", this);
        m_titleLabel->setAlignment(Qt::AlignCenter);
        m_titleLabel->setAttribute(Qt::WA_TranslucentBackground);
        m_titleLabel->setStyleSheet(
            "background: transparent;"
            "color: #ffffff;"
            "font-size: 30px;"
            "font-weight: 700;"
            "font-family: 'Segoe UI', sans-serif;"
            "letter-spacing: 1px;"
        );
        mainLayout->addWidget(m_titleLabel);

        mainLayout->addSpacing(8);

        // Subtitle
        m_subtitleLabel = new QLabel("AI Model Deployment Tool for STM32 Microcontrollers", this);
        m_subtitleLabel->setAlignment(Qt::AlignCenter);
        m_subtitleLabel->setAttribute(Qt::WA_TranslucentBackground);
        m_subtitleLabel->setStyleSheet(
            "background: transparent;"
            "color: #7ab8d4;"
            "font-size: 13px;"
            "font-family: 'Segoe UI', sans-serif;"
        );
        mainLayout->addWidget(m_subtitleLabel);

        mainLayout->addSpacing(6);

        // Org + version
        m_orgLabel = new QLabel("Marmara University  ·  v1.0.0", this);
        m_orgLabel->setAlignment(Qt::AlignCenter);
        m_orgLabel->setAttribute(Qt::WA_TranslucentBackground);
        m_orgLabel->setStyleSheet(
            "background: transparent;"
            "color: #4a7a9b;"
            "font-size: 11px;"
            "font-family: 'Segoe UI', sans-serif;"
        );
        mainLayout->addWidget(m_orgLabel);

        mainLayout->addStretch();

        // Progress bar
        m_progressBar = new QProgressBar(this);
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
        m_progressBar->setTextVisible(false);
        m_progressBar->setFixedHeight(4);
        m_progressBar->setStyleSheet(
            "QProgressBar {"
            "  background: rgba(255,255,255,0.08);"
            "  border: none;"
            "  border-radius: 2px;"
            "}"
            "QProgressBar::chunk {"
            "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "    stop:0 #00d4ff, stop:1 #0077ff);"
            "  border-radius: 2px;"
            "}"
        );
        mainLayout->addWidget(m_progressBar);

        mainLayout->addSpacing(10);

        // Loading status
        m_statusLabel = new QLabel("Initializing...", this);
        m_statusLabel->setAlignment(Qt::AlignCenter);
        m_statusLabel->setAttribute(Qt::WA_TranslucentBackground);
        m_statusLabel->setStyleSheet(
            "background: transparent;"
            "color: #4a7a9b;"
            "font-size: 11px;"
            "font-family: 'Segoe UI', sans-serif;"
        );
        mainLayout->addWidget(m_statusLabel);
    }

    void startAnimation()
    {
        // Fade in
        auto *fadeIn = new QPropertyAnimation(m_opacityEffect, "opacity", this);
        fadeIn->setDuration(500);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);
        fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

        // Progress bar animation
        m_progressTimer = new QTimer(this);
        connect(m_progressTimer, &QTimer::timeout, this, &SplashScreen::updateProgress);
        m_progressTimer->start(35);
    }

    void updateProgress()
    {
        int val = m_progressBar->value() + 1;
        m_progressBar->setValue(val);

        if (val < 30) {
            m_statusLabel->setText("Loading resources...");
        } else if (val < 60) {
            m_statusLabel->setText("Initializing modules...");
        } else if (val < 85) {
            m_statusLabel->setText("Starting serial interface...");
        } else {
            m_statusLabel->setText("Ready.");
        }

        if (val >= 100)
            m_progressTimer->stop();
    }

    QGraphicsOpacityEffect *m_opacityEffect = nullptr;
    QLabel        *m_logoLabel     = nullptr;
    QLabel        *m_titleLabel    = nullptr;
    QLabel        *m_subtitleLabel = nullptr;
    QLabel        *m_orgLabel      = nullptr;
    QLabel        *m_statusLabel   = nullptr;
    QProgressBar  *m_progressBar   = nullptr;
    QTimer        *m_progressTimer = nullptr;
};
