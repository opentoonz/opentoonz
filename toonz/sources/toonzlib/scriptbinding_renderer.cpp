

#include "toonz/scriptbinding_renderer.h"
#include "toonz/scriptbinding_scene.h"
#include "toonz/scriptbinding_level.h"
#include "toonz/txsheet.h"
#include "toonz/txshsimplelevel.h"

#include "toonz/toonzscene.h"
#include "trenderer.h"
#include "toonz/scenefx.h"
#include "toonz/sceneproperties.h"
#include "toonz/tcamera.h"
#include "toutputproperties.h"
#include <QEventLoop>
#include <QWaitCondition>

#include "timagecache.h"

namespace TScriptBinding {

//=======================================================

static QScriptValue getScene(QScriptContext *context,
                             const QScriptValue &sceneArg, Scene *&scene) {
  scene = qscriptvalue_cast<Scene *>(sceneArg);
  if (!scene)
    return context->throwError(
        QObject::tr("First argument must be a scene : %1")
            .arg(sceneArg.toString()));
  if (scene->getToonzScene() == 0)
    return context->throwError(QObject::tr("Can't render empty scene"));
  return QScriptValue();
}

static void valueToIntList(const QScriptValue &arr, QList<int> &list) {
  list.clear();
  if (arr.isArray()) {
    int length = arr.property("length").toInteger();
    for (int i = 0; i < length; i++) list.append(arr.property(i).toInteger());
  }
}

//=======================================================

class Renderer::Imp final : public TRenderPort {
public:
  TScriptBinding::Image *m_outputImage;
  TScriptBinding::Level *m_outputLevel;
  TPointD m_cameraDpi;

  bool m_completed;
  TRenderer m_renderer;

  QList<int> m_columnList;
  QList<int> m_frameList;

  Imp() : m_completed(false) {
    m_renderer.setThreadsCount(1);
    m_renderer.addPort(this);
    m_outputImage = 0;
    m_outputLevel = 0;
  }

  ~Imp() {}

  void setRenderArea(ToonzScene *scene) {
    TDimension cameraRes = scene->getCurrentCamera()->getRes();
    double rx = cameraRes.lx * 0.5, ry = cameraRes.ly * 0.5;
    TRectD renderArea(-rx, -ry, rx, ry);
    TRenderPort::setRenderArea(renderArea);
    m_cameraDpi = scene->getCurrentCamera()->getDpi();
  }

  void enableColumns(ToonzScene *scene, QList<bool> &oldStatus) {
    if (m_columnList.empty()) return;
    QList<bool> newStatus;
    TXsheet *xsh = scene->getXsheet();
    for (int i = 0; i < xsh->getColumnCount(); i++) {
      oldStatus.append(xsh->getColumn(i)->isPreviewVisible());
      newStatus.append(false);
    }
    for (int i : m_columnList) {
      if (0 <= i && i < xsh->getColumnCount()) newStatus[i] = true;
    }
    for (int i = 0; i < newStatus.length(); i++) {
      xsh->getColumn(i)->setPreviewVisible(newStatus[i]);
    }
  }

  void restoreColumns(ToonzScene *scene, const QList<bool> &oldStatus) {
    TXsheet *xsh = scene->getXsheet();
    for (int i = 0; i < oldStatus.length(); i++) {
      xsh->getColumn(i)->setPreviewVisible(oldStatus[i]);
    }
  }

  std::vector<TRenderer::RenderData> *makeRenderData(
      ToonzScene *scene, const std::vector<int> &rows) {
    TRenderSettings settings =
        scene->getProperties()->getOutputProperties()->getRenderSettings();

    QList<bool> oldColumnStates;
    enableColumns(scene, oldColumnStates);

    std::vector<TRenderer::RenderData> *rds =
        new std::vector<TRenderer::RenderData>;
    for (int i = 0; i < (int)rows.size(); i++) {
      double frame = rows[i];
      TFxP sceneFx = buildSceneFx(scene, frame, 1, false);
      TFxPair fxPair;
      fxPair.m_frameA = sceneFx;
      rds->push_back(TRenderer::RenderData(frame, settings, fxPair));
    }

    restoreColumns(scene, oldColumnStates);
    return rds;
  }

  void render(std::vector<TRenderer::RenderData> *rds) {
    QMutex mutex;
    mutex.lock();
    m_completed = false;
    m_renderer.startRendering(rds);

    while (!m_completed) {
      QEventLoop loop;
      loop.processEvents();
      QWaitCondition waitCondition;
      waitCondition.wait(&mutex, 100);
    }
    mutex.unlock();
  }

  void renderFrame(ToonzScene *scene, int row, Image *outputImage) {
    setRenderArea(scene);
    std::vector<int> rows;
    rows.push_back(row);
    m_outputImage = outputImage;
    m_outputLevel = 0;
    render(makeRenderData(scene, rows));
  }

  void renderScene(ToonzScene *scene, Level *outputLevel) {
    setRenderArea(scene);
    std::vector<int> rows;
    if (m_frameList.empty()) {
      for (int i = 0; i < scene->getFrameCount(); i++) rows.push_back(i);
    } else {
      for (int i = 0; i < m_frameList.length(); i++)
        rows.push_back(m_frameList[i]);
    }
    m_outputImage = 0;
    m_outputLevel = outputLevel;
    render(makeRenderData(scene, rows));
  }

