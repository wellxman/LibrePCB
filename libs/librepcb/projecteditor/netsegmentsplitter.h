/*
 * LibrePCB - Professional EDA for everyone!
 * Copyright (C) 2013 LibrePCB Developers, see AUTHORS.md for contributors.
 * https://librepcb.org/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBREPCB_PROJECT_EDITOR_NETSEGMENTSPLITTER_H
#define LIBREPCB_PROJECT_EDITOR_NETSEGMENTSPLITTER_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include <librepcb/common/units/all_length_units.h>
#include <librepcb/common/uuid.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace project {
namespace editor {

/*******************************************************************************
 *  Class NetSegmentSplitter
 ******************************************************************************/

/**
 * @brief The NetSegmentSplitter class
 */
class NetSegmentSplitter final {
public:
  // Types
  struct Anchor {
    QVariant id;
    Point    position;
    bool operator==(const Anchor& rhs) const noexcept { return id == rhs.id; }
  };
  struct NetLine {
    QVariant id;
    QVariant startAnchor;
    QVariant endAnchor;
    bool operator==(const NetLine& rhs) const noexcept { return id == rhs.id; }
  };
  struct NetLabel {
    QVariant id;
    Point    position;
    bool operator==(const NetLine& rhs) const noexcept { return id == rhs.id; }
  };
  struct Segment {
    QList<Anchor>   anchors;
    QList<NetLine>  lines;
    QList<NetLabel> labels;
  };

  // Constructors / Destructor
  NetSegmentSplitter() noexcept;
  NetSegmentSplitter(const NetSegmentSplitter& other) = delete;
  ~NetSegmentSplitter() noexcept;

  // General Methods
  void addAnchor(const QVariant& id, const Point& position) noexcept {
    Anchor anchor{id, position};
    if (!mAnchors.contains(anchor)) {
      mAnchors.append(anchor);
    }
  }
  void addNetLine(const QVariant& id, const QVariant& startAnchor,
                  const QVariant& endAnchor) noexcept {
    mLines.append(NetLine{id, startAnchor, endAnchor});
  }
  void addNetLabel(const QVariant& id, const Point& position) noexcept {
    mLabels.append(NetLabel{id, position});
  }
  QList<Segment> split() const noexcept;

  // Operator Overloadings
  NetSegmentSplitter& operator=(const NetSegmentSplitter& rhs) = delete;

private:  // Methods
  void findConnectedElements(Anchor anchor, QList<Anchor>& availableAnchors,
                             QList<NetLine>& availableLines,
                             QList<Anchor>&  connectedAnchors,
                             QList<NetLine>& connectedLines) const noexcept;
  static void   addNetLabelToNearestNetSegment(const NetLabel& label,
                                               QList<Segment>& segments) noexcept;
  static Length getDistanceBetweenNetLabelAndNetSegment(
      const NetLabel& label, const Segment& segment) noexcept;

private:  // Data
  QList<Anchor>   mAnchors;
  QList<NetLine>  mLines;
  QList<NetLabel> mLabels;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace project
}  // namespace librepcb

#endif
