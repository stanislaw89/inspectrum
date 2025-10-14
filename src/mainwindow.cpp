/*
 *  Copyright (C) 2015, Mike Walters <mike@flomp.net>
 *
 *  This file is part of inspectrum.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QMessageBox>
#include <QtWidgets>
#include <QPixmapCache>
#include <QRubberBand>
#include <sstream>

#include "mainwindow.h"
#include "util.h"

MainWindow::MainWindow()
{
    setWindowTitle(tr("inspectrum"));

    QPixmapCache::setCacheLimit(40960);

    dock = new SpectrogramControls(tr("Controls"), this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    input = new InputSource();
    input->subscribe(this);

    QSettings settings;
    tuner = new Tuner(settings.value("FFTSize", 9).toInt(), this);

    plots = new PlotView(input, tuner);
    setCentralWidget(plots);

    // Connect dock inputs
    connect(dock, &SpectrogramControls::openFile, this, &MainWindow::openFile);

    connect(dock->sampleRate, static_cast<void (QLineEdit::*)(const QString&)>(&QLineEdit::textChanged), this, static_cast<void (MainWindow::*)(QString)>(&MainWindow::setSampleRate));
    connect(dock->centerFrequency, static_cast<void (QLineEdit::*)(const QString&)>(&QLineEdit::textChanged), this, static_cast<void (MainWindow::*)(QString)>(&MainWindow::setCenterFrequency));
    connect(dock, static_cast<void (SpectrogramControls::*)(int, int)>(&SpectrogramControls::fftOrZoomChanged), plots, &PlotView::setFFTAndZoom);
    connect(dock->powerMaxSlider, &QSlider::valueChanged, plots, &PlotView::setPowerMax);
    connect(dock->powerMinSlider, &QSlider::valueChanged, plots, &PlotView::setPowerMin);
    connect(dock->squelchSlider, &QSlider::valueChanged, plots, &PlotView::setSquelch);
    connect(dock->cursorsCheckBox, &QCheckBox::stateChanged, plots, &PlotView::enableCursors);
    connect(dock->cursorsFreezeCheckBox, &QCheckBox::stateChanged, plots, &PlotView::freezeCursors);
    connect(dock->scalesCheckBox, &QCheckBox::stateChanged, plots, &PlotView::enableScales);
    connect(dock->annosCheckBox, &QCheckBox::stateChanged, plots, &PlotView::enableAnnotations);
    connect(dock->annosCheckBox, &QCheckBox::stateChanged, dock, &SpectrogramControls::enableAnnotations);
    connect(dock->commentsCheckBox, &QCheckBox::stateChanged, plots, &PlotView::enableAnnotationCommentsTooltips);
    connect(dock->cursorSymbolsSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), plots, &PlotView::setCursorSegments);
    connect(tuner, &Tuner::tunerMoved, dock, &SpectrogramControls::tunerMoved);

    // Connect dock outputs
    connect(plots, &PlotView::timeSelectionChanged, dock, &SpectrogramControls::timeSelectionChanged);
    connect(plots, &PlotView::zoomIn, dock, &SpectrogramControls::zoomIn);
    connect(plots, &PlotView::zoomOut, dock, &SpectrogramControls::zoomOut);
    connect(plots, &PlotView::coordinateClick, dock, &SpectrogramControls::coordinateClick);

    void coordinateClick(double time_position, double frequency);

    // Set defaults after making connections so everything is in sync
    dock->setDefaults();

}

void MainWindow::openFile(QString fileName)
{
    QString title="%1: %2";
    this->setWindowTitle(title.arg(QApplication::applicationName(),fileName.section('/',-1,-1)));

    // Try to parse osmocom_fft filenames and extract the sample rate and center frequency.
    // Example file name: "name-f2.411200e+09-s5.000000e+06-t20160807180210.cfile"
    QRegExp rx("(.*)-f(.*)-s(.*)-.*\\.cfile");
    QString basename = fileName.section('/',-1,-1);

    if (rx.exactMatch(basename)) {
        QString centerfreq = rx.cap(2);
        QString samplerate = rx.cap(3);

        std::stringstream ss(samplerate.toUtf8().constData());

        // Needs to be a double as the number is in scientific format
        double rate;
        ss >> rate;
        if (!ss.fail()) {
            setSampleRate(rate);
        }


        std::stringstream ssfreq(centerfreq.toUtf8().constData());
        double freq;
        ssfreq >> freq;
        if (!ssfreq.fail()) {
        	setCenterFrequency(freq);
        }
    }

    try
    {
        input->openFile(fileName.toUtf8().constData());
        if (input->rate() > 0) {
            setSampleRate(input->rate());
        }
        if (input->centerFrequency() > 0) {
            setCenterFrequency(input->centerFrequency());
        }
    }
    catch (const std::exception &ex)
    {
        QMessageBox msgBox(QMessageBox::Critical, "Inspectrum openFile error", QString("%1: %2").arg(fileName).arg(ex.what()));
        msgBox.exec();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
		case Qt::Key_C:
			dock->cursorsCheckBox->setChecked(! plots->cursorsAreEnabled());
			break;
		case Qt::Key_W:
			if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
				QApplication::closeAllWindows();
			}
			break;

		default:
			QMainWindow::keyPressEvent(event); // Pass to base class
    }
}

void MainWindow::invalidateEvent()
{
    plots->setSampleRate(input->rate());
    plots->setCenterFrequency(input->centerFrequency());

    // Only update the text box if it is not already representing
    // the current value. Otherwise the cursor might jump or the
    // representation might change (e.g. to scientific).
    double currentValue = dock->sampleRate->text().toDouble();
    if(QString::number(input->rate()) != QString::number(currentValue)) {
        setSampleRate(input->rate());
    }
}

void MainWindow::setSampleRate(QString rate)
{
    auto sampleRate = rate.toDouble();
    input->setSampleRate(sampleRate);
    plots->setSampleRate(sampleRate);

    // Save the sample rate in settings as we're likely to be opening the same file across multiple runs
    QSettings settings;
    settings.setValue("SampleRate", sampleRate);
}

void MainWindow::setSampleRate(double rate)
{
    dock->sampleRate->setText(QString::number(rate, 'f', 0));
}

void MainWindow::setCenterFrequency(QString freq)
{
    auto centerFreq = freq.toDouble();
    input->setCenterFrequency(centerFreq);
    plots->setCenterFrequency(centerFreq);

    // Save the sample rate in settings as we're likely to be opening the same file across multiple runs
    QSettings settings;
    settings.setValue("CenterFrequency", centerFreq);
}

void MainWindow::setCenterFrequency(double freq)
{
    dock->centerFrequency->setText(QString::number(freq, 'f', 0));
}

void MainWindow::setFormat(QString fmt)
{
    input->setFormat(fmt.toUtf8().constData());
}
