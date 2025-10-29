#ifndef CPU_SERVER_H
#define CPU_SERVER_H

#include <QMainWindow>
#include <QUdpSocket>
#include <QTimer>
#include <QVector>
#include "qcustomplot.h"

class CpuServer : public QMainWindow
{
    Q_OBJECT

public:
    explicit CpuServer(QWidget *parent = nullptr);
    ~CpuServer();

private slots:
    void readPendingDatagrams();
    void updatePlot();

private:
    void setupUI();
    void setupNetwork();
    void setupPlot();

    QUdpSocket *udpSocket;
    QCustomPlot *customPlot;
    QTimer *updateTimer;

    QVector<QCPGraph*> graphs;
    QVector<QVector<double>> dataBuffers;
    int maxDataPoints;
    int timeCounter;

    int coreCount;
    double lastTotalUsage;
    QVector<double> coreUsageData; // Для хранения данных по ядрам
};

#endif // CPU_SERVER_H
