/*
 *  Copyright (C) 2015-2016, Mike Walters <mike@flomp.net>
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

#include "plotview.h"
#include <iostream>
#include <fstream>
#include <QtGlobal>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QMenu>
#include <QPainter>
#include <QProgressDialog>
#include <QRadioButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QToolTip>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QProcess>
#include <QMessageBox>
#include <QSettings>

#include "plots.h"
#include "symbolprogoutput.h"



PlotView::PlotView(InputSource *input, Tuner *tuner) : cursors(this), viewRange({0, 0}), selectedSamples({0,0})
{
    mainSampleSource = input;
    setDragMode(QGraphicsView::ScrollHandDrag);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setMouseTracking(true);
    enableCursors(false);
    freezeCursors(false);
    connect(&cursors, &Cursors::cursorsMoved, this, &PlotView::cursorsMoved);

    spectrogramPlot = new SpectrogramPlot(std::shared_ptr<SampleSource<std::complex<float>>>(mainSampleSource), tuner);
    auto tunerOutput = std::dynamic_pointer_cast<SampleSource<std::complex<float>>>(spectrogramPlot->output());

    connect(tuner, &Tuner::tunerMoved, spectrogramPlot, &SpectrogramPlot::tunerMoved);

    enableScales(true);

    enableAnnotations(true);
    enableAnnotationCommentsTooltips(true);

    addPlot(spectrogramPlot);

    mainSampleSource->subscribe(this);
}

void PlotView::addPlot(Plot *plot)
{
    plots.emplace_back(plot);
    connect(plot, &Plot::repaint, this, &PlotView::repaint);
}

void PlotView::mouseMoveEvent(QMouseEvent *event)
{
    updateAnnotationTooltip(event);
    QGraphicsView::mouseMoveEvent(event);
}

void PlotView::mouseReleaseEvent(QMouseEvent *event)
{
    // This is used to show the tooltip again on drag release if the mouse is
    // hovering over an annotation.
    updateAnnotationTooltip(event);
    QGraphicsView::mouseReleaseEvent(event);
}

void PlotView::updateAnnotationTooltip(QMouseEvent *event)
{
    // If there are any mouse buttons pressed, we assume
    // that the plot is being dragged and hide the tooltip.
    bool isDrag = event->buttons() != Qt::NoButton;
    if (!annotationCommentsEnabled
        || !spectrogramPlot->isAnnotationsEnabled()
        || isDrag)  {
        QToolTip::hideText();
    } else {
        QString* comment = spectrogramPlot->mouseAnnotationComment(event);
        if (comment != nullptr) {
            QToolTip::showText(event->globalPos(), *comment);
        } else {
            QToolTip::hideText();
        }
    }
}


void PlotView::contextMenuEvent(QContextMenuEvent * event)
{
    QMenu menu;

    // Get selected plot
    Plot *selectedPlot = nullptr;
    auto it = plots.begin();
    int y = -verticalScrollBar()->value();
    for (; it != plots.end(); it++) {
        auto&& plot = *it;
        if (range_t<int>{y, y + plot->height()}.contains(event->pos().y())) {
            selectedPlot = plot.get();
            break;
        }
        y += plot->height();
    }
    if (selectedPlot == nullptr)
        return;

    // Add actions to add derived plots
    // that are compatible with selectedPlot's output
    QMenu *plotsMenu = menu.addMenu("Add derived plot");
    auto src = selectedPlot->output();
    last_src_used = src;
    auto compatiblePlots = as_range(Plots::plots.equal_range(src->sampleType()));
    for (auto p : compatiblePlots) {
        auto plotInfo = p.second;
        auto action = new QAction(QString("Add %1").arg(plotInfo.name), plotsMenu);
        auto plotCreator = plotInfo.creator;
        connect(
            action, &QAction::triggered,
            this, [=]() {
                addPlot(plotCreator(src));
            }
        );
        plotsMenu->addAction(action);
    }

    // Add submenu for extracting symbols
    QMenu *extractMenu = menu.addMenu("Extract symbols");
    // Add action to extract symbols from selected plot to stdout
    auto extract = new QAction("To stdout", extractMenu);
    connect(
        extract, &QAction::triggered,
        this, [=]() {
            extractSymbols(src, false);
        }
    );
    extract->setEnabled(cursorsEnabled && (src->sampleType() == typeid(float)));
    extractMenu->addAction(extract);



    // Add action to run external program
    auto runProgramAction = new QAction("Feed to External...", extractMenu);
    connect(
    		runProgramAction, &QAction::triggered,
			this, [=]() {
    			selectAndFeedExternalProgram(src);
    		}
    );

    runProgramAction->setEnabled(cursorsEnabled && (src->sampleType() == typeid(float)));
    extractMenu->addAction(runProgramAction);






    // Add action to extract symbols from selected plot to clipboard
    auto extractClipboard = new QAction("Copy to clipboard", extractMenu);
    connect(
        extractClipboard, &QAction::triggered,
        this, [=]() {
            extractSymbols(src, true);
        }
    );
    extractClipboard->setEnabled(cursorsEnabled && (src->sampleType() == typeid(float)));
    extractMenu->addAction(extractClipboard);

    // Add action to export the selected samples into a file
    auto save = new QAction("Export samples to file...", &menu);
    connect(
        save, &QAction::triggered,
        this, [=]() {
            if (selectedPlot == spectrogramPlot) {
                exportSamples(spectrogramPlot->tunerEnabled() ? spectrogramPlot->output() : spectrogramPlot->input());
            } else {
                exportSamples(src);
            }
        }
    );
    menu.addAction(save);

    // Add action to remove the selected plot
    auto rem = new QAction("Remove plot", &menu);
    connect(
        rem, &QAction::triggered,
        this, [=]() {
    	    last_src_used = nullptr;
            plots.erase(it);
        }
    );
    // Don't allow remove the first plot (the spectrogram)
    rem->setEnabled(it != plots.begin());
    menu.addAction(rem);

    updateViewRange(false);
    if(menu.exec(event->globalPos()))
        updateView(false);

}

void PlotView::cursorsMoved()
{
    selectedSamples = {
        columnToSample(horizontalScrollBar()->value() + cursors.selection().minimum),
        columnToSample(horizontalScrollBar()->value() + cursors.selection().maximum)
    };

    samplesPerSegment = selectedSamples.length() / cursors.segments();

    emitTimeSelection();
    viewport()->update();
}

void PlotView::emitTimeSelection()
{
    size_t sampleCount = selectedSamples.length();
    float selectionTime = sampleCount / (float)mainSampleSource->rate();
    emit timeSelectionChanged(selectionTime);
}

void PlotView::freezeCursors(bool enabled) {

    cursorsFrozen.enabled = enabled;
	if (cursorsEnabled) {
		if (enabled) {
			cursorsFrozen.range = cursors.selection();
		}

		cursors.frozen(enabled);
	}
}
void PlotView::enableCursors(bool enabled)
{
    cursorsEnabled = enabled;
    if (enabled) {
        int margin = viewport()->rect().width() / 3;


        // Update cursors
        int startColumn = viewport()->rect().left() + margin;
        if (selectedSamples.minimum != selectedSamples.maximum) {

        	int colWidth = sampleToColumn(selectedSamples.maximum) - sampleToColumn(selectedSamples.minimum);

        	range_t<int> newSelection = {
        			startColumn,
					startColumn + colWidth
        	        };
        	cursors.setSelection(newSelection);
        } else {
        	cursors.setSelection({startColumn, viewport()->rect().right() - margin});
        }

        //
        cursorsMoved();
    }
    viewport()->update();
}
void PlotView::keyPressEvent(QKeyEvent *event) {

    QSettings settings;

	bool shiftMod = QApplication::keyboardModifiers() & Qt::ShiftModifier;
	bool ctrlMod = QApplication::keyboardModifiers() & Qt::ControlModifier;
	QScrollBar *scrollBar = horizontalScrollBar();
	std::shared_ptr<AbstractSampleSource> plot_src;

    switch (event->key()) {
		case Qt::Key_Left:
			if (ctrlMod) {
				scrollBar->setValue(scrollBar->value() + scrollBar->pageStep() * -1);
			} else {
				scrollBar->setValue(scrollBar->value() + scrollBar->singleStep() * -1);
			}

			break;
		case Qt::Key_Right:
			if (ctrlMod) {
				scrollBar->setValue(scrollBar->value() + scrollBar->pageStep());
			} else {
				scrollBar->setValue(scrollBar->value() + scrollBar->singleStep());
			}
			break;

		case Qt::Key_Up:
			if (! (cursorsEnabled || ctrlMod) ) {
				QGraphicsView::keyPressEvent(event); // Pass to base class
				break;
			}
			if (shiftMod)
				cursors.setSelection({cursors.selection().minimum,cursors.selection().maximum+10});
			else
				cursors.setSelection({cursors.selection().minimum,cursors.selection().maximum+1});

	        cursorsMoved();
			break;
		case Qt::Key_Down:
			if (! (cursorsEnabled || ctrlMod) ) {
				QGraphicsView::keyPressEvent(event); // Pass to base class
				break;
			}
			if (shiftMod) {
				if (cursors.selection().maximum - cursors.selection().minimum > 10)
					cursors.setSelection({cursors.selection().minimum,cursors.selection().maximum-10});
			} else {
				if (cursors.selection().maximum - cursors.selection().minimum > 2)
					cursors.setSelection({cursors.selection().minimum,cursors.selection().maximum-1});
			}


	        cursorsMoved();
			break;
		case Qt::Key_F:
			if (spectrogramPlot->tunerEnabled()) {
				break;
			}
			plot_src = spectrogramPlot->output();
			addPlot(Plots::frequencyPlot(plot_src));
			repaint();
			break;
		case Qt::Key_R:
			if (last_src_used && cursorsEnabled) {

			    QString lastProgramPath = settings.value("lastProgramPath", QString("")).toString();
			    if (lastProgramPath != "") {
			    	feedSymbolsToExternalProgram(lastProgramPath, last_src_used);
			    }
			}
			break;


    default:
        QGraphicsView::keyPressEvent(event); // Pass to base class
    }
}
bool PlotView::viewportEvent(QEvent *event) {
    // Handle wheel events for zooming (before the parent's handler to stop normal scrolling)

    if (event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = (QWheelEvent*)event;
        int delta = wheelEvent->angleDelta().y();

        if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
    		QScrollBar *scrollBar = horizontalScrollBar();
        	scrollBar->setValue(scrollBar->value() + scrollBar->pageStep() * -1 * delta);

        	return true;

        } else if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
            bool canZoomIn = zoomLevel < fftSize;
            bool canZoomOut = zoomLevel > 1;
            if ((delta > 0 && canZoomIn) || (delta < 0 && canZoomOut)) {
                scrollZoomStepsAccumulated += delta;

                // `updateViewRange()` keeps the center sample in the same place after zoom. Apply
                // a scroll adjustment to keep the sample under the mouse cursor in the same place instead.
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                zoomPos = wheelEvent->position().x();
#else
                zoomPos = wheelEvent->pos().x();
#endif
                zoomSample = columnToSample(horizontalScrollBar()->value() + zoomPos);
                if (scrollZoomStepsAccumulated >= 120) {
                    scrollZoomStepsAccumulated -= 120;
                    emit zoomIn();
                } else if (scrollZoomStepsAccumulated <= -120) {
                    scrollZoomStepsAccumulated += 120;
                    emit zoomOut();
                }
            }
            return true;
        } else {
        	QScrollBar *scrollBar = horizontalScrollBar();
        	scrollBar->setValue(scrollBar->value() - delta);
        }
    }

    // Pass mouse events to individual plot objects
    if (event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseMove ||
        event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::Leave) {

        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);


        int plotY = -verticalScrollBar()->value();

        if ( (QApplication::keyboardModifiers() & Qt::ControlModifier) &&
        		(event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease)) {
			double clickTime = (columnToSample(mouseEvent->pos().x()) + viewRange.minimum) /  sampleRate;

			// don't know how to translate mouse coords to frequency: TODO:FIXME -- setting at 0 for now
			if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
				emit coordinateClick(clickTime, 0, false);
			} else {
				emit coordinateClick(clickTime, 0, event->type() == QEvent::MouseButtonPress);
			}
			return true;
        }


        for (auto&& plot : plots) {
            bool result = plot->mouseEvent(
                event->type(),
                QMouseEvent(
                    event->type(),
                    QPoint(mouseEvent->pos().x(), mouseEvent->pos().y() - plotY),
                    mouseEvent->button(),
                    mouseEvent->buttons(),
                    QApplication::keyboardModifiers()
                )
            );
            if (result)
                return true;
            plotY += plot->height();
        }

        if (cursorsEnabled)
            if (cursors.mouseEvent(event->type(), *mouseEvent))
                return true;
    }

    // Handle parent eveents
    return QGraphicsView::viewportEvent(event);
}

QByteArray vectorToTextByteArray(const std::vector<float>& data)
{
    QString text;
    for (float value : data) {
        text += QString::number(value, 'f', 6) + ","; // One float per line, 6 decimal places
    }
    return text.toUtf8(); // Convert to QByteArray with UTF-8 encoding
}

void PlotView::feedSymbolsToExternalProgram(QString programPath, std::shared_ptr<AbstractSampleSource> src) {

    if (!cursorsEnabled)
        return;

    auto floatSrc = std::dynamic_pointer_cast<SampleSource<float>>(src);
    if (!floatSrc)
        return;

    QProcess process(this);

    // Configure the process to allow writing to stdin
    process.setProcessChannelMode(QProcess::MergedChannels); // Merge stdout and stderr for simplicity
    process.start(programPath, QStringList()); // No arguments, adjust if needed

    if (!process.waitForStarted()) {
        QMessageBox::critical(this, "Error", "Failed to start program: " + process.errorString());
        return;
    }


    auto samples = floatSrc->getSamples(selectedSamples.minimum, selectedSamples.length());
    auto step = (float)selectedSamples.length() / cursors.segments();
    auto symbols = std::vector<float>();
    for (auto i = step / 2; i < selectedSamples.length(); i += step)
    {
        symbols.push_back(samples[i]);
    }

    // Step 3: Pipe data to the program's stdin
    // Assume data is a QString or QByteArray from your class
    QByteArray dataToSend = vectorToTextByteArray(symbols);
    process.write(dataToSend);
    process.closeWriteChannel(); // Signal EOF to stdin

    // Step 4: Wait for the process to finish (optional, depending on your needs)
    if (!process.waitForFinished()) {
        QMessageBox::critical(this, "Error", "Program failed: " + process.errorString());
        return;
    }

    // Optional: Read output if needed
    QByteArray output = process.readAllStandardOutput();
    if (!output.isEmpty()) {
                SymbolProgOutput outputDialog(QString(output), this);
                outputDialog.exec();
    }

}
void PlotView::selectAndFeedExternalProgram(std::shared_ptr<AbstractSampleSource> src) {

    if (!cursorsEnabled)
        return;
    // Step 1: Open a file dialog to select the program
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setWindowTitle("Select External Program");


    QSettings settings;
    QString lastProgramPath = settings.value("lastProgramPath", QDir::homePath()).toString();
    dialog.selectFile(lastProgramPath);
    // Optional: Filter for executable files (platform-specific)
#ifdef Q_OS_WIN
    dialog.setNameFilter("Executables (*.exe);;All Files (*)");
#else
    dialog.setNameFilter("All Files (*)");
#endif

    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return; // User canceled or didn't select a file
    }

    QString programPath = dialog.selectedFiles().first();

    // Save the selected program path to QSettings
    settings.setValue("lastProgramPath", programPath);

    feedSymbolsToExternalProgram(programPath, src);
}

void PlotView::extractSymbols(std::shared_ptr<AbstractSampleSource> src,
                              bool toClipboard)
{
    if (!cursorsEnabled)
        return;
    auto floatSrc = std::dynamic_pointer_cast<SampleSource<float>>(src);
    if (!floatSrc)
        return;
    auto samples = floatSrc->getSamples(selectedSamples.minimum, selectedSamples.length());
    auto step = (float)selectedSamples.length() / cursors.segments();
    auto symbols = std::vector<float>();
    for (auto i = step / 2; i < selectedSamples.length(); i += step)
    {
        symbols.push_back(samples[i]);
    }
    if (!toClipboard) {
        for (auto f : symbols)
            std::cout << f << ", ";
        std::cout << std::endl << std::flush;
    } else {
        QClipboard *clipboard = QGuiApplication::clipboard();
        QString symbolText;
        QTextStream symbolStream(&symbolText);
        for (auto f : symbols)
            symbolStream << f << ", ";
        clipboard->setText(symbolText);
    }
}

void PlotView::exportSamples(std::shared_ptr<AbstractSampleSource> src)
{
    if (src->sampleType() == typeid(std::complex<float>)) {
        exportSamples<std::complex<float>>(src);
    } else {
        exportSamples<float>(src);
    }
}

template<typename SOURCETYPE>
void PlotView::exportSamples(std::shared_ptr<AbstractSampleSource> src)
{
    auto sampleSrc = std::dynamic_pointer_cast<SampleSource<SOURCETYPE>>(src);
    if (!sampleSrc) {
        return;
    }

    QFileDialog dialog(this);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilter(getFileNameFilter<SOURCETYPE>());
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);

    QGroupBox groupBox("Selection To Export", &dialog);
    QVBoxLayout vbox(&groupBox);

    QRadioButton cursorSelection("Cursor Selection", &groupBox);
    QRadioButton currentView("Current View", &groupBox);
    QRadioButton completeFile("Complete File (Experimental)", &groupBox);

    if (cursorsEnabled) {
        cursorSelection.setChecked(true);
    } else {
        currentView.setChecked(true);
        cursorSelection.setEnabled(false);
    }

    vbox.addWidget(&cursorSelection);
    vbox.addWidget(&currentView);
    vbox.addWidget(&completeFile);
    vbox.addStretch(1);

    groupBox.setLayout(&vbox);

    QGridLayout *l = dialog.findChild<QGridLayout*>();
    l->addWidget(&groupBox, 4, 1);

    QGroupBox groupBox2("Decimation");
    QSpinBox decimation(&groupBox2);
    decimation.setMinimum(1);
    decimation.setValue(1 / sampleSrc->relativeBandwidth());

    QVBoxLayout vbox2;
    vbox2.addWidget(&decimation);

    groupBox2.setLayout(&vbox2);
    l->addWidget(&groupBox2, 4, 2);

    if (dialog.exec()) {
        QStringList fileNames = dialog.selectedFiles();

        size_t start, end;
        if (cursorSelection.isChecked()) {
            start = selectedSamples.minimum;
            end = start + selectedSamples.length();
        } else if(currentView.isChecked()) {
            start = viewRange.minimum;
            end = start + viewRange.length();
        } else {
            start = 0;
            end = sampleSrc->count();
        }

        std::ofstream os (fileNames[0].toStdString(), std::ios::binary);

        size_t index;
        // viewRange.length() is used as some less arbitrary step value
        size_t step = viewRange.length();

        QProgressDialog progress("Exporting samples...", "Cancel", start, end, this);
        progress.setWindowModality(Qt::WindowModal);
        for (index = start; index < end; index += step) {
            progress.setValue(index);
            if (progress.wasCanceled())
                break;

            size_t length = std::min(step, end - index);
            auto samples = sampleSrc->getSamples(index, length);
            if (samples != nullptr) {
                for (auto i = 0; i < length; i += decimation.value()) {
                    os.write((const char*)&samples[i], sizeof(SOURCETYPE));
                }
            }
        }
    }
}

void PlotView::invalidateEvent()
{
    horizontalScrollBar()->setMinimum(0);
    horizontalScrollBar()->setMaximum(sampleToColumn(mainSampleSource->count()));
}

void PlotView::repaint()
{
    viewport()->update();
}

#include <iostream>
void PlotView::setCursorSegments(int segments)
{

    int curSegments = cursors.segments();
    if (curSegments != segments) {

    	int deltaSegments = segments - curSegments;
        // Calculate number of samples per segment
        selectedSamples.maximum = selectedSamples.maximum + (deltaSegments * samplesPerSegment);
    }
    cursors.setSegments(segments);
    updateView();
    emitTimeSelection();
}

void PlotView::setFFTAndZoom(int size, int zoom)
{
    auto oldSamplesPerColumn = samplesPerColumn();

    // Set new FFT size
    fftSize = size;
    if (spectrogramPlot != nullptr)
        spectrogramPlot->setFFTSize(size);

    // Set new zoom level
    zoomLevel = zoom;
    if (spectrogramPlot != nullptr)
        spectrogramPlot->setZoomLevel(zoom);

    // Update horizontal (time) scrollbar
    horizontalScrollBar()->setSingleStep(10);
    horizontalScrollBar()->setPageStep(100);

    updateView(true, samplesPerColumn() < oldSamplesPerColumn);
}

void PlotView::setPowerMin(int power)
{
    powerMin = power;
    if (spectrogramPlot != nullptr)
        spectrogramPlot->setPowerMin(power);
    updateView();
}

void PlotView::setSquelch(int sq) {
	squelch = sq;
	if (spectrogramPlot != nullptr)
		spectrogramPlot->setSquelch(sq);
    updateView();
}

void PlotView::setPowerMax(int power)
{
    powerMax = power;
    if (spectrogramPlot != nullptr)
        spectrogramPlot->setPowerMax(power);
    updateView();
}

void PlotView::paintEvent(QPaintEvent *event)
{
    if (mainSampleSource == nullptr) return;

    QRect rect = QRect(0, 0, width(), height());
    QPainter painter(viewport());
    painter.fillRect(rect, Qt::black);


#define PLOT_LAYER(paintFunc)                                                   \
    {                                                                           \
        int y = -verticalScrollBar()->value();                                  \
        for (auto&& plot : plots) {                                             \
            QRect rect = QRect(0, y, width(), plot->height());                  \
            plot->paintFunc(painter, rect, viewRange);                          \
            y += plot->height();                                                \
        }                                                                       \
    }

    PLOT_LAYER(paintBack);
    PLOT_LAYER(paintMid);
    PLOT_LAYER(paintFront);
    if (cursorsEnabled)
        cursors.paintFront(painter, rect, viewRange);

    if (timeScaleEnabled) {
        paintTimeScale(painter, rect, viewRange);
    }


#undef PLOT_LAYER
}

void PlotView::paintTimeScale(QPainter &painter, QRect &rect, range_t<size_t> sampleRange)
{
    float startTime = (float)sampleRange.minimum / sampleRate;
    float stopTime = (float)sampleRange.maximum / sampleRate;
    float duration = stopTime - startTime;

    if (duration <= 0)
        return;

    painter.save();

    QPen pen(Qt::white, 1, Qt::SolidLine);
    painter.setPen(pen);
    QFontMetrics fm(painter.font());

    int tickWidth = 80;
    int maxTicks = rect.width() / tickWidth;

    double durationPerTick = 10 * pow(10, floor(log(duration / maxTicks) / log(10)));

    double firstTick = int(startTime / durationPerTick) * durationPerTick;

    double tick = firstTick;

    while (tick <= stopTime) {

        size_t tickSample = tick * sampleRate;
        int tickLine = sampleToColumn(tickSample - sampleRange.minimum);

        char buf[128];
        snprintf(buf, sizeof(buf), "%.06f", tick);
        painter.drawLine(tickLine, 0, tickLine, 30);
        painter.drawText(tickLine + 2, 25, buf);

        tick += durationPerTick;
    }

    // Draw small ticks
    durationPerTick /= 10;
    firstTick = int(startTime / durationPerTick) * durationPerTick;
    tick = firstTick;
    while (tick <= stopTime) {

        size_t tickSample = tick * sampleRate;
        int tickLine = sampleToColumn(tickSample - sampleRange.minimum);

        painter.drawLine(tickLine, 0, tickLine, 10);
        tick += durationPerTick;
    }

    painter.restore();
}

int PlotView::plotsHeight()
{
    int height = 0;
    for (auto&& plot : plots) {
        height += plot->height();
    }
    return height;
}

void PlotView::resizeEvent(QResizeEvent * event)
{
    updateView();
}

size_t PlotView::samplesPerColumn()
{
    return fftSize / zoomLevel;
}

void PlotView::scrollContentsBy(int dx, int dy)
{
    updateView();
}

void PlotView::showEvent(QShowEvent *event)
{
    // Intentionally left blank. See #171
}

void PlotView::updateViewRange(bool reCenter)
{
    // Update current view
    auto start = columnToSample(horizontalScrollBar()->value());
    viewRange = {start, std::min(start + columnToSample(width()), mainSampleSource->count())};

    // Adjust time offset to zoom around central sample
    if (reCenter) {
        horizontalScrollBar()->setValue(
            sampleToColumn(zoomSample) - zoomPos
        );
    }
    zoomSample = viewRange.minimum + viewRange.length() / 2;
    zoomPos = width() / 2;
}

void PlotView::updateView(bool reCenter, bool expanding)
{
    if (!expanding) {
        updateViewRange(reCenter);
    }
    horizontalScrollBar()->setMaximum(std::max(0, sampleToColumn(mainSampleSource->count()) - width()));
    verticalScrollBar()->setMaximum(std::max(0, plotsHeight() - viewport()->height()));
    if (expanding) {
        updateViewRange(reCenter);
    }

    // Update cursors
    range_t<int> newSelection = {
        sampleToColumn(selectedSamples.minimum) - horizontalScrollBar()->value(),
        sampleToColumn(selectedSamples.maximum) - horizontalScrollBar()->value()
    };
    cursors.setSelection(newSelection);

    // Re-paint
    viewport()->update();
}

void PlotView::setSampleRate(double rate)
{
    sampleRate = rate;

    if (spectrogramPlot != nullptr)
        spectrogramPlot->setSampleRate(rate);

    emitTimeSelection();
}


void PlotView::setCenterFrequency(double freq) {

    centerFrequency = freq;

    if (spectrogramPlot != nullptr)
        spectrogramPlot->setCenterFrequency(freq);

}

void PlotView::enableScales(bool enabled)
{
    timeScaleEnabled = enabled;

    if (spectrogramPlot != nullptr)
        spectrogramPlot->enableScales(enabled);

    viewport()->update();
}

void PlotView::enableAnnotations(bool enabled)
{
    if (spectrogramPlot != nullptr)
        spectrogramPlot->enableAnnotations(enabled);

    viewport()->update();
}

void PlotView::enableAnnotationCommentsTooltips(bool enabled)
{
    annotationCommentsEnabled = enabled;

    viewport()->update();
}

int PlotView::sampleToColumn(size_t sample)
{
    return sample / samplesPerColumn();
}

size_t PlotView::columnToSample(int col)
{
    return col * samplesPerColumn();
}
