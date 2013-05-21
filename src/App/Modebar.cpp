// App/Modebar.cpp - This file is part of eln

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

// Modebar.C

#include "Modebar.H"
#include "ToolItem.H"
#include "Mode.H"
#include "MarkSizeItem.H"
#include "LineWidthItem.H"

Modebar::Modebar(Mode *mode, QGraphicsItem *parent):
  Toolbar(parent), mode(mode) {
  ToolItem *t = new ToolItem();
  t->setSvg(":icons/browse.svg");
  addTool(modeToId(Mode::Browse), t);

  t = new ToolItem();
  t->setSvg(":icons/type.svg");
  addTool(modeToId(Mode::Type), t);

  t = new ToolItem();
  t->setSvg(":icons/move.svg");
  addTool(modeToId(Mode::MoveResize), t);

  MarkSizeItem *mst = new MarkSizeItem(mode->markSize());
  mst->setShape(mode->shape());
  mst->setColor(mode->color());
  addTool(modeToId(Mode::Mark), mst);
  connect(mode, SIGNAL(shapeChanged(GfxMarkData::Shape)),
	  mst, SLOT(setShape(GfxMarkData::Shape)));
  connect(mode, SIGNAL(markSizeChanged(double)),
	  mst, SLOT(setMarkSize(double)));
  connect(mode, SIGNAL(colorChanged(QColor)),
	  mst, SLOT(setColor(QColor)));

  LineWidthItem *lwt = new LineWidthItem(mode->lineWidth());
  connect(mode, SIGNAL(colorChanged(QColor)),
	  lwt, SLOT(setColor(QColor)));
  connect(mode, SIGNAL(lineWidthChanged(double)),
	  lwt, SLOT(setLineWidth(double)));
  addTool(modeToId(Mode::Freehand), lwt);
  
  t = new ToolItem();
  t->setSvg(":icons/note.svg");
  addTool(modeToId(Mode::Annotate), t);

  t = new ToolItem();
  t->setSvg(":icons/highlight.svg");
  addTool(modeToId(Mode::Highlight), t);

  t = new ToolItem();
  t->setSvg(":icons/strikeout.svg");
  addTool(modeToId(Mode::Strikeout), t);

  t = new ToolItem();
  t->setSvg(":icons/plain.svg");
  addTool(modeToId(Mode::Plain), t);
  
  select(modeToId(mode->mode()));
  connect(mode, SIGNAL(modeChanged(Mode::M)), SLOT(updateMode()));
}

Modebar::~Modebar() {
}

void Modebar::updateMode() {
  select(modeToId(mode->mode()));
}

Mode::M Modebar::idToMode(QString s) {
  return Mode::M(s.toInt());
}

QString Modebar::modeToId(Mode::M m) {
  return QString::number(m);
}

void Modebar::doLeftClick(QString id) {
  mode->setMode(idToMode(id));
}