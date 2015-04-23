// ResourceMagic/Magician.cpp - This file is part of eln

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

// Magician.C

#include "Magician.h"
#include <QDebug>
#include <QtGlobal>
#include <QStringList>

Magician::Magician() {
}

Magician::~Magician() {
}

bool Magician::matches(QString) const {
  return true;
}

QUrl Magician::webUrl(QString) const {
  return QUrl();
}

QUrl Magician::objectUrl(QString) const {
  return QUrl();
}

QString Magician::title(QString) const {
  return QString();
}

QString Magician::desc(QString) const {
  return QString();
}

bool Magician::objectUrlNeedsWebPage(QString) const {
  return false;
}

QUrl Magician::objectUrlFromWebPage(QString tag, QString) const {
  return objectUrl(tag);
}

//////////////////////////////////////////////////////////////////////

SimpleMagician::SimpleMagician() {
}

SimpleMagician::SimpleMagician(QVariantMap const &dict) {
  setMatcher(QRegExp(dict["re"].toString()));
  setWebUrlBuilder(dict["web"].toString());
  setObjectUrlBuilder(dict["object"].toString());
}

SimpleMagician::~SimpleMagician() {
}

bool SimpleMagician::matches(QString ref) const {
  bool ok = re.exactMatch(ref);
  if (ok)
    qDebug() << "SimpleMagician" << re.pattern() << "matches" << ref;
  return ok;
}

void SimpleMagician::setMatcher(QRegExp r) {
  re = r;
}

QUrl SimpleMagician::webUrl(QString ref) const {
  if (webUrlBuilder.isEmpty())
    return QUrl();
  else
    return QUrl(webUrlBuilder.arg(ref));
}

QUrl SimpleMagician::objectUrl(QString ref) const {
  if (objectUrlBuilder.isEmpty())
    return QUrl();
  else
    return QUrl(objectUrlBuilder.arg(ref));
}

void SimpleMagician::setWebUrlBuilder(QString s) {
  webUrlBuilder = s;
}

void SimpleMagician::setObjectUrlBuilder(QString s) {
  objectUrlBuilder = s;
}

bool UrlMagician::matches(QString s) const {
  if (s.startsWith("http://")
      || s.startsWith("https://")
      || s.startsWith("file://")
      || s.startsWith("~/")
      || s.startsWith("/"))
    return true;

  QStringList spl = s.split("/");
  if (spl[0].startsWith("www.")
      || spl[0].endsWith(".com")
      || spl[0].endsWith(".net")
      || spl[0].endsWith(".org")
      || spl[0].endsWith(".edu"))
    return true;

  return false;
}

QUrl UrlMagician::objectUrl(QString s) const {
  if (s.startsWith("http://")
      || s.startsWith("https://")
      || s.startsWith("file://"))
    return QUrl(s);

  if (s.startsWith("/"))
    return QUrl("file://" + s);

  if (s.startsWith("~/")) {
    QString home = qgetenv("HOME");
    return QUrl("file://" + home + "/" + s.mid(2));
  }
  
  QStringList spl = s.split("/");  
  if (spl[0].startsWith("www.")
      || spl[0].endsWith(".com")
      || spl[0].endsWith(".net")
      || spl[0].endsWith(".org")
      || spl[0].endsWith(".edu"))
    return QUrl("http://" + s);

  return QUrl(s);
}

