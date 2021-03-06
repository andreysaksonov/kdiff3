/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl <joachim.eibl at gmx.de>      *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef DIFF_H
#define DIFF_H

#include "common.h"
#include "fileaccess.h"
#include "LineRef.h"
#include "SourceData.h"
#include "Logging.h"

#include <QList>
#include <QVector>

class Options;

//These enums must be sequential with no gaps to allow loop interiation of values
enum e_SrcSelector
{
   Min = -1,
   Invalid=-1,
   None=0,
   A = 1,
   B = 2,
   C = 3,
   Max=C
};

enum e_MergeDetails
{
   eDefault,
   eNoChange,
   eBChanged,
   eCChanged,
   eBCChanged,         // conflict
   eBCChangedAndEqual, // possible conflict
   eBDeleted,
   eCDeleted,
   eBCDeleted,         // possible conflict

   eBChanged_CDeleted, // conflict
   eCChanged_BDeleted, // conflict
   eBAdded,
   eCAdded,
   eBCAdded,           // conflict
   eBCAddedAndEqual    // possible conflict
};

enum ChangeFlag{
    NoChange = 0,
    AChanged = 0x1,
    BChanged = 0x2,
    Both = AChanged | BChanged
};

Q_DECLARE_FLAGS(ChangeFlags, ChangeFlag);
Q_DECLARE_OPERATORS_FOR_FLAGS(ChangeFlags);

// Each range with matching elements is followed by a range with differences on either side.
// Then again range of matching elements should follow.
class Diff
{
  private:
    qint32 nofEquals = 0;

    qint64 mDiff1 = 0;
    qint64 mDiff2 = 0;
  public:
    Diff(qint32 eq, const qint64 inDiff1, const qint64 inDiff2)
    {
        nofEquals = eq;
        mDiff1 = inDiff1;
        mDiff2 = inDiff2;
    }

    inline qint32 numberOfEquals() const { return nofEquals; };

    inline qint64 diff1() const { return mDiff1; };
    inline qint64 diff2() const { return mDiff2; };

    inline void setNumberOfEquals(const qint32 inNumOfEquals) { nofEquals = inNumOfEquals; }

    inline void adjustNumberOfEquals(const qint64 delta) { nofEquals += delta; }
    inline void adjustDiff1(const qint64 delta) { mDiff1 += delta; }
    inline void adjustDiff2(const qint64 delta) { mDiff2 += delta; }
};

typedef std::list<Diff> DiffList;

class LineData
{
  private:
    QSharedPointer<QString> mBuffer;
    //QString pLine;
    QtNumberType mFirstNonWhiteChar = 0;
    qint64 mOffset = 0;
    QtNumberType mSize = 0;
    bool bContainsPureComment = false;//TODO: Move me

  public:
    explicit LineData() = default; // needed for Qt internal reasons should not be used.
    inline LineData(const QSharedPointer<QString> &buffer, const qint64 inOffset, QtNumberType inSize = 0, QtNumberType inFirstNonWhiteChar=0, bool inIsPureComment=false)
    {
        mBuffer = buffer;
        mOffset = inOffset;
        mSize = inSize;
        bContainsPureComment = inIsPureComment;
        mFirstNonWhiteChar = inFirstNonWhiteChar;
    }
    Q_REQUIRED_RESULT inline int size() const { return mSize; }
    Q_REQUIRED_RESULT inline qint32 getFirstNonWhiteChar() const { return mFirstNonWhiteChar; }

    /*
        QString::fromRawData allows us to create a light weight QString backed by the buffer memmory.
    */
    Q_REQUIRED_RESULT inline const QString getLine() const { return QString::fromRawData(mBuffer->data() + mOffset, mSize); }
    Q_REQUIRED_RESULT inline const QSharedPointer<QString>& getBuffer() const { return mBuffer; }

    Q_REQUIRED_RESULT inline qint64 getOffset() const { return mOffset; }
    Q_REQUIRED_RESULT int width(int tabSize) const; // Calcs width considering tabs.
    //int occurrences;
    inline bool whiteLine() const { return mFirstNonWhiteChar == mSize - 1; }

    inline bool isPureComment() const { return bContainsPureComment; }
    inline void setPureComment(const bool bPureComment) { bContainsPureComment = bPureComment; }

    static bool equal(const LineData& l1, const LineData& l2, bool bStrict);
};

class ManualDiffHelpList; // A list of corresponding ranges

class Diff3LineList;
class Diff3LineVector;

class DiffBufferInfo
{
  private:
    const QVector<LineData>* mLineDataA;
    const QVector<LineData>* mLineDataB;
    const QVector<LineData>* mLineDataC;

