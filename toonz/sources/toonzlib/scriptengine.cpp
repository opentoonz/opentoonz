
#include "toonz/scriptengine.h"

#include <QtGlobal>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)

#include "toonz/scriptbinding.h"
#include "toonz/scriptbinding_files.h"
#include "trenderer.h"
#include "toonz/toonzfolders.h"
#include <QScriptEngine>
#include <QScriptProgram>
#include <QFile>
#include <QTextStream>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QtGlobal>
#include <QApplication>

//=========================================================

namespace {

void sleepMilliseconds(unsigned long msec) {
  QMutex mutex;
  mutex.lock();
  QWaitCondition waitCondition;
  waitCondition.wait(&mutex, msec);
  mutex.unlock();
}

QString print(QScriptValue arg, bool addQuotes) {
  if (arg.isArray()) {
    QString s   = "[";
    quint32 len = arg.property(QLatin1String("length")).toUInt32();
    for (quint32 i = 0; i < len; ++i) {
      QScriptValue item = arg.property(i);
      if (i > 0) s += ",";
      s += print(item, addQuotes);
    }
    s += "]";
    return s;
  } else if (arg.isBool()) {
    return arg.toBool() ? "true" : "false";
  } else if (arg.isString()) {
    if (addQuotes)
      return "\"" + arg.toString() + "\"";
    else
      return arg.toString();
  } else
    return arg.toString();
}

QScriptValue printFunction(QScriptContext* context, QScriptEngine* engine) {
  QString result;
  for (int i = 0; i < context->argumentCount(); ++i) {
    if (i > 0) result.append(" ");
    result.append(print(context->argument(i), false));
  }
  QScriptValue calleeData = context->callee().data();
  ScriptEngine* se        = qobject_cast<ScriptEngine*>(calleeData.toQObject());
  se->emitOutput(ScriptEngine::SimpleText, result);
  sleep(50);
  return se->voidValue();
}

QScriptValue warningFunction(QScriptContext* context, QScriptEngine* engine) {
  QString result;
  for (int i = 0; i < context->argumentCount(); ++i) {
    if (i > 0) result.append(" ");
    result.append(print(context->argument(i), false));
  }
  QScriptValue calleeData = context->callee().data();
  ScriptEngine* se        = qobject_cast<ScriptEngine*>(calleeData.toQObject());
  se->emitOutput(ScriptEngine::Warning, result);
  sleep(50);
  return se->voidValue();
}

QScriptValue runFunction(QScriptContext* context, QScriptEngine* engine) {
  if (context->argumentCount() != 1) {
    return context->throwError("expected one parameter");
  }
  TFilePath fp;
  QScriptValue err =
      TScriptBinding::checkFilePath(context, context->argument(0), fp);
  if (err.isError()) return err;
  if (!fp.isAbsolute()) {
    fp = ToonzFolder::getLibraryFolder() + "scripts" + fp;
  }

  QString fpStr = QString::fromStdWString(fp.getWideString());
  QFile file(fpStr);
  if (!file.open(QIODevice::ReadOnly)) {
    return context->throwError("can't read file " + fpStr);
  }
  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  QScriptProgram program(content, fpStr);

  QScriptContext* parent = context->parentContext();
  if (parent != 0) {
    context->setActivationObject(context->parentContext()->activationObject());
    context->setThisObject(context->parentContext()->thisObject());
  }

  QScriptValue ret = engine->evaluate(program);

  if (engine->hasUncaughtException()) {
    int line = engine->uncaughtExceptionLineNumber();
    return context->throwError(QString("%1, at line %2 of %3")
                                   .arg(ret.toString())
                                   .arg(line)
                                   .arg(fpStr));
  }

  return ret;
}

/*
  QScriptValue glub(QScriptContext *context, QScriptEngine *engine)
  {
    QScriptValue global = engine->globalObject();
    QScriptValue te = global.property("_engine");
    ScriptEngine *se = qscriptvalue_cast<ScriptEngine*>(te);
    se->postCommand(context->argument(0));
    return 0;
  }
  */

}  // namespace

//=========================================================

class ScriptEngine::Executor final : public QThread {
  ScriptEngine* m_engine;
  QString m_cmd;

public:
  Executor(ScriptEngine* engine, const QString& cmd)
      : m_engine(engine), m_cmd(cmd) {}

  void run() override {
    m_engine->m_engine->collectGarbage();
    QScriptValue result = m_engine->m_engine->evaluate(m_cmd);
    if (result.isError()) {
      m_engine->emitOutput(ScriptEngine::SyntaxError, result.toString());
    } else if (result.isUndefined()) {
      m_engine->emitOutput(ScriptEngine::UndefinedEvaluationResult,
                           "undefined");
    } else {
      if (qscriptvalue_cast<TScriptBinding::Void*>(result)) {
      } else {
        m_engine->emitOutput(ScriptEngine::EvaluationResult,
                             print(result, true));
      }
    }
  }
};

class ScriptEngine::MainThreadEvaluationData {
public:
  QMutex m_mutex;
  QWaitCondition m_cond;
  QScriptValue m_fun, m_args, m_result;
};

//=========================================================

inline void defineFunction(ScriptEngine* se, const QString& name,
                           QScriptEngine::FunctionSignature f) {
  QScriptEngine* engine = se->getQScriptEngine();
  QScriptValue fObj     = engine->newFunction(f);
  fObj.setData(engine->newQObject(se));
  engine->globalObject().setProperty(name, fObj);
}

//=========================================================

ScriptEngine::ScriptEngine() : m_executor(0), m_engine(new QScriptEngine()) {
  // I must call TRenderer::initialize(), because a script could cause a
  // rendering driven by a working thread
  TRenderer::initialize();

  m_mainThreadEvaluationData = new MainThreadEvaluationData();
  QScriptValue global        = m_engine->globalObject();
  QScriptValue ctor;

  QScriptEngine& engine = *m_engine;

  defineFunction(this, "print", printFunction);
  defineFunction(this, "warning", warningFunction);
  defineFunction(this, "run", runFunction);

  /*
QScriptValue print = engine.newFunction(printFunction);
print.setData(engine.newQObject(this));
engine.globalObject().setProperty("print", print);

QScriptValue print = engine.newFunction(printFunction);
print.setData(engine.newQObject(this));
engine.globalObject().setProperty("print", print);

QScriptValue run = engine.newFunction(runFunction);
run.setData(engine.newQObject(this));
engine.globalObject().setProperty("run", run);

*/
  // QScriptValue g = engine.newFunction(glub);
  // g.setData(engine.newQObject(this));
  // engine.globalObject().setProperty("glub", g);

  // engine.globalObject().setProperty("_engine", engine.newQObject(this));

  m_voidValue  = new QScriptValue();
  *m_voidValue = engine.newQObject(new TScriptBinding::Void(),
                                   QScriptEngine::AutoOwnership);

  engine.globalObject().setProperty("void", *m_voidValue);

  TScriptBinding::bindAll(engine);
  bool ret = connect(this, SIGNAL(mainThreadEvaluationPosted()), this,
                     SLOT(onMainThreadEvaluationPosted()));
  assert(ret);
}

ScriptEngine::~ScriptEngine() {
  delete m_mainThreadEvaluationData;
  delete m_voidValue;
}

const QScriptValue& ScriptEngine::evaluateOnMainThread(
    const QScriptValue& fun, const QScriptValue& arguments) {
  MainThreadEvaluationData* d = m_mainThreadEvaluationData;
  QMutexLocker locker(&d->m_mutex);
  d->m_fun    = fun;
  d->m_args   = arguments;
  d->m_result = QScriptValue();
  emit mainThreadEvaluationPosted();
  d->m_cond.wait(&d->m_mutex);
  return d->m_result;
}

void ScriptEngine::onMainThreadEvaluationPosted() {
  Q_ASSERT(qApp && qApp->thread() == QThread::currentThread());
  MainThreadEvaluationData* d = m_mainThreadEvaluationData;
  QMutexLocker locker(&d->m_mutex);
  d->m_result = d->m_fun.call(d->m_fun, d->m_args);
  d->m_cond.wakeOne();
}

void ScriptEngine::evaluate(const QString& cmd) {
  if (m_executor) return;
  m_executor = new Executor(this, cmd);
  connect(m_executor, SIGNAL(finished()), this, SLOT(onTerminated()));
  m_executor->start();
}

bool ScriptEngine::wait(unsigned long time) {
  if (!m_executor) return true;
  const bool finished = m_executor->wait(time);
  if (finished) onTerminated();
  return finished;
}

bool ScriptEngine::isEvaluating() const { return m_engine->isEvaluating(); }

void ScriptEngine::interrupt() { m_engine->abortEvaluation(); }

void ScriptEngine::onTerminated() {
  if (!m_executor) return;
  emit evaluationDone();
  delete m_executor;
  m_executor = 0;
}

#else

#include "toonz/toonzfolders.h"
#include "toonz/levelset.h"
#include "toonz/levelproperties.h"
#include "toonz/tcamera.h"
#include "toonz/tcenterlinevectorizer.h"
#include "toonz/toonzscene.h"
#include "toonz/toonzimageutils.h"
#include "toonz/tproject.h"
#include "toonz/txshcell.h"
#include "toonz/txsheet.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"
#include "convert2tlv.h"
#include "tfiletype.h"
#include "tgeometry.h"
#include "timage.h"
#include "timage_io.h"
#include "tlevel.h"
#include "tlevel_io.h"
#include "tpalette.h"
#include "trenderer.h"
#include "trasterimage.h"
#include "trop.h"
#include "tsystem.h"
#include "ttoonzimage.h"
#include "tvectorimage.h"

#include <QApplication>
#include <QColor>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJSValue>
#include <QMutex>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QThread>
#include <QWaitCondition>

#include <utility>
#include <cmath>
#include <limits>

//=========================================================

namespace Stage {
extern const double inch;
}

