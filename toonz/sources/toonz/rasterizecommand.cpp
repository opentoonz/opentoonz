
// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "selectionutils.h"
#include "convertpopup.h"

// TnzQt includes
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tcamera.h"

// TnzCore includes
#include "tsystem.h"
#include "ttoonzimage.h"

// Tnz6 includes
#include "drawingdata.h"
#include "xsheetviewer.h"

// TnzTools includes
#include "tools/toolhandle.h"

// TnzQt includes
#include "toonzqt/icongenerator.h"
#include "toonzqt/menubarcommand.h"

// TnzLib includes
#include "toonz/preferences.h"
#include "toonz/tcolumnhandle.h"

// TnzCore includes
#include "filebrowsermodel.h"

#include <QCoreApplication>

using namespace DVGui;
using namespace SelectionUtils;

// Convert to ToonzRaster From Raster or Vector

namespace {
        
    // CreateLevelUndo
    class CreateLevelUndo final : public TUndo {
        int m_rowIndex;
        int m_columnIndex;
        int m_frameCount;
        int m_oldLevelCount;
        int m_step;
        TXshSimpleLevelP m_sl;
        bool m_areColumnsShifted;
        bool m_keepLevel;

    public:
        CreateLevelUndo(int row, int column, int frameCount, int step,
            bool areColumnsShifted, bool keepLevel = false)
            : m_rowIndex(row)
            , m_columnIndex(column)
            , m_frameCount(frameCount)
            , m_step(step)
            , m_sl(0)
            , m_keepLevel(keepLevel)
            , m_areColumnsShifted(areColumnsShifted) {
            TApp* app = TApp::instance();
            ToonzScene* scene = app->getCurrentScene()->getScene();
            m_oldLevelCount = scene->getLevelSet()->getLevelCount();
        }
        ~CreateLevelUndo() { m_sl = 0; }

        void onAdd(TXshSimpleLevelP sl) { m_sl = sl; }

        void undo() const override {
            TApp* app = TApp::instance();
            ToonzScene* scene = app->getCurrentScene()->getScene();
            TXsheet* xsh = scene->getXsheet();
            if (m_areColumnsShifted)
                xsh->removeColumn(m_columnIndex);
            else if (m_frameCount > 0)
                xsh->removeCells(m_rowIndex, m_columnIndex, m_frameCount);
            if (!m_keepLevel) {
                TLevelSet* levelSet = scene->getLevelSet();
                if (levelSet) {
                    int m = levelSet->getLevelCount();
                    while (m > 0 && m > m_oldLevelCount) {
                        --m;
                        TXshLevel* level = levelSet->getLevel(m);
                        if (level) levelSet->removeLevel(level);
                    }
                }
            }
            app->getCurrentScene()->notifySceneChanged();
            app->getCurrentScene()->notifyCastChange();
            app->getCurrentXsheet()->notifyXsheetChanged();
        }

        void redo() const override {
            if (!m_sl.getPointer()) return;
            TApp* app = TApp::instance();
            ToonzScene* scene = app->getCurrentScene()->getScene();
            scene->getLevelSet()->insertLevel(m_sl.getPointer());
            TXsheet* xsh = scene->getXsheet();
            if (m_areColumnsShifted) xsh->insertColumn(m_columnIndex);
            std::vector<TFrameId> fids;
            m_sl->getFids(fids);
            int i = m_rowIndex;
            int f = 0;
            while (i < m_frameCount + m_rowIndex) {
                TFrameId fid = (fids.size() != 0) ? fids[f] : i;
                TXshCell cell(m_sl.getPointer(), fid);
                f++;
                xsh->setCell(i, m_columnIndex, cell);
                int appo = i++;
                while (i < m_step + appo) xsh->setCell(i++, m_columnIndex, cell);
            }
            app->getCurrentScene()->notifySceneChanged();
            app->getCurrentScene()->notifyCastChange();
            app->getCurrentXsheet()->notifyXsheetChanged();
        }

