// App/OneLink.cpp - This file is part of eln

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


#include "OneLink.h"
#include "TextItem.h"
#include "PreviewPopper.h"
#include "ResManager.h"
#include "ResourceMagic.h"
#include "Assert.h"
#include "SheetScene.h"
#include "PageView.h"
#include "TextData.h"
#include "OpenCmd.h"

#include <QPainter>
#include <QGraphicsSceneHoverEvent>
#include <QDebug>
#include <QProcess>

OneLink::OneLink(class MarkupData *md, class TextItem *item):
  QObject(item), md(md), ti(item) {
  //  start = end = -1;
  popper = 0;
  busy = false;
  lastRef = "";
  update();
}

OneLink::~OneLink() {
}

void OneLink::update() {
  qDebug() << "OneLink::update" << refText();
  
  if (!hasArchive() || !hasPreview()) {
    if (ti->isWritable()) {
      getArchiveAndPreview();
    }
  }
  /*
    This must happen whenever the MarkupData changes or periodically.
    Previously, this was assured because TextMarkings got an update() whenever
    text was inserted or removed from the item. It also had newMark() and
    deleteMark() functions to update the list of HoverRegions. This
    functionality should now move to LinkHelper.
  */
}
  
bool OneLink::mousePress(QGraphicsSceneMouseEvent *e) {
  qDebug() << "OneLink::mousePress" << refText();

  if (ti->mode()->mode()==Mode::Browse
      || (e->modifiers() & Qt::ControlModifier)) {
    activate(e);
    return true;
  } else {
    return false;
  }
}

void OneLink::activate(QGraphicsSceneMouseEvent *e) {
  qDebug() << "OneLink::activate event from " << e->widget();
  if (e->modifiers() & Qt::ShiftModifier)
    openLink();
  else 
    openArchive();
}

bool OneLink::mouseDoubleClick(QGraphicsSceneMouseEvent *e) {
  activate(e);
  return true;
}

void OneLink::enter(QGraphicsSceneHoverEvent *e) {
  qDebug() << "OneLink::enter" << refText();
  
  if (popper) {
    popper->popup();
    return;
  }
  QString txt = refText();
  if (txt.isEmpty())
    return;
  Resource *r = resource();
  if (r)
    popper = new PreviewPopper(r, e->screenPos(), this);
}

void OneLink::leave() {
  if (popper) 
    popper->deleteLater();
  popper = 0;
}

Resource *OneLink::resource() const {
  ResManager *resmgr = md->resManager();
  if (!resmgr) {
    qDebug() << "OneLink: no resource manager";
    return 0;
  }
  return resmgr->byTag(refText());
}

bool OneLink::hasArchive() const {
  Resource *res = resource();
  return res ? res->hasArchive() : false;
}

bool OneLink::hasPreview() const {
  Resource *res = resource();
  return res ? res->hasPreview() : false;
}

QString OneLink::refText() const {
  TextCursor c(ti->document(), md->start(), md->end());
  return c.selectedText();
}

void OneLink::openLink() {
  Resource *r = resource();
  if (!r) {
    qDebug() << "OneLink: openURL" << refText() <<  "(no url)";
    return;
  }
  qDebug() << "OneLink: openURL" << r->sourceURL();
  if (r->sourceURL().scheme() == "page") {
    openPage(true);
  } else {
    QStringList args;
    args << r->sourceURL().toString();
    bool ok = QProcess::startDetached(OpenCmd::command(), args);
    if (!ok)
      qDebug() << "Failed to start external process " << OpenCmd::command();
  }
}

void OneLink::openPage(bool newView) {
  Resource *r = resource();
  ASSERT(r);
  QString tag = r->tag();

  ASSERT(ti);
  SheetScene *s = dynamic_cast<SheetScene *>(ti->scene());
  ASSERT(s);
  PageView *pv = dynamic_cast<PageView *>(s->eventView());
  ASSERT(pv);

  if (newView)
    pv->newView(tag);
  else
    pv->gotoEntryPage(tag);
}

void OneLink::openArchive() {
  Resource *r = resource();
  if (!r) {
    qDebug() << "OneLink: openArchive" << refText() << "(no arch)";
    return;
  }
  if (!hasArchive()) {
    openLink();
    return;
  }

  qDebug() << "OneLink: openArchive" << r->archivePath();
  if (r->sourceURL().scheme() == "page") {
    openPage();
  } else {
    QStringList args;
    args << r->archivePath();
    bool ok = QProcess::startDetached(OpenCmd::command(), args);
    if (!ok)
      qDebug() << "Failed to start external process " << OpenCmd::command();
  }
}  

void OneLink::getArchiveAndPreview() {
  qDebug() << "OneLink::getArchiveAndPreview" << refText() << lastRef;

  if (refText()==lastRef || busy)
    return; // we know we can't do it

  ResManager *resmgr = md->resManager();
  if (!resmgr) {
    qDebug() << "OneLink: no resource manager";
    return;
  }
  
  QString newRef = refText();
  if (newRef!=lastRef && !lastRef.isEmpty()) {
    md->detachResource(lastRef);
    md->resManager()->perhapsDropResource(lastRef);
  }
  
  lastRef = newRef;

  Resource *r = resmgr->byTag(newRef);
  if (r) {
    lastRefIsNew = false;
  } else {
    r = md->resManager()->newResource(newRef);
    lastRefIsNew = true;
  }
  connect(r, SIGNAL(finished()), SLOT(downloadFinished()),
	  Qt::UniqueConnection);

  if (!md->resourceTags().contains(newRef))
    md->attachResource(newRef);
  
  busy = true;
  r->getArchiveAndPreview();
}

void OneLink::downloadFinished() {
  qDebug() << "OneLink::downloadFinished" << refText();
  if (!busy) {
    qDebug() << "not busy";
    return;
  }
  ASSERT(busy);
  ResManager *resmgr = md->resManager();
  Resource *r = resmgr->byTag(lastRef);
  if (!r || refText()!=lastRef) {
    /* Either the resource got destroyed somehow, or we have already
       changed; so we're not interested in the results anymore. */
    if (lastRefIsNew) 
      resmgr->dropResource(r);
  } else if (r->hasArchive() || r->hasPreview()
             || !r->title().isEmpty() || !r->description().isEmpty()) {
    // at least somewhat successful
    qDebug() << "Attaching new resource" << lastRef;
    md->attachResource(lastRef);
  } else {
    // utter failure
    if (lastRefIsNew) {
      resmgr->dropResource(r);
    }
  }
  busy = false;
}