namespace {

void sleepMilliseconds(unsigned long msec) {
  QMutex mutex;
  mutex.lock();
  QWaitCondition waitCondition;
  waitCondition.wait(&mutex, msec);
  mutex.unlock();
}

QString print(const QJSValue& arg, bool addQuotes) {
  if (arg.isArray()) {
    QString s     = "[";
    const int len = arg.property(QLatin1String("length")).toInt();
    for (int i = 0; i < len; ++i) {
      QJSValue item = arg.property(i);
      if (i > 0) s += ",";
      s += print(item, addQuotes);
    }
    s += "]";
    return s;
  } else if (arg.isBool()) {
    return arg.toBool() ? "true" : "false";
  } else if (arg.isString()) {
    if (addQuotes)
      return "\"" + arg.toString() + "\"";
    else
      return arg.toString();
  } else {
    return arg.toString();
  }
}

QString formatError(const QJSValue& value, const QString& fallbackFileName) {
  QString message = value.toString();
  const int line  = value.property(QStringLiteral("lineNumber")).toInt();
  const QString fileName =
      value.property(QStringLiteral("fileName")).toString();
  if (line > 0) {
    const QString source = fileName.isEmpty() ? fallbackFileName : fileName;
    if (!source.isEmpty())
      message = QStringLiteral("%1, at line %2 of %3")
                    .arg(message)
                    .arg(line)
                    .arg(source);
    else
      message = QStringLiteral("%1, at line %2").arg(message).arg(line);
  }
  return message;
}

QString levelTypeName(int type) {
  switch (type) {
  case NO_XSHLEVEL:
    return QStringLiteral("Empty");
  case PLI_XSHLEVEL:
    return QStringLiteral("Vector");
  case TZP_XSHLEVEL:
    return QStringLiteral("ToonzRaster");
  case OVL_XSHLEVEL:
    return QStringLiteral("Raster");
  default:
    return QStringLiteral("Unknown");
  }
}

int levelTypeFromName(const QString& type) {
  if (type == QStringLiteral("Vector")) return PLI_XSHLEVEL;
  if (type == QStringLiteral("ToonzRaster")) return TZP_XSHLEVEL;
  if (type == QStringLiteral("Raster")) return OVL_XSHLEVEL;
  return NO_XSHLEVEL;
}

QString imageTypeName(TImage* image) {
  if (!image) return QStringLiteral("Empty");

  switch (image->getType()) {
  case TImage::RASTER:
    return QStringLiteral("Raster");
  case TImage::TOONZ_RASTER:
    return QStringLiteral("ToonzRaster");
  case TImage::VECTOR:
    return QStringLiteral("Vector");
  default:
    return QStringLiteral("Unknown");
  }
}

int levelTypeFromImage(TImage* image) {
  if (!image) return NO_XSHLEVEL;

  switch (image->getType()) {
  case TImage::TOONZ_RASTER:
    return TZP_XSHLEVEL;
  case TImage::RASTER:
    return OVL_XSHLEVEL;
  case TImage::VECTOR:
    return PLI_XSHLEVEL;
  default:
    return NO_XSHLEVEL;
  }
}

QVariantList transformToValues(const TAffine& aff) {
  QVariantList values;
  values << aff.a11 << aff.a12 << aff.a13 << aff.a21 << aff.a22 << aff.a23;
  return values;
}

TAffine transformFromValues(const QVariantList& values) {
  if (values.size() != 6) return TAffine();
  return TAffine(values.at(0).toDouble(), values.at(1).toDouble(),
                 values.at(2).toDouble(), values.at(3).toDouble(),
                 values.at(4).toDouble(), values.at(5).toDouble());
}

QString transformDescription(const TAffine& aff) {
  if (aff.isIdentity()) return QObject::tr("Identity");
  if (aff.isTranslation()) {
    return QObject::tr("Translation(%1,%2)").arg(aff.a13).arg(aff.a23);
  }

  QString translationPart;
  if (aff.a13 != 0.0 || aff.a23 != 0.0) {
    translationPart =
        QObject::tr("Translation(%1,%2)").arg(aff.a13).arg(aff.a23);
  }

  if (std::fabs(aff.det() - 1.0) < 1.0e-8) {
    constexpr double kPi       = 3.14159265358979323846;
    double phi                 = std::atan2(aff.a12, aff.a11) * 180.0 / kPi;
    phi                        = -(0.001 * std::floor(1000 * phi + 0.5));
    const QString rotationPart = QObject::tr("Rotation(%1)").arg(phi);
    return translationPart.isEmpty() ? rotationPart
                                     : translationPart + "*" + rotationPart;
  }

  if (aff.a12 == 0.0 && aff.a21 == 0.0) {
    QString scalePart;
    if (aff.a11 == aff.a22) {
      scalePart = QObject::tr("Scale(%1%)").arg(aff.a11 * 100);
    } else {
      scalePart =
          QObject::tr("Scale(%1%, %2%)").arg(aff.a11 * 100).arg(aff.a22 * 100);
    }
    return translationPart.isEmpty() ? scalePart
                                     : translationPart + "*" + scalePart;
  }

  return QObject::tr("Transform(%1, %2, %3;  %4, %5, %6)")
      .arg(aff.a11)
      .arg(aff.a12)
      .arg(aff.a13)
      .arg(aff.a21)
      .arg(aff.a22)
      .arg(aff.a23);
}

QString imageBuilderTypeName(TImage* image) {
  if (!image) return QStringLiteral("Empty");
  return imageTypeName(image);
}

QVariantMap defaultOutlineVectorizerState() {
  QVariantMap state;
  state.insert(QStringLiteral("accuracy"), 8);
  state.insert(QStringLiteral("despeckling"), 4);
  state.insert(QStringLiteral("preservePaintedAreas"), false);
  state.insert(QStringLiteral("cornerAdherence"), 50.0);
  state.insert(QStringLiteral("cornerAngle"), 45.0);
  state.insert(QStringLiteral("cornerCurveRadius"), 25.0);
  state.insert(QStringLiteral("maxColors"), 50);
  state.insert(QStringLiteral("transparentColor"), QStringLiteral("#000000"));
  state.insert(QStringLiteral("toneThreshold"), 128);
  return state;
}

bool isOutlineVectorizerProperty(const QString& name) {
  static const QSet<QString> properties = {
      QStringLiteral("accuracy"),
      QStringLiteral("despeckling"),
      QStringLiteral("preservePaintedAreas"),
      QStringLiteral("cornerAdherence"),
      QStringLiteral("cornerAngle"),
      QStringLiteral("cornerCurveRadius"),
      QStringLiteral("maxColors"),
      QStringLiteral("transparentColor"),
      QStringLiteral("toneThreshold")};
  return properties.contains(name);
}

NewOutlineConfiguration outlineConfigurationFromState(
    const QVariantMap& state) {
  NewOutlineConfiguration parameters;
  parameters.m_transparentColor = TPixel32::Transparent;

  parameters.m_mergeTol =
      5.0 - state.value(QStringLiteral("accuracy"), 8).toInt() * 0.5;
  parameters.m_despeckling =
      state.value(QStringLiteral("despeckling"), 4).toInt();
  parameters.m_leaveUnpainted =
      !state.value(QStringLiteral("preservePaintedAreas"), false).toBool();
  parameters.m_adherenceTol =
      state.value(QStringLiteral("cornerAdherence"), 50.0).toDouble() * 0.01;
  parameters.m_angleTol =
      state.value(QStringLiteral("cornerAngle"), 45.0).toDouble() / 180.0;
  parameters.m_relativeTol =
      state.value(QStringLiteral("cornerCurveRadius"), 25.0).toDouble() * 0.01;
  parameters.m_maxColors = state.value(QStringLiteral("maxColors"), 50).toInt();
  parameters.m_toneTol =
      state.value(QStringLiteral("toneThreshold"), 128).toInt();

  const QColor transparentColor = QColor::fromString(
      state.value(QStringLiteral("transparentColor"), QStringLiteral("#000000"))
          .toString());
  if (transparentColor.isValid()) {
    parameters.m_transparentColor =
        TPixel32(transparentColor.red(), transparentColor.green(),
                 transparentColor.blue(), transparentColor.alpha());
  }
  return parameters;
}

QVariantMap defaultCenterlineVectorizerState() {
  QVariantMap state;
  state.insert(QStringLiteral("threshold"), 8);
  state.insert(QStringLiteral("accuracy"), 9);
  state.insert(QStringLiteral("despeckling"), 5);
  state.insert(QStringLiteral("maxThickness"), 200.0);
  state.insert(QStringLiteral("thicknessCalibration"), 100.0);
  state.insert(QStringLiteral("preservePaintedAreas"), false);
  state.insert(QStringLiteral("addBorder"), false);
  state.insert(QStringLiteral("eir"), false);
  return state;
}

bool isCenterlineVectorizerProperty(const QString& name) {
  static const QSet<QString> properties = {
      QStringLiteral("threshold"),
      QStringLiteral("accuracy"),
      QStringLiteral("despeckling"),
      QStringLiteral("maxThickness"),
      QStringLiteral("thicknessCalibration"),
      QStringLiteral("preservePaintedAreas"),
      QStringLiteral("addBorder"),
      QStringLiteral("eir")};
  return properties.contains(name);
}

CenterlineConfiguration centerlineConfigurationFromState(
    const QVariantMap& state) {
  CenterlineConfiguration parameters;
  parameters.m_threshold =
      state.value(QStringLiteral("threshold"), 8).toInt() * 25;
  parameters.m_penalty =
      10.0 - state.value(QStringLiteral("accuracy"), 9).toDouble();
  parameters.m_despeckling =
      state.value(QStringLiteral("despeckling"), 5).toInt() * 2;
  parameters.m_maxThickness =
      state.value(QStringLiteral("maxThickness"), 200.0).toDouble() / 2.0;
  parameters.m_thicknessRatio =
      state.value(QStringLiteral("thicknessCalibration"), 100.0).toDouble();
  parameters.m_leaveUnpainted =
      !state.value(QStringLiteral("preservePaintedAreas"), false).toBool();
  parameters.m_makeFrame =
      state.value(QStringLiteral("addBorder"), false).toBool();
  parameters.m_naaSource = state.value(QStringLiteral("eir"), false).toBool();
  return parameters;
}

QVariantMap defaultRasterizerState() {
  QVariantMap state;
  state.insert(QStringLiteral("colorMapped"), false);
  state.insert(QStringLiteral("xres"), 720);
  state.insert(QStringLiteral("yres"), 576);
  state.insert(QStringLiteral("dpi"), 72.0);
  state.insert(QStringLiteral("antialiasing"), true);
  return state;
}

bool isRasterizerProperty(const QString& name) {
  static const QSet<QString> properties = {
      QStringLiteral("colorMapped"), QStringLiteral("xres"),
      QStringLiteral("yres"), QStringLiteral("dpi"),
      QStringLiteral("antialiasing")};
  return properties.contains(name);
}

bool frameIdFromString(const QString& value, TFrameId& fid, QString& error) {
  static const QRegularExpression re(QStringLiteral(R"(^(-?\d+)(\w?)$)"));
  QString normalized = value.trimmed();
  normalized.remove(QChar::Null);
  normalized.remove(QLatin1Char('\r'));
  normalized.remove(QLatin1Char('\n'));
  const QRegularExpressionMatch match = re.match(normalized);
  if (!match.hasMatch()) {
    error =
        QObject::tr("Argument '%1' does not look like a FrameId").arg(value);
    return false;
  }

  const int number     = match.captured(1).toInt();
  const QString letter = match.captured(2);
  fid = letter.length() == 1
            ? TFrameId(number, static_cast<wchar_t>(letter[0].unicode()))
            : TFrameId(number);
  error.clear();
  return true;
}

QVariant scriptFrameIdValue(const TFrameId& fid) {
  if (fid.getLetter().isEmpty()) return fid.getNumber();
  return QString::fromStdString(fid.expand());
}

TImageP loadScriptLevelFrame(TXshSimpleLevel* level, const TFrameId& frameId) {
  if (!level) return TImageP();

  TImageP image = level->getFrame(frameId, false);
  if (image) return image;

  TFilePath path = level->getPath();
  if (path == TFilePath()) return TImageP();

  if (ToonzScene* scene = level->getScene()) {
    path = scene->decodeFilePath(path);
  }

  try {
    TLevelReaderP reader(path);
    TLevelP levelInfo = reader->loadInfo();
    TFrameId readerFrameId = frameId;
    if (levelInfo) {
      bool matchedFrameId = false;
      for (TLevel::Iterator it = levelInfo->begin(); it != levelInfo->end();
           ++it) {
        if (it->first == frameId) {
          readerFrameId = it->first;
          matchedFrameId = true;
          break;
        }
      }
      if (!matchedFrameId && levelInfo->getFrameCount() == 1) {
        readerFrameId = levelInfo->begin()->first;
      }
    }
    image = reader->getFrameReader(readerFrameId)->load();
    if (image) image->setPalette(level->getPalette());
  } catch (...) {
    return TImageP();
  }

  return image;
}

const char kBootstrapScript[] = R"JS(
(function(global) {
  var nextFilePathId = 0;
  var nextToonzRasterConverterId = 0;
  var nextRendererId = 0;

  function toFilePathString(value) {
    if (value instanceof FilePath)
      return value.__path;
    return String(value);
  }

  function filePathArgument(value) {
    if (value instanceof FilePath)
      return value.__path;
    if (typeof value === "string" || value instanceof String)
      return String(value);
    throw new Error("Argument doesn't look like a file path : " +
                    String(value));
  }

  function throwIfError(message) {
    if (message)
      throw new Error(message);
  }

  function isNumberArgument(value) {
    return typeof value === "number" && isFinite(value);
  }

  function sceneNumberArgument(value, name) {
    if (!isNumberArgument(value))
      throw new Error(name + " must be a number : " + String(value));
    return Number(value);
  }

  function throwTransformArgumentError(method, expected) {
    throw new Error("Bad arguments: Transform." + method + " expected " +
                    expected);
  }

  function isStringArgument(value) {
    return typeof value === "string" || value instanceof String;
  }

  function sceneId(scene) {
    if (!(scene instanceof Scene) || scene.__sceneId < 0)
      throw new Error("Invalid Scene object");
    return scene.__sceneId;
  }

  function levelId(level) {
    if (!(level instanceof Level) || level.__levelId < 0)
      throw new Error("Invalid Level object");
    return level.__levelId;
  }

  function imageId(image) {
    if (!(image instanceof Image) || image.__imageId < 0)
      throw new Error("Invalid Image object");
    return image.__imageId;
  }

  function transformId(transform) {
    if (!(transform instanceof Transform) || transform.__transformId < 0)
      throw new Error("Invalid Transform object");
    return transform.__transformId;
  }

  function imageBuilderId(imageBuilder) {
    if (!(imageBuilder instanceof ImageBuilder) ||
        imageBuilder.__imageBuilderId < 0)
      throw new Error("Invalid ImageBuilder object");
    return imageBuilder.__imageBuilderId;
  }

  function outlineVectorizerId(vectorizer) {
    if (!(vectorizer instanceof OutlineVectorizer) ||
        vectorizer.__outlineVectorizerId < 0)
      throw new Error("Invalid OutlineVectorizer object");
    return vectorizer.__outlineVectorizerId;
  }

  function centerlineVectorizerId(vectorizer) {
    if (!(vectorizer instanceof CenterlineVectorizer) ||
        vectorizer.__centerlineVectorizerId < 0)
      throw new Error("Invalid CenterlineVectorizer object");
    return vectorizer.__centerlineVectorizerId;
  }

  function rasterizerId(rasterizer) {
    if (!(rasterizer instanceof Rasterizer) || rasterizer.__rasterizerId < 0)
      throw new Error("Invalid Rasterizer object");
    return rasterizer.__rasterizerId;
  }

  function rendererId(renderer) {
    if (!(renderer instanceof Renderer) || renderer.__rendererId < 0)
      throw new Error("Invalid Renderer object");
    return renderer.__rendererId;
  }

  function rendererSceneId(scene) {
    if (!(scene instanceof Scene))
      throw new Error("First argument must be a scene : " + String(scene));
    return sceneId(scene);
  }

  function throwRendererDeferred(method) {
    throw new Error("Qt 6 script Renderer." + method +
                    " is deferred until the rendering backend migration " +
                    "boundary is stable");
  }

  function createLevel(levelIdValue) {
    var level = Object.create(Level.prototype);
    level.__levelId = Number(levelIdValue);
    return level;
  }

  function createImage(imageIdValue) {
    var image = Object.create(Image.prototype);
    image.__imageId = Number(imageIdValue);
    return image;
  }

  function createTransform(transformIdValue) {
    var transform = Object.create(Transform.prototype);
    transform.__transformId = Number(transformIdValue);
    return transform;
  }

  function levelFromResult(result) {
    throwIfError(result.error || "");
    if (result.id === undefined || Number(result.id) < 0)
      return undefined;
    return createLevel(result.id);
  }

  function imageFromResult(result) {
    throwIfError(result.error || "");
    if (result.id === undefined || Number(result.id) < 0)
      return undefined;
    return createImage(result.id);
  }

  function transformFromResult(result) {
    throwIfError(result.error || "");
    if (result.id === undefined || Number(result.id) < 0)
      return undefined;
    return createTransform(result.id);
  }

  function mapLevelFrames(sourceLevel, convertFrame) {
    var result = new Level();
    var fids = sourceLevel.getFrameIds();
    for (var i = 0; i < fids.length; ++i) {
      var frame = sourceLevel.getFrame(fids[i]);
      if (frame === undefined)
        continue;

      var converted = undefined;
      try {
        converted = convertFrame(frame);
        if (converted !== undefined)
          result.setFrame(fids[i], converted);
      } finally {
        if (converted instanceof Image && converted !== frame)
          converted.dispose();
        frame.dispose();
      }
    }
    return result;
  }

  function format(value, addQuotes) {
    if (Array.isArray(value)) {
      var items = [];
      for (var i = 0; i < value.length; ++i)
        items.push(format(value[i], addQuotes));
      return "[" + items.join(",") + "]";
    }
    if (typeof value === "boolean")
      return value ? "true" : "false";
    if (typeof value === "string")
      return addQuotes ? "\"" + value + "\"" : value;
    if (value === undefined)
      return "undefined";
    if (value === null)
      return "null";
    return String(value);
  }

  global.print = function() {
    var items = [];
    for (var i = 0; i < arguments.length; ++i)
      items.push(format(arguments[i], false));
    __opentoonzScriptEngine.emitScriptOutput(0, items.join(" "));
    return global["void"];
  };

  global.warning = function() {
    var items = [];
    for (var i = 0; i < arguments.length; ++i)
      items.push(format(arguments[i], false));
    __opentoonzScriptEngine.emitScriptOutput(1, items.join(" "));
    return global["void"];
  };

  global.run = function(path) {
    if (arguments.length !== 1)
      throw new Error("expected one parameter");

    var script = __opentoonzScriptEngine.readScriptFile(
      filePathArgument(path));
    if (!script.ok)
      throw new Error(String(script.error));
    return (0, eval)(String(script.content) + "\n//# sourceURL=" +
                     encodeURI(String(script.path)));
  };

  global.ToonzVersion = "7.1";
  global["void"] = {};

  global.FilePath = function(path) {
    this.__filePathId = ++nextFilePathId;
    this.__path = arguments.length === 1 ? toFilePathString(path) : "";
  };

  Object.defineProperty(FilePath.prototype, "id", {
    get: function() {
      return this.__filePathId;
    }
  });

  FilePath.prototype.toString = function() {
    return __opentoonzScriptEngine.filePathToString(this.__path);
  };

  FilePath.prototype.valueOf = function() {
    return this.__path;
  };

  Object.defineProperty(FilePath.prototype, "extension", {
    get: function() {
      return __opentoonzScriptEngine.filePathExtension(this.__path);
    },
    set: function(extension) {
      this.__path = __opentoonzScriptEngine.filePathWithExtension(
        this.__path, String(extension));
    }
  });

  Object.defineProperty(FilePath.prototype, "name", {
    get: function() {
      return __opentoonzScriptEngine.filePathName(this.__path);
    },
    set: function(name) {
      this.__path = __opentoonzScriptEngine.filePathWithName(
        this.__path, String(name));
    }
  });

  Object.defineProperty(FilePath.prototype, "parentDirectory", {
    get: function() {
      return new FilePath(
        __opentoonzScriptEngine.filePathParentDirectory(this.__path));
    },
    set: function(parentDirectory) {
      this.__path = __opentoonzScriptEngine.filePathWithParentDirectory(
        this.__path, filePathArgument(parentDirectory));
    }
  });

  Object.defineProperty(FilePath.prototype, "exists", {
    get: function() {
      return __opentoonzScriptEngine.filePathExists(this.__path);
    }
  });

  Object.defineProperty(FilePath.prototype, "lastModified", {
    get: function() {
      var msecs = __opentoonzScriptEngine.filePathLastModified(this.__path);
      return isNaN(msecs) ? new Date(NaN) : new Date(msecs);
    }
  });

  Object.defineProperty(FilePath.prototype, "isDirectory", {
    get: function() {
      return __opentoonzScriptEngine.filePathIsDirectory(this.__path);
    }
  });

  FilePath.prototype.withExtension = function(extension) {
    return new FilePath(__opentoonzScriptEngine.filePathWithExtension(
      this.__path, String(extension)));
  };

  FilePath.prototype.withName = function(name) {
    return new FilePath(__opentoonzScriptEngine.filePathWithName(
      this.__path, String(name)));
  };

  FilePath.prototype.withParentDirectory = function(parentDirectory) {
    return new FilePath(__opentoonzScriptEngine.filePathWithParentDirectory(
      this.__path, filePathArgument(parentDirectory)));
  };

  FilePath.prototype.concat = function(value) {
    var path = filePathArgument(value);
    if (__opentoonzScriptEngine.filePathIsAbsolute(path))
      throw new Error("can't concatenate an absolute path : " +
                      String(value));
    return new FilePath(__opentoonzScriptEngine.filePathConcat(
      this.__path, path));
  };

  FilePath.prototype.files = function() {
    if (!this.isDirectory)
      throw new Error(this.toString() + " is not a directory");
    var paths = __opentoonzScriptEngine.filePathFiles(this.__path);
    var result = [];
    for (var i = 0; i < paths.length; ++i)
      result.push(new FilePath(paths[i]));
    return result;
  };

  global.Image = function(path) {
    if (arguments.length > 1)
      throw new Error("Bad argument count. expected: [path]");

    this.__imageId = __opentoonzScriptEngine.imageCreate();
    if (arguments.length === 1)
      this.load(path);
  };

  Object.defineProperty(Image.prototype, "id", {
    get: function() {
      return this.__imageId;
    }
  });

  Image.prototype.dispose = function() {
    if (this.__imageId >= 0)
      __opentoonzScriptEngine.imageDelete(this.__imageId);
    this.__imageId = -1;
  };

  Image.prototype.toString = function() {
    return __opentoonzScriptEngine.imageToString(imageId(this));
  };

  Object.defineProperty(Image.prototype, "type", {
    get: function() {
      return __opentoonzScriptEngine.imageType(imageId(this));
    }
  });

  Object.defineProperty(Image.prototype, "width", {
    get: function() {
      return __opentoonzScriptEngine.imageWidth(imageId(this));
    }
  });

  Object.defineProperty(Image.prototype, "height", {
    get: function() {
      return __opentoonzScriptEngine.imageHeight(imageId(this));
    }
  });

  Object.defineProperty(Image.prototype, "dpi", {
    get: function() {
      return __opentoonzScriptEngine.imageDpi(imageId(this));
    }
  });

  Image.prototype.load = function(path) {
    throwIfError(__opentoonzScriptEngine.imageLoad(
      imageId(this), filePathArgument(path)));
    return this;
  };

  Image.prototype.save = function(path) {
    throwIfError(__opentoonzScriptEngine.imageSave(
      imageId(this), filePathArgument(path)));
    return this;
  };

  global.Transform = function() {
    this.__transformId = __opentoonzScriptEngine.transformCreate();
  };

  Object.defineProperty(Transform.prototype, "id", {
    get: function() {
      return this.__transformId;
    }
  });

  Transform.prototype.dispose = function() {
    if (this.__transformId >= 0)
      __opentoonzScriptEngine.transformDelete(this.__transformId);
    this.__transformId = -1;
  };

  Transform.prototype.toString = function() {
    return __opentoonzScriptEngine.transformToString(transformId(this));
  };

  Transform.prototype.translate = function(x, y) {
    var id = transformId(this);
    if (arguments.length !== 2 || !isNumberArgument(x) || !isNumberArgument(y))
      throwTransformArgumentError("translate", "x,y");
    return transformFromResult(__opentoonzScriptEngine.transformTranslate(
      id, x, y));
  };

  Transform.prototype.rotate = function(degrees) {
    var id = transformId(this);
    if (arguments.length !== 1 || !isNumberArgument(degrees))
      throwTransformArgumentError("rotate", "degrees");
    return transformFromResult(__opentoonzScriptEngine.transformRotate(
      id, degrees));
  };

  Transform.prototype.scale = function(sx, sy) {
    var id = transformId(this);
    if (arguments.length !== 1 && arguments.length !== 2)
      throwTransformArgumentError("scale", "s[,sy]");
    if (!isNumberArgument(sx) ||
        (arguments.length === 2 && !isNumberArgument(sy)))
      throwTransformArgumentError("scale", "s[,sy]");
    if (arguments.length === 1)
      sy = sx;
    return transformFromResult(__opentoonzScriptEngine.transformScale(
      id, sx, sy));
  };

  global.ImageBuilder = function(width, height, type) {
    var result;
    if (arguments.length === 0) {
      result = __opentoonzScriptEngine.imageBuilderCreate(-1, -1, "");
    } else if (arguments.length === 2 || arguments.length === 3) {
      if (!isNumberArgument(width) || !isNumberArgument(height))
        throw new Error("Bad arguments: expected width,height[,type]");
      result = __opentoonzScriptEngine.imageBuilderCreate(
        width, height,
        type === undefined ? "" : String(type));
    } else {
      throw new Error("Bad argument count. expected: width,height[,type]");
    }
    throwIfError(result.error || "");
    this.__imageBuilderId = Number(result.id);
  };

  Object.defineProperty(ImageBuilder.prototype, "id", {
    get: function() {
      return this.__imageBuilderId;
    }
  });

  ImageBuilder.prototype.dispose = function() {
    if (this.__imageBuilderId >= 0)
      __opentoonzScriptEngine.imageBuilderDelete(this.__imageBuilderId);
    this.__imageBuilderId = -1;
  };

  ImageBuilder.prototype.toString = function() {
    return __opentoonzScriptEngine.imageBuilderToString(
      imageBuilderId(this));
  };

  Object.defineProperty(ImageBuilder.prototype, "image", {
    get: function() {
      return imageFromResult(__opentoonzScriptEngine.imageBuilderImage(
        imageBuilderId(this)));
    }
  });

  ImageBuilder.prototype.clear = function() {
    throwIfError(__opentoonzScriptEngine.imageBuilderClear(
      imageBuilderId(this)));
    return this;
  };

  ImageBuilder.prototype.fill = function(colorName) {
    throwIfError(__opentoonzScriptEngine.imageBuilderFill(
      imageBuilderId(this), String(colorName)));
    return this;
  };

  ImageBuilder.prototype.add = function(image, transform) {
    var transformIdValue = -1;
    if (transform !== undefined) {
      if (!(transform instanceof Transform)) {
        throw new Error("Bad argument (" + String(transform) +
                        "): should be a Transformation");
      }
      transformIdValue = transformId(transform);
    }
    throwIfError(__opentoonzScriptEngine.imageBuilderAdd(
      imageBuilderId(this), imageId(image), transformIdValue));
    return this;
  };

  global.ToonzRasterConverter = function() {
    this.__toonzRasterConverterId = ++nextToonzRasterConverterId;
    this.flatSource = false;
  };

  Object.defineProperty(ToonzRasterConverter.prototype, "id", {
    get: function() {
      return this.__toonzRasterConverterId;
    }
  });

  ToonzRasterConverter.prototype.toString = function() {
    return "ToonzRasterConverter";
  };

  ToonzRasterConverter.prototype.dispose = function() {
    this.__toonzRasterConverterId = -1;
  };

  ToonzRasterConverter.prototype.foo = function(x) {
    return Number(x) * 2;
  };

  ToonzRasterConverter.prototype.convert = function(value) {
    if (arguments.length !== 1)
      throw new Error("Expected one argument (a raster Level or a raster Image)");
    if (this.__toonzRasterConverterId < 0)
      throw new Error("Invalid ToonzRasterConverter object");
    return ToonzRasterConverter.convert(value);
  };

  ToonzRasterConverter.convert = function(value) {
    if (arguments.length !== 1)
      throw new Error("Expected one argument (a raster Level or a raster Image)");

    if (value instanceof Level) {
      if (value.type !== "Raster")
        throw new Error("Can't convert a " + value.type + " level");
      if (value.frameCount <= 0)
        throw new Error("Can't convert a level with no frames");

      return mapLevelFrames(value, function(frame) {
        return ToonzRasterConverter.convert(frame);
      });
    }

    if (!(value instanceof Image)) {
      throw new Error("Bad argument (" + String(value) +
                      "): should be a raster Level or a raster Image");
    }
    if (value.type !== "Raster")
      throw new Error("Can't convert a " + value.type + " image");

    return imageFromResult(__opentoonzScriptEngine.toonzRasterConvertImage(
      imageId(value)));
  };

  function defineOutlineVectorizerProperty(name) {
    Object.defineProperty(OutlineVectorizer.prototype, name, {
      get: function() {
        return __opentoonzScriptEngine.outlineVectorizerProperty(
          outlineVectorizerId(this), name);
      },
      set: function(value) {
        throwIfError(__opentoonzScriptEngine.outlineVectorizerSetProperty(
          outlineVectorizerId(this), name, value));
      }
    });
  }

  global.OutlineVectorizer = function() {
    this.__outlineVectorizerId =
      __opentoonzScriptEngine.outlineVectorizerCreate();
  };

  Object.defineProperty(OutlineVectorizer.prototype, "id", {
    get: function() {
      return this.__outlineVectorizerId;
    }
  });

  OutlineVectorizer.prototype.dispose = function() {
    if (this.__outlineVectorizerId >= 0)
      __opentoonzScriptEngine.outlineVectorizerDelete(
        this.__outlineVectorizerId);
    this.__outlineVectorizerId = -1;
  };

  OutlineVectorizer.prototype.toString = function() {
    return __opentoonzScriptEngine.outlineVectorizerToString(
      outlineVectorizerId(this));
  };

  OutlineVectorizer.prototype.vectorize = function(value) {
    if (value instanceof Level) {
      if (value.type !== "Raster" && value.type !== "ToonzRaster")
        throw new Error("Can't vectorize a " + value.type + " level");
      if (value.frameCount <= 0)
        throw new Error("Can't vectorize a level with no frames");

      var vectorizer = this;
      return mapLevelFrames(value, function(frame) {
        return vectorizer.vectorize(frame);
      });
    }

    if (!(value instanceof Image)) {
      throw new Error("Bad argument (" + String(value) +
                      "): should be an Image or a Level");
    }
    if (value.type !== "Raster" && value.type !== "ToonzRaster")
      throw new Error("Can't vectorize a " + value.type + " image");

    return imageFromResult(
      __opentoonzScriptEngine.outlineVectorizerVectorizeImage(
        outlineVectorizerId(this), imageId(value)));
  };

  defineOutlineVectorizerProperty("accuracy");
  defineOutlineVectorizerProperty("despeckling");
  defineOutlineVectorizerProperty("preservePaintedAreas");
  defineOutlineVectorizerProperty("cornerAdherence");
  defineOutlineVectorizerProperty("cornerAngle");
  defineOutlineVectorizerProperty("cornerCurveRadius");
  defineOutlineVectorizerProperty("maxColors");
  defineOutlineVectorizerProperty("transparentColor");
  defineOutlineVectorizerProperty("toneThreshold");

  function defineCenterlineVectorizerProperty(name) {
    Object.defineProperty(CenterlineVectorizer.prototype, name, {
      get: function() {
        return __opentoonzScriptEngine.centerlineVectorizerProperty(
          centerlineVectorizerId(this), name);
      },
      set: function(value) {
        throwIfError(__opentoonzScriptEngine.centerlineVectorizerSetProperty(
          centerlineVectorizerId(this), name, value));
      }
    });
  }

  global.CenterlineVectorizer = function() {
    this.__centerlineVectorizerId =
      __opentoonzScriptEngine.centerlineVectorizerCreate();
  };

  Object.defineProperty(CenterlineVectorizer.prototype, "id", {
    get: function() {
      return this.__centerlineVectorizerId;
    }
  });

  CenterlineVectorizer.prototype.dispose = function() {
    if (this.__centerlineVectorizerId >= 0)
      __opentoonzScriptEngine.centerlineVectorizerDelete(
        this.__centerlineVectorizerId);
    this.__centerlineVectorizerId = -1;
  };

  CenterlineVectorizer.prototype.toString = function() {
    return __opentoonzScriptEngine.centerlineVectorizerToString(
      centerlineVectorizerId(this));
  };

  CenterlineVectorizer.prototype.vectorize = function(value) {
    if (value instanceof Level) {
      if (value.type !== "Raster" && value.type !== "ToonzRaster")
        throw new Error("Can't vectorize a " + value.type + " level");
      if (value.frameCount <= 0)
        throw new Error("Can't vectorize a level with no frames");

      var vectorizer = this;
      return mapLevelFrames(value, function(frame) {
        return vectorizer.vectorize(frame);
      });
    }

    if (!(value instanceof Image)) {
      throw new Error("Bad argument (" + String(value) +
                      "): should be an Image or a Level");
    }
    if (value.type !== "Raster" && value.type !== "ToonzRaster")
      throw new Error("Can't vectorize a " + value.type + " image");

    return imageFromResult(
      __opentoonzScriptEngine.centerlineVectorizerVectorizeImage(
        centerlineVectorizerId(this), imageId(value)));
  };

  defineCenterlineVectorizerProperty("threshold");
  defineCenterlineVectorizerProperty("accuracy");
  defineCenterlineVectorizerProperty("despeckling");
  defineCenterlineVectorizerProperty("maxThickness");
  defineCenterlineVectorizerProperty("thicknessCalibration");
  defineCenterlineVectorizerProperty("preservePaintedAreas");
  defineCenterlineVectorizerProperty("addBorder");
  defineCenterlineVectorizerProperty("eir");

  function defineRasterizerProperty(name) {
    Object.defineProperty(Rasterizer.prototype, name, {
      get: function() {
        return __opentoonzScriptEngine.rasterizerProperty(
          rasterizerId(this), name);
      },
      set: function(value) {
        throwIfError(__opentoonzScriptEngine.rasterizerSetProperty(
          rasterizerId(this), name, value));
      }
    });
  }

  global.Rasterizer = function() {
    this.__rasterizerId = __opentoonzScriptEngine.rasterizerCreate();
  };

  Object.defineProperty(Rasterizer.prototype, "id", {
    get: function() {
      return this.__rasterizerId;
    }
  });

  Rasterizer.prototype.dispose = function() {
    if (this.__rasterizerId >= 0)
      __opentoonzScriptEngine.rasterizerDelete(this.__rasterizerId);
    this.__rasterizerId = -1;
  };

  Rasterizer.prototype.toString = function() {
    return __opentoonzScriptEngine.rasterizerToString(rasterizerId(this));
  };

  Rasterizer.prototype.rasterize = function(value) {
    rasterizerId(this);
    if (value instanceof Level) {
      if (value.type !== "Vector")
        throw new Error("Expected a vector level: " + value.toString());

      var rasterizer = this;
      return mapLevelFrames(value, function(frame) {
        return rasterizer.rasterize(frame);
      });
    }

    if (!(value instanceof Image)) {
      throw new Error("Argument must be a vector level or image : " +
                      String(value));
    }
    if (value.type !== "Vector")
      throw new Error("Expected a vector image: " + value.toString());

    return imageFromResult(__opentoonzScriptEngine.rasterizerRasterizeImage(
      rasterizerId(this), imageId(value)));
  };

  defineRasterizerProperty("colorMapped");
  defineRasterizerProperty("xres");
  defineRasterizerProperty("yres");
  defineRasterizerProperty("dpi");
  defineRasterizerProperty("antialiasing");

  global.Renderer = function() {
    this.__rendererId = ++nextRendererId;
    this.frames = [];
    this.columns = [];
  };

  Object.defineProperty(Renderer.prototype, "id", {
    get: function() {
      return this.__rendererId;
    }
  });

  Renderer.prototype.dispose = function() {
    this.__rendererId = -1;
  };

  Renderer.prototype.toString = function() {
    rendererId(this);
    return "Renderer";
  };

  Renderer.prototype.renderScene = function() {
    rendererId(this);
    rendererSceneId(arguments[0]);
    throwRendererDeferred("renderScene");
  };

  Renderer.prototype.renderFrame = function() {
    rendererId(this);
    rendererSceneId(arguments[0]);
    if (!isNumberArgument(arguments[1]))
      throw new Error("Second argument must be a frame number : " +
                      String(arguments[1]));
    throwRendererDeferred("renderFrame");
  };

  Renderer.prototype.dumpCache = function() {
    rendererId(this);
    throwRendererDeferred("dumpCache");
  };

  global.Level = function(path) {
    this.__levelId = __opentoonzScriptEngine.levelCreate();
    if (arguments.length === 1)
      this.load(path);
  };

  Object.defineProperty(Level.prototype, "id", {
    get: function() {
      return this.__levelId;
    }
  });

  Level.prototype.dispose = function() {
    if (this.__levelId >= 0)
      __opentoonzScriptEngine.levelDelete(this.__levelId);
    this.__levelId = -1;
  };

  Level.prototype.toString = function() {
    return __opentoonzScriptEngine.levelToString(levelId(this));
  };

  Object.defineProperty(Level.prototype, "type", {
    get: function() {
      return __opentoonzScriptEngine.levelType(levelId(this));
    }
  });

  Object.defineProperty(Level.prototype, "frameCount", {
    get: function() {
      return __opentoonzScriptEngine.levelFrameCount(levelId(this));
    }
  });

  Object.defineProperty(Level.prototype, "name", {
    get: function() {
      return __opentoonzScriptEngine.levelName(levelId(this));
    },
    set: function(name) {
      throwIfError(__opentoonzScriptEngine.levelSetName(
        levelId(this), String(name)));
    }
  });

  Object.defineProperty(Level.prototype, "path", {
    get: function() {
      var id = levelId(this);
      if (__opentoonzScriptEngine.levelType(id) === "Empty")
        return undefined;
      return new FilePath(__opentoonzScriptEngine.levelPath(id));
    },
    set: function(path) {
      throwIfError(__opentoonzScriptEngine.levelSetPath(
        levelId(this), filePathArgument(path)));
    }
  });

  Level.prototype.getFrameIds = function() {
    return __opentoonzScriptEngine.levelFrameIds(levelId(this));
  };

  Level.prototype.getFrame = function(fid) {
    return imageFromResult(__opentoonzScriptEngine.levelGetFrame(
      levelId(this), String(fid)));
  };

  Level.prototype.getFrameByIndex = function(index) {
    if (!isNumberArgument(index))
      throw new Error("frame index (" + String(index) + ") must be a number");
    return imageFromResult(__opentoonzScriptEngine.levelGetFrameByIndex(
      levelId(this), index));
  };

  Level.prototype.setFrame = function(fid, image) {
    throwIfError(__opentoonzScriptEngine.levelSetFrame(
      levelId(this), String(fid), imageId(image)));
    return this;
  };

  Level.prototype.load = function(path) {
    throwIfError(__opentoonzScriptEngine.levelLoad(
      levelId(this), filePathArgument(path)));
    return this;
  };

  Level.prototype.save = function(path) {
    throwIfError(__opentoonzScriptEngine.levelSave(
      levelId(this), filePathArgument(path)));
    return this;
  };

  global.Scene = function(path) {
    this.__sceneId = __opentoonzScriptEngine.sceneCreate();
    if (arguments.length === 1)
      this.load(path);
  };

  Object.defineProperty(Scene.prototype, "id", {
    get: function() {
      return this.__sceneId;
    }
  });

  Scene.prototype.dispose = function() {
    if (this.__sceneId >= 0)
      __opentoonzScriptEngine.sceneDelete(this.__sceneId);
    this.__sceneId = -1;
  };

  Scene.prototype.toString = function() {
    return __opentoonzScriptEngine.sceneToString(sceneId(this));
  };

  Object.defineProperty(Scene.prototype, "frameCount", {
    get: function() {
      return __opentoonzScriptEngine.sceneFrameCount(sceneId(this));
    }
  });

  Object.defineProperty(Scene.prototype, "columnCount", {
    get: function() {
      return __opentoonzScriptEngine.sceneColumnCount(sceneId(this));
    }
  });

  Scene.prototype.load = function(path) {
    throwIfError(__opentoonzScriptEngine.sceneLoad(
      sceneId(this), filePathArgument(path)));
    return this;
  };

  Scene.prototype.save = function(path) {
    throwIfError(__opentoonzScriptEngine.sceneSave(
      sceneId(this), filePathArgument(path)));
    return this;
  };

  Scene.prototype.insertColumn = function(column) {
    var columnValue = sceneNumberArgument(column, "Column argument");
    throwIfError(__opentoonzScriptEngine.sceneInsertColumn(
      sceneId(this), columnValue));
    return this;
  };

  Scene.prototype.deleteColumn = function(column) {
    var columnValue = sceneNumberArgument(column, "Column argument");
    throwIfError(__opentoonzScriptEngine.sceneDeleteColumn(
      sceneId(this), columnValue));
    return this;
  };

  Scene.prototype.getCell = function(row, column) {
    var rowValue = sceneNumberArgument(row, "Row argument");
    var columnValue = sceneNumberArgument(column, "Column argument");
    var cell = __opentoonzScriptEngine.sceneGetCell(
      sceneId(this), rowValue, columnValue);
    throwIfError(cell.error || "");
    if (cell.empty)
      return undefined;
    return { level: createLevel(cell.levelId), fid: cell.fid };
  };

  Scene.prototype.setCell = function(row, column, levelOrCell, fid) {
    var rowValue = sceneNumberArgument(row, "Row argument");
    var columnValue = sceneNumberArgument(column, "Column argument");
    var message = "";
    if (arguments.length < 3 ||
        (arguments.length === 3 && levelOrCell === undefined)) {
      message = __opentoonzScriptEngine.sceneClearCell(
        sceneId(this), rowValue, columnValue);
    } else if (arguments.length === 3) {
      if (typeof levelOrCell !== "object" ||
          levelOrCell.level === undefined ||
          levelOrCell.fid === undefined) {
        throw new Error("Third argument should be an object with attributes " +
                        "'level' and 'fid'");
      }
      return this.setCell(row, column, levelOrCell.level, levelOrCell.fid);
    } else if (levelOrCell instanceof Level) {
      message = __opentoonzScriptEngine.sceneSetCell(
        sceneId(this), rowValue, columnValue,
        levelId(levelOrCell), String(fid));
    } else if (isStringArgument(levelOrCell)) {
      message = __opentoonzScriptEngine.sceneSetCellByLevelName(
        sceneId(this), rowValue, columnValue,
        String(levelOrCell), String(fid));
    } else {
      throw new Error(String(levelOrCell) +
                      " : Expected a Level instance or a level name");
    }
    throwIfError(message);
    return this;
  };

  Scene.prototype.getLevels = function() {
    var ids = __opentoonzScriptEngine.sceneLevelIds(sceneId(this));
    var result = [];
    for (var i = 0; i < ids.length; ++i)
      result.push(createLevel(ids[i]));
    return result;
  };

  Scene.prototype.getLevel = function(name) {
    return levelFromResult(__opentoonzScriptEngine.sceneGetLevel(
      sceneId(this), String(name)));
  };

  Scene.prototype.newLevel = function(type, name) {
    return levelFromResult(__opentoonzScriptEngine.sceneNewLevel(
      sceneId(this), String(type), String(name)));
  };

  Scene.prototype.loadLevel = function(name, path) {
    return levelFromResult(__opentoonzScriptEngine.sceneLoadLevel(
      sceneId(this), String(name), filePathArgument(path)));
  };
})(typeof globalThis !== "undefined" ? globalThis : this);
)JS";