    LineCount m_sizeA;
    LineCount m_sizeB;
    LineCount m_sizeC;
    const Diff3LineList* m_pDiff3LineList;
    const Diff3LineVector* m_pDiff3LineVector;
public:
    void init(Diff3LineList* d3ll, const Diff3LineVector* d3lv,
              const QVector<LineData>* pldA, LineCount sizeA, const QVector<LineData>* pldB, LineCount sizeB, const QVector<LineData>* pldC, LineCount sizeC);

    inline const QVector<LineData>* getLineData(e_SrcSelector srcIndex) const
    {
        switch(srcIndex)
        {
            case A:
                return mLineDataA;
            case B:
                return mLineDataB;
            case C:
                return mLineDataC;
            default:
                return nullptr;
        }
    }
};

class Diff3Line
{
  private:
    friend class Diff3LineList;
    LineRef lineA;
    LineRef lineB;
    LineRef lineC;

    bool bAEqC = false; // These are true if equal or only white-space changes exist.
    bool bBEqC = false;
    bool bAEqB = false;

    bool bWhiteLineA = false;
    bool bWhiteLineB = false;
    bool bWhiteLineC  = false;

    DiffList* pFineAB = nullptr; // These are NULL only if completely equal or if either source doesn't exist.
    DiffList* pFineBC = nullptr;
    DiffList* pFineCA = nullptr;


    qint32 mLinesNeededForDisplay = 1;    // Due to wordwrap
    qint32 mSumLinesNeededForDisplay = 0; // For fast conversion to m_diff3WrapLineVector

  public:
    static QSharedPointer<DiffBufferInfo> m_pDiffBufferInfo; // For convenience

    ~Diff3Line()
    {
        if(pFineAB != nullptr) delete pFineAB;
        if(pFineBC != nullptr) delete pFineBC;
        if(pFineCA != nullptr) delete pFineCA;
        pFineAB = nullptr;
        pFineBC = nullptr;
        pFineCA = nullptr;
    }
    LineRef getLineA() const { return lineA; }
    LineRef getLineB() const { return lineB; }
    LineRef getLineC() const { return lineC; }

    inline void setLineA(const LineRef& line) { lineA = line; }
    inline void setLineB(const LineRef& line) { lineB = line; }
    inline void setLineC(const LineRef& line) { lineC = line; }

    inline bool isEqualAB() const { return bAEqB; }
    inline bool isEqualAC() const { return bAEqC; }
    inline bool isEqualBC() const { return bBEqC; }

    inline bool isWhiteLine(e_SrcSelector src) const
    {
        Q_ASSERT(src == A || src == B || src == C);

        switch(src)
        {
            case A:
                return bWhiteLineA;
            case B:
                return bWhiteLineB;
            case C:
                return bWhiteLineC;
            default:
                //should never get here
                Q_ASSERT(false);
                return false;
        }
    }

    bool operator==(const Diff3Line& d3l) const
    {
        return lineA == d3l.lineA && lineB == d3l.lineB && lineC == d3l.lineC && bAEqB == d3l.bAEqB && bAEqC == d3l.bAEqC && bBEqC == d3l.bBEqC;
    }

    const LineData* getLineData(e_SrcSelector src) const
    {
        Q_ASSERT(m_pDiffBufferInfo != nullptr);
        //Use at() here not [] to avoid using really weird syntax
        if(src == A && lineA.isValid()) return &m_pDiffBufferInfo->getLineData(src)->at(lineA);
        if(src == B && lineB.isValid()) return &m_pDiffBufferInfo->getLineData(src)->at(lineB);
        if(src == C && lineC.isValid()) return &m_pDiffBufferInfo->getLineData(src)->at(lineC);

        return nullptr;
    }
    const QString getString(const e_SrcSelector src) const
    {
        const LineData* pld = getLineData(src);
        if(pld)
            return pld->getLine();
        else
            return QString();
    }
    LineRef getLineInFile(e_SrcSelector src) const
    {
        if(src == A) return lineA;
        if(src == B) return lineB;
        if(src == C) return lineC;
        return -1;
    }

    inline qint32 sumLinesNeededForDisplay() const { return mSumLinesNeededForDisplay; }

    inline qint32 linesNeededForDisplay() const { return mLinesNeededForDisplay; }

    void setLinesNeeded(const qint32 lines) { mLinesNeededForDisplay = lines; }
    bool fineDiff(bool bTextsTotalEqual, const e_SrcSelector selector, const QVector<LineData>* v1, const QVector<LineData>* v2);
    void mergeOneLine(e_MergeDetails& mergeDetails, bool& bConflict, bool& bLineRemoved, e_SrcSelector& src, bool bTwoInputs) const;