        int getSize() const override { return sizeof * this; }
        QString getHistoryString() override {
            return QObject::tr("Create Level %1  at Column %2")
                .arg(QString::fromStdWString(m_sl->getName()))
                .arg(QString::number(m_columnIndex + 1));
        }
    };

    static TXshSimpleLevel* getNewToonzRasterLevel(
        TXshSimpleLevel* sourceSl) {
        ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
        TFilePath sourcePath = sourceSl->getPath();
        std::wstring sourceName = sourcePath.getWideName();
        TFilePath parentDir = sourceSl->getPath().getParentDir();
        TFilePath fp = scene->getDefaultLevelPath(TZP_XSHLEVEL, sourceName)
            .withParentDir(parentDir);
        TFilePath actualFp = scene->decodeFilePath(fp);

        int i = 1;
        std::wstring newName = sourceName;
        while (TSystem::doesExistFileOrLevel(actualFp)) {
            newName = sourceName + QString::number(i).toStdWString();
            fp = scene->getDefaultLevelPath(TZP_XSHLEVEL, newName)
                .withParentDir(parentDir);
            actualFp = scene->decodeFilePath(fp);
            i++;
        }
        parentDir = scene->decodeFilePath(parentDir);

        TXshLevel* level =
            scene->createNewLevel(TZP_XSHLEVEL, newName, TDimension(), 0, fp);
        TXshSimpleLevel* sl = dynamic_cast<TXshSimpleLevel*>(level);
        return sl;
    }

    bool convertVector(TXshSimpleLevel* in, TXshSimpleLevel* out
        , std::set<TFrameId>& frameIdsSet) {
        assert(in->getType() == TXshLevelType::PLI_XSHLEVEL);
        // get camera settings
        TApp* app = TApp::instance();
        TCamera* camera = app->getCurrentScene()->getScene()->getCurrentCamera();
        double dpi = camera->getDpi().x;
        int xres = camera->getRes().lx;
        int yres = camera->getRes().ly;

        in->getProperties()->setDpiPolicy(LevelProperties::DP_ImageDpi);
        in->getProperties()->setDpi(dpi);
        in->getProperties()->setImageDpi(TPointD(dpi, dpi));
        in->getProperties()->setImageRes(TDimension(xres, yres));

        std::vector<TFrameId> frameIds;
        in->getFids(frameIds);
        frameIdsSet = std::set<TFrameId>(frameIds.begin(), frameIds.end());
        DrawingData* data = new DrawingData();
        data->setLevelFrames(in, frameIdsSet);

        // This is where the copying actually happens
        for (auto id : frameIdsSet) {
            TRasterCM32P raster(xres, yres);
            raster->fill(TPixelCM32());
            TToonzImageP firstImage(raster, TRect());
            firstImage->setDpi(dpi, dpi);
            out->setFrame(id, firstImage);
            firstImage->setSavebox(TRect(0, 0, xres - 1, yres - 1));
        }

        bool keepOriginalPalette = false;
        bool success = data->getLevelFrames(
            out, frameIdsSet, DrawingData::OVER_SELECTION, true, keepOriginalPalette,
            true);  // setting is redo = true skips the question about the palette
        return success;
    }

    bool convertRaster(TXshSimpleLevel* in, TXshSimpleLevel* out,
        std::set<TFrameId>& frameIdsSet)
    {
        ConvertPopup popup(false);
        TFilePath path = 
            TApp::instance()->getCurrentScene()->getScene()->decodeFilePath(in->getPath());
        if (!TSystem::doesExistFileOrLevel(path))return false;
        in->save();
        popup.setFiles({path});
        popup.setFormat("tlv");
        popup.show();
        popup.adjustSize();
        while (popup.isVisible() || popup.isConverting())
            QCoreApplication::processEvents(QEventLoop::AllEvents |
                QEventLoop::WaitForMoreEvents);
        path = popup.getConvetedPath(path);
        if (path.isEmpty()) return false;
        out->setPath(path);
        out->load();
        std::vector<TFrameId> fids = out->getFids();
        frameIdsSet = std::set<TFrameId>(fids.begin(), fids.end());
        return true;
    }