void installBootstrap(QJSEngine* engine, ScriptEngine* scriptEngine) {
  QJSEngine::setObjectOwnership(scriptEngine, QJSEngine::CppOwnership);
  engine->globalObject().setProperty(QStringLiteral("__opentoonzScriptEngine"),
                                     engine->newQObject(scriptEngine));
  engine->evaluate(QString::fromLatin1(kBootstrapScript),
                   QStringLiteral("opentoonz-script-bootstrap.js"));
}

bool isVoidResult(QJSEngine* engine, const QJSValue& result) {
  QJSValue voidValue =
      engine->globalObject().property(QStringLiteral("void"));
  return voidValue.isObject() && result.strictlyEquals(voidValue);
}

}  // namespace

//=========================================================

class ScriptEngine::Executor final : public QThread {
  ScriptEngine* m_engine;
  QString m_cmd;

public:
  Executor(ScriptEngine* engine, const QString& cmd)
      : m_engine(engine), m_cmd(cmd) {}

  void run() override {
    m_engine->m_engine->setInterrupted(false);
    m_engine->m_engine->collectGarbage();
    QJSValue result = m_engine->m_engine->evaluate(m_cmd);
    if (result.isError()) {
      m_engine->emitOutput(ScriptEngine::SyntaxError, formatError(result, ""));
    } else if (isVoidResult(m_engine->m_engine, result)) {
    } else if (result.isUndefined()) {
      m_engine->emitOutput(ScriptEngine::UndefinedEvaluationResult,
                           "undefined");
    } else {
      m_engine->emitOutput(ScriptEngine::EvaluationResult, print(result, true));
    }
  }
};

