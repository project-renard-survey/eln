// Items/TextItem.cpp - This file is part of eln

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

// TextItem.C

#include <QTextLayout>
#include "TextItem.H"
#include "TextData.H"
#include "TextMarkings.H"
#include "Mode.H"
#include "EntryScene.H"
#include "Style.H"
#include "ResManager.H"
#include "HoverRegion.H"
#include "BlockItem.H"
#include "ResourceMagic.H"
#include "Assert.H"
#include "TeXCodes.H"
#include "Digraphs.H"
#include "TextBlockItem.H"
#include "Cursors.H"

#include <QFont>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <QKeyEvent>
#include <QDebug>
#include <QTextBlock>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>

#include "LateNoteItem.H" 
#include "LateNoteData.H" 

TextItem::TextItem(TextData *data, Item *parent, bool noFinalize,
		   QTextDocument *altdoc):
  Item(data, parent) {
  hasAltDoc = altdoc;
  markings_ = 0;
  text = new TextItemText(this);
  if (altdoc)
    text->setDocument(altdoc);
  
  mayMark = true;
  mayNote = false;
  mayMove = false;
  lateMarkType = MarkupData::Normal;
  allowParagraphs_ = true;

  if (!altdoc)
    initializeFormat();
  
  text->setTextInteractionFlags(Qt::TextSelectableByMouse);
  connect(text, SIGNAL(invisibleFocus(QPointF)),
	  this, SIGNAL(invisibleFocus(QPointF)));

  if (!altdoc) {
    text->setPlainText(data->text());
    QTextCursor tc(textCursor());
    tc.movePosition(QTextCursor::Start);
    QTextBlockFormat fmt = tc.blockFormat();
    tc.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    tc.setBlockFormat(fmt);
  }
  
  if (!noFinalize) 
    finalizeConstructor();
}

void TextItem::finalizeConstructor(int sheet) {
  foreach (LateNoteData *lnd, data()->children<LateNoteData>())
    if (sheet<0 || lnd->sheet()==sheet)
      create(lnd, this);
  foreach (GfxNoteData *gnd,  data()->children<GfxNoteData>())
    if (!dynamic_cast<LateNoteData *>(gnd)) // ugly, but hey.
      if (sheet<0 || gnd->sheet()==sheet)
	create(gnd, this);

  if (!markings_)
    markings_ = new TextMarkings(data(), this);
  if (hasAltDoc)
    markings_->setSecundary();

  connect(document(), SIGNAL(contentsChange(int, int, int)),
	  this, SLOT(docChange()));
}

bool TextItem::allowNotes() const {
  return mayNote;
}

void TextItem::setAllowNotes(bool y) {
  mayNote = y;
}

void TextItem::makeWritable() {
  Item::makeWritable();
  makeWritableNoRecurse();
}

void TextItem::makeWritableNoRecurse() {
  // this ugliness is for the sake of title items that have notes attached
  Item::makeWritableNoRecurse();
  text->setTextInteractionFlags(Qt::TextEditorInteraction);
  text->setCursor(QCursor(Qt::IBeamCursor));
  setFlag(ItemIsFocusable);
  setFocusProxy(text);
}

void TextItem::setAllowMoves() {
  mayMove = true;
  setAcceptHoverEvents(true);
  text->setAcceptHoverEvents(true);
  connect(mode(), SIGNAL(modeChanged(Mode::M)),
	  SLOT(modeChange(Mode::M)));
}

TextItem::~TextItem() {
}

void TextItem::initializeFormat() {
  setFont(style().font("text-font"));
  setDefaultTextColor(style().color("text-color"));
}

void TextItem::docChange() {
  QString plainText = text->toPlainText();

  if (data()->text() == plainText) {
    // trivial change; this happens if markup changes
    return;
  }
  qDebug() << "docchange" << plainText << " was " << data()->text();
  ASSERT(isWritable());
  data()->setText(plainText);
  emit textChanged();
}

bool TextItem::focusIn(QFocusEvent *) {
  return false;
}

bool TextItem::focusOut(QFocusEvent *) {
  if (document()->isEmpty()) {
    emit abandoned();
  }
  return false;
}