    //-----------------------------------------------------------------------------
    // Convert from Vector/Raster to ToonzRaster
    void exec() {
        // set up basics
        TApp* app = TApp::instance();
        int row = app->getCurrentFrame()->getFrame();
        int col = app->getCurrentColumn()->getColumnIndex();
        int i;

        ToonzScene* scene = app->getCurrentScene()->getScene();
        TXsheet* xsh = scene->getXsheet();
        int r0, c0, r1, c1;
        std::vector<TXshLevel*> levels;
        bool isCellSelection = getSelectedLevels(levels, r0, c0, r1, c1);
        if (levels.empty()) return;

        int newIndexColumn = c1 + 1;
        TUndoManager::manager()->beginBlock();
        for (TXshLevel* const level : levels) {
            int type = level->getType();
            if (type != TXshLevelType::PLI_XSHLEVEL &&
                type != TXshLevelType::OVL_XSHLEVEL) continue;

            TXshSimpleLevel* sourceSl = level->getSimpleLevel();
            TXshSimpleLevel* rsl = getNewToonzRasterLevel(sourceSl);
            assert(rsl);

            std::set<TFrameId> frameIdsSet;
            bool success =
                (type == TXshLevelType::PLI_XSHLEVEL) ?
                convertVector(sourceSl, rsl, frameIdsSet) :
                convertRaster(sourceSl, rsl, frameIdsSet);
            if (!success) continue;

            int totalImages = frameIdsSet.size();

            // expose the new frames
            if (isCellSelection) {
                xsh->insertColumn(newIndexColumn);

                int r, c;
                bool foundColumn = false;
                for (c = c0; c <= c1 && !foundColumn; c++) {
                    for (r = r0; r <= r1; r++) {
                        TXshCell cell = xsh->getCell(r, c);
                        TXshSimpleLevel* level =
                            (!cell.isEmpty()) ? cell.getSimpleLevel() : 0;
                        if (level != sourceSl) continue;
                        foundColumn = true;
                        TFrameId curFid = cell.getFrameId();
                        for (auto const& fid : frameIdsSet) {
                            if (fid.getNumber() ==
                                curFid.getNumber() ||  // Hanno stesso numero di frame
                                (fid.getNumber() == 1 &&
                                    curFid.getNumber() ==
                                    -2))  // La vecchia cella non ha numero di frame
                                xsh->setCell(r, newIndexColumn, TXshCell(rsl, fid));
                        }
                    }
                }
                newIndexColumn += 1;

                CreateLevelUndo* undo = new CreateLevelUndo(row, newIndexColumn, totalImages, 1, true);
                TUndoManager::manager()->add(undo);


                invalidateIcons(rsl, frameIdsSet);
                rsl->save(rsl->getPath(), TFilePath(), true);

                DvDirModel::instance()->refreshFolder(rsl->getPath().getParentDir());

                undo->onAdd(rsl);
            }
            else {
                std::vector<TFrameId> gomi;
                scene->getXsheet()->exposeLevel(
                    0, scene->getXsheet()->getFirstFreeColumnIndex(), rsl, gomi);
            }
        }
        TUndoManager::manager()->endBlock();

        app->getCurrentScene()->notifySceneChanged();
        app->getCurrentScene()->notifyCastChange();
        app->getCurrentXsheet()->notifyXsheetChanged();

        app->getCurrentTool()->onImageChanged(
            (TImage::Type)app->getCurrentImageType());
    }
}//namespace

//*****************************************************************************
//    RasterizerPopupCommand instantiation
//*****************************************************************************

class RasterizerCommandHandler final : public MenuItemHandler {
public:
    RasterizerCommandHandler() : MenuItemHandler(MI_ConvertToToonzRaster) {};
    void execute() override { ::exec(); };
} rasterizerCommandHandler;