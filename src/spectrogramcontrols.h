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

#pragma once

#include <QDockWidget>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include "tuner.h"

class SpectrogramControls : public QDockWidget
{
    Q_OBJECT

public:
    SpectrogramControls(const QString & title, QWidget * parent);
    void setDefaults();

signals:
    void fftOrZoomChanged(int fftSize, int zoomLevel);
    void openFile(QString fileName);

public slots:
    void timeSelectionChanged(float time);
    void zoomIn();
    void zoomOut();
    void tunerMoved(int deviation);
    void enableAnnotations(bool enabled);
    void coordinateClick(double time_pos, double freq_pos, bool down);


private slots:
    void fftSizeChanged(int value);
    void zoomLevelChanged(int value);
    void powerMinChanged(int value);
    void powerMaxChanged(int value);
    void squelchChanged(int value);
    void fileOpenButtonClicked();
    void cursorsStateChanged(int state);

private:
    QWidget *widget;
    QFormLayout *layout;
    void clearCursorLabels();
    void fftOrZoomChanged(void);
    int getBandwidth(int deviation);

public:
    QPushButton *fileOpenButton;
    QLineEdit *sampleRate;
    QLineEdit *centerFrequency;
    QSlider *fftSizeSlider;
    QSlider *zoomLevelSlider;
    QSlider *powerMaxSlider;
    QSlider *powerMinSlider;
    QSlider *squelchSlider;
    QCheckBox *cursorsCheckBox;
    QSpinBox *cursorSymbolsSpinBox;
    QCheckBox *cursorsFreezeCheckBox;
    QLabel *rateLabel;
    QLabel *periodLabel;
    QLabel *symbolRateLabel;
    QLabel *symbolPeriodLabel;
    QLabel *bandwidthLabel;

    QLabel *startTimeLabel;
    double startTime;
    double endTime;
    QLabel *endTimeLabel;
    QLabel *deltaTimeLabel;


    QCheckBox *scalesCheckBox;
    QCheckBox *annosCheckBox;
    QCheckBox *commentsCheckBox;
};
