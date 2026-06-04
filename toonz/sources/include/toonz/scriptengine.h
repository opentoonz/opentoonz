#pragma once

#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

// #define SCRIPTENGINE_TEST

#include "tcommon.h"
#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include <QObject>
#include <QMetaType>
#include <QStringList>
#include <QtGlobal>
#include <QVariantList>
#include <QVariantMap>

#ifndef OPENTOONZ_QT_MAJOR
#if defined(QT_VERSION) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define OPENTOONZ_QT_MAJOR 6
#else
#define OPENTOONZ_QT_MAJOR 5
#endif
#endif

class Foo;
class TImage;
class ToonzScene;
class TXshSimpleLevel;
#if OPENTOONZ_QT_MAJOR < 6
class QScriptValue;
class QScriptEngine;
#else
#include <QHash>
class QJSEngine;
class QJSValue;
Q_MOC_INCLUDE("QJSValue")
#endif

class DVAPI ScriptCommand {
public:
  ScriptCommand() {}
  virtual ~ScriptCommand() {}

  virtual void execute() = 0;
};

class DVAPI ScriptEngine final : public QObject {
  Q_OBJECT
#if OPENTOONZ_QT_MAJOR < 6
  QScriptEngine* m_engine;
#else
  QJSEngine* m_engine;
  QHash<int, ToonzScene*> m_qjsScenes;
  QHash<int, TXshSimpleLevel*> m_qjsLevels;
  QHash<int, int> m_qjsLevelScenes;
  QHash<int, ToonzScene*> m_qjsLevelOwnedScenes;
  QHash<int, TImage*> m_qjsImages;
  QHash<int, QVariantList> m_qjsTransforms;
  QHash<int, TImage*> m_qjsImageBuilders;
  QHash<int, int> m_qjsImageBuilderWidths;
  QHash<int, int> m_qjsImageBuilderHeights;
  QHash<int, QVariantMap> m_qjsOutlineVectorizers;
  QHash<int, QVariantMap> m_qjsCenterlineVectorizers;
  QHash<int, QVariantMap> m_qjsRasterizers;
  int m_nextQjsSceneId                = 0;
  int m_nextQjsLevelId                = 0;
  int m_nextQjsImageId                = 0;
  int m_nextQjsTransformId            = 0;
  int m_nextQjsImageBuilderId         = 0;
  int m_nextQjsOutlineVectorizerId    = 0;
  int m_nextQjsCenterlineVectorizerId = 0;
  int m_nextQjsRasterizerId           = 0;
  bool m_qjsEvaluating                = false;
#endif
  class Executor;
  friend class Executor;
  Executor* m_executor;
  class MainThreadEvaluationData;
  MainThreadEvaluationData* m_mainThreadEvaluationData;
#if OPENTOONZ_QT_MAJOR < 6
  QScriptValue* m_voidValue;
#endif

public:
  ScriptEngine();
  ~ScriptEngine();

  void evaluate(const QString& cmd);
  void interrupt();
  bool isEvaluating() const;
  bool wait(unsigned long time = ULONG_MAX);

  enum OutputType {
    SimpleText,
    Warning,
    ExecutionError,
    SyntaxError,
    EvaluationResult,
    UndefinedEvaluationResult
  };
  void emitOutput(OutputType type, const QString& value) {
    emit output((int)type, value);
  }

#if OPENTOONZ_QT_MAJOR < 6
  const QScriptValue& evaluateOnMainThread(const QScriptValue& fun,
                                           const QScriptValue& arguments);

  QScriptEngine* getQScriptEngine() const { return m_engine; }
  const QScriptValue& voidValue() const { return *m_voidValue; }
#else
  QJSEngine* getQJSEngine() const { return m_engine; }