class ScriptEngine::MainThreadEvaluationData {};

//=========================================================

ScriptEngine::ScriptEngine() : m_executor(0), m_engine(new QJSEngine()) {
  // I must call TRenderer::initialize(), because a script could cause a
  // rendering driven by a working thread
  TRenderer::initialize();

  m_mainThreadEvaluationData = new MainThreadEvaluationData();
  installBootstrap(m_engine, this);
}

ScriptEngine::~ScriptEngine() {
  clearQjsRasterizers();
  clearQjsCenterlineVectorizers();
  clearQjsOutlineVectorizers();
  clearQjsImageBuilders();
  clearQjsTransforms();
  clearQjsImages();
  clearQjsLevels();
  clearQjsScenes();
  delete m_mainThreadEvaluationData;
  delete m_engine;
}

void ScriptEngine::emitScriptOutput(int type, const QString& value) {
  const OutputType outputType = type == Warning ? Warning : SimpleText;
  emitOutput(outputType, value);
  sleepMilliseconds(50);
}

QVariantMap ScriptEngine::readScriptFile(const QString& path) {
  QVariantMap result;
  TFilePath fp(path.toStdWString());
  if (!fp.isAbsolute()) {
    fp = ToonzFolder::getLibraryFolder() + "scripts" + fp;
  }

  const QString fpStr = QString::fromStdWString(fp.getWideString());
  result.insert(QStringLiteral("ok"), false);
  result.insert(QStringLiteral("path"), fpStr);

  QFile file(fpStr);
  if (!file.open(QIODevice::ReadOnly)) {
    result.insert(QStringLiteral("error"), "can't read file " + fpStr);
    return result;
  }

  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  result.insert(QStringLiteral("ok"), true);
  result.insert(QStringLiteral("content"), content);
  return result;
}

