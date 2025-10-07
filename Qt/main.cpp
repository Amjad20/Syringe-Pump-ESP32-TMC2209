#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QGridLayout>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QComboBox>
#include <QDebug>
#include <QTextEdit>
#include <QTimer>

class InfusionPumpUI : public QWidget {
    QSerialPort serial;
    QComboBox *portSelector;
    QTextEdit *logOutput;
    QLineEdit *sizeInput, *volumeInput, *timeInput;
    QProgressBar *progressBar;
    QTimer *progressTimer;
    int totalTimeSeconds;  // total time in seconds
    int progressStep;      // current progress as a percentage
    int progressIncrement; // progress increment per second

public:
    InfusionPumpUI(QWidget *parent = nullptr) : QWidget(parent), totalTimeSeconds(0), progressStep(0), progressIncrement(0) {
        setWindowTitle("Infusion Pump - Sodium Nitroprusside");
        setFixedSize(800, 400);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        QGridLayout *gridLayout = new QGridLayout();
        QHBoxLayout *bottomLayout = new QHBoxLayout();

        QLabel *title = new QLabel("Syringe Pump");
        title->setStyleSheet("background-color: darkred; color: white; font-size: 20px; padding: 5px; text-align: center;");
        title->setAlignment(Qt::AlignCenter);

        portSelector = new QComboBox();
        QPushButton *refreshPorts = new QPushButton("Refresh Ports");
        QPushButton *connectButton = new QPushButton("Connect");

        refreshAvailablePorts();

        connect(refreshPorts, &QPushButton::clicked, this, &InfusionPumpUI::refreshAvailablePorts);
        connect(connectButton, &QPushButton::clicked, this, &InfusionPumpUI::connectSerialPort);

        gridLayout->addWidget(new QLabel("Select COM Port:"), 0, 0);
        gridLayout->addWidget(portSelector, 0, 1);
        gridLayout->addWidget(refreshPorts, 0, 2);
        gridLayout->addWidget(connectButton, 0, 3);

        sizeInput = new QLineEdit("30");
        volumeInput = new QLineEdit("1");
        timeInput = new QLineEdit("00:30");

        gridLayout->addWidget(new QLabel("Syringe size:"), 1, 0);
        gridLayout->addWidget(sizeInput, 1, 1);
        gridLayout->addWidget(new QLabel("Bolus per ml:"), 2, 0);
        gridLayout->addWidget(volumeInput, 2, 1);
        gridLayout->addWidget(new QLabel("Time:"), 3, 0);
        gridLayout->addWidget(timeInput, 3, 1);

        QLabel *pressureLabel = new QLabel("Pressure:");
        progressBar = new QProgressBar();
        progressBar->setRange(0, 100);  // Set the range from 0 to 100 for the percentage
        progressBar->setValue(0);
        progressBar->setTextVisible(true);
        gridLayout->addWidget(pressureLabel, 6, 0);
        gridLayout->addWidget(progressBar, 6, 1, 1, 2);

        QPushButton *bolusButton = new QPushButton("Bolus");
        bolusButton->setStyleSheet("background-color: blue; color: white; font-size: 16px;");
        QPushButton *stopButton = new QPushButton("Stop");
        stopButton->setStyleSheet("background-color: red; color: white; font-size: 16px;");
        bottomLayout->addWidget(bolusButton);
        bottomLayout->addWidget(stopButton);

        logOutput = new QTextEdit();
        logOutput->setReadOnly(true);
        logOutput->setStyleSheet("background-color: black; color: white; font-size: 14px;");

        mainLayout->addWidget(title);
        mainLayout->addLayout(gridLayout);
        mainLayout->addWidget(logOutput);
        mainLayout->addLayout(bottomLayout);
        setLayout(mainLayout);

        connect(bolusButton, &QPushButton::clicked, this, &InfusionPumpUI::sendBolusData);
        connect(&serial, &QSerialPort::readyRead, this, &InfusionPumpUI::readSerialData);
        connect(stopButton, &QPushButton::clicked, this, &InfusionPumpUI::stopInfusion);

        progressTimer = new QTimer(this);
        connect(progressTimer, &QTimer::timeout, this, &InfusionPumpUI::updateProgress);
    }

    void refreshAvailablePorts() {
        portSelector->clear();
        for (const QSerialPortInfo &port : QSerialPortInfo::availablePorts()) {
            portSelector->addItem(port.portName());
        }
    }

    void connectSerialPort() {
        if (serial.isOpen()) {
            serial.close();
        }
        serial.setPortName(portSelector->currentText());
        serial.setBaudRate(QSerialPort::Baud115200);
        serial.setDataBits(QSerialPort::Data8);
        serial.setParity(QSerialPort::NoParity);
        serial.setStopBits(QSerialPort::OneStop);
        serial.setFlowControl(QSerialPort::NoFlowControl);

        if (serial.open(QIODevice::ReadWrite)) {
            logOutput->append("Connected to " + serial.portName());
        } else {
            logOutput->append("Failed to open serial port!");
        }
    }

    void sendBolusData() {
        if (serial.isOpen()) {
            QString data = "Length:" + sizeInput->text() + "," +
                           "Size:" + volumeInput->text() + "," +
                           "Time:" + timeInput->text() + "\n";
            serial.write(data.toUtf8());
            logOutput->append("Sent: " + data);

            // Parse the time input (in format HH:MM)
            QString timeStr = timeInput->text();
            QStringList timeParts = timeStr.split(":");
            if (timeParts.size() == 2) {
                int minutes = timeParts[0].toInt();
                int seconds = timeParts[1].toInt();
                totalTimeSeconds = minutes * 60 + seconds;

                // Calculate progress increment per second
                progressIncrement = 100 / totalTimeSeconds;  // How much to increase per second

                progressStep = 0;  // Reset progress
                progressBar->setValue(0);
                progressTimer->start(1000); // Timer fires every second
            }
        } else {
            logOutput->append("Serial port not open!");
        }
    }

    void readSerialData() {
        while (serial.canReadLine()) {
            QByteArray data = serial.readLine().trimmed();
            logOutput->append("Received: " + QString(data));
        }
    }

    void stopInfusion() {
        if (serial.isOpen()) {
            serial.write("STOP\n");
            logOutput->append("Sent: STOP");
            progressTimer->stop();
            progressBar->setValue(0);
        } else {
            logOutput->append("Serial port not open!");
        }
    }

    void updateProgress() {
        if (progressStep < 100) {
            progressStep += progressIncrement; // Increase by the calculated increment
            progressBar->setValue(progressStep);
        } else {
            progressStep = 100; // Ensure the progress doesn't exceed 100%
            progressBar->setValue(progressStep);
            progressTimer->stop();
            logOutput->append("Infusion Complete.");
        }
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    InfusionPumpUI window;
    window.show();
    return app.exec();
}
