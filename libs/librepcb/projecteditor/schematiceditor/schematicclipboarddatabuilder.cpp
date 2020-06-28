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
#include "schematicclipboarddatabuilder.h"

#include "../netsegmentsplitter.h"

#include <librepcb/common/fileio/transactionalfilesystem.h>
#include <librepcb/library/cmp/component.h>
#include <librepcb/library/sym/symbol.h>
#include <librepcb/project/circuit/circuit.h>
#include <librepcb/project/circuit/componentinstance.h>
#include <librepcb/project/circuit/componentsignalinstance.h>
#include <librepcb/project/circuit/netsignal.h>
#include <librepcb/project/project.h>
#include <librepcb/project/schematics/items/si_netlabel.h>
#include <librepcb/project/schematics/items/si_netpoint.h>
#include <librepcb/project/schematics/items/si_netsegment.h>
#include <librepcb/project/schematics/items/si_symbol.h>
#include <librepcb/project/schematics/items/si_symbolpin.h>
#include <librepcb/project/schematics/schematic.h>
#include <librepcb/project/schematics/schematicselectionquery.h>

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

SchematicClipboardDataBuilder::SchematicClipboardDataBuilder(
    Schematic& schematic) noexcept
  : mSchematic(schematic) {
}

SchematicClipboardDataBuilder::~SchematicClipboardDataBuilder() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

