// Data/GfxData.cpp - This file is part of eln

/* eln is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   eln is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with eln.  If not, see <http://www.gnu.org/licenses/>.
*/

// GfxData.C

#include "GfxData.H"
#include "GfxBlockData.H"

GfxData::GfxData(Data *parent): Data(parent) {
  setType("gfx");
}

GfxData::~GfxData() {
}

double GfxData::x() const {
  return x_;
}
double GfxData::y() const {
  return y_;
}

void GfxData::setX(double x) {
  x_ = x;
  markModified();
}

void GfxData::setY(double y) {
  y_ = y;
  markModified();
}

QPointF GfxData::pos() const {
  return QPointF(x_, y_);
}

void GfxData::setPos(QPointF xy) {
  x_ = xy.x();
  y_ = xy.y();
  markModified();
}