QString ScriptEngine::runScriptFile(const QString& path) {
  const QVariantMap script = readScriptFile(path);
  if (!script.value(QStringLiteral("ok")).toBool()) {
    emitOutput(ExecutionError, script.value(QStringLiteral("error")).toString());
    return QString();
  }

  const QString fpStr   = script.value(QStringLiteral("path")).toString();
  const QString content = script.value(QStringLiteral("content")).toString();
  QJSValue result       = m_engine->evaluate(content, fpStr);
  if (result.isError()) {
    emitOutput(SyntaxError, formatError(result, fpStr));
    return QString();
  }
  if (isVoidResult(m_engine, result)) return QString();
  if (result.isUndefined()) return QString();
  return print(result, true);
}

QString ScriptEngine::filePathToString(const QString& path) const {
  return tr("\"%1\"").arg(path);
}

QString ScriptEngine::filePathExtension(const QString& path) const {
  return QString::fromStdString(TFilePath(path.toStdWString()).getType());
}

QString ScriptEngine::filePathWithExtension(const QString& path,
                                            const QString& extension) const {
  return QString::fromStdWString(TFilePath(path.toStdWString())
                                     .withType(extension.toStdString())
                                     .getWideString());
}

QString ScriptEngine::filePathName(const QString& path) const {
  return QString::fromStdString(TFilePath(path.toStdWString()).getName());
}

QString ScriptEngine::filePathWithName(const QString& path,
                                       const QString& name) const {
  return QString::fromStdWString(TFilePath(path.toStdWString())
                                     .withName(name.toStdString())
                                     .getWideString());
}

QString ScriptEngine::filePathParentDirectory(const QString& path) const {
  return QString::fromStdWString(
      TFilePath(path.toStdWString()).getParentDir().getWideString());
}

QString ScriptEngine::filePathWithParentDirectory(
    const QString& path, const QString& parentDirectory) const {
  return QString::fromStdWString(
      TFilePath(path.toStdWString())
          .withParentDir(TFilePath(parentDirectory.toStdWString()))
          .getWideString());
}

bool ScriptEngine::filePathExists(const QString& path) const {
  return QFile(path).exists();
}

bool ScriptEngine::filePathIsDirectory(const QString& path) const {
  return QFileInfo(path).isDir();
}

bool ScriptEngine::filePathIsAbsolute(const QString& path) const {
  return TFilePath(path.toStdWString()).isAbsolute();
}

double ScriptEngine::filePathLastModified(const QString& path) const {
  const QDateTime lastModified = QFileInfo(path).lastModified();
  if (!lastModified.isValid()) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return static_cast<double>(lastModified.toMSecsSinceEpoch());
}

QString ScriptEngine::filePathConcat(const QString& path,
                                     const QString& value) const {
  return QString::fromStdWString(
      (TFilePath(path.toStdWString()) + TFilePath(value.toStdWString()))
          .getWideString());
}

QStringList ScriptEngine::filePathFiles(const QString& path) const {
  QStringList result;
  TFilePathSet fpset;
  try {
    TSystem::readDirectory(fpset, TFilePath(path.toStdWString()));
    for (TFilePathSet::iterator it = fpset.begin(); it != fpset.end(); ++it) {
      result.append(QString::fromStdWString(it->getWideString()));
    }
  } catch (...) {
  }
  return result;
}

ToonzScene* ScriptEngine::qjsScene(int sceneId) const {
  return m_qjsScenes.value(sceneId, nullptr);
}

TXshSimpleLevel* ScriptEngine::qjsLevel(int levelId) const {
  return m_qjsLevels.value(levelId, nullptr);
}

TImage* ScriptEngine::qjsImage(int imageId) const {
  return m_qjsImages.value(imageId, nullptr);
}

TImage* ScriptEngine::qjsImageBuilder(int imageBuilderId) const {
  return m_qjsImageBuilders.value(imageBuilderId, nullptr);
}

ToonzScene* ScriptEngine::levelScene(int levelId) const {
  const int sceneId = m_qjsLevelScenes.value(levelId, -1);
  if (sceneId >= 0) return qjsScene(sceneId);
  return m_qjsLevelOwnedScenes.value(levelId, nullptr);
}

ToonzScene* ScriptEngine::ensureLevelScene(int levelId) {
  const int sceneId = m_qjsLevelScenes.value(levelId, -1);
  if (sceneId >= 0) return qjsScene(sceneId);

  ToonzScene* scene = m_qjsLevelOwnedScenes.value(levelId, nullptr);
  if (!scene) {
    scene = new ToonzScene();
    TProjectManager::instance()->initializeScene(scene);
    m_qjsLevelOwnedScenes.insert(levelId, scene);
  }
  return scene;
}

int ScriptEngine::levelCreate(TXshSimpleLevel* level, int sceneId) {
  const int levelId = ++m_nextQjsLevelId;
  if (level) level->addRef();
  m_qjsLevels.insert(levelId, level);
  m_qjsLevelScenes.insert(levelId, sceneId);
  if (!level && sceneId < 0) {
    ToonzScene* scene = new ToonzScene();
    TProjectManager::instance()->initializeScene(scene);
    m_qjsLevelOwnedScenes.insert(levelId, scene);
  }
  return levelId;
}

void ScriptEngine::levelAssign(int levelId, TXshSimpleLevel* level) {
  TXshSimpleLevel* previous = m_qjsLevels.value(levelId, nullptr);
  if (level) level->addRef();
  if (previous) previous->release();
  m_qjsLevels.insert(levelId, level);
}

QVariantMap ScriptEngine::levelResult(int levelId, const QString& error) const {
  QVariantMap result;
  result.insert(QStringLiteral("id"), levelId);
  result.insert(QStringLiteral("error"), error);
  return result;
}

int ScriptEngine::imageCreate(TImage* image) {
  const int imageId = ++m_nextQjsImageId;
  if (image) image->addRef();
  m_qjsImages.insert(imageId, image);
  return imageId;
}

void ScriptEngine::imageAssign(int imageId, TImage* image) {
  TImage* previous = m_qjsImages.value(imageId, nullptr);
  if (image) image->addRef();
  if (previous) previous->release();
  m_qjsImages.insert(imageId, image);
}

QVariantMap ScriptEngine::imageResult(int imageId, const QString& error) const {
  QVariantMap result;
  result.insert(QStringLiteral("id"), imageId);
  result.insert(QStringLiteral("error"), error);
  return result;
}

int ScriptEngine::transformCreate(const QVariantList& transform) {
  const int transformId = ++m_nextQjsTransformId;
  m_qjsTransforms.insert(transformId, transform);
  return transformId;
}

QVariantMap ScriptEngine::transformResult(int transformId,
                                          const QString& error) const {
  QVariantMap result;
  result.insert(QStringLiteral("id"), transformId);
  result.insert(QStringLiteral("error"), error);
  return result;
}

int ScriptEngine::imageBuilderCreate(TImage* image, int width, int height) {
  const int imageBuilderId = ++m_nextQjsImageBuilderId;
  if (image) image->addRef();
  m_qjsImageBuilders.insert(imageBuilderId, image);
  m_qjsImageBuilderWidths.insert(imageBuilderId, width);
  m_qjsImageBuilderHeights.insert(imageBuilderId, height);
  return imageBuilderId;
}

void ScriptEngine::imageBuilderAssign(int imageBuilderId, TImage* image) {
  TImage* previous = m_qjsImageBuilders.value(imageBuilderId, nullptr);
  if (image) image->addRef();
  if (previous) previous->release();
  m_qjsImageBuilders.insert(imageBuilderId, image);
}

QVariantMap ScriptEngine::imageBuilderResult(int imageBuilderId,
                                             const QString& error) const {
  QVariantMap result;
  result.insert(QStringLiteral("id"), imageBuilderId);
  result.insert(QStringLiteral("error"), error);
  return result;
}

QVariantMap ScriptEngine::outlineVectorizerState(
    int outlineVectorizerId) const {
  return m_qjsOutlineVectorizers.value(outlineVectorizerId, QVariantMap());
}

QVariantMap ScriptEngine::centerlineVectorizerState(
    int centerlineVectorizerId) const {
  return m_qjsCenterlineVectorizers.value(centerlineVectorizerId,
                                          QVariantMap());
}

QVariantMap ScriptEngine::rasterizerState(int rasterizerId) const {
  return m_qjsRasterizers.value(rasterizerId, QVariantMap());
}

void ScriptEngine::clearQjsLevels() {
  QList<int> levelIds = m_qjsLevels.keys();
  for (int levelId : std::as_const(levelIds)) {
    levelDelete(levelId);
  }
}

void ScriptEngine::clearQjsLevelsForScene(int sceneId) {
  QList<int> levelIds;
  for (auto it = m_qjsLevelScenes.constBegin();
       it != m_qjsLevelScenes.constEnd(); ++it) {
    if (it.value() == sceneId) levelIds.append(it.key());
  }

  for (int levelId : std::as_const(levelIds)) {
    levelDelete(levelId);
  }
}

void ScriptEngine::clearQjsScenes() {
  qDeleteAll(m_qjsScenes);
  m_qjsScenes.clear();
}

void ScriptEngine::clearQjsImages() {
  for (TImage* image : std::as_const(m_qjsImages)) {
    if (image) image->release();
  }
  m_qjsImages.clear();
}

void ScriptEngine::clearQjsTransforms() { m_qjsTransforms.clear(); }

void ScriptEngine::clearQjsImageBuilders() {
  for (TImage* image : std::as_const(m_qjsImageBuilders)) {
    if (image) image->release();
  }
  m_qjsImageBuilders.clear();
  m_qjsImageBuilderWidths.clear();
  m_qjsImageBuilderHeights.clear();
}

void ScriptEngine::clearQjsOutlineVectorizers() {
  m_qjsOutlineVectorizers.clear();
}

void ScriptEngine::clearQjsCenterlineVectorizers() {
  m_qjsCenterlineVectorizers.clear();
}

void ScriptEngine::clearQjsRasterizers() { m_qjsRasterizers.clear(); }

int ScriptEngine::sceneCreate() {
  ToonzScene* scene = new ToonzScene();
  TProjectManager::instance()->initializeScene(scene);

  const int sceneId = ++m_nextQjsSceneId;
  m_qjsScenes.insert(sceneId, scene);
  return sceneId;
}

void ScriptEngine::sceneDelete(int sceneId) {
  clearQjsLevelsForScene(sceneId);
  delete m_qjsScenes.take(sceneId);
}

QString ScriptEngine::sceneToString(int sceneId) const {
  ToonzScene* scene = qjsScene(sceneId);
  if (!scene) return QStringLiteral("Scene (invalid)");
  return QStringLiteral("Scene (%1 frames)").arg(scene->getFrameCount());
}

int ScriptEngine::sceneFrameCount(int sceneId) const {
  ToonzScene* scene = qjsScene(sceneId);
  return scene ? scene->getFrameCount() : -1;
}

int ScriptEngine::sceneColumnCount(int sceneId) const {
  ToonzScene* scene = qjsScene(sceneId);
  return scene ? scene->getXsheet()->getColumnCount() : -1;
}

QString ScriptEngine::sceneLoad(int sceneId, const QString& path) {
  ToonzScene* scene = qjsScene(sceneId);
  if (!scene) return QStringLiteral("Invalid Scene object");

  TFilePath fp(path.toStdWString());
  if (!fp.isAbsolute()) {
    fp = TProjectManager::instance()->getCurrentProject()->getScenesPath() + fp;
  }

  try {
    if (!TSystem::doesExistFileOrLevel(fp)) {
      return tr("File %1 doesn't exist").arg(path);
    }
    scene->load(fp);
  } catch (...) {
    return tr("Exception reading %1").arg(path);
  }
  return QString();
}

QString ScriptEngine::sceneSave(int sceneId, const QString& path) {
  ToonzScene* scene = qjsScene(sceneId);
  if (!scene) return QStringLiteral("Invalid Scene object");

  TFilePath fp(path.toStdWString());
  if (!fp.isAbsolute()) {
    fp = TProjectManager::instance()->getCurrentProject()->getScenesPath() + fp;
  }

  try {
    // Qt 6 headless script smokes validate scene data I/O here. Scene icon
    // generation still crosses the offscreen renderer/backend boundary.
    scene->save(fp, nullptr, false);
  } catch (...) {
    return tr("Exception writing %1").arg(path);
  }
  return QString();
}

QString ScriptEngine::sceneInsertColumn(int sceneId, int column) {
  ToonzScene* scene = qjsScene(sceneId);
  if (!scene) return QStringLiteral("Invalid Scene object");
  if (column < 0) return tr("Bad column value: %1").arg(column);

  scene->getXsheet()->insertColumn(column);
  return QString();
}

QString ScriptEngine::sceneDeleteColumn(int sceneId, int column) {
  ToonzScene* scene = qjsScene(sceneId);
  if (!scene) return QStringLiteral("Invalid Scene object");
  if (column < 0) return tr("Bad column value: %1").arg(column);

  scene->getXsheet()->removeColumn(column);
  return QString();
}

QVariantMap ScriptEngine::sceneGetCell(int sceneId, int row, int column) {
  QVariantMap result;
  result.insert(QStringLiteral("empty"), true);
  result.insert(QStringLiteral("error"), QString());

  ToonzScene* scene = qjsScene(sceneId);
  if (!scene) {
    result.insert(QStringLiteral("error"),
                  QStringLiteral("Invalid Scene object"));
    return result;
  }
  if (row < 0 || column < 0) {
    result.insert(QStringLiteral("error"),
                  QStringLiteral("Bad row/col values"));
    return result;
  }

  const TXshCell& cell   = scene->getXsheet()->getCell(row, column);
  TXshSimpleLevel* level = cell.getSimpleLevel();
  if (!level) return result;

  result.insert(QStringLiteral("empty"), false);
  result.insert(QStringLiteral("levelId"), levelCreate(level, sceneId));
  result.insert(QStringLiteral("fid"), scriptFrameIdValue(cell.m_frameId));
  return result;
}