std::unique_ptr<SchematicClipboardData> SchematicClipboardDataBuilder::generate(
    const Point& cursorPos) const noexcept {
  std::unique_ptr<SchematicClipboardData> data(
      new SchematicClipboardData(mSchematic.getUuid(), cursorPos));

  // get all selected items
  std::unique_ptr<SchematicSelectionQuery> query(
      mSchematic.createSelectionQuery());
  query->addSelectedSymbols();
  query->addSelectedNetLines();
  query->addSelectedNetLabels();
  // query->addNetPointsOfNetLines();

  // add components
  foreach (SI_Symbol* symbol, query->getSymbols()) {
    std::unique_ptr<TransactionalDirectory> dir = data->getDirectory(
        "cmp/" %
        symbol->getComponentInstance().getLibComponent().getUuid().toStr());
    if (dir->getFiles().isEmpty()) {
      symbol->getComponentInstance().getLibComponent().getDirectory().copyTo(
          *dir);
    }
    data->getComponentInstances().append(
        std::make_shared<SchematicClipboardData::ComponentInstance>(
            symbol->getComponentInstance().getUuid(),
            symbol->getComponentInstance().getLibComponent().getUuid(),
            symbol->getComponentInstance().getSymbolVariant().getUuid(),
            symbol->getComponentInstance().getDefaultDeviceUuid(),
            symbol->getComponentInstance().getName(),
            symbol->getComponentInstance().getValue(),
            symbol->getComponentInstance().getAttributes()));
  }

  // add symbols
  foreach (SI_Symbol* symbol, query->getSymbols()) {
    std::unique_ptr<TransactionalDirectory> dir =
        data->getDirectory("sym/" % symbol->getLibSymbol().getUuid().toStr());
    if (dir->getFiles().isEmpty()) {
      symbol->getLibSymbol().getDirectory().copyTo(*dir);
    }
    data->getSymbolInstances().append(
        std::make_shared<SchematicClipboardData::SymbolInstance>(
            symbol->getUuid(), symbol->getComponentInstance().getUuid(),
            symbol->getCompSymbVarItem().getUuid(), symbol->getPosition(),
            symbol->getRotation(), symbol->getMirrored()));
  }

  // add (splitted) net segments including netpoints, netlines and netlabels
  foreach (SI_NetSegment* netsegment, mSchematic.getNetSegments()) {
    NetSegmentSplitter splitter;
    foreach (SI_NetLine* netline, query->getNetLines()) {
      if (&netline->getNetSegment() != netsegment) {
        continue;
      }
      splitter.addAnchor(
          QVariant::fromValue(static_cast<void*>(&netline->getStartPoint())),
          netline->getStartPoint().getPosition());
      splitter.addAnchor(
          QVariant::fromValue(static_cast<void*>(&netline->getEndPoint())),
          netline->getEndPoint().getPosition());
      splitter.addNetLine(
          QVariant::fromValue(static_cast<void*>(netline)),
          QVariant::fromValue(static_cast<void*>(&netline->getStartPoint())),
          QVariant::fromValue(static_cast<void*>(&netline->getEndPoint())));
    }
    foreach (SI_NetLabel* netlabel, query->getNetLabels()) {
      if (&netlabel->getNetSegment() != netsegment) {
        continue;
      }
      splitter.addNetLabel(QVariant::fromValue(static_cast<void*>(netlabel)),
                           netlabel->getPosition());
    }

    QList<NetSegmentSplitter::Segment> splitSegments = splitter.split();
    foreach (const NetSegmentSplitter::Segment& splitSegment, splitSegments) {
      std::shared_ptr<SchematicClipboardData::NetSegment> newSegment =
          std::make_shared<SchematicClipboardData::NetSegment>(
              netsegment->getNetSignal().getName());
      data->getNetSegments().append(newSegment);

      QHash<SI_SymbolPin*, std::shared_ptr<SchematicClipboardData::NetPoint>>
          replacedPins;
      foreach (const NetSegmentSplitter::Anchor& anchor, splitSegment.anchors) {
        SI_NetLineAnchor* pAnchor =
            static_cast<SI_NetLineAnchor*>(anchor.id.value<void*>());
        if (SI_NetPoint* np = dynamic_cast<SI_NetPoint*>(pAnchor)) {
          newSegment->points.append(
              std::make_shared<SchematicClipboardData::NetPoint>(
                  np->getUuid(), np->getPosition()));
        } else if (SI_SymbolPin* pin = dynamic_cast<SI_SymbolPin*>(pAnchor)) {
          if (!query->getSymbols().contains(&pin->getSymbol())) {
            // Symbol will not be copied, thus replacing the pin by a netpoint
            std::shared_ptr<SchematicClipboardData::NetPoint> np =
                std::make_shared<SchematicClipboardData::NetPoint>(
                    Uuid::createRandom(), pin->getPosition());
            replacedPins.insert(pin, np);
            newSegment->points.append(np);
          }
        }
      }
      foreach (const NetSegmentSplitter::NetLine& line, splitSegment.lines) {
        SI_NetLine* nl = static_cast<SI_NetLine*>(line.id.value<void*>());
        std::shared_ptr<SchematicClipboardData::NetLine> copy =
            std::make_shared<SchematicClipboardData::NetLine>(nl->getUuid());
        if (SI_NetPoint* netpoint =
                dynamic_cast<SI_NetPoint*>(&nl->getStartPoint())) {
          copy->startJunction = netpoint->getUuid();
        } else if (SI_SymbolPin* pin =
                       dynamic_cast<SI_SymbolPin*>(&nl->getStartPoint())) {
          if (auto np = replacedPins.value(pin)) {
            copy->startJunction = np->uuid;
          } else {
            copy->startSymbol = pin->getSymbol().getUuid();
            copy->startPin    = pin->getLibPinUuid();
          }
        } else {
          Q_ASSERT(false);
        }
        if (SI_NetPoint* netpoint =
                dynamic_cast<SI_NetPoint*>(&nl->getEndPoint())) {
          copy->endJunction = netpoint->getUuid();
        } else if (SI_SymbolPin* pin =
                       dynamic_cast<SI_SymbolPin*>(&nl->getEndPoint())) {
          if (auto np = replacedPins.value(pin)) {
            copy->endJunction = np->uuid;
          } else {
            copy->endSymbol = pin->getSymbol().getUuid();
            copy->endPin    = pin->getLibPinUuid();
          }
        } else {
          Q_ASSERT(false);
        }
        newSegment->lines.append(copy);
      }
      foreach (const NetSegmentSplitter::NetLabel& label, splitSegment.labels) {
        SI_NetLabel* nl = static_cast<SI_NetLabel*>(label.id.value<void*>());
        newSegment->labels.append(
            std::make_shared<SchematicClipboardData::NetLabel>(
                nl->getUuid(), nl->getPosition(), nl->getRotation()));
      }
    }
  }

  return data;
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace project
}  // namespace librepcb
