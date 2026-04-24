/*
 *  Copyright (C) 2016, Mike Walters <mike@flomp.net>
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

#include <cstdlib>
#include "tuner.h"

Tuner::Tuner(int height, QObject * parent) : height(height), QObject::QObject(parent)
{
    minCursor = new Cursor(Qt::Horizontal, Qt::SizeVerCursor, this);
    cfCursor = new Cursor(Qt::Horizontal, Qt::SizeAllCursor, this);
    maxCursor = new Cursor(Qt::Horizontal, Qt::SizeVerCursor, this);
    connect(minCursor, &Cursor::posChanged, this, &Tuner::cursorMoved);
    connect(cfCursor, &Cursor::posChanged, this, &Tuner::cursorMoved);
    connect(maxCursor, &Cursor::posChanged, this, &Tuner::cursorMoved);

    cfCursor->setPos(100);
    _deviation = 10;
    minPosition = cfCursor->pos() - _deviation;
    maxPosition = cfCursor->pos() + _deviation;
    updateCursors();
}

int Tuner::centre()
{
    return centreFromEdges();
}

int Tuner::centreFromEdges() const
{
    return (minPosition + maxPosition) / 2;
}

void Tuner::cursorMoved()
{
    Cursor *sender = static_cast<Cursor*>(QObject::sender());
    auto posRange = range_t<int>{0, height};

    if (sender == cfCursor) {
        int delta = posRange.clip(sender->pos()) - centreFromEdges();
        delta = std::max(-minPosition, std::min(delta, height - maxPosition));
        minPosition += delta;
        maxPosition += delta;
    } else {
        int currentCentre = cfCursor->pos();
        int maxDeviation = std::max(minimumDeviation, std::min(currentCentre, height - currentCentre));

        if (currentModifiers & Qt::ShiftModifier) {
            if (sender == minCursor) {
                minPosition = range_t<int>{0, maxPosition - (minimumDeviation * 2)}.clip(posRange.clip(sender->pos()));
            } else {
                maxPosition = range_t<int>{minPosition + (minimumDeviation * 2), height}.clip(posRange.clip(sender->pos()));
            }
        } else {
            int newDeviation = std::abs(posRange.clip(sender->pos()) - currentCentre);
            _deviation = range_t<int>{minimumDeviation, maxDeviation}.clip(newDeviation);
            minPosition = currentCentre - _deviation;
            maxPosition = currentCentre + _deviation;
        }
    }

    _deviation = std::max(1, (maxPosition - minPosition) / 2);

    updateCursors();
}

int Tuner::deviation()
{
    return _deviation;
}

bool Tuner::mouseEvent(QEvent::Type type, QMouseEvent event)
{
    currentModifiers = event.modifiers();

    if (cfCursor->mouseEvent(type, event))
        return true;
    if (minCursor->mouseEvent(type, event))
        return true;
    if (maxCursor->mouseEvent(type, event))
        return true;

    return false;
}

void Tuner::paintFront(QPainter &painter, QRect &rect, range_t<size_t> sampleRange)
{
    painter.save();

    QRect cursorRect(rect.left(), rect.top() + minCursor->pos(), rect.right(), maxCursor->pos() - minCursor->pos());

    // Draw translucent white fill for highlight
    painter.fillRect(
        cursorRect,
        QBrush(QColor(255, 255, 255, 50))
    );

    // Draw tuner edges
    painter.setPen(QPen(Qt::white, 1, Qt::SolidLine));
    painter.drawLine(rect.left(), rect.top() + minCursor->pos(), rect.right(), rect.top() + minCursor->pos());
    painter.drawLine(rect.left(), rect.top() + maxCursor->pos(), rect.right(), rect.top() + maxCursor->pos());

    // Draw centre freq
    painter.setPen(QPen(Qt::red, 1, Qt::SolidLine));
    painter.drawLine(rect.left(), rect.top() + cfCursor->pos(), rect.right(), rect.top() + cfCursor->pos());

    painter.restore();
}

void Tuner::setCentre(int centre)
{
    int delta = centre - centreFromEdges();
    delta = std::max(-minPosition, std::min(delta, height - maxPosition));
    minPosition += delta;
    maxPosition += delta;
    updateCursors();
}

void Tuner::setDeviation(int dev)
{
    _deviation = std::max(1, dev);
    int currentCentre = centreFromEdges();
    int maxDeviation = std::max(1, std::min(currentCentre, height - currentCentre));
    _deviation = std::min(_deviation, maxDeviation);
    minPosition = currentCentre - _deviation;
    maxPosition = currentCentre + _deviation;
    updateCursors();
}

void Tuner::setHeight(int height)
{
    this->height = height;
    minPosition = range_t<int>{0, height}.clip(minPosition);
    maxPosition = range_t<int>{0, height}.clip(maxPosition);

    if (maxPosition - minPosition < minimumDeviation * 2) {
        maxPosition = std::min(height, minPosition + minimumDeviation * 2);
        minPosition = std::max(0, maxPosition - minimumDeviation * 2);
    }
}

void Tuner::updateCursors()
{
    cfCursor->setPos(centreFromEdges());
    minCursor->setPos(minPosition);
    maxCursor->setPos(maxPosition);
    emit tunerMoved(centre(), _deviation);
}