bool TextItem::mouseDoubleClick(QGraphicsSceneMouseEvent *e) {
  if (e->button()!=Qt::LeftButton)
    return false;
  if (mode()->mode()==Mode::Type)
    return false;
  if (text->hasFocus())
    text->clearFocus();
  e->ignore();
  return true;
}

bool TextItem::mousePress(QGraphicsSceneMouseEvent *e) {
  switch (e->button()) {
  case Qt::LeftButton:
    switch (mode()->mode()) {
    case Mode::Type: 
      qDebug() << "TI:mousepress:type";
      return false; // TextItemText will decide whether to edit or not
    case Mode::MoveResize:
      if (mayMove) {
        bool resize = shouldResize(e->pos());
        GfxNoteItem *gni = dynamic_cast<GfxNoteItem*>(parent());
        if (gni)
          gni->childMousePress(e->scenePos(), e->button(), resize);
      }
      break;
    case Mode::Highlight:
      attemptMarkup(e->pos(), MarkupData::Emphasize);
      break;
    case Mode::Strikeout:
      attemptMarkup(e->pos(), MarkupData::StrikeThrough);
      break;
    case Mode::Plain:
      attemptMarkup(e->pos(), MarkupData::Normal);
      break;
    default:
      break;
    }
    e->accept();
    if (text->hasFocus())
      text->clearFocus();
    return true;
  case Qt::MiddleButton:
    if (mode()->mode() == Mode::Type) {
      QClipboard *cb = QApplication::clipboard();
      QString txt = cb->text(QClipboard::Selection);
      if (!txt.isEmpty()) {
      	QTextCursor c = textCursor();
      	int pos = pointToPos(e->pos());
      	if (pos>=0) 
      	  c.setPosition(pos);
      	c.insertText(txt);
      	setFocus();
      	setTextCursor(c);
      }
      return false;
      // Bizarrely, if I call accept() and return true, the text is inserted
      // yet again, but with unwanted formatting.
      //   e->accept();
      //   return true;
    } else {
      return false;
    }
  default:
    return false;
  }
}

int TextItem::pointToPos(QPointF p) const {
  return ::pointToPos(text, text->mapFromParent(p));
}

QPointF TextItem::posToPoint(int pos) const {
  return text->mapToParent(::posToPoint(text, pos));
}
      

void TextItem::attemptMarkup(QPointF p, MarkupData::Style m) {
  qDebug() << "TextItem::attemptMarkup" << p << m;
  int pos = pointToPos(p);
  qDebug() << "  pos:"<<pos;
  if (pos<0)
    return;
  lateMarkType = m;
  lateMarkStart = pos;
  grabMouse();
}

void TextItem::mouseMoveEvent(QGraphicsSceneMouseEvent *evt) {
  int pos = pointToPos(evt->pos());
  qDebug() << "TextItem::mouseMove" << lateMarkStart << evt->pos() << pos
	   << MarkupData::styleName(lateMarkType);
  if (pos<0)
    return;
  int s, e;
  if (lateMarkStart<pos) {
    s = lateMarkStart;
    e = pos;
  } else {
    s = pos;
    e = lateMarkStart;
  }
  
  if (lateMarkType==MarkupData::Normal) {
    // unmark
    foreach (MarkupData *md, data()->children<MarkupData>()) {
      if (md->isRecent() && (md->style()==MarkupData::Emphasize
                             || md->style()==MarkupData::StrikeThrough)) {
        int mds = md->start();
        int mde = md->end();
	if (mds<e && mde>s) {
          MarkupData::Style mdst = md->style();
          QDateTime cre = md->created();
          markings_->deleteMark(md);
          if (mde>e) {
            markings_->newMark(mdst, e, mde);
            MarkupData *md1 = markupAt(e, mdst);
            if (md1)
              md1->setCreated(cre);
          }
          if (mds<s) {
            markings_->newMark(mdst, mds, s);
            MarkupData *md1 = markupAt(s, mdst);
            if (md1)
              md1->setCreated(cre);
          }
        }
      }
    }
  } else {
    addMarkup(lateMarkType, s, e); // will be auto-merged
  }
  qDebug() << "  -> markings now:";
  foreach (MarkupData *md, data()->children<MarkupData>()) 
    qDebug() << "    " << md->styleName(md->style())
             << md->start() << md->end();
}

void TextItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *) {
  ungrabMouse();
  lateMarkType = MarkupData::Normal;
}

bool TextItem::keyPressAsMotion(QKeyEvent *e) {
  switch (e->key()) {
  case Qt::Key_Escape: {
    QTextCursor c = textCursor();
    c.clearSelection();
    setTextCursor(c);
    clearFocus();
  } return true;
  case Qt::Key_Return: case Qt::Key_Enter:
    if (!allowParagraphs_) {
      emit futileMovementKey(e->key(), e->modifiers());
      return true;
    } break;
  case Qt::Key_Backspace:
    if (textCursor().atStart() && !textCursor().hasSelection()) {
      emit futileMovementKey(e->key(), e->modifiers());
      return true;
    } break;
  case Qt::Key_Delete:
    if (textCursor().atEnd() && !textCursor().hasSelection()) {
      emit futileMovementKey(e->key(), e->modifiers());
      return true;
    } break;
  case Qt::Key_Left: case Qt::Key_Up:
  case Qt::Key_Right: case Qt::Key_Down:
  case Qt::Key_PageUp: case Qt::Key_PageDown: {
    QTextCursor pre = textCursor();
    text->internalKeyPressEvent(e);
    QTextCursor post = textCursor();
    if (e->key()==Qt::Key_PageDown) {
      qDebug() << "TextItem::pagedown " << pre.position() << ";" << post.position();
    }
    if (pre.position() == post.position())
      emit futileMovementKey(e->key(), e->modifiers());
    return true;
  } break;
  }
  return false;
}

bool TextItem::keyPressWithControl(QKeyEvent *e) {
  if (!(e->modifiers() & Qt::ControlModifier))
    return false;
  if (keyPressAsSimpleStyle(e->key(), textCursor()))
    return true;

  if (mode()->mathMode())
    tryTeXCode(true);
  
  switch (e->key()) {
  case Qt::Key_V:
    tryToPaste();
    return true;
  case Qt::Key_N:
    tryFootnote();
    return true;
  case Qt::Key_L:
    tryExplicitLink();
    return true;
  case Qt::Key_Period:
    tryScriptStyles(textCursor());
    return true;
  case Qt::Key_Backslash:
    tryTeXCode();
    return true;
  case Qt::Key_2:
    insertBasicHtml(QString::fromUtf8("²"), textCursor().position());
    return true;
  case Qt::Key_3:
    insertBasicHtml(QString::fromUtf8("³"), textCursor().position());
    return true;
  case Qt::Key_4:
    insertBasicHtml(QString::fromUtf8("⁴"), textCursor().position());
    return true;
  case Qt::Key_Space:
    insertBasicHtml(QString::fromUtf8(" "), textCursor().position());
    return true;
  case Qt::Key_Enter: case Qt::Key_Return:
    insertBasicHtml(QString("\n"), textCursor().position());
    return true;
  default:
    return false;
  }
}

bool TextItem::keyPressAsSimpleStyle(int key, QTextCursor const &cursor) {
  switch (key) {
  case Qt::Key_Slash:
    if (mode()->mathMode())
      tryTeXCode();
    toggleSimpleStyle(MarkupData::Italic, cursor);
    return true;
  case Qt::Key_8: case Qt::Key_Asterisk: case Qt::Key_Comma:
    toggleSimpleStyle(MarkupData::Bold, cursor);
    return true;
  case Qt::Key_6: // cas Qt::Key_Hat:
    toggleSimpleStyle(MarkupData::Superscript, cursor);
    return true;
  case Qt::Key_Minus: // Underscore and Minus are on the same key
    // on my keyboard, but they generate different codes
    toggleSimpleStyle(MarkupData::Subscript, cursor);
    return true;
  case Qt::Key_Underscore:
    toggleSimpleStyle(MarkupData::Underline, cursor);
    return true;
  case Qt::Key_1: case Qt::Key_Exclam:
    toggleSimpleStyle(MarkupData::Emphasize, cursor);
    return true;
  case Qt::Key_Equal:
    toggleSimpleStyle(MarkupData::StrikeThrough, cursor);
    return true;
  default:
    return false;
  }
}

