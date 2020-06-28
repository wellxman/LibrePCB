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

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "netsegmentsplitter.h"

#include <librepcb/common/toolbox.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace project {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

NetSegmentSplitter::NetSegmentSplitter() noexcept
  : mAnchors(), mLines(), mLabels() {
}

NetSegmentSplitter::~NetSegmentSplitter() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

QList<NetSegmentSplitter::Segment> NetSegmentSplitter::split() const noexcept {
  QList<Segment> segments;

  // Split netsegment by anchors and lines
  QList<Anchor>  availableAnchors = mAnchors;
  QList<NetLine> availableLines   = mLines;
  while (!availableAnchors.isEmpty()) {
    Segment segment;
    findConnectedElements(availableAnchors.first(), availableAnchors,
                          availableLines, segment.anchors, segment.lines);
    segments.append(segment);
  }
  Q_ASSERT(availableAnchors.isEmpty() && availableLines.isEmpty());

  // Add netlabels to their nearest netsegment, but only if there exists at
  // least one netsegment
  if (!segments.isEmpty()) {
    foreach (const NetLabel& label, mLabels) {
      addNetLabelToNearestNetSegment(label, segments);
    }
  }

  return segments;
}

void NetSegmentSplitter::findConnectedElements(
    Anchor anchor, QList<Anchor>& availableAnchors,
    QList<NetLine>& availableLines, QList<Anchor>& connectedAnchors,
    QList<NetLine>& connectedLines) const noexcept {
  Q_ASSERT(availableAnchors.contains(anchor));
  availableAnchors.removeOne(anchor);

  Q_ASSERT(!connectedAnchors.contains(anchor));
  connectedAnchors.append(anchor);

  foreach (const NetLine& line, mLines) {
    if (((line.startAnchor == anchor.id) || (line.endAnchor == anchor.id)) &&
        (availableLines.contains(line)) && (!connectedLines.contains(line))) {
      connectedLines.append(line);
      availableLines.removeOne(line);
      QVariant otherAnchorId =
          (line.startAnchor == anchor.id) ? line.endAnchor : line.startAnchor;
      foreach (const Anchor& otherAnchor, mAnchors) {
        if ((otherAnchor.id == otherAnchorId) &&
            (availableAnchors.contains(otherAnchor)) &&
            (!connectedAnchors.contains(otherAnchor))) {
          findConnectedElements(otherAnchor, availableAnchors, availableLines,
                                connectedAnchors, connectedLines);
        }
      }
    }
  }
}

void NetSegmentSplitter::addNetLabelToNearestNetSegment(
    const NetLabel& label, QList<Segment>& segments) noexcept {
  int    nearestIndex = -1;
  Length nearestDistance;
  for (int i = 0; i < segments.count(); ++i) {
    Length distance =
        getDistanceBetweenNetLabelAndNetSegment(label, segments.at(i));
    if ((distance < nearestDistance) || (nearestIndex < 0)) {
      nearestIndex    = i;
      nearestDistance = distance;
    }
  }
  Q_ASSERT(nearestIndex >= 0);
  segments[nearestIndex].labels.append(label);
}

Length NetSegmentSplitter::getDistanceBetweenNetLabelAndNetSegment(
    const NetLabel& label, const Segment& segment) noexcept {
  bool   firstRun = true;
  Length nearestDistance;
  foreach (const Anchor& anchor, segment.anchors) {
    UnsignedLength distance = (anchor.position - label.position).getLength();
    if ((distance < nearestDistance) || firstRun) {
      nearestDistance = *distance;
      firstRun        = false;
    }
  }
  foreach (const NetLine& line, segment.lines) {
    Point start, end;
    foreach (const Anchor& anchor, segment.anchors) {
      if (anchor.id == line.startAnchor) {
        start = anchor.position;
      }
      if (anchor.id == line.endAnchor) {
        end = anchor.position;
      }
    }
    UnsignedLength distance = Toolbox::shortestDistanceBetweenPointAndLine(
        label.position, start, end);
    if ((distance < nearestDistance) || firstRun) {
      nearestDistance = *distance;
      firstRun        = false;
    }
  }
  Q_ASSERT(firstRun == false);
  return nearestDistance;
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace project
}  // namespace librepcb