    void getLineInfo(const e_SrcSelector winIdx, const bool isTriple, LineRef& lineIdx,
        DiffList*& pFineDiff1, DiffList*& pFineDiff2, // return values
        ChangeFlags& changed, ChangeFlags& changed2) const;
  private:
    void setFineDiff(const e_SrcSelector selector, DiffList* pDiffList)
    {
        Q_ASSERT(selector == A || selector == B || selector == C);
        if(selector == A)
        {
            if(pFineAB != nullptr)
                delete pFineAB;
            pFineAB = pDiffList;
        }
        else if(selector == B)
        {
            if(pFineBC != nullptr)
                delete pFineBC;
            pFineBC = pDiffList;
        }
        else if(selector == C)
        {
            if(pFineCA)
                delete pFineCA;
            pFineCA = pDiffList;
        }
    }
};

class Diff3LineList : public std::list<Diff3Line>
{
  public:
    bool fineDiff(const e_SrcSelector selector, const QVector<LineData>* v1, const QVector<LineData>* v2);
    void calcDiff3LineVector(Diff3LineVector& d3lv);
    void calcWhiteDiff3Lines(const QVector<LineData>* pldA, const QVector<LineData>* pldB, const QVector<LineData>* pldC);

    void calcDiff3LineListUsingAB(const DiffList* pDiffListAB);
    void calcDiff3LineListUsingAC(const DiffList* pDiffListAC);
    void calcDiff3LineListUsingBC(const DiffList* pDiffListBC);
    
    void correctManualDiffAlignment(ManualDiffHelpList* pManualDiffHelpList);

    void calcDiff3LineListTrim(const QVector<LineData>* pldA, const QVector<LineData>* pldB, const QVector<LineData>* pldC, ManualDiffHelpList* pManualDiffHelpList);


    LineCount recalcWordWrap(bool resetDisplayCount)
    {
        LineCount sumOfLines = 0;

        for(Diff3Line& d3l: *this)
        {
            if(resetDisplayCount)
                d3l.mLinesNeededForDisplay = 1;
            
            d3l.mSumLinesNeededForDisplay = sumOfLines;
            sumOfLines += d3l.linesNeededForDisplay();
        }
        
        return sumOfLines;
    }
    
    //TODO: Add safety guards to prevent list from getting too large. Same problem as with QLinkedList.
    qint32 size() const
    {
        if(std::list<Diff3Line>::size() > (size_t)TYPE_MAX(qint32))//explicit cast to silence gcc
        {
            qCDebug(kdiffMain) << "Diff3Line: List too large. size=" << std::list<Diff3Line>::size();
            Q_ASSERT(false); //Unsupported size
            return 0;
        }
        return (qint32)std::list<Diff3Line>::size();
    } //safe for small files same limit as exited with QLinkedList. This should ultimatly be removed.

    void debugLineCheck(const LineCount size, const e_SrcSelector srcSelector) const;

    qint32 numberOfLines(bool bWordWrap) const 
    {
        if(bWordWrap)
        {
            qint32 lines;

            lines = 0;
            Diff3LineList::const_iterator i;
            for(i = begin(); i != end(); ++i)
            {
                lines += i->linesNeededForDisplay();
            }

            return lines;
        }
        else
        {
            return size();
        }
    }
};

class Diff3LineVector : public QVector<Diff3Line*>
{
};

struct Diff3WrapLine
{
    Diff3Line* pD3L;
    int diff3LineIndex;
    int wrapLineOffset;
    int wrapLineLength;
};

typedef QVector<Diff3WrapLine> Diff3WrapLineVector;

class TotalDiffStatus
{
  public:
    inline void reset()
    {
        bBinaryAEqC = false;
        bBinaryBEqC = false;
        bBinaryAEqB = false;
        bTextAEqC = false;
        bTextBEqC = false;
        bTextAEqB = false;
        nofUnsolvedConflicts = 0;
        nofSolvedConflicts = 0;
        nofWhitespaceConflicts = 0;
    }

    inline int getUnsolvedConflicts() const { return nofUnsolvedConflicts; }
    inline void setUnsolvedConflicts(const int unsolved) { nofUnsolvedConflicts = unsolved; }

    inline int getSolvedConflicts() const { return nofSolvedConflicts; }
    inline void setSolvedConflicts(const int solved) { nofSolvedConflicts = solved; }

    inline int getWhitespaceConflicts() const { return nofWhitespaceConflicts; }
    inline void setWhitespaceConflicts(const int wintespace) { nofWhitespaceConflicts = wintespace; }