  Q_INVOKABLE void emitScriptOutput(int type, const QString& value);
  Q_INVOKABLE QVariantMap readScriptFile(const QString& path);
  Q_INVOKABLE QJSValue evaluateScriptContent(const QString& content,
                                             const QString& path);
  Q_INVOKABLE QString runScriptFile(const QString& path);
  Q_INVOKABLE QString filePathToString(const QString& path) const;
  Q_INVOKABLE QString filePathExtension(const QString& path) const;
  Q_INVOKABLE QString filePathWithExtension(const QString& path,
                                            const QString& extension) const;
  Q_INVOKABLE QString filePathName(const QString& path) const;
  Q_INVOKABLE QString filePathWithName(const QString& path,
                                       const QString& name) const;
  Q_INVOKABLE QString filePathParentDirectory(const QString& path) const;
  Q_INVOKABLE QString filePathWithParentDirectory(
      const QString& path, const QString& parentDirectory) const;
  Q_INVOKABLE bool filePathExists(const QString& path) const;
  Q_INVOKABLE bool filePathIsDirectory(const QString& path) const;
  Q_INVOKABLE bool filePathIsAbsolute(const QString& path) const;
  Q_INVOKABLE double filePathLastModified(const QString& path) const;
  Q_INVOKABLE QString filePathConcat(const QString& path,
                                     const QString& value) const;
  Q_INVOKABLE QStringList filePathFiles(const QString& path) const;
  Q_INVOKABLE int sceneCreate();
  Q_INVOKABLE void sceneDelete(int sceneId);
  Q_INVOKABLE QString sceneToString(int sceneId) const;
  Q_INVOKABLE int sceneFrameCount(int sceneId) const;
  Q_INVOKABLE int sceneColumnCount(int sceneId) const;
  Q_INVOKABLE QString sceneLoad(int sceneId, const QString& path);
  Q_INVOKABLE QString sceneSave(int sceneId, const QString& path);
  Q_INVOKABLE QString sceneInsertColumn(int sceneId, int column);
  Q_INVOKABLE QString sceneDeleteColumn(int sceneId, int column);
  Q_INVOKABLE QVariantMap sceneGetCell(int sceneId, int row, int column);
  Q_INVOKABLE QString sceneSetCell(int sceneId, int row, int column,
                                   int levelId, const QString& fid);
  Q_INVOKABLE QString sceneSetCellByLevelName(int sceneId, int row, int column,
                                              const QString& levelName,
                                              const QString& fid);
  Q_INVOKABLE QString sceneClearCell(int sceneId, int row, int column);
  Q_INVOKABLE QVariantList sceneLevelIds(int sceneId);
  Q_INVOKABLE QVariantMap sceneGetLevel(int sceneId, const QString& name);
  Q_INVOKABLE QVariantMap sceneNewLevel(int sceneId, const QString& type,
                                        const QString& name);
  Q_INVOKABLE QVariantMap sceneLoadLevel(int sceneId, const QString& name,
                                         const QString& path);
  Q_INVOKABLE int levelCreate();
  Q_INVOKABLE void levelDelete(int levelId);
  Q_INVOKABLE QString levelToString(int levelId) const;
  Q_INVOKABLE QString levelType(int levelId) const;
  Q_INVOKABLE int levelFrameCount(int levelId) const;
  Q_INVOKABLE QString levelName(int levelId) const;
  Q_INVOKABLE QString levelSetName(int levelId, const QString& name);
  Q_INVOKABLE QString levelPath(int levelId) const;
  Q_INVOKABLE QString levelSetPath(int levelId, const QString& path);
  Q_INVOKABLE QStringList levelFrameIds(int levelId) const;
  Q_INVOKABLE QVariantMap levelGetFrame(int levelId, const QString& fid);
  Q_INVOKABLE QVariantMap levelGetFrameByIndex(int levelId, int index);
  Q_INVOKABLE QString levelSetFrame(int levelId, const QString& fid,
                                    int imageId);
  Q_INVOKABLE QString levelLoad(int levelId, const QString& path);
  Q_INVOKABLE QString levelSave(int levelId, const QString& path) const;
  Q_INVOKABLE int imageCreate();
  Q_INVOKABLE void imageDelete(int imageId);
  Q_INVOKABLE QString imageToString(int imageId) const;
  Q_INVOKABLE QString imageType(int imageId) const;
  Q_INVOKABLE int imageWidth(int imageId) const;
  Q_INVOKABLE int imageHeight(int imageId) const;
  Q_INVOKABLE double imageDpi(int imageId) const;
  Q_INVOKABLE QString imageLoad(int imageId, const QString& path);
  Q_INVOKABLE QString imageSave(int imageId, const QString& path) const;
  Q_INVOKABLE int transformCreate();
  Q_INVOKABLE void transformDelete(int transformId);
  Q_INVOKABLE QString transformToString(int transformId) const;
  Q_INVOKABLE QVariantMap transformTranslate(int transformId, double x,
                                             double y);
  Q_INVOKABLE QVariantMap transformRotate(int transformId, double degrees);
  Q_INVOKABLE QVariantMap transformScale(int transformId, double sx, double sy);
  Q_INVOKABLE QVariantMap imageBuilderCreate(int width, int height,
                                             const QString& type);
  Q_INVOKABLE void imageBuilderDelete(int imageBuilderId);
  Q_INVOKABLE QString imageBuilderToString(int imageBuilderId) const;
  Q_INVOKABLE QVariantMap imageBuilderImage(int imageBuilderId);
  Q_INVOKABLE QString imageBuilderClear(int imageBuilderId);
  Q_INVOKABLE QString imageBuilderFill(int imageBuilderId,
                                       const QString& colorName);
  Q_INVOKABLE QString imageBuilderAdd(int imageBuilderId, int imageId,
                                      int transformId);
  Q_INVOKABLE QVariantMap toonzRasterConvertImage(int imageId);
  Q_INVOKABLE int outlineVectorizerCreate();
  Q_INVOKABLE void outlineVectorizerDelete(int outlineVectorizerId);
  Q_INVOKABLE QString outlineVectorizerToString(int outlineVectorizerId) const;
  Q_INVOKABLE QVariant outlineVectorizerProperty(int outlineVectorizerId,
                                                 const QString& name) const;
  Q_INVOKABLE QString outlineVectorizerSetProperty(int outlineVectorizerId,
                                                   const QString& name,
                                                   const QVariant& value);
  Q_INVOKABLE QVariantMap
  outlineVectorizerVectorizeImage(int outlineVectorizerId, int imageId);
  Q_INVOKABLE int centerlineVectorizerCreate();
  Q_INVOKABLE void centerlineVectorizerDelete(int centerlineVectorizerId);
  Q_INVOKABLE QString
  centerlineVectorizerToString(int centerlineVectorizerId) const;
  Q_INVOKABLE QVariant centerlineVectorizerProperty(int centerlineVectorizerId,
                                                    const QString& name) const;
  Q_INVOKABLE QString centerlineVectorizerSetProperty(
      int centerlineVectorizerId, const QString& name, const QVariant& value);
  Q_INVOKABLE QVariantMap
  centerlineVectorizerVectorizeImage(int centerlineVectorizerId, int imageId);
  Q_INVOKABLE int rasterizerCreate();
  Q_INVOKABLE void rasterizerDelete(int rasterizerId);
  Q_INVOKABLE QString rasterizerToString(int rasterizerId) const;
  Q_INVOKABLE QVariant rasterizerProperty(int rasterizerId,
                                          const QString& name) const;
  Q_INVOKABLE QString rasterizerSetProperty(int rasterizerId,
                                            const QString& name,
                                            const QVariant& value);
  Q_INVOKABLE QVariantMap rasterizerRasterizeImage(int rasterizerId,
                                                   int imageId);
  Q_INVOKABLE QVariantMap rendererRenderFrame(int rendererId, int sceneId,
                                              int frame,
                                              const QVariantList& columns);
  Q_INVOKABLE QVariantMap rendererRenderScene(int rendererId, int sceneId,
                                              const QVariantList& frames,
                                              const QVariantList& columns);
  Q_INVOKABLE QVariantMap rendererDumpCache(int rendererId);