bool TextItem::tryTeXCode(bool noX) {
  QTextCursor c(textCursor());
  if (!c.hasSelection()) {
    QTextCursor m = document()->find(QRegExp("([^A-Za-z])"),
				     c, QTextDocument::FindBackward);
    int start = m.hasSelection() ? m.selectionEnd() : 0;
    m = document()->find(QRegExp("([^A-Za-z])"),
			 start);
    int end = m.hasSelection() ? m.selectionStart() : data()->text().size();
    c.setPosition(start);
    c.setPosition(end, QTextCursor::KeepAnchor);
  }
  // got a word
  QString key = c.selectedText();
  if (!TeXCodes::contains(key))
    return false;
  if (noX && key.size()==1)
    return false;
  QString val = TeXCodes::map(key);
  c.deleteChar(); // delete the word
  if (document()->characterAt(c.position()-1)=='\\')
    c.deletePreviousChar(); // delete any preceding backslash
  if (val.startsWith("x")) {
    // this is "vec", or "dot", or similar
    if (document()->characterAt(c.position()-1).isSpace())
      c.deletePreviousChar(); // delete previous space
    c.insertText(val.mid(1));
  } else {
    c.insertText(val); // insert the replacement code
  }
  return true;
}

bool TextItem::keyPressAsSpecialEvent(QKeyEvent *e) {
  if (e->key()==Qt::Key_Tab || e->key()==Qt::Key_Backtab) {
    QTextCursor tc(textCursor());
    
    TextBlockItem *p = dynamic_cast<TextBlockItem *>(parent());
    if (p) {
      // we are in a text block, so we could fiddle with indentation
      bool hasIndent = p->data()->indented();
      bool hasDedent = p->data()->dedented();
      bool hasShift = e->modifiers() & Qt::ShiftModifier;
      bool hasControl = e->modifiers() & Qt::ControlModifier;
      if (hasControl) {
        p->data()->setDisplayed(!p->data()->displayed());
      } else if (hasShift) {
        if (hasIndent) 
          p->data()->setIndented(false);
        else if (hasDedent)
          p->data()->setIndented(true);
        else
          p->data()->setDedented(true);
      } else if (tc.position()==0) {
	if (hasIndent)
	  return false; // allow Tab to be inserted at start
	else
          p->data()->setIndented(true);
      } else {
	// no control, no shift, not at start
	QTextDocument *doc = document();
	if (doc->blockCount()==1
	    && doc->firstBlock().lineCount()==1
	    && doc->firstBlock().layout()->lineAt(0).naturalTextWidth()
	    < (style().real("page-width")-style().real("margin-left")
	       -style().real("margin-right"))*2.0/3.0) {
	  emit multicellular(tc.position(), data());
	  return true; // do not allow Tab to be inserted
	} else {
	  return false; // allow Tab to be inserted
	}
      }
      p->initializeFormat();
      return true;
    }
  }
  return false;
}

bool TextItem::keyPressAsSpecialChar(QKeyEvent *e) {
  QTextCursor c(textCursor());
  QChar charBefore = document()->characterAt(c.position()-1);
  QChar charBefore2 = document()->characterAt(c.position()-2);
  QString charNow = e->text();
  QString digraph = QString(charBefore) + charNow;
  QString trigraph = QString(charBefore2) + digraph;
  if (Digraphs::contains(digraph)) {
    c.deletePreviousChar();
    c.insertText(Digraphs::map(digraph));
    return true;
  } else if (Digraphs::contains(trigraph)) {
    c.deletePreviousChar();
    c.deletePreviousChar();
    c.insertText(Digraphs::map(trigraph));
    return true;
  } else if (Digraphs::contains(charNow)) {
    c.insertText(Digraphs::map(charNow));
    return true;
  } else if (charNow=="\"") {
    if (charBefore.isSpace() || charBefore.isNull()
	|| digraph=="(\"" || digraph=="[\"" || digraph=="{\""
	|| digraph==QString::fromUtf8("‘\"")) 
      c.insertText(QString::fromUtf8("“"));
    else
      c.insertText(QString::fromUtf8("”"));
    return true;
  } else if (digraph==QString::fromUtf8("--")) {
    c.deletePreviousChar();
    if (document()->characterAt(c.position()-1).isDigit()) 
      c.insertText(QString::fromUtf8("‒")); // figure dash
    else 
      c.insertText(QString::fromUtf8("–")); // en dash
    return true;
  } else if (charNow[0].isDigit() && charBefore==QChar('-')
	     && QString(" ([{^_@$/").contains(charBefore2)) {
    c.deletePreviousChar();
    c.insertText(QString::fromUtf8("−")); // replace minus sign
    return false; // insert digit as normal
  } else {
    return false;
  }
}

  
bool TextItem::keyPress(QKeyEvent *e) {
  return
    keyPressWithControl(e)
    || keyPressAsSpecialChar(e)
    || (mode()->mathMode() && keyPressAsMath(e))
    || keyPressAsMotion(e)
    || keyPressAsSpecialEvent(e)
    ;
}

