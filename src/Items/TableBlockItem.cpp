// Items/TableBlockItem.cpp - This file is part of eln

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

// TableBlockItem.cpp

#include "TableBlockItem.H"
#include "TableItem.H"

TextItem *TTICreator::create(TextData *data, Item *parent) const {
  TableData *d = dynamic_cast<TableData*>(data);
  ASSERT(d);
  return new TableItem(d, parent);
}

TableBlockItem::TableBlockItem(TableBlockData *data, Item *parent):
  TextBlockItem(data, parent, TTICreator()) {
  item_ = firstChild<TableItem>();
  ASSERT(item_);
}

TableBlockItem::~TableBlockItem() {
}


TableItem *TableBlockItem::table() {
  return item_;
}