    inline int getNonWhitespaceConflicts() { return getUnsolvedConflicts() + getSolvedConflicts() - getWhitespaceConflicts(); }

    bool isBinaryEqualAC() const { return bBinaryAEqC; }
    bool isBinaryEqualBC() const { return bBinaryBEqC; }
    bool isBinaryEqualAB() const { return bBinaryAEqB; }

    bool bBinaryAEqC = false;
    bool bBinaryBEqC = false;
    bool bBinaryAEqB = false;

    bool bTextAEqC = false;
    bool bTextBEqC = false;
    bool bTextAEqB = false;

  private:
    int nofUnsolvedConflicts = 0;
    int nofSolvedConflicts = 0;
    int nofWhitespaceConflicts = 0;
};

// Three corresponding ranges. (Minimum size of a valid range is one line.)
class ManualDiffHelpEntry
{
  private:
    LineRef lineA1;
    LineRef lineA2;
    LineRef lineB1;
    LineRef lineB2;
    LineRef lineC1;
    LineRef lineC2;

  public:
    LineRef& firstLine(e_SrcSelector winIdx)
    {
        return winIdx == A ? lineA1 : (winIdx == B ? lineB1 : lineC1);
    }
    LineRef& lastLine(e_SrcSelector winIdx)
    {
        return winIdx == A ? lineA2 : (winIdx == B ? lineB2 : lineC2);
    }
    bool isLineInRange(LineRef line, e_SrcSelector winIdx)
    {
        return line.isValid() && line >= firstLine(winIdx) && line <= lastLine(winIdx);
    }
    bool operator==(const ManualDiffHelpEntry& r) const
    {
        return lineA1 == r.lineA1 && lineB1 == r.lineB1 && lineC1 == r.lineC1 &&
               lineA2 == r.lineA2 && lineB2 == r.lineB2 && lineC2 == r.lineC2;
    }

    int calcManualDiffFirstDiff3LineIdx(const Diff3LineVector& d3lv);

    void getRangeForUI(const e_SrcSelector winIdx, LineRef *rangeLine1, LineRef *rangeLine2) const {
        if(winIdx == A) {
            *rangeLine1 = lineA1;
            *rangeLine2 = lineA2;
        }
        if(winIdx == B) {
            *rangeLine1 = lineB1;
            *rangeLine2 = lineB2;
        }
        if(winIdx == C) {
            *rangeLine1 = lineC1;
            *rangeLine2 = lineC2;
        }
    }

    inline const LineRef& getLine1(const e_SrcSelector winIdx) const { return winIdx == A ? lineA1 : winIdx == B ? lineB1 : lineC1;}
    inline const LineRef& getLine2(const e_SrcSelector winIdx) const { return winIdx == A ? lineA2 : winIdx == B ? lineB2 : lineC2;}
    bool isValidMove(int line1, int line2, e_SrcSelector winIdx1, e_SrcSelector winIdx2) const;
};

// A list of corresponding ranges
class ManualDiffHelpList: public std::list<ManualDiffHelpEntry>
{
    public:
        bool isValidMove(int line1, int line2, e_SrcSelector winIdx1, e_SrcSelector winIdx2) const;
        void insertEntry(e_SrcSelector winIdx, LineRef firstLine, LineRef lastLine);

        bool runDiff(const QVector<LineData>* p1, LineRef size1, const QVector<LineData>* p2, LineRef size2, DiffList& diffList,
                     e_SrcSelector winIdx1, e_SrcSelector winIdx2,
                     const QSharedPointer<Options> &pOptions);
};

void calcDiff(const QString &line1, const QString &line2, DiffList& diffList, int match, int maxSearchRange);


bool fineDiff(
    Diff3LineList& diff3LineList,
    int selector,
    const QVector<LineData>* v1,
    const QVector<LineData>* v2);

inline bool isWhite(QChar c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

/** Returns the number of equivalent spaces at position outPos.
*/
inline int tabber(int outPos, int tabSize)
{
    return tabSize - (outPos % tabSize);
}

/** Returns a line number where the linerange [line, line+nofLines] can
    be displayed best. If it fits into the currently visible range then
    the returned value is the current firstLine.
*/
int getBestFirstLine(int line, int nofLines, int firstLine, int visibleLines);

extern bool g_bIgnoreWhiteSpace;
extern bool g_bIgnoreTrivialMatches;
extern bool g_bAutoSolve;

enum e_CoordType
{
    eFileCoords,
    eD3LLineCoords,
    eWrapCoords
};

QString calcHistorySortKey(const QString& keyOrder, QRegExp& matchedRegExpr, const QStringList& parenthesesGroupList);
bool findParenthesesGroups(const QString& s, QStringList& sl);
#endif