QString ScriptEngine::sceneSetCell(int sceneId, int row, int column,
                                   int levelId, const QString& fid) {
  ToonzScene* scene = qjsScene(sceneId);
  if (!scene) return QStringLiteral("Invalid Scene object");
  if (row < 0 || column < 0) return QStringLiteral("Bad row/col values");

  TXshSimpleLevel* level = qjsLevel(levelId);
  if (!level || m_qjsLevelScenes.value(levelId, -1) != sceneId) {
    return tr("Level is not included in the scene");
  }

  QString error;
  TFrameId frameId;
  if (!frameIdFromString(fid, frameId, error)) return error;

  scene->getXsheet()->setCell(row, column, TXshCell(level, frameId));
  return QString();
}

QString ScriptEngine::sceneSetCellByLevelName(int sceneId, int row, int column,
                                              const QString& levelName,
                                              const QString& fid) {
  ToonzScene* scene = qjsScene(sceneId);
  if (!scene) return QStringLiteral("Invalid Scene object");
  if (row < 0 || column < 0) return QStringLiteral("Bad row/col values");

  TXshLevel* level = scene->getLevelSet()->getLevel(levelName.toStdWString());
  if (!level) {
    return tr("Level '%1' is not included in the scene").arg(levelName);
  }

  QString error;
  TFrameId frameId;
  if (!frameIdFromString(fid, frameId, error)) return error;

  scene->getXsheet()->setCell(row, column, TXshCell(level, frameId));
  return QString();
}

QString ScriptEngine::sceneClearCell(int sceneId, int row, int column) {
  ToonzScene* scene = qjsScene(sceneId);
  if (!scene) return QStringLiteral("Invalid Scene object");
  if (row < 0 || column < 0) return QStringLiteral("Bad row/col values");

  scene->getXsheet()->setCell(row, column, TXshCell());
  return QString();
}

QVariantList ScriptEngine::sceneLevelIds(int sceneId) {
  QVariantList result;
  ToonzScene* scene = qjsScene(sceneId);
  if (!scene) return result;

  std::vector<TXshLevel*> levels;
  scene->getLevelSet()->listLevels(levels);
  for (TXshLevel* level : levels) {
    TXshSimpleLevel* simpleLevel = level ? level->getSimpleLevel() : nullptr;
    if (simpleLevel) result.append(levelCreate(simpleLevel, sceneId));
  }
  return result;
}

QVariantMap ScriptEngine::sceneGetLevel(int sceneId, const QString& name) {
  ToonzScene* scene = qjsScene(sceneId);
  if (!scene) return levelResult(-1, QStringLiteral("Invalid Scene object"));

  TXshLevel* level = scene->getLevelSet()->getLevel(name.toStdWString());
  TXshSimpleLevel* simpleLevel = level ? level->getSimpleLevel() : nullptr;
  if (!simpleLevel) return levelResult(-1);
  return levelResult(levelCreate(simpleLevel, sceneId));
}

QVariantMap ScriptEngine::sceneNewLevel(int sceneId, const QString& type,
                                        const QString& name) {
  ToonzScene* scene = qjsScene(sceneId);
  if (!scene) return levelResult(-1, QStringLiteral("Invalid Scene object"));

  const int levelType = levelTypeFromName(type);
  if (levelType == NO_XSHLEVEL) {
    return levelResult(
        -1, tr("Bad level type (%1): must be Vector,Raster or ToonzRaster")
                .arg(type));
  }

  if (scene->getLevelSet()->hasLevel(name.toStdWString())) {
    return levelResult(
        -1, tr("Can't add the level: name(%1) is already used").arg(name));
  }

  TXshLevel* level = scene->createNewLevel(levelType, name.toStdWString());
  TXshSimpleLevel* simpleLevel = level ? level->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    return levelResult(-1, tr("Could not create level %1").arg(name));
  }

  simpleLevel->setDirtyFlag(true);
  return levelResult(levelCreate(simpleLevel, sceneId));
}

QVariantMap ScriptEngine::sceneLoadLevel(int sceneId, const QString& name,
                                         const QString& path) {
  ToonzScene* scene = qjsScene(sceneId);
  if (!scene) return levelResult(-1, QStringLiteral("Invalid Scene object"));

  if (scene->getLevelSet()->hasLevel(name.toStdWString())) {
    return levelResult(
        -1, tr("Can't add the level: name(%1) is already used").arg(name));
  }

  TFilePath fp(path.toStdWString());
  const TFilePath decodedPath = scene->decodeFilePath(fp);
  const TFileType::Type type  = TFileType::getInfo(fp);
  if ((type & TFileType::VIEWABLE) == 0) {
    return levelResult(
        -1, tr("Can't load this kind of file as a level : %1").arg(path));
  }

  try {
    if (!TSystem::doesExistFileOrLevel(decodedPath)) {
      return levelResult(-1, tr("File %1 doesn't exist").arg(path));
    }

    TXshLevel* level = scene->loadLevel(fp, nullptr, name.toStdWString());
    TXshSimpleLevel* simpleLevel = level ? level->getSimpleLevel() : nullptr;
    if (!simpleLevel) {
      return levelResult(-1, tr("Could not load level %1").arg(path));
    }

    return levelResult(levelCreate(simpleLevel, sceneId));
  } catch (...) {
    return levelResult(-1, tr("Exception reading %1").arg(path));
  }
}

int ScriptEngine::levelCreate() { return levelCreate(nullptr, -1); }

void ScriptEngine::levelDelete(int levelId) {
  ToonzScene* ownedScene = m_qjsLevelOwnedScenes.take(levelId);
  TXshSimpleLevel* level = m_qjsLevels.take(levelId);
  m_qjsLevelScenes.remove(levelId);
  delete ownedScene;
  if (level) level->release();
}

QString ScriptEngine::levelToString(int levelId) const {
  TXshSimpleLevel* level = qjsLevel(levelId);
  if (!level) return QStringLiteral("Empty level");

  QString info = QStringLiteral("(");
  QString comma;
  const QString name = levelName(levelId);
  if (!name.isEmpty()) {
    info.append(comma).append(name);
    comma = QStringLiteral(", ");
  }

  info.append(comma).append(tr("%1 frames").arg(level->getFrameCount()));
  info.append(QStringLiteral(")"));

  switch (level->getType()) {
  case PLI_XSHLEVEL:
    return QStringLiteral("Vector level %1").arg(info);
  case TZP_XSHLEVEL:
    return QStringLiteral("Toonz level %1").arg(info);
  case OVL_XSHLEVEL:
    return QStringLiteral("Raster level %1").arg(info);
  default:
    return QStringLiteral("Level %1").arg(info);
  }
}

QString ScriptEngine::levelType(int levelId) const {
  TXshSimpleLevel* level = qjsLevel(levelId);
  return level ? levelTypeName(level->getType()) : levelTypeName(NO_XSHLEVEL);
}

int ScriptEngine::levelFrameCount(int levelId) const {
  TXshSimpleLevel* level = qjsLevel(levelId);
  return level ? level->getFrameCount() : 0;
}

QString ScriptEngine::levelName(int levelId) const {
  TXshSimpleLevel* level = qjsLevel(levelId);
  return level ? QString::fromStdWString(level->getName()) : QString();
}

QString ScriptEngine::levelSetName(int levelId, const QString& name) {
  if (!m_qjsLevels.contains(levelId))
    return QStringLiteral("Invalid Level object");
  TXshSimpleLevel* level = qjsLevel(levelId);
  if (!level) return QString();
  level->setName(name.toStdWString());
  return QString();
}

QString ScriptEngine::levelPath(int levelId) const {
  TXshSimpleLevel* level = qjsLevel(levelId);
  if (!level) return QString();
  return QString::fromStdWString(level->getPath().getWideString());
}

QString ScriptEngine::levelSetPath(int levelId, const QString& path) {
  TXshSimpleLevel* level = qjsLevel(levelId);
  if (!level) return QStringLiteral("Cannot set path on empty level");

  TFilePath fp(path.toStdWString());
  level->setPath(fp);
  try {
    level->load();
  } catch (...) {
    return tr("Exception loading level (%1)").arg(path);
  }
  return QString();
}

QStringList ScriptEngine::levelFrameIds(int levelId) const {
  QStringList result;
  TXshSimpleLevel* level = qjsLevel(levelId);
  if (!level || level->getFrameCount() == 0) return result;

  std::vector<TFrameId> fids;
  level->getFids(fids);
  for (const TFrameId& fid : fids) {
    result.append(QString::fromStdString(fid.expand()));
  }
  return result;
}

QVariantMap ScriptEngine::levelGetFrame(int levelId, const QString& fid) {
  TXshSimpleLevel* level = qjsLevel(levelId);
  if (!level)
    return imageResult(-1, QStringLiteral("An empty level has no frames"));
  if (level->getFrameCount() == 0)
    return imageResult(-1, QStringLiteral("An empty level has no frames"));

  QString error;
  TFrameId frameId;
  if (!frameIdFromString(fid, frameId, error)) return imageResult(-1, error);

  TImageP image = loadScriptLevelFrame(level, frameId);
  if (!image) return imageResult(-1);
  return imageResult(imageCreate(image.getPointer()));
}

QVariantMap ScriptEngine::levelGetFrameByIndex(int levelId, int index) {
  TXshSimpleLevel* level = qjsLevel(levelId);
  if (!level)
    return imageResult(-1, QStringLiteral("An empty level has no frames"));
  if (level->getFrameCount() == 0)
    return imageResult(-1, QStringLiteral("An empty level has no frames"));
  if (index < 0 || index >= level->getFrameCount()) {
    return imageResult(-1, tr("frame index (%1) is out of range (0-%2)")
                               .arg(index)
                               .arg(level->getFrameCount() - 1));
  }

  TFrameId frameId = level->index2fid(index);
  TImageP image    = loadScriptLevelFrame(level, frameId);
  if (!image) return imageResult(-1);
  return imageResult(imageCreate(image.getPointer()));
}

QString ScriptEngine::levelSetFrame(int levelId, const QString& fid,
                                    int imageId) {
  if (!m_qjsLevels.contains(levelId))
    return QStringLiteral("Invalid Level object");

  TXshSimpleLevel* level = qjsLevel(levelId);

  TImageP image(qjsImage(imageId));
  if (!image) {
    return tr("second argument (%1) is not an image").arg(imageId);
  }

  QString error;
  TFrameId frameId;
  if (!frameIdFromString(fid, frameId, error)) return error;

  const int imageLevelType = levelTypeFromImage(image.getPointer());
  if (imageLevelType == NO_XSHLEVEL) {
    return tr("cannot insert a %1 image into a level")
        .arg(imageTypeName(image.getPointer()));
  }

  if (!level) {
    ToonzScene* scene = ensureLevelScene(levelId);
    if (!scene) return QStringLiteral("Invalid Level object");

    TXshLevel* xshLevel = scene->createNewLevel(imageLevelType);
    level               = xshLevel ? xshLevel->getSimpleLevel() : nullptr;
    if (!level) {
      return tr("Could not create a %1 level")
          .arg(imageTypeName(image.getPointer()));
    }

    levelAssign(levelId, level);
    if (imageLevelType != PLI_XSHLEVEL) {
      LevelProperties* properties = level->getProperties();
      const double dpi            = imageDpi(imageId);
      properties->setDpiPolicy(LevelProperties::DP_ImageDpi);
      properties->setDpi(dpi);
      properties->setImageDpi(TPointD(dpi, dpi));
      properties->setImageRes(
          TDimension(imageWidth(imageId), imageHeight(imageId)));
    }
  } else if (level->getType() != imageLevelType) {
    return tr("cannot insert a %1 image to a %2 level")
        .arg(imageTypeName(image.getPointer()),
             levelTypeName(level->getType()));
  }

  if (level->getFrameCount() == 0) {
    level->setPalette(image->getPalette());
  }
  level->setFrame(frameId, image);
  level->setDirtyFlag(true);
  return QString();
}

QString ScriptEngine::levelLoad(int levelId, const QString& path) {
  if (!m_qjsLevels.contains(levelId))
    return QStringLiteral("Invalid Level object");

  ToonzScene* scene = ensureLevelScene(levelId);
  if (!scene) return QStringLiteral("Invalid Level object");

  TXshSimpleLevel* oldLevel = qjsLevel(levelId);
  if (oldLevel) {
    scene->getLevelSet()->removeLevel(oldLevel, true);
    levelAssign(levelId, nullptr);
  }

  TFilePath fp(path.toStdWString());
  try {
    const TFilePath decodedPath = scene->decodeFilePath(fp);
    if (!TSystem::doesExistFileOrLevel(decodedPath)) {
      return tr("File %1 doesn't exist").arg(path);
    }

    const TFileType::Type fileType = TFileType::getInfo(fp);
    if (!TFileType::isVector(fileType) &&
        !(fileType & TFileType::CMAPPED_IMAGE) &&
        !(fileType & TFileType::RASTER_IMAGE)) {
      return tr("File %1 is unsupported").arg(path);
    }

    TXshLevel* level             = scene->loadLevel(fp);
    TXshSimpleLevel* simpleLevel = level ? level->getSimpleLevel() : nullptr;
    if (!simpleLevel) return tr("Could not load level %1").arg(path);

    levelAssign(levelId, simpleLevel);
  } catch (...) {
    return tr("Exception reading %1").arg(path);
  }

  return QString();
}

QString ScriptEngine::levelSave(int levelId, const QString& path) const {
  TXshSimpleLevel* level = qjsLevel(levelId);
  if (!level || level->getFrameCount() == 0) {
    return tr("Can't save an empty level");
  }

  TFilePath fp(path.toStdWString());
  const TFileType::Type fileType = TFileType::getInfo(fp);

  bool isCompatible = false;
  if (TFileType::isFullColor(fileType)) {
    isCompatible = level->getType() == OVL_XSHLEVEL;
  } else if (TFileType::isVector(fileType)) {
    isCompatible = level->getType() == PLI_XSHLEVEL;
  } else if (fileType & TFileType::CMAPPED_IMAGE) {
    isCompatible = level->getType() == TZP_XSHLEVEL;
  } else {
    return tr("Unrecognized file type: %1").arg(path);
  }

  if (!isCompatible) {
    return tr("Can't save a %1 level to this file type: %2")
        .arg(levelTypeName(level->getType()), path);
  }

  try {
    TFilePath outputPath = fp;
    if (ToonzScene* scene = level->getScene()) {
      outputPath = scene->decodeFilePath(outputPath);
    }

    std::vector<TFrameId> fids;
    level->getFids(fids);
    TLevelWriterP writer(outputPath);
    if (level->getPalette()) writer->setPalette(level->getPalette());

    for (const TFrameId& fid : fids) {
      TImageP image = loadScriptLevelFrame(level, fid);
      if (!image) {
        return tr("Could not read frame %1").arg(
            QString::fromStdString(fid.expand()));
      }
      if (TToonzImageP toonzImage = image) {
        if (TRasterCM32P raster = toonzImage->getRaster()) {
          toonzImage->setSavebox(raster->getBounds());
        }
      }
      writer->getFrameWriter(fid)->save(image);
    }
  } catch (const TSystemException& error) {
    return tr("Exception writing %1")
        .arg(QString::fromStdWString(error.getMessage()));
  } catch (...) {
    return tr("Exception writing %1").arg(path);
  }

  return QString();
}