  QString installConsoleBridge(QObject* bridge);
  TXshSimpleLevel* scriptLevelForView(int levelId) const;
  TImage* scriptImageForView(int imageId) const;

private:
  ToonzScene* qjsScene(int sceneId) const;
  TXshSimpleLevel* qjsLevel(int levelId) const;
  TImage* qjsImage(int imageId) const;
  TImage* qjsImageBuilder(int imageBuilderId) const;
  ToonzScene* levelScene(int levelId) const;
  ToonzScene* ensureLevelScene(int levelId);
  int levelCreate(TXshSimpleLevel* level, int sceneId);
  void levelAssign(int levelId, TXshSimpleLevel* level);
  QVariantMap levelResult(int levelId, const QString& error = QString()) const;
  int imageCreate(TImage* image);
  void imageAssign(int imageId, TImage* image);
  QVariantMap imageResult(int imageId, const QString& error = QString()) const;
  int transformCreate(const QVariantList& transform);
  QVariantMap transformResult(int transformId,
                              const QString& error = QString()) const;
  int imageBuilderCreate(TImage* image, int width, int height);
  void imageBuilderAssign(int imageBuilderId, TImage* image);
  QVariantMap imageBuilderResult(int imageBuilderId,
                                 const QString& error = QString()) const;
  QVariantMap outlineVectorizerState(int outlineVectorizerId) const;
  QVariantMap centerlineVectorizerState(int centerlineVectorizerId) const;
  QVariantMap rasterizerState(int rasterizerId) const;
  void clearQjsLevels();
  void clearQjsLevelsForScene(int sceneId);
  void clearQjsScenes();
  void clearQjsImages();
  void clearQjsTransforms();
  void clearQjsImageBuilders();
  void clearQjsOutlineVectorizers();
  void clearQjsCenterlineVectorizers();
  void clearQjsRasterizers();
#endif

protected slots:
  void onTerminated();
  void onMainThreadEvaluationPosted();

signals:
  void evaluationDone();
  void output(int type, const QString&);

  void mainThreadEvaluationPosted();
};

Q_DECLARE_METATYPE(ScriptEngine*)

#endif
