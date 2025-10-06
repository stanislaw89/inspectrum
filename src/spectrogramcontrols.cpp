/*
 *  Copyright (C) 2015, Mike Walters <mike@flomp.net>
 *  Copyright (C) 2015, Jared Boone <jared@sharebrained.com>
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

#include "spectrogramcontrols.h"
#include <QIntValidator>
#include <QFileDialog>
#include <QSettings>
#include <QLabel>
#include <cmath>
#include "util.h"

SpectrogramControls::SpectrogramControls(const QString & title, QWidget * parent)
    : QDockWidget::QDockWidget(title, parent)
{
    widget = new QWidget(this);
    layout = new QFormLayout(widget);

    fileOpenButton = new QPushButton("Open file...", widget);
    layout->addRow(fileOpenButton);

    sampleRate = new QLineEdit();
    auto double_validator = new QDoubleValidator(this);
    double_validator->setBottom(0.0);
    sampleRate->setValidator(double_validator);
    layout->addRow(new QLabel(tr("Sample rate:")), sampleRate);




    centerFrequency = new QLineEdit();
    auto freq_double_validator = new QDoubleValidator(this);
    freq_double_validator->setBottom(0.0);
    centerFrequency->setValidator(freq_double_validator);
    layout->addRow(new QLabel(tr("Center frequency:")), centerFrequency);

    // Spectrogram settings
    layout->addRow(new QLabel()); // TODO: find a better way to add an empty row?
    layout->addRow(new QLabel(tr("<b>Spectrogram</b>")));

    fftSizeSlider = new QSlider(Qt::Horizontal, widget);
    fftSizeSlider->setRange(4, 13);
    fftSizeSlider->setPageStep(1);

    layout->addRow(new QLabel(tr("FFT size:")), fftSizeSlider);

    zoomLevelSlider = new QSlider(Qt::Horizontal, widget);
    zoomLevelSlider->setRange(0, 10);
    zoomLevelSlider->setPageStep(1);

    layout->addRow(new QLabel(tr("Zoom:")), zoomLevelSlider);

    powerMaxSlider = new QSlider(Qt::Horizontal, widget);
    powerMaxSlider->setRange(-140, 10);
    layout->addRow(new QLabel(tr("Power max:")), powerMaxSlider);

    powerMinSlider = new QSlider(Qt::Horizontal, widget);
    powerMinSlider->setRange(-140, 10);
    layout->addRow(new QLabel(tr("Power min:")), powerMinSlider);

    squelchSlider = new QSlider(Qt::Horizontal, widget);
    squelchSlider->setRange(0, 21);
    layout->addRow(new QLabel(tr("Squelch:")), squelchSlider);



    scalesCheckBox = new QCheckBox(widget);
    scalesCheckBox->setCheckState(Qt::Checked);
    layout->addRow(new QLabel(tr("Scales:")), scalesCheckBox);

    // Time selection settings
    layout->addRow(new QLabel()); // TODO: find a better way to add an empty row?
    layout->addRow(new QLabel(tr("<b>Time selection</b>")));

    cursorsCheckBox = new QCheckBox(widget);
    layout->addRow(new QLabel(tr("Enable cursors:")), cursorsCheckBox);
    cursorsFreezeCheckBox = new QCheckBox(widget);
    layout->addRow(new QLabel(tr("Freeze cursors:")), cursorsFreezeCheckBox);

    cursorSymbolsSpinBox = new QSpinBox();
    cursorSymbolsSpinBox->setMinimum(1);
    cursorSymbolsSpinBox->setMaximum(99999);
    layout->addRow(new QLabel(tr("Symbols:")), cursorSymbolsSpinBox);

    rateLabel = new QLabel();
    layout->addRow(new QLabel(tr("Rate:")), rateLabel);

    periodLabel = new QLabel();
    layout->addRow(new QLabel(tr("Period:")), periodLabel);

    symbolRateLabel = new QLabel();
    layout->addRow(new QLabel(tr("Symbol rate:")), symbolRateLabel);

    symbolPeriodLabel = new QLabel();
    layout->addRow(new QLabel(tr("Symbol period:")), symbolPeriodLabel);

    bandwidthLabel = new QLabel();
    layout->addRow(new QLabel(tr("Bandwidth:")), bandwidthLabel);

    // SigMF selection settings
    layout->addRow(new QLabel()); // TODO: find a better way to add an empty row?
    layout->addRow(new QLabel(tr("<b>SigMF Control</b>")));

    annosCheckBox = new QCheckBox(widget);
    layout->addRow(new QLabel(tr("Display Annotations:")), annosCheckBox);
    commentsCheckBox = new QCheckBox(widget);
    layout->addRow(new QLabel(tr("Display annotation comments tooltips:")), commentsCheckBox);



    // SigMF selection settings
    layout->addRow(new QLabel()); // TODO: find a better way to add an empty row?
    layout->addRow(new QLabel(tr("<b>Time (ctrl-click, drag)</b>")));

    startTimeLabel = new QLabel();
    layout->addRow(new QLabel(tr("Start:")), startTimeLabel);
    endTimeLabel = new QLabel();
    layout->addRow(new QLabel(tr("End:")), endTimeLabel);
    deltaTimeLabel = new QLabel();
    layout->addRow(new QLabel(tr("Delta:")), deltaTimeLabel);







    widget->setLayout(layout);
    setWidget(widget);

    connect(fftSizeSlider, &QSlider::valueChanged, this, &SpectrogramControls::fftSizeChanged);
    connect(zoomLevelSlider, &QSlider::valueChanged, this, &SpectrogramControls::zoomLevelChanged);
    connect(fileOpenButton, &QPushButton::clicked, this, &SpectrogramControls::fileOpenButtonClicked);
    connect(cursorsCheckBox, &QCheckBox::stateChanged, this, &SpectrogramControls::cursorsStateChanged);
    connect(powerMinSlider, &QSlider::valueChanged, this, &SpectrogramControls::powerMinChanged);
    connect(powerMaxSlider, &QSlider::valueChanged, this, &SpectrogramControls::powerMaxChanged);
    connect(squelchSlider, &QSlider::valueChanged, this, &SpectrogramControls::squelchChanged);
}

void SpectrogramControls::clearCursorLabels()
{
    periodLabel->setText("");
    rateLabel->setText("");
    symbolPeriodLabel->setText("");
    symbolRateLabel->setText("");
    bandwidthLabel->setText("");
}

void SpectrogramControls::cursorsStateChanged(int state)
{
    if (state == Qt::Unchecked) {
        clearCursorLabels();
    }
}

void SpectrogramControls::setDefaults()
{
    fftOrZoomChanged();

    cursorsCheckBox->setCheckState(Qt::Unchecked);
    cursorSymbolsSpinBox->setValue(1);

    annosCheckBox->setCheckState(Qt::Checked);
    commentsCheckBox->setCheckState(Qt::Checked);

    // Try to set the sample rate from the last-used value
    QSettings settings;
    int savedSampleRate = settings.value("SampleRate", 8000000).toInt();
    sampleRate->setText(QString::number(savedSampleRate));
    fftSizeSlider->setValue(settings.value("FFTSize", 9).toInt());
    powerMaxSlider->setValue(settings.value("PowerMax", 0).toInt());
    powerMinSlider->setValue(settings.value("PowerMin", -100).toInt());
    squelchSlider->setValue(settings.value("Squelch", 0).toInt());
    zoomLevelSlider->setValue(settings.value("ZoomLevel", 0).toInt());

    int savedFreq = settings.value("CenterFrequency", 0).toInt();
    centerFrequency->setText(QString::number(savedFreq));
}

void SpectrogramControls::fftOrZoomChanged(void)
{
    int fftSize = pow(2, fftSizeSlider->value());
    int zoomLevel = std::min(fftSize, (int)pow(2, zoomLevelSlider->value()));
    emit fftOrZoomChanged(fftSize, zoomLevel);
}

void SpectrogramControls::fftSizeChanged(int value)
{
    QSettings settings;
    settings.setValue("FFTSize", value);
    fftOrZoomChanged();
}

void SpectrogramControls::zoomLevelChanged(int value)
{
    QSettings settings;
    settings.setValue("ZoomLevel", value);
    fftOrZoomChanged();
}

void SpectrogramControls::powerMinChanged(int value)
{
    QSettings settings;
    settings.setValue("PowerMin", value);
}

void SpectrogramControls::squelchChanged(int value)
{
    QSettings settings;
    settings.setValue("Squelch", value);
}


void SpectrogramControls::powerMaxChanged(int value)
{
    QSettings settings;
    settings.setValue("PowerMax", value);
}

void SpectrogramControls::fileOpenButtonClicked()
{
    QSettings settings;
    QString fileName;
    QFileDialog fileSelect(this);
    fileSelect.setNameFilter(tr("All files (*);;"
                "complex<float> file (*.cfile *.cf32 *.fc32);;"
                "complex<int8> HackRF file (*.cs8 *.sc8 *.c8);;"
                "complex<int16> Fancy file (*.cs16 *.sc16 *.c16);;"
                "complex<uint8> RTL-SDR file (*.cu8 *.uc8)"));

    // Try and load a saved state
    {
        QByteArray savedState = settings.value("OpenFileState").toByteArray();
        fileSelect.restoreState(savedState);

        // Filter doesn't seem to be considered part of the saved state
        QString lastUsedFilter = settings.value("OpenFileFilter").toString();
        if(lastUsedFilter.size())
            fileSelect.selectNameFilter(lastUsedFilter);
    }

    if(fileSelect.exec())
    {
        fileName = fileSelect.selectedFiles()[0];

        // Remember the state of the dialog for the next time
        QByteArray dialogState = fileSelect.saveState();
        settings.setValue("OpenFileState", dialogState);
        settings.setValue("OpenFileFilter", fileSelect.selectedNameFilter());
    }

    if (!fileName.isEmpty())
        emit openFile(fileName);
}

void SpectrogramControls::timeSelectionChanged(float time)
{
    if (cursorsCheckBox->checkState() == Qt::Checked) {
        periodLabel->setText(QString::fromStdString(formatSIValue(time)) + "s");
        rateLabel->setText(QString::fromStdString(formatSIValue(1 / time)) + "Hz");

        int symbols = cursorSymbolsSpinBox->value();
        symbolPeriodLabel->setText(QString::fromStdString(formatSIValue(time / symbols)) + "s");
        symbolRateLabel->setText(QString::fromStdString(formatSIValue(symbols / time)) + "Bd");
    }
}

void SpectrogramControls::coordinateClick(double time_pos, double freq_pos, bool down) {

	if (down) {
		endTimeLabel->setText("");
		deltaTimeLabel->setText("");
		startTimeLabel->setText(QString::fromStdString(formatSIValue(time_pos)) + "s");
		startTime = time_pos;

	} else {
		endTime = time_pos;
		if (endTime == startTime) {
			deltaTimeLabel->setText("");
			endTimeLabel->setText("");
		} else {
			if (endTime < startTime) {
				deltaTimeLabel->setText(QString("-") + QString::fromStdString(formatSIValue(startTime - endTime)) + "s");
			} else {
				deltaTimeLabel->setText(QString::fromStdString(formatSIValue(endTime - startTime)) + "s");
			}
			endTimeLabel->setText(QString::fromStdString(formatSIValue(time_pos)) + "s");
		}
	}
}


void SpectrogramControls::zoomIn()
{
    zoomLevelSlider->setValue(zoomLevelSlider->value() + 1);
}

void SpectrogramControls::zoomOut()
{
    zoomLevelSlider->setValue(zoomLevelSlider->value() - 1);
}

void SpectrogramControls::tunerMoved(int deviation)
{
    // int deviation is width in pixels from plot
    bandwidthLabel->setText(QString::number(getBandwidth(deviation)) + "kHz");
}

int SpectrogramControls::getBandwidth(int deviation) {
    double rate = sampleRate->text().toDouble();
    double fftSize = pow(2, fftSizeSlider->value());
    double hzPerPx = rate / fftSize;
    return deviation * hzPerPx / 1000 * 2;
}
void SpectrogramControls::enableAnnotations(bool enabled) {
    // disable annotation comments checkbox when annotations are disabled
    commentsCheckBox->setEnabled(enabled);
}