  void onRenderRasterStarted(const RenderData &renderData) override {
    int a = 1;
  }
  void onRenderRasterCompleted(const RenderData &renderData) override {
    TRasterP outputRaster = renderData.m_rasA;
    TRasterImageP img(outputRaster->clone());
    img->setDpi(m_cameraDpi.x, m_cameraDpi.y);
    if (m_outputImage)
      m_outputImage->setImg(img);
    else if (m_outputLevel) {
      std::vector<std::string> ids;
      for (int i = 0; i < (int)renderData.m_frames.size(); i++) {
        TFrameId fid((int)(renderData.m_frames[i]) + 1);
        m_outputLevel->setFrame(fid, img);
        std::string id = m_outputLevel->getSimpleLevel()->getImageId(fid);
        ids.push_back(id);
      }
      img = TImageP();
      for (int i = 0; i < (int)ids.size(); i++)
        TImageCache::instance()->compress(ids[i]);
    }
  }
  void onRenderFailure(const RenderData &renderData, TException &e) override {}
  void onRenderFinished() { m_completed = true; }
};  // class RenderEngine

//=======================================================

Renderer::Renderer() : m_imp(new Imp()) {}

Renderer::~Renderer() {}

QScriptValue Renderer::ctor(QScriptContext *context, QScriptEngine *engine) {
  QScriptValue r = create(engine, new Renderer());
  r.setProperty("frames", engine->newArray());
  r.setProperty("columns", engine->newArray());
  return r;
}

QScriptValue Renderer::toString() { return "Renderer"; }

QScriptValue Renderer::renderScene(const QScriptValue &sceneArg) {
  QScriptValue obj = context()->thisObject();
  valueToIntList(obj.property("frames"), m_imp->m_frameList);
  valueToIntList(obj.property("columns"), m_imp->m_columnList);

  Scene *scene     = 0;
  QScriptValue err = getScene(context(), sceneArg, scene);
  if (err.isError()) return err;

  Level *outputLevel = new Level();
  // engine()->collectGarbage();
  m_imp->renderScene(scene->getToonzScene(), outputLevel);
  return create(engine(), outputLevel);
  /*
for(int row=0;row<scene->getToonzScene()->getFrameCount();row++)
{
engine()->collectGarbage();
TImageP img = renderEngine.renderFrame(row);
if(img)
{
  QScriptValue frame = create(new Image(img));
  QScriptValueList args; args << QString::number(row+1) << frame;
  newLevel.property("setFrame").call(newLevel, args);
}
else
{
  return context()->throwError(tr("Render failed"));
}
}
return newLevel;
*/
}

Q_INVOKABLE QScriptValue Renderer::renderFrame(const QScriptValue &sceneArg,
                                               int frame) {
  QScriptValue obj = context()->thisObject();
  valueToIntList(obj.property("columns"), m_imp->m_columnList);

  Scene *scene     = 0;
  QScriptValue err = getScene(context(), sceneArg, scene);
  if (err.isError()) return err;

  Image *outputImage = new Image();
  engine()->collectGarbage();
  m_imp->renderFrame(scene->getToonzScene(), frame, outputImage);
  return create(engine(), outputImage);

  /*
Scene *scene = 0;
QScriptValue err = getScene(context(), sceneArg, scene);
if(err.isError()) return err;




engine()->collectGarbage();

RenderEngine renderEngine(scene->getToonzScene());
TImageP img = renderEngine.renderFrame(frame);

for(int i=0;i<oldStatus.length();i++)
xsh->getColumn(i)->setPreviewVisible(oldStatus[i]);

if(img)
{
return create(engine(), new Image(img));
}
else
{
return context()->throwError(tr("Render failed"));
}
*/
}

/*
  QScriptValue Renderer::renderColumns(const QScriptValue &sceneArg, const
  QScriptValue &columnListArg)
  {
    Scene *scene = 0;
    QScriptValue err = getScene(context(), sceneArg, scene);
    if(err.isError()) return err;

    QList<bool> oldStatus;
    QList<bool> newStatus;
    TXsheet *xsh = scene->getToonzScene()->getXsheet();
    for(int i=0;i<xsh->getColumnCount();i++)
    {
      oldStatus.append(xsh->getColumn(i)->isPreviewVisible());
      newStatus.append(false);
    }

    if(!columnListArg.isArray())
      return context()->throwError(tr("Second argument must be an array of
  column indices : ").arg(columnListArg.toString()));
    int m = columnListArg.property("length").toInt32();
    for(quint32 i=0;i<(int)m;i++)
    {
      QScriptValue c = columnListArg.property(i);
      if(!c.isNumber())
      {
        return context()->throwError(tr("Second argument must be an array of
  integer numbers : %1 (#%2)")
          .arg(columnListArg.toString())
          .arg(i));
      }
      int index = c.toInteger();
      if(0<=index && index<newStatus.length()) newStatus[index] = true;
    }
    for(int i=0;i<newStatus.length();i++)
      xsh->getColumn(i)->setPreviewVisible(newStatus[i]);

    err = QScriptValue();
    QScriptValue newLevel = create(new Level());
    RenderEngine renderEngine(scene->getToonzScene());
    for(int row=0;row<scene->getToonzScene()->getFrameCount();row++)
    {
      engine()->collectGarbage();
      TImageP img = renderEngine.renderFrame(row);
      if(img)
      {
        QScriptValue frame = create(new Image(img));
        QScriptValueList args; args << QString::number(row+1) << frame;
        newLevel.property("setFrame").call(newLevel, args);
      }
      else
      {
        err = context()->throwError(tr("Render failed"));
        break;
      }
    }

    for(int i=0;i<oldStatus.length();i++)
      xsh->getColumn(i)->setPreviewVisible(oldStatus[i]);
    if(err.isError()) return err;
    else return newLevel;
  }
  */

void Renderer::dumpCache() {
  TImageCache::instance()->outputMap(0, "C:\\Users\\gmt\\PLI\\cache.log");
}

}  // namespace TScriptBinding