bool TextItem::charBeforeIsLetter(int pos) const {
  return document()->characterAt(pos-1).isLetter();
  // also returns false at start of doc
}

bool TextItem::charAfterIsLetter(int pos) const {
  return document()->characterAt(pos).isLetter();
  // also returns false at end of doc
}


static bool balancedBrackets(QString s) {
  static QString brackets = QString::fromUtf8("()<>{}[]{}⁅⁆〈〉⎡⎤⎣⎦❬❭❰❱❲❳❴❵⟦⟧⟨⟩⟪⟫⟬⟭⦃⦄⦇⦈⦉⦊⦋⦌⦍⦎⦏⦐⦑⦒⦓⦔⦕⦖⦗⦘⧼⧽〈〉《》「」『』【】〔〕〖〗〘〙");
 for (int i=0; i<brackets.size(); i+=2)
   if (s.count(brackets[i]) != s.count(brackets[i+1]))
     return false;
 return true;
}

bool TextItem::tryScriptStyles(QTextCursor c, bool onlyIfBalanced) {
  /* Returns true if we decide to make a superscript or subscript, that is,
     if there is a preceding "^" or "_".
   */
  QTextCursor m = document()->find(QRegExp("\\^|_"),
				   c, QTextDocument::FindBackward);
  if (!m.hasSelection())
    return false; // no "^" or "_"
  if (m.selectionEnd() == c.position())
    return false; // empty selection

  qDebug() << "tryScriptStyles " << onlyIfBalanced;
  if (onlyIfBalanced) {
    QTextCursor scr(m);
    scr.setPosition(c.position(), QTextCursor::KeepAnchor);
    if (!balancedBrackets(scr.selectedText()))
      return false;
  }  
  
  QString mrk = m.selectedText();
  m.deleteChar();
  addMarkup(mrk=="^"
	    ? MarkupData::Superscript
	    : MarkupData::Subscript,
	    m.position(), c.position());
  return true;
}


void TextItem::toggleSimpleStyle(MarkupData::Style type,
                                 QTextCursor const &c) {
  MarkupData *oldmd = markupAt(c.position(), type);
  int start=-1;
  int end=-1;
  if (c.hasSelection()) {
    start = c.selectionStart();
    end = c.selectionEnd();
  } else {
    int base = c.position();
    if (document()->characterAt(base-1).isDigit()) {
      start = base-1;
      while (document()->characterAt(start-1).isDigit())
	start--;
      end = base;
      while (document()->characterAt(end).isDigit())
	end++;
    } else if (document()->characterAt(base-1).isLetter()) {
      start = base-1;
      while (document()->characterAt(start-1).isLetter())
	start--;
      end = base;
      while (document()->characterAt(end).isLetter())
	end++;
    } else {
      return;
    }
    start = refineStart(start, base);
    end = refineEnd(end, base);
  }
  int min = c.block().position();
  int max = min + c.block().length() - 1;
  if (start<min)
    start = min;
  if (end>max)
    end = max;
  
  if (oldmd && oldmd->start()==start && oldmd->end()==end) 
    markings_->deleteMark(oldmd);
  else if (start<end) 
    addMarkup(type, start, end);
  if (type==MarkupData::Italic) {
    QTextCursor d(c);
    if (d.hasSelection())
      d.setPosition(d.selectionEnd());
    d.insertText(QString::fromUtf8(" ")); // hair space 0x200a
  }
}
  
void TextItem::addMarkup(MarkupData::Style t, int start, int end) {
  markings_->newMark(t, start, end);
}

