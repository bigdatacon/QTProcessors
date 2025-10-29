#include "cpu_server.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QMessageBox>
#include <QDateTime>
#include <QRandomGenerator>

CpuServer::CpuServer(QWidget *parent)
    : QMainWindow(parent)
    , udpSocket(nullptr)
    , customPlot(nullptr)
    , updateTimer(nullptr)
    , maxDataPoints(100)
    , timeCounter(0)
    , coreCount(0)
    , lastTotalUsage(0.0)
{
    setupUI();
    setupNetwork();
    setupPlot();

    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &CpuServer::updatePlot);
    updateTimer->start(1000); // 1 Гц
}

CpuServer::~CpuServer()
{
    if (udpSocket) {
        udpSocket->close();
    }
}

void CpuServer::setupUI()
{
    setWindowTitle("CPU Monitor Server");
    setMinimumSize(1000, 600);

    // Центральный виджет
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // Основной layout
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(5, 5, 5, 5); // Уменьшаем отступы
    mainLayout->setSpacing(5); // Уменьшаем расстояние между элементами

    // Заголовок - компактный и по центру
    QLabel *titleLabel = new QLabel("Мониторинг загрузки CPU", this);
    titleLabel->setStyleSheet("font-size: 16pt; font-weight: bold; padding: 2px;");
    titleLabel->setAlignment(Qt::AlignCenter); // Выравнивание по центру
    titleLabel->setMaximumHeight(30); // Ограничиваем высоту заголовка

    mainLayout->addWidget(titleLabel);

    // Статус бар
    statusBar()->showMessage("Ожидание данных от UDP клиента...");
}
void CpuServer::setupPlot()
{
    // Создаем QCustomPlot
    customPlot = new QCustomPlot(this);

    // Делаем график растягивающимся
    customPlot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Настраиваем оси
    customPlot->xAxis->setLabel("Время (секунды)");
    customPlot->yAxis->setLabel("Загрузка CPU (%)");
    customPlot->xAxis->setRange(0, maxDataPoints);
    customPlot->yAxis->setRange(0, 100);

    // Включаем антиалиасинг
    customPlot->setAntialiasedElements(QCP::aeAll);

    // Добавляем в layout - график будет растягиваться
    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(centralWidget()->layout());
    if (layout) {
        layout->addWidget(customPlot, 1); // Второй параметр 1 - коэффициент растяжения
    }
}

void CpuServer::setupNetwork()
{
    udpSocket = new QUdpSocket(this);

    if (!udpSocket->bind(QHostAddress::LocalHost, 1234)) {
        QMessageBox::critical(this, "Ошибка",
                            "Не удалось открыть сокет на порту 1234");
        return;
    }

    connect(udpSocket, &QUdpSocket::readyRead,
            this, &CpuServer::readPendingDatagrams);

    statusBar()->showMessage("Сервер запущен на localhost:1234");
}

void CpuServer::readPendingDatagrams()
{
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(udpSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        udpSocket->readDatagram(datagram.data(), datagram.size(),
                              &sender, &senderPort);

        // Парсим данные
        QString dataStr = QString::fromUtf8(datagram);
        QStringList values = dataStr.split(',');

        if (values.size() > 0) {
            bool ok;
            double totalUsage = values[0].toDouble(&ok);
            if (ok && totalUsage >= 0 && totalUsage <= 100) {
                lastTotalUsage = totalUsage;

                // Сохраняем данные по ядрам
                coreUsageData.clear();
                for (int i = 1; i < values.size(); i++) {
                    double coreUsage = values[i].toDouble(&ok);
                    if (ok && coreUsage >= 0 && coreUsage <= 100) {
                        coreUsageData.append(coreUsage);
                    }
                }

                // Определяем количество ядер
                int newCoreCount = coreUsageData.size();
                if (newCoreCount != coreCount) {
                    coreCount = newCoreCount;

                    // Пересоздаем графики при изменении числа ядер
                    customPlot->clearGraphs();
                    graphs.clear();
                    dataBuffers.clear();

                    // Создаем график для общей загрузки
                    QCPGraph *totalGraph = customPlot->addGraph();
                    totalGraph->setName("Общая загрузка");
                    totalGraph->setPen(QPen(Qt::red, 2));
                    graphs.append(totalGraph);
                    dataBuffers.append(QVector<double>());

                    // Создаем графики для ядер
                    QColor coreColors[] = {Qt::blue, Qt::green, Qt::yellow, Qt::cyan,
                                          Qt::magenta, Qt::darkCyan, Qt::darkRed, Qt::darkGreen};

                    for (int i = 0; i < coreCount; i++) {
                        QCPGraph *coreGraph = customPlot->addGraph();
                        coreGraph->setName(QString("CPU%1").arg(i));
                        coreGraph->setPen(QPen(coreColors[i % 8], 1.5));
                        graphs.append(coreGraph);
                        dataBuffers.append(QVector<double>());
                    }

                    // Настраиваем легенду
                    customPlot->legend->setVisible(true);
                    customPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop|Qt::AlignRight);
                }

                statusBar()->showMessage(
                    QString("Общая: %1% | Ядер: %2 | %3")
                    .arg(totalUsage, 0, 'f', 1)
                    .arg(coreCount)
                    .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                );
            }
        }
    }
}

void CpuServer::updatePlot()
{
    if (lastTotalUsage < 0) return;

    // Увеличиваем счетчик времени
    timeCounter++;

    // Обновляем диапазон оси X
    customPlot->xAxis->setRange(qMax(0, timeCounter - maxDataPoints), timeCounter);

    // Обновляем данные для КАЖДОГО графика
    for (int i = 0; i < graphs.size(); i++) {
        if (i >= dataBuffers.size()) continue;

        QCPGraph *graph = graphs[i];
        QVector<double> &buffer = dataBuffers[i];

        // Очищаем старые данные
        if (buffer.size() > maxDataPoints) {
            buffer.removeFirst();
        }

        // Добавляем новую точку с реальными данными
        double value;
        if (i == 0) {
            value = lastTotalUsage; // Общая загрузка
        } else if (i - 1 < coreUsageData.size()) {
            value = coreUsageData[i - 1]; // Данные по ядрам
        } else {
            value = 0; // Запасной вариант
        }

        buffer.append(value);

        // Создаем данные для графика
        QVector<double> xData, yData;
        for (int j = 0; j < buffer.size(); j++) {
            xData.append(timeCounter - buffer.size() + j + 1);
            yData.append(buffer[j]);
        }

        graph->setData(xData, yData);
    }

    // Перерисовываем график
    customPlot->replot();
}