int ScriptEngine::imageCreate() { return imageCreate(nullptr); }

void ScriptEngine::imageDelete(int imageId) {
  TImage* image = m_qjsImages.take(imageId);
  if (image) image->release();
}

QString ScriptEngine::imageToString(int imageId) const {
  TImage* image = qjsImage(imageId);
  if (!image) return QStringLiteral("Empty image");

  switch (image->getType()) {
  case TImage::RASTER:
    return QStringLiteral("Raster image ( %1 x %2 )")
        .arg(imageWidth(imageId))
        .arg(imageHeight(imageId));
  case TImage::TOONZ_RASTER:
    return QStringLiteral("Toonz raster image ( %1 x %2 )")
        .arg(imageWidth(imageId))
        .arg(imageHeight(imageId));
  case TImage::VECTOR:
    return QStringLiteral("Vector image");
  default:
    return QStringLiteral("Image");
  }
}

QString ScriptEngine::imageType(int imageId) const {
  return imageTypeName(qjsImage(imageId));
}

int ScriptEngine::imageWidth(int imageId) const {
  TImage* image   = qjsImage(imageId);
  TRasterP raster = image ? image->raster() : TRasterP();
  return raster ? raster->getSize().lx : 0;
}

int ScriptEngine::imageHeight(int imageId) const {
  TImage* image   = qjsImage(imageId);
  TRasterP raster = image ? image->raster() : TRasterP();
  return raster ? raster->getSize().ly : 0;
}

double ScriptEngine::imageDpi(int imageId) const {
  TImageP image(qjsImage(imageId));
  if (TRasterImageP ri = image) {
    double dpix = 0.0, dpiy = 0.0;
    ri->getDpi(dpix, dpiy);
    return dpix;
  }
  if (TToonzImageP ti = image) {
    double dpix = 0.0, dpiy = 0.0;
    ti->getDpi(dpix, dpiy);
    return dpix;
  }
  return 0.0;
}

QString ScriptEngine::imageLoad(int imageId, const QString& path) {
  if (!m_qjsImages.contains(imageId))
    return QStringLiteral("Invalid Image object");

  imageAssign(imageId, nullptr);

  TFilePath fp(path.toStdWString());
  try {
    if (!TSystem::doesExistFileOrLevel(fp)) {
      return tr("File %1 doesn't exist").arg(path);
    }

    TImageP image;
    TFileType::Type fileType = TFileType::getInfo(fp);
    if (TFileType::isLevel(fileType)) {
      TLevelReaderP reader(fp);
      TLevelP level = reader->loadInfo();
      if (!level || level->getFrameCount() == 0) {
        return tr("%1 contains no frames").arg(path);
      }

      TFrameId frameId = fp.getFrame();
      if (frameId == TFrameId::NO_FRAME || frameId == TFrameId::EMPTY_FRAME) {
        frameId = level->begin()->first;
      }

      image = reader->getFrameReader(frameId)->load();
      if (!image) return tr("Could not read %1").arg(path);
      image->setPalette(level->getPalette());
      if (level->getFrameCount() > 1 &&
          (fp.getFrame() == TFrameId::EMPTY_FRAME ||
           fp.getFrame() == TFrameId::NO_FRAME)) {
        emitOutput(Warning,
                   tr("Loaded first frame of %1").arg(level->getFrameCount()));
      }
    } else if (!TImageReader::load(fp, image)) {
      return tr("File %1 not found or not readable").arg(path);
    }

    imageAssign(imageId, image.getPointer());
  } catch (...) {
    return tr("Unexpected error while reading image %1").arg(path);
  }

  return QString();
}

QString ScriptEngine::imageSave(int imageId, const QString& path) const {
  TImageP image(qjsImage(imageId));
  if (!image) return QStringLiteral("Can't save an empty image");

  TFilePath fp(path.toStdWString());
  TFileType::Type fileType = TFileType::getInfo(fp);

  bool isCompatible = false;
  if (TFileType::isFullColor(fileType)) {
    isCompatible = image->getType() == TImage::RASTER;
  } else if (TFileType::isVector(fileType)) {
    isCompatible = image->getType() == TImage::VECTOR;
  } else if (fileType & TFileType::CMAPPED_IMAGE) {
    isCompatible = image->getType() == TImage::TOONZ_RASTER;
  } else {
    return tr("Unrecognized file type: %1").arg(path);
  }

  if (!isCompatible) {
    return tr("Can't save a %1 image to this file type: %2")
        .arg(imageTypeName(image.getPointer()), path);
  }

  try {
    if (TFileType::isLevel(fileType)) {
      TLevelP level = new TLevel();
      level->setPalette(image->getPalette());
      level->setFrame(TFrameId(1), image);
      TLevelWriterP writer(fp);
      if (image->getPalette()) writer->setPalette(image->getPalette());
      writer->save(level);
    } else {
      TImageWriterP writer(fp);
      writer->save(image);
    }
  } catch (...) {
    return tr("Unexpected error while writing image %1").arg(path);
  }

  return QString();
}

int ScriptEngine::transformCreate() {
  return transformCreate(transformToValues(TAffine()));
}

void ScriptEngine::transformDelete(int transformId) {
  m_qjsTransforms.remove(transformId);
}

QString ScriptEngine::transformToString(int transformId) const {
  if (!m_qjsTransforms.contains(transformId))
    return QStringLiteral("Invalid Transform object");
  return transformDescription(
      transformFromValues(m_qjsTransforms.value(transformId)));
}

QVariantMap ScriptEngine::transformTranslate(int transformId, double x,
                                             double y) {
  if (!m_qjsTransforms.contains(transformId))
    return transformResult(-1, QStringLiteral("Invalid Transform object"));

  const TAffine aff = transformFromValues(m_qjsTransforms.value(transformId));
  return transformResult(
      transformCreate(transformToValues(TTranslation(x, y) * aff)));
}

QVariantMap ScriptEngine::transformRotate(int transformId, double degrees) {
  if (!m_qjsTransforms.contains(transformId))
    return transformResult(-1, QStringLiteral("Invalid Transform object"));

  const TAffine aff = transformFromValues(m_qjsTransforms.value(transformId));
  return transformResult(
      transformCreate(transformToValues(TRotation(degrees) * aff)));
}

QVariantMap ScriptEngine::transformScale(int transformId, double sx,
                                         double sy) {
  if (!m_qjsTransforms.contains(transformId))
    return transformResult(-1, QStringLiteral("Invalid Transform object"));

  const TAffine aff = transformFromValues(m_qjsTransforms.value(transformId));
  return transformResult(
      transformCreate(transformToValues(TScale(sx, sy) * aff)));
}

QVariantMap ScriptEngine::imageBuilderCreate(int width, int height,
                                             const QString& type) {
  if (width < 0 && height < 0)
    return imageBuilderResult(imageBuilderCreate(nullptr, 0, 0));

  if (width <= 0 || height <= 0)
    return imageBuilderResult(-1, QStringLiteral("Bad size"));

  TImageP image;
  if (type == QStringLiteral("Raster")) {
    image = new TRasterImage(TRaster32P(width, height));
  } else if (type == QStringLiteral("ToonzRaster")) {
    image = new TToonzImage(TRasterCM32P(width, height),
                            TRect(0, 0, width, height));
  } else if (!type.isEmpty()) {
    return imageBuilderResult(
        -1,
        tr("Bad argument (%1): should be 'Raster' or ToonzRaster").arg(type));
  }

  return imageBuilderResult(
      imageBuilderCreate(image.getPointer(), width, height));
}

void ScriptEngine::imageBuilderDelete(int imageBuilderId) {
  TImage* image = m_qjsImageBuilders.take(imageBuilderId);
  if (image) image->release();
  m_qjsImageBuilderWidths.remove(imageBuilderId);
  m_qjsImageBuilderHeights.remove(imageBuilderId);
}

QString ScriptEngine::imageBuilderToString(int imageBuilderId) const {
  if (!m_qjsImageBuilders.contains(imageBuilderId))
    return QStringLiteral("Invalid ImageBuilder object");
  return tr("ImageBuilder(%1 image)")
      .arg(imageBuilderTypeName(qjsImageBuilder(imageBuilderId)));
}

QVariantMap ScriptEngine::imageBuilderImage(int imageBuilderId) {
  if (!m_qjsImageBuilders.contains(imageBuilderId))
    return imageResult(-1, QStringLiteral("Invalid ImageBuilder object"));
  return imageResult(imageCreate(qjsImageBuilder(imageBuilderId)));
}

QString ScriptEngine::imageBuilderClear(int imageBuilderId) {
  if (!m_qjsImageBuilders.contains(imageBuilderId))
    return QStringLiteral("Invalid ImageBuilder object");
  imageBuilderAssign(imageBuilderId, nullptr);
  return QString();
}

QString ScriptEngine::imageBuilderFill(int imageBuilderId,
                                       const QString& colorName) {
  if (!m_qjsImageBuilders.contains(imageBuilderId))
    return QStringLiteral("Invalid ImageBuilder object");

  QColor color = QColor::fromString(colorName);
  if (!color.isValid()) {
    return tr("%1 is not a valid color (valid color names are 'red', "
              "'transparent', '#FF8800', etc.)")
        .arg(colorName);
  }

  const TPixel32 pixel(color.red(), color.green(), color.blue(), color.alpha());
  TImageP image(qjsImageBuilder(imageBuilderId));
  if (image) {
    if (image->getType() != TImage::RASTER)
      return QStringLiteral("Can't fill a non-'Raster' image");
    TRaster32P raster = image->raster();
    if (raster) raster->fill(pixel);
  } else {
    const int width  = m_qjsImageBuilderWidths.value(imageBuilderId, 0);
    const int height = m_qjsImageBuilderHeights.value(imageBuilderId, 0);
    if (width > 0 && height > 0) {
      TRaster32P raster(width, height);
      raster->fill(pixel);
      imageBuilderAssign(imageBuilderId, new TRasterImage(raster));
    }
  }
  return QString();
}

QString ScriptEngine::imageBuilderAdd(int imageBuilderId, int imageId,
                                      int transformId) {
  if (!m_qjsImageBuilders.contains(imageBuilderId))
    return QStringLiteral("Invalid ImageBuilder object");

  TImageP image(qjsImage(imageId));
  if (!image) {
    return tr("Bad argument (%1): should be an Image (not empty)").arg(imageId);
  }

  TAffine aff;
  if (transformId >= 0) {
    if (!m_qjsTransforms.contains(transformId))
      return QStringLiteral("Invalid Transform object");
    aff = transformFromValues(m_qjsTransforms.value(transformId));
  }

  TImageP builderImage(qjsImageBuilder(imageBuilderId));
  if (std::fabs(aff.det()) < 0.001) return QString();
  if (builderImage && builderImage->getType() != image->getType())
    return QStringLiteral("Image type mismatch");

  const int width  = m_qjsImageBuilderWidths.value(imageBuilderId, 0);
  const int height = m_qjsImageBuilderHeights.value(imageBuilderId, 0);

  if (!builderImage && image->getType() != TImage::VECTOR && width > 0 &&
      height > 0) {
    TRasterP raster = image->raster()->create(width, height);
    if (image->getType() == TImage::RASTER) {
      builderImage = TRasterImageP(raster);
    } else if (image->getType() == TImage::TOONZ_RASTER) {
      TRasterCM32P cmRaster = raster;
      builderImage          = TToonzImageP(cmRaster, raster->getBounds());
      builderImage->setPalette(image->getPalette());
    } else {
      return QStringLiteral("Bad image type");
    }
    imageBuilderAssign(imageBuilderId, builderImage.getPointer());
  }

  if (!builderImage && aff.isIdentity()) {
    TImageP clone(image->cloneImage());
    imageBuilderAssign(imageBuilderId, clone.getPointer());
  } else if (image->getType() == TImage::VECTOR) {
    if (!builderImage) {
      TVectorImageP vectorImage = image->cloneImage();
      vectorImage->transform(aff);
      imageBuilderAssign(imageBuilderId, vectorImage.getPointer());
    } else {
      TVectorImageP up = image;
      TVectorImageP dn = builderImage;
      dn->mergeImage(up, aff);
    }
  } else {
    if (image->getType() != TImage::TOONZ_RASTER &&
        image->getType() != TImage::RASTER) {
      return QStringLiteral("Bad image type");
    }

    TRasterP up = image->raster();
    if (!builderImage) {
      TRasterP raster = up->create();
      raster->clear();
      if (image->getType() == TImage::TOONZ_RASTER) {
        TRasterCM32P cmRaster = raster;
        builderImage          = TToonzImageP(
            cmRaster, TRect(0, 0, raster->getLx() - 1, raster->getLy() - 1));
        builderImage->setPalette(image->getPalette());
      } else {
        builderImage = TRasterImageP(raster);
      }
      imageBuilderAssign(imageBuilderId, builderImage.getPointer());
    }

    TRasterP down = builderImage->raster();
    if (aff.isTranslation() && aff.a13 == std::floor(aff.a13) &&
        aff.a23 == std::floor(aff.a23)) {
      const TPoint delta =
          -up->getCenter() + down->getCenter() +
          TPoint(static_cast<int>(aff.a13), static_cast<int>(aff.a23));
      TRop::over(down, up, delta);
    } else {
      const TAffine rasterAff = TTranslation(down->getCenterD()) * aff *
                                TTranslation(-up->getCenterD());
      TRop::over(down, up, rasterAff, TRop::Mitchell);
    }
  }
  return QString();
}