void TextItem::addMarkup(MarkupData *d) {
  markings_->newMark(d);
}

MarkupData *TextItem::markupAt(int pos, MarkupData::Style typ) {
  return markupAt(pos, pos, typ);
}

MarkupData *TextItem::markupAt(int start, int end, MarkupData::Style typ) {
  foreach (MarkupData *md, data()->children<MarkupData>()) 
    if (md->style()==typ && md->end()>=start && md->start()<=end)
      return md;
  return 0;
}


int TextItem::refineStart(int start, int base) {
  /* Shrinks a region for applysimplestyle to not cross any other style edges
     This function shrinks from the start.
   */
  foreach (MarkupData *md, data()->children<MarkupData>()) {
    int s = md->start();
    int e = md->end();
    if (s>start && s<base)
      start = s;
    if (e>start && e<base)
      start = e;
  }
  return start;
}

int TextItem::refineEnd(int end, int base) {
  /* Shrinks a region for applysimplestyle to not cross any other style edges.
     This function shrinks from the end.
   */
  foreach (MarkupData *md, data()->children<MarkupData>()) {
    int s = md->start();
    int e = md->end();
    if (s>=base && s<end)
      end = s;
    if (e>=base && e<end)
      end = e;
  }
  return end;
}

static bool approvedMark(QString s) {
  QString marks = "*@#%$&+"; // add more later
  return marks.contains(s);
}

QString TextItem::markedText(MarkupData *md) {
  ASSERT(md);
  QTextCursor c = textCursor();
  c.setPosition(md->start());
  c.setPosition(md->end(), QTextCursor::KeepAnchor);
  return c.selectedText();
}

bool TextItem::tryExplicitLink() {
  QTextCursor m = ResourceMagic::explicitLinkAt(textCursor(), style());
  if (!m.hasSelection())
    return false;
  int start = m.selectionStart();
  int end = m.selectionEnd();
  MarkupData *oldmd = markupAt(start, end, MarkupData::Link);
  if (oldmd) {
    // undo link mark
    markings_->deleteMark(oldmd);
    // if the old link exactly matches our selection, just drop it;
    // otherwise, replace it.
    if  (oldmd->start()==start && oldmd->end()==end) 
      return false;
  }
  if (end>start) {
    addMarkup(MarkupData::Link, start, end);
    return true;
  } else {
    return false;
  }
}

bool TextItem::tryFootnote() {
  BlockItem *anc = ancestralBlock();
  if (!anc) {
    qDebug() << "Cannot add footnote without ancestral block";
    return false;
  }

  EntryScene *bs = dynamic_cast<EntryScene*>(anc->baseScene());
  ASSERT(bs);
  int i = bs->findBlock(anc);
  ASSERT(i>=0);
  
  QTextCursor c = textCursor();
  MarkupData *oldmd = markupAt(c.position(), MarkupData::FootnoteRef);
  int start=-1;
  int end=-1;
  bool mayDelete = false;
  if (c.hasSelection()) {
    start = c.selectionStart();
    end = c.selectionEnd();
    mayDelete = true;
  } else {
    QTextCursor m = document()->find(QRegExp("[^-\\w]"), c,
				     QTextDocument::FindBackward);
    QString mrk = m.selectedText();
    start = m.hasSelection() ? m.selectionEnd() : 0;
    m = document()->find(QRegExp("[^-\\w]"), c);
    end = m.hasSelection() ? m.selectionStart() : data()->text().size();
    if (start==end && start>0) 
      if (approvedMark(mrk))
	--start; // markup is a single non-word char like "*".
  }

  if (oldmd && oldmd->start()==start && oldmd->end()==end) {
    if (mayDelete) {
      // delete old mark
      BlockItem *bi = ancestralBlock();
      if (bi) 
	bi->refTextChange(oldmd->text(), ""); // remove any footnotes
      markings_->deleteMark(oldmd);
    } else {
      return false; // should perhaps give focus to the footnote
    }
    return false;
  } else if (start<end) {
    addMarkup(MarkupData::FootnoteRef, start, end);
    MarkupData *md = markupAt(start, end, MarkupData::FootnoteRef);
    ASSERT(md);
    bs->newFootnote(i, markedText(md));
    return true;
  } else {
    return false;
  }
}

