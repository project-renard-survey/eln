// Book/Search.h - This file is part of eln

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


// Search.H

#ifndef SEARCH_H

#define SEARCH_H

#include <QString>
#include <QDateTime>
#include <QList>
#include <QThread>
#include <QMutex>

#include "Notebook.h"

struct SearchResult {
  enum Type {
    Unknown,
    InTextBlock,
    InTableBlock,
    InGfxNote,
    InLateNote,
    InFootnote,
  };
  Type type;
  int page;
  int startPageOfEntry;
  QString phrase; // search text
  QString context; // entire text of containing object
  QString entryTitle;
  QDateTime cre, mod; // of containing object
  QString uuid; // of containing object
  QList<int> whereInContext;
};

class Search: public QThread {
  Q_OBJECT;
public:
  Search(Notebook *book);
  virtual ~Search();
  QList<SearchResult> immediatelyFindPhrase(QString) const;
  void startSearchForPhrase(QString);
  void abandonSearch();
  bool isSearchComplete();
  bool isSearching();
  QList<SearchResult> searchResults() const;
signals:
  void searchCompleted();
private:
  static void addToResults(QList<SearchResult> &dest, QString phrase,
                           QString entryTitle,
                           Data const *data, int entryPage, int dataPage);
  void run();
  static QString untable(class TableData const *);
private:
  Notebook *book;
  QString phrase;
  QList<SearchResult> results;
  bool abandon;
  mutable QMutex mutex;
};

#endif