QVariantMap ScriptEngine::toonzRasterConvertImage(int imageId) {
  TImageP image(qjsImage(imageId));
  if (!image) {
    return imageResult(
        -1, tr("Bad argument (%1): should be a raster Image").arg(imageId));
  }

  TRasterImageP rasterImage = image;
  if (!rasterImage) {
    return imageResult(
        -1,
        tr("Can't convert a %1 image").arg(imageTypeName(image.getPointer())));
  }

  try {
    RasterToToonzRasterConverter converter;
    TToonzImageP toonzImage = converter.convert(rasterImage);
    if (!toonzImage) {
      return imageResult(-1, tr("Could not convert raster image"));
    }
    toonzImage->setPalette(converter.getPalette().getPointer());
    return imageResult(imageCreate(toonzImage.getPointer()));
  } catch (...) {
    return imageResult(-1, tr("Unexpected error while converting image"));
  }
}

int ScriptEngine::outlineVectorizerCreate() {
  const int outlineVectorizerId = ++m_nextQjsOutlineVectorizerId;
  m_qjsOutlineVectorizers.insert(outlineVectorizerId,
                                 defaultOutlineVectorizerState());
  return outlineVectorizerId;
}

void ScriptEngine::outlineVectorizerDelete(int outlineVectorizerId) {
  m_qjsOutlineVectorizers.remove(outlineVectorizerId);
}

QString ScriptEngine::outlineVectorizerToString(int outlineVectorizerId) const {
  if (!m_qjsOutlineVectorizers.contains(outlineVectorizerId))
    return QStringLiteral("Invalid OutlineVectorizer object");
  return QStringLiteral("Outline Vectorizer");
}

QVariant ScriptEngine::outlineVectorizerProperty(int outlineVectorizerId,
                                                 const QString& name) const {
  if (!m_qjsOutlineVectorizers.contains(outlineVectorizerId)) return QVariant();
  return outlineVectorizerState(outlineVectorizerId).value(name);
}

QString ScriptEngine::outlineVectorizerSetProperty(int outlineVectorizerId,
                                                   const QString& name,
                                                   const QVariant& value) {
  if (!m_qjsOutlineVectorizers.contains(outlineVectorizerId))
    return QStringLiteral("Invalid OutlineVectorizer object");
  if (!isOutlineVectorizerProperty(name)) {
    return tr("Unknown OutlineVectorizer property: %1").arg(name);
  }

  QVariantMap state = outlineVectorizerState(outlineVectorizerId);
  if (name == QStringLiteral("transparentColor")) {
    const QString colorName = value.toString();
    if (!QColor::fromString(colorName).isValid())
      return tr("Invalid color: %1").arg(colorName);
    state.insert(name, colorName);
  } else if (name == QStringLiteral("preservePaintedAreas")) {
    state.insert(name, value.toBool());
  } else if (name == QStringLiteral("cornerAdherence") ||
             name == QStringLiteral("cornerAngle") ||
             name == QStringLiteral("cornerCurveRadius")) {
    state.insert(name, value.toDouble());
  } else {
    state.insert(name, value.toInt());
  }
  m_qjsOutlineVectorizers.insert(outlineVectorizerId, state);
  return QString();
}

QVariantMap ScriptEngine::outlineVectorizerVectorizeImage(
    int outlineVectorizerId, int imageId) {
  if (!m_qjsOutlineVectorizers.contains(outlineVectorizerId)) {
    return imageResult(-1, QStringLiteral("Invalid OutlineVectorizer object"));
  }

  TImageP source(qjsImage(imageId));
  if (!source) {
    return imageResult(
        -1, tr("Bad argument (%1): should be an Image").arg(imageId));
  }
  if (source->getType() != TImage::RASTER &&
      source->getType() != TImage::TOONZ_RASTER) {
    return imageResult(-1, tr("Can't vectorize a %1 image")
                               .arg(imageTypeName(source.getPointer())));
  }

  NewOutlineConfiguration parameters = outlineConfigurationFromState(
      outlineVectorizerState(outlineVectorizerId));

  TPaletteP palette;
  double dpix = Stage::inch / 72.0;
  double dpiy = Stage::inch / 72.0;
  TPointD center;
  if (TRasterImageP rasterImage = source) {
    rasterImage->getDpi(dpix, dpiy);
    center = rasterImage->getRaster()->getCenterD();
  } else if (TToonzImageP toonzImage = source) {
    toonzImage->getDpi(dpix, dpiy);
    center  = toonzImage->getRaster()->getCenterD();
    palette = toonzImage->getPalette();
  }
  if (!palette) palette = new TPalette();

  TAffine dpiAffine;
  if (dpix != 0.0 && dpiy != 0.0) {
    dpiAffine = TScale(Stage::inch / dpix, Stage::inch / dpiy);
  }
  parameters.m_affine     = dpiAffine * TTranslation(-center);
  parameters.m_thickScale = norm(dpiAffine * TPointD(1, 0));

  try {
    VectorizerCore vectorizer;
    TVectorImageP vectorImage =
        vectorizer.vectorize(source, parameters, palette.getPointer());
    if (!vectorImage) return imageResult(-1, tr("Vectorization failed"));

    vectorImage->setPalette(palette.getPointer());
    return imageResult(imageCreate(vectorImage.getPointer()));
  } catch (...) {
    return imageResult(-1, tr("Vectorization failed"));
  }
}

int ScriptEngine::centerlineVectorizerCreate() {
  const int centerlineVectorizerId = ++m_nextQjsCenterlineVectorizerId;
  m_qjsCenterlineVectorizers.insert(centerlineVectorizerId,
                                    defaultCenterlineVectorizerState());
  return centerlineVectorizerId;
}

void ScriptEngine::centerlineVectorizerDelete(int centerlineVectorizerId) {
  m_qjsCenterlineVectorizers.remove(centerlineVectorizerId);
}

QString ScriptEngine::centerlineVectorizerToString(
    int centerlineVectorizerId) const {
  if (!m_qjsCenterlineVectorizers.contains(centerlineVectorizerId))
    return QStringLiteral("Invalid CenterlineVectorizer object");
  return QStringLiteral("Centerline Vectorizer");
}

QVariant ScriptEngine::centerlineVectorizerProperty(int centerlineVectorizerId,
                                                    const QString& name) const {
  if (!m_qjsCenterlineVectorizers.contains(centerlineVectorizerId))
    return QVariant();
  return centerlineVectorizerState(centerlineVectorizerId).value(name);
}

QString ScriptEngine::centerlineVectorizerSetProperty(
    int centerlineVectorizerId, const QString& name, const QVariant& value) {
  if (!m_qjsCenterlineVectorizers.contains(centerlineVectorizerId))
    return QStringLiteral("Invalid CenterlineVectorizer object");
  if (!isCenterlineVectorizerProperty(name)) {
    return tr("Unknown CenterlineVectorizer property: %1").arg(name);
  }

  QVariantMap state = centerlineVectorizerState(centerlineVectorizerId);
  if (name == QStringLiteral("preservePaintedAreas") ||
      name == QStringLiteral("addBorder") || name == QStringLiteral("eir")) {
    state.insert(name, value.toBool());
  } else if (name == QStringLiteral("maxThickness") ||
             name == QStringLiteral("thicknessCalibration")) {
    state.insert(name, value.toDouble());
  } else {
    state.insert(name, value.toInt());
  }
  m_qjsCenterlineVectorizers.insert(centerlineVectorizerId, state);
  return QString();
}

QVariantMap ScriptEngine::centerlineVectorizerVectorizeImage(
    int centerlineVectorizerId, int imageId) {
  if (!m_qjsCenterlineVectorizers.contains(centerlineVectorizerId)) {
    return imageResult(-1,
                       QStringLiteral("Invalid CenterlineVectorizer object"));
  }

  TImageP source(qjsImage(imageId));
  if (!source) {
    return imageResult(
        -1, tr("Bad argument (%1): should be an Image").arg(imageId));
  }
  if (source->getType() != TImage::RASTER &&
      source->getType() != TImage::TOONZ_RASTER) {
    return imageResult(-1, tr("Can't vectorize a %1 image")
                               .arg(imageTypeName(source.getPointer())));
  }

  CenterlineConfiguration parameters = centerlineConfigurationFromState(
      centerlineVectorizerState(centerlineVectorizerId));

  TPaletteP palette;
  double dpix = Stage::inch / 72.0;
  double dpiy = Stage::inch / 72.0;
  TPointD center;
  if (TRasterImageP rasterImage = source) {
    rasterImage->getDpi(dpix, dpiy);
    center = rasterImage->getRaster()->getCenterD();
  } else if (TToonzImageP toonzImage = source) {
    toonzImage->getDpi(dpix, dpiy);
    center  = toonzImage->getRaster()->getCenterD();
    palette = toonzImage->getPalette();
  }
  if (!palette) palette = new TPalette();

  TAffine dpiAffine;
  if (dpix != 0.0 && dpiy != 0.0) {
    dpiAffine = TScale(Stage::inch / dpix, Stage::inch / dpiy);
  }
  parameters.m_affine     = dpiAffine * TTranslation(-center);
  parameters.m_thickScale = norm(dpiAffine * TPointD(1, 0));

  try {
    VectorizerCore vectorizer;
    TVectorImageP vectorImage =
        vectorizer.vectorize(source, parameters, palette.getPointer());
    if (!vectorImage) return imageResult(-1, tr("Vectorization failed"));

    vectorImage->setPalette(palette.getPointer());
    return imageResult(imageCreate(vectorImage.getPointer()));
  } catch (...) {
    return imageResult(-1, tr("Vectorization failed"));
  }
}

int ScriptEngine::rasterizerCreate() {
  const int rasterizerId = ++m_nextQjsRasterizerId;
  m_qjsRasterizers.insert(rasterizerId, defaultRasterizerState());
  return rasterizerId;
}

void ScriptEngine::rasterizerDelete(int rasterizerId) {
  m_qjsRasterizers.remove(rasterizerId);
}

QString ScriptEngine::rasterizerToString(int rasterizerId) const {
  if (!m_qjsRasterizers.contains(rasterizerId))
    return QStringLiteral("Invalid Rasterizer object");
  return QStringLiteral("Rasterizer");
}

QVariant ScriptEngine::rasterizerProperty(int rasterizerId,
                                          const QString& name) const {
  if (!m_qjsRasterizers.contains(rasterizerId)) return QVariant();
  return rasterizerState(rasterizerId).value(name);
}

QString ScriptEngine::rasterizerSetProperty(int rasterizerId,
                                            const QString& name,
                                            const QVariant& value) {
  if (!m_qjsRasterizers.contains(rasterizerId))
    return QStringLiteral("Invalid Rasterizer object");
  if (!isRasterizerProperty(name)) {
    return tr("Unknown Rasterizer property: %1").arg(name);
  }

  QVariantMap state = rasterizerState(rasterizerId);
  if (name == QStringLiteral("colorMapped") ||
      name == QStringLiteral("antialiasing")) {
    state.insert(name, value.toBool());
  } else if (name == QStringLiteral("dpi")) {
    state.insert(name, value.toDouble());
  } else {
    state.insert(name, value.toInt());
  }
  m_qjsRasterizers.insert(rasterizerId, state);
  return QString();
}

QVariantMap ScriptEngine::rasterizerRasterizeImage(int rasterizerId,
                                                   int imageId) {
  if (!m_qjsRasterizers.contains(rasterizerId)) {
    return imageResult(-1, QStringLiteral("Invalid Rasterizer object"));
  }

  const QVariantMap state = rasterizerState(rasterizerId);
  if (!state.value(QStringLiteral("colorMapped"), false).toBool()) {
    return imageResult(
        -1, tr("Qt 6 script Rasterizer currently supports only colorMapped "
               "vector-to-ToonzRaster rasterization"));
  }

  const int xres   = state.value(QStringLiteral("xres"), 720).toInt();
  const int yres   = state.value(QStringLiteral("yres"), 576).toInt();
  const double dpi = state.value(QStringLiteral("dpi"), 72.0).toDouble();
  if (xres <= 0 || yres <= 0 || dpi <= 0.0) {
    return imageResult(-1, tr("Bad Rasterizer resolution or dpi"));
  }

  TImageP image(qjsImage(imageId));
  TVectorImageP vectorImage = image;
  if (!vectorImage) {
    return imageResult(-1, tr("Expected a vector image: %1")
                               .arg(imageTypeName(image.getPointer())));
  }

  TPalette* palette = vectorImage->getPalette();
  if (!palette) {
    return imageResult(-1, tr("Vector image has no palette"));
  }

  const TDimension res(xres, yres);
  TCamera camera;
  camera.setRes(res);
  camera.setSize(TDimensionD(xres / dpi, yres / dpi));

  try {
    TToonzImageP toonzImage = ToonzImageUtils::vectorToToonzImage(
        vectorImage, camera.getStageToCameraRef(), palette, TPointD(), res, 0,
        true);
    if (!toonzImage) return imageResult(-1, tr("Rasterization failed"));
    toonzImage->setPalette(palette);
    return imageResult(imageCreate(toonzImage.getPointer()));
  } catch (...) {
    return imageResult(-1, tr("Rasterization failed"));
  }
}

void ScriptEngine::evaluate(const QString& cmd) {
  if (m_qjsEvaluating) return;
  m_qjsEvaluating = true;

  m_engine->setInterrupted(false);
  m_engine->collectGarbage();
  QJSValue result = m_engine->evaluate(cmd);
  if (result.isError()) {
    emitOutput(ScriptEngine::SyntaxError, formatError(result, ""));
  } else if (isVoidResult(m_engine, result)) {
  } else if (result.isUndefined()) {
    emitOutput(ScriptEngine::UndefinedEvaluationResult, "undefined");
  } else {
    emitOutput(ScriptEngine::EvaluationResult, print(result, true));
  }

  m_qjsEvaluating = false;
  emit evaluationDone();
}

bool ScriptEngine::wait(unsigned long time) {
  (void)time;
  return !m_qjsEvaluating;
}

bool ScriptEngine::isEvaluating() const { return m_qjsEvaluating; }

void ScriptEngine::interrupt() { m_engine->setInterrupted(true); }

void ScriptEngine::onMainThreadEvaluationPosted() {}

void ScriptEngine::onTerminated() {
  if (!m_executor) return;
  emit evaluationDone();
  delete m_executor;
  m_executor = 0;
}

#endif