bool TextItem::tryToPaste() {
  QClipboard *cb = QApplication::clipboard();
  QMimeData const *md = cb->mimeData(QClipboard::Clipboard);
  qDebug() << "TextItem::tryToPaste" << md;
  if (md->hasImage()) {
    return false;
  } else if (md->hasUrls()) {
    return false; // perhaps we should allow URLs, but format specially?
  } else if (md->hasText()) {
    QString txt = md->text();
    QTextCursor c = textCursor();
    c.insertText(txt);
    return true;
  } else {
    return false;
  }
}  

bool TextItem::allowParagraphs() const {
  return allowParagraphs_;
}

void TextItem::setAllowParagraphs(bool yes) {
  allowParagraphs_ = yes;
}

bool TextItem::shouldResize(QPointF p) const {
  GfxNoteItem *gni = dynamic_cast<GfxNoteItem*>(parent());
  if (!gni)
    return false;
  double tw = gni->data()->textWidth();
  if (tw<=0)
    tw = netBounds().width();
  bool should = p.x()-netBounds().left() > .75*tw;
  return should;
}
 
void TextItem::modeChange(Mode::M m) {
  Qt::CursorShape cs = defaultCursor();
  switch (m) {
  case Mode::Type:
    if (isWritable())
      cs = Qt::IBeamCursor;
    break;
  case Mode::Annotate:
    cs = Qt::CrossCursor;
    break;
  case Mode::MoveResize:
    if (mayMove) {
      if (shouldResize(cursorPos))
	cs = Qt::SplitHCursor;
      else
	cs = Qt::SizeAllCursor;
    }
    break;
  case Mode::Highlight: case Mode::Strikeout: case Mode::Plain:
    cs = Qt::UpArrowCursor;
    break;
  default:
    break;
  }
  text->setCursor(Cursors::refined(cs));
}

void TextItem::hoverMove(QGraphicsSceneHoverEvent *e) {
  cursorPos = e->pos(); // cache for the use of modifierChanged
  modeChange(mode()->mode());
  e->accept();
}

void TextItem::updateRefText(QString olds, QString news) {
  emit refTextChange(olds, news);
}

QRectF TextItem::boundingRect() const {
  return QRectF();
  //  return text->boundingRect();
}

void TextItem::paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) {
}

void TextItem::setBoxVisible(bool v) {
  text->setBoxVisible(v);
}

void TextItem::setTextWidth(double d) {
  text->setTextWidth(d);
  emit widthChanged();
}

void TextItem::insertBasicHtml(QString html, int pos) {
  QTextCursor c(document());
  c.setPosition(pos);
  QRegExp tag("<(.*)>");
  tag.setMinimal(true);
  QList<int> italicStarts;
  QList<int> boldStarts;
  while (!html.isEmpty()) {
    int idx = tag.indexIn(html);
    if (idx>=0) {
      QString cap = tag.cap(1);
      c.insertText(html.left(idx));
      html = html.mid(idx + tag.matchedLength());
      if (cap=="i") 
	italicStarts.append(c.position());
      else if (cap=="b")
	boldStarts.append(c.position());
      else if (cap=="/i" && !italicStarts.isEmpty()) 
	addMarkup(MarkupData::Italic, italicStarts.takeLast(), c.position());
      else if (cap=="/b" && !boldStarts.isEmpty()) 
	addMarkup(MarkupData::Bold, boldStarts.takeLast(), c.position());
    } else {
      c.insertText(html);
      break;
    }
  }
  while (!italicStarts.isEmpty())
    addMarkup(MarkupData::Italic, italicStarts.takeLast(), c.position());
  while (!boldStarts.isEmpty())
    addMarkup(MarkupData::Bold, boldStarts.takeLast(), c.position());
}

QRectF TextItem::netBounds() const {
  return text->mapRectToParent(text->boundingRect());
}

QRectF TextItem::clipRect() const {
  return clip_;
}

bool TextItem::clips() const {
  return !clip_.isNull();
}

void TextItem::setClip(QRectF r) {
  qDebug() << "TI::setClip " << r << boundingRect() << netBounds();
  clip_ = r;
  text->setClip(r);
}

void TextItem::unclip() {
  clip_ = QRectF();
  text->unclip();
}

  
