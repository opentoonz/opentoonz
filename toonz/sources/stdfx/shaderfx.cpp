

#include "stdfx/shaderfx.h"

// TnzStdfx includes
#include "stdfx.h"
#include "stdfx/shaderinterface.h"
#include "stdfx/shadingcontext.h"

// TnzBase includes
#include "tfxparam.h"
#include "tparamset.h"
#include "trenderresourcemanager.h"
#include "trenderer.h"

// TnzCore includes
#include "tthread.h"
#include "tfilepath.h"
#include "tstream.h"
#include "tfunctorinvoker.h"
#include "tmsgcore.h"

// Qt includes
#include <QDir>
#include <QOpenGLShaderProgram>
#include <QCoreApplication>
#include <QOffscreenSurface>

// Glew include
#include <GL/glew.h>

// tcg includes
#include "tcg/tcg_function_types.h"
#include "tcg/tcg_unique_ptr.h"

// Boost includes
#include <boost/any.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

// Diagnostics include
//#define DIAGNOSTICS
#ifdef DIAGNOSTICS
#include "diagnostics.h"
#endif

//===================================================

//    Forward Declarations

class ShaderFxDeclaration;

//===================================================

//****************************************************************************
//    Local Namespace  stuff
//****************************************************************************

namespace {

// Classes

struct ContextLocker {
  ShadingContext &m_ctx;
  bool m_locked;

public:
  ContextLocker(ShadingContext &ctx) : m_ctx(ctx), m_locked(false) { relock(); }
  ~ContextLocker() {
    if (m_locked) unlock();
  }

  void relock() {
    assert(!m_locked), m_locked = true;
    m_ctx.makeCurrent();
  }

  void unlock() {
    assert(m_locked), m_locked = false;
    m_ctx.doneCurrent();
  }
};

struct ProgramBinder {
  QOpenGLShaderProgram *m_prog;

public:
  ProgramBinder(QOpenGLShaderProgram *prog) : m_prog(prog) { m_prog->bind(); }
  ~ProgramBinder() {
    glUseProgram(0);  // m_prog->release();
  }
};

struct RectF {
  GLfloat m_val[4];
  RectF(GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1) {
    m_val[0] = x0, m_val[1] = y0, m_val[2] = x1, m_val[3] = y1;
  }
  RectF(const TRectD &rect) {
    m_val[0] = rect.x0, m_val[1] = rect.y0, m_val[2] = rect.x1,
    m_val[3] = rect.y1;
  }

  operator TRectD() const {
    return TRectD(m_val[0], m_val[1], m_val[2], m_val[3]);
  }
  bool operator==(const RectF &rect) const {
    return (memcmp(m_val, rect.m_val, sizeof(this)) == 0);
  }
};

struct AffineF {
  GLfloat m_val[9];
  operator TAffine() const {
    return TAffine(m_val[0], m_val[3], m_val[6], m_val[1], m_val[4], m_val[7]);
  }
  // Observe that mat3 from GLSL stores elements column-wise; this explains the
  // weird indexing
};

// Global Variables

typedef std::map<QString, ShaderFxDeclaration *> FxDeclarationsMap;
FxDeclarationsMap l_shaderFxDeclarations;

enum Measures { NONE, PERCENT, LENGTH, ANGLE, MEASURESCOUNT };

static const std::string l_measureNames[MEASURESCOUNT] = {"", "percentage",
                                                          "fxLength", "angle"};

static const TParamUIConcept::Type
    l_conceptTypes[ShaderInterface::CONCEPTSCOUNT -
                   ShaderInterface::UI_CONCEPTS] = {
        TParamUIConcept::RADIUS,  TParamUIConcept::WIDTH,
        TParamUIConcept::ANGLE,   TParamUIConcept::POINT,
        TParamUIConcept::POINT_2, TParamUIConcept::VECTOR,
        TParamUIConcept::POLAR,   TParamUIConcept::SIZE,
        TParamUIConcept::QUAD,    TParamUIConcept::RECT};

// Functions

inline bool isObsolete(const TFilePath &fp, const QDateTime &lastModified) {
  QFileInfo fInfo(QString::fromStdWString(fp.getWideString()));
  return (lastModified != fInfo.lastModified());
}

inline TRectD tileRect(const TTile &tile) {
  const TDimension &dim = tile.getRaster()->getSize();
  return TRectD(tile.m_pos, TDimensionD(dim.lx, dim.ly));
}

inline void ceilRect(TRectD &rect) {
  rect.x0 = tfloor(rect.x0), rect.y0 = tfloor(rect.y0);
  rect.x1 = tceil(rect.x1), rect.y1 = tceil(rect.y1);
}

}  // namespace

//****************************************************************************
//    Shader Fx  declaration
//****************************************************************************

class ShaderFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(ShaderFx)

  const ShaderInterface *m_shaderInterface;  //!< Shader fx 'description'.
  std::vector<boost::any>
      m_params;  //!< Parameters for the shader fx. The actual parameter
                 //!< type depends on the shader interface declaration.
  std::vector<TParamUIConcept>
      m_uiConcepts;  //!< UI concepts related to m_params.
  boost::ptr_vector<TRasterFxPort>
      m_inputPorts;  //!< Input ports for the shader fx.

public:
  ShaderFx() : m_shaderInterface() {
    assert(false);
  }  // Necessary due to TPersist inheritance, but must NOT be used
  ShaderFx(const ShaderInterface *shaderInterface)
      : m_shaderInterface(shaderInterface) {
    initialize();
  }

  // void setShaderInterface(const ShaderInterface& shaderInterface);
  void initialize();

  void getParamUIs(TParamUIConcept *&params, int &length) override;
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;
  bool canHandle(const TRenderSettings &info, double frame) override;

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &ri) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

private:
  QOpenGLShaderProgram *touchShaderProgram(
      const ShaderInterface::ShaderData &sd, ShadingContext &context,
      int varyingsCount = 0, const GLchar **varyings = 0);

  void bindParameters(QOpenGLShaderProgram *shaderProgram, double frame);

  void bindWorldTransform(QOpenGLShaderProgram *shaderProgram,
                          const TAffine &worldToDst);

  void getInputData(const TRectD &rect, double frame, const TRenderSettings &ri,
                    std::vector<TRectD> &inputRects,
                    std::vector<TAffine> &inputAffines,
                    ShadingContext &context);
};

//****************************************************************************
//    ShaderFxDeclaration  definition
//****************************************************************************

class ShaderFxDeclaration final : public TFxDeclaration {
  ShaderInterface m_shaderInterface;

public:
  ShaderFxDeclaration(const ShaderInterface &shaderInterface)
      : TFxDeclaration(
            TFxInfo(shaderInterface.mainShader().m_name.toStdString(), false))
      , m_shaderInterface(shaderInterface) {}

  TPersist *create() const override { return new ShaderFx(&m_shaderInterface); }
};

//****************************************************************************
//    ShadingContextManager  definition
//****************************************************************************

class ShadingContextManager final : public QObject {
  mutable QMutex m_mutex;

  std::unique_ptr<ShadingContext> m_shadingContext;
  TAtomicVar m_activeRenderInstances;
  std::unique_ptr<QOffscreenSurface> m_surface;

public:
  ShadingContextManager() {
    /*
The ShadingContext's QGLPixelBuffer must be destroyed *before* the global
QApplication
is. So, we will attach to a suitable parent object whose lifespan is shorter.

FYI - yes, this approach was adopted after a long and PAINFUL wrestling session
with Qt.
Suggestions are welcome as this is a tad beyond ridiculous...
*/

    QObject *mainScopeBoundObject =
        QCoreApplication::instance()->findChild<QObject *>("mainScope");

    assert(thread() ==
           mainScopeBoundObject
               ->thread());  // Parent object must be in the same thread,
    // setParent(mainScopeBoundObject);  // otherwise reparenting fails
    m_surface.reset(new QOffscreenSurface());
    m_surface->create();
    m_shadingContext.reset(new ShadingContext(m_surface.get()));
  }

  static ShadingContextManager *instance() {
    static ShadingContextManager *theManager = new ShadingContextManager;
    return theManager;
  }

  QMutex *mutex() const { return &m_mutex; }

  const ShadingContext &shadingContext() const { return *m_shadingContext; }
  ShadingContext &shadingContext() { return *m_shadingContext; }

  void onRenderInstanceStart() { ++m_activeRenderInstances; }

  void onRenderInstanceEnd() {
    if (--m_activeRenderInstances == 0) {
      QMutexLocker mLocker(&m_mutex);

      // Release the shading context's output buffer
      ::ContextLocker cLocker(*m_shadingContext);
      m_shadingContext->resize(0, 0);

#ifdef DIAGNOSTICS
      DIAGNOSTICS_DUMP("ShaderLogs");
      DIAGNOSTICS_CLEAR;
#endif
    }
  }

  ShadingContext::Support touchSupport() {
    struct {
      ShadingContextManager *m_this;
      ShadingContext::Support support() {
        QMutexLocker mLocker(&m_this->m_mutex);
        ::ContextLocker cLocker(*m_this->m_shadingContext);

        return ShadingContext::support();
      }
    } locals = {this};

    static ShadingContext::Support sup = locals.support();

    static bool sentMsg = false;
    if (!sentMsg) {
      switch (sup) {
      case ShadingContext::NO_PIXEL_BUFFER:
        DVGui::warning(QOpenGLShaderProgram::tr(
            "This system configuration does not support OpenGL Pixel Buffers. "
            "Shader Fxs will not be able to render."));
        break;

      case ShadingContext::NO_SHADERS:
        DVGui::warning(QOpenGLShaderProgram::tr(
            "This system configuration does not support OpenGL Shader "
            "Programs. Shader Fxs will not be able to render."));
        break;
      }

      sentMsg = true;
    }

    return sup;
  }

  QOffscreenSurface *getSurface() { return m_surface.get(); }
};

template class DV_EXPORT_API TFxDeclarationT<ShaderFx>;

//****************************************************************************
//    ShadingContextManagerDelegate  definition
//****************************************************************************

class MessageCreateContext final : public TThread::Message {
  ShadingContextManager *man;

public:
  MessageCreateContext(ShadingContextManager *ctx) : man(ctx) {}

  void onDeliver() override { man->onRenderInstanceEnd(); }

  TThread::Message *clone() const override {
    return new MessageCreateContext(*this);
  }
};

class SCMDelegate final : public TRenderResourceManager {
  T_RENDER_RESOURCE_MANAGER

  void onRenderInstanceStart(unsigned long id) override {
    ShadingContextManager::instance()->onRenderInstanceStart();
  }

  void onRenderInstanceEnd(unsigned long id) override {
    if (!TThread::isMainThread()) {
      /* tofflinegl のときとは逆で main thread に dispatch する */
      MessageCreateContext(ShadingContextManager::instance()).sendBlocking();
    } else {
      ShadingContextManager::instance()->onRenderInstanceEnd();
    }
  }
};

//-------------------------------------------------------------------

class SCMDelegateGenerator final : public TRenderResourceManagerGenerator {
public:
  SCMDelegateGenerator() : TRenderResourceManagerGenerator(false) {
    /*
Again, this has to do with the manager's lifetime issue.
The SCM must be created in the MAIN THREAD, but NOT BEFORE the
QCoreApplication itself has been created. The easiest way to do so
is scheduling a slot to be executed as soon as event processing starts.
*/

    struct InstanceSCM final : public TFunctorInvoker::BaseFunctor {
      void operator()() override { ShadingContextManager::instance(); }
    };

    TFunctorInvoker::instance()->invokeQueued(new InstanceSCM);
  }

  TRenderResourceManager *operator()() override { return new SCMDelegate; }
};

MANAGER_FILESCOPE_DECLARATION(SCMDelegate, SCMDelegateGenerator)

//****************************************************************************
//    Shader Fx  implementation
//****************************************************************************

void ShaderFx::initialize() {
  struct {
    ShaderFx *m_this;

    inline void addUiConcept(const ShaderInterface::Parameter &siParam,
                             const TParamP &param) {
      if (siParam.m_concept.m_type >= ShaderInterface::UI_CONCEPTS &&
          siParam.m_concept.m_type < ShaderInterface::CONCEPTSCOUNT) {
        m_this->m_uiConcepts.push_back(TParamUIConcept());

        TParamUIConcept &uiConcept = m_this->m_uiConcepts.back();
        uiConcept.m_type           = ::l_conceptTypes[siParam.m_concept.m_type -
                                            ShaderInterface::UI_CONCEPTS];
        uiConcept.m_label = siParam.m_concept.m_label.toStdString();
        uiConcept.m_params.push_back(param);
      }
    }

    inline void addUiConcept(const ShaderInterface::ParameterConcept &concept) {
      if (!concept.isUI() || concept.m_parameterNames.empty()) return;

      TParamUIConcept uiConcept = {
          ::l_conceptTypes[concept.m_type - ShaderInterface::UI_CONCEPTS],
          concept.m_label.toStdString()};

      int n, nCount = int(concept.m_parameterNames.size());
      for (n = 0; n != nCount; ++n) {
        TParam *param = m_this->getParams()->getParam(
            concept.m_parameterNames[n].toStdString());
        if (!param) break;

        uiConcept.m_params.push_back(param);
      }

      if (uiConcept.m_params.size() == concept.m_parameterNames.size())
        m_this->m_uiConcepts.push_back(uiConcept);
    }

  } locals = {this};

  assert(m_params.empty());  // Interfaces should not be re-set

  // Allocate parameters following the specified interface
  const std::vector<ShaderInterface::Parameter> &siParams =
      m_shaderInterface->parameters();

  int p, pCount = int(siParams.size());
  m_params.reserve(pCount);

  for (p = 0; p != pCount; ++p) {
    const ShaderInterface::Parameter &siParam = siParams[p];

    switch (siParam.m_type) {
    case ShaderInterface::BOOL: {
      TBoolParamP param(siParam.m_default.m_bool);

      m_params.push_back(param);
      bindParam(this, siParam.m_name.toStdString(),
                *boost::unsafe_any_cast<TBoolParamP>(&m_params.back()));

      break;
    }

    case ShaderInterface::FLOAT: {
      TDoubleParamP param(siParam.m_default.m_float);
      param->setValueRange(siParam.m_range[0].m_float,
                           siParam.m_range[1].m_float);

      locals.addUiConcept(siParam, param);

      switch (siParam.m_concept.m_type) {
      case ShaderInterface::PERCENT:
        param->setMeasureName(l_measureNames[PERCENT]);
        break;

      case ShaderInterface::LENGTH:
      case ShaderInterface::RADIUS_UI:
      case ShaderInterface::WIDTH_UI:
      case ShaderInterface::SIZE_UI:
        param->setMeasureName(l_measureNames[LENGTH]);
        break;

      case ShaderInterface::ANGLE:
      case ShaderInterface::ANGLE_UI:
        param->setMeasureName(l_measureNames[ANGLE]);
        break;
      }

      m_params.push_back(param);
      bindParam(this, siParam.m_name.toStdString(),
                *boost::unsafe_any_cast<TDoubleParamP>(&m_params.back()));
      break;
    }

    case ShaderInterface::VEC2: {
      TPointParamP param(
          TPointD(siParam.m_default.m_vec2[0], siParam.m_default.m_vec2[1]));

      param->getX()->setValueRange(siParam.m_range[0].m_vec2[0],
                                   siParam.m_range[1].m_vec2[0]);
      param->getY()->setValueRange(siParam.m_range[0].m_vec2[1],
                                   siParam.m_range[1].m_vec2[1]);

      locals.addUiConcept(siParam, param);

      switch (siParam.m_concept.m_type) {
      case ShaderInterface::PERCENT:
        param->getX()->setMeasureName(l_measureNames[PERCENT]);
        param->getY()->setMeasureName(l_measureNames[PERCENT]);
        break;

      case ShaderInterface::LENGTH:
      case ShaderInterface::POINT:
      case ShaderInterface::POINT_UI:
      case ShaderInterface::VECTOR_UI:
      case ShaderInterface::WIDTH_UI:
      case ShaderInterface::SIZE_UI:
        param->getX()->setMeasureName(l_measureNames[LENGTH]);
        param->getY()->setMeasureName(l_measureNames[LENGTH]);
        break;

      case ShaderInterface::ANGLE:
      case ShaderInterface::ANGLE_UI:
        param->getX()->setMeasureName(l_measureNames[ANGLE]);
        param->getY()->setMeasureName(l_measureNames[ANGLE]);
        break;
      }

      m_params.push_back(param);
      bindParam(this, siParam.m_name.toStdString(),
                *boost::unsafe_any_cast<TPointParamP>(&m_params.back()));
      break;
    }

    case ShaderInterface::INT: {
      TIntParamP param(siParam.m_default.m_int);
      param->setValueRange(siParam.m_range[0].m_int, siParam.m_range[1].m_int);

      m_params.push_back(param);
      bindParam(this, siParam.m_name.toStdString(),
                *boost::unsafe_any_cast<TIntParamP>(&m_params.back()));
      break;
    }

    case ShaderInterface::RGBA: {
      TPixelParamP param(
          TPixel32(siParam.m_default.m_rgba[0], siParam.m_default.m_rgba[1],
                   siParam.m_default.m_rgba[2], siParam.m_default.m_rgba[3]));

      m_params.push_back(param);
      bindParam(this, siParam.m_name.toStdString(),
                *boost::unsafe_any_cast<TPixelParamP>(&m_params.back()));
      break;
    }

    case ShaderInterface::RGB: {
      TPixelParamP param(TPixel32(siParam.m_default.m_rgb[0],
                                  siParam.m_default.m_rgb[1],
                                  siParam.m_default.m_rgb[2]));

      param->enableMatte(false);

      m_params.push_back(param);
      bindParam(this, siParam.m_name.toStdString(),
                *boost::unsafe_any_cast<TPixelParamP>(&m_params.back()));
      break;
    }
    }
  }

  // Add composite UI concepts
  const std::vector<ShaderInterface::ParameterConcept> &parConcepts =
      m_shaderInterface->m_parConcepts;

  int c, cCount = int(parConcepts.size());
  for (c = 0; c != cCount; ++c) locals.addUiConcept(parConcepts[c]);

  // Add input ports
  const std::vector<QString> &inputPorts = m_shaderInterface->inputPorts();

  int i, iCount = int(inputPorts.size());
  m_inputPorts.reserve(iCount);

  for (i = 0; i != iCount; ++i) {
    m_inputPorts.push_back(new TRasterFxPort);
    addInputPort(inputPorts[i].toStdString(), m_inputPorts[i]);
  }
}

//-------------------------------------------------------------------

void ShaderFx::getParamUIs(TParamUIConcept *&params, int &length) {
  length = int(m_uiConcepts.size());
  params = new TParamUIConcept[length];

  std::copy(m_uiConcepts.begin(), m_uiConcepts.end(), params);
}

//-------------------------------------------------------------------

bool ShaderFx::doGetBBox(double frame, TRectD &bbox,
                         const TRenderSettings &info) {
  static const ::RectF infiniteRectF(-(std::numeric_limits<GLfloat>::max)(),
                                     -(std::numeric_limits<GLfloat>::max)(),
                                     (std::numeric_limits<GLfloat>::max)(),
                                     (std::numeric_limits<GLfloat>::max)());

  bbox = TConsts::infiniteRectD;

  const ShaderInterface::ShaderData &sd = m_shaderInterface->bboxShader();
  if (!sd.isValid()) return true;

  ShadingContextManager *manager = ShadingContextManager::instance();
  if (manager->touchSupport() != ShadingContext::OK) return true;

  // Remember: info.m_affine MUST NOT BE CONSIDERED in doGetBBox's
  // implementation
  ::RectF bboxF(infiniteRectF);

  QMutexLocker mLocker(manager->mutex());

  // ShadingContext& context = manager->shadingContext();
  std::shared_ptr<ShadingContext> shadingContextPtr(
      new ShadingContext(manager->getSurface()));
  ShadingContext &context = *shadingContextPtr.get();

  ::ContextLocker cLocker(context);

  // Build the varyings data
  QOpenGLShaderProgram *prog = 0;
  {
    const GLchar *varyingNames[] = {"outputBBox"};
    prog = touchShaderProgram(sd, context, 1, &varyingNames[0]);
  }

  int pCount = getInputPortCount();

  std::vector<RectF> inputBBoxes(pCount, ::RectF(TRectD()));

  for (int p = 0; p != pCount; ++p) {
    TRasterFxPort &port = m_inputPorts[p];
    if (port.isConnected()) {
      TRectD inputBBox;

      cLocker.unlock();
      mLocker.unlock();

      if (port->doGetBBox(frame, inputBBox, info))
        inputBBoxes[p] = (inputBBox == TConsts::infiniteRectD)
                             ? infiniteRectF
                             : ::RectF(inputBBox);

      mLocker.relock();
      cLocker.relock();
    }
  }

  {
    ProgramBinder progBinder(prog);

    // Bind uniform parameters
    bindParameters(prog, frame);

    prog->setUniformValue("infiniteRect", infiniteRectF.m_val[0],
                          infiniteRectF.m_val[1], infiniteRectF.m_val[2],
                          infiniteRectF.m_val[3]);

    prog->setUniformValueArray("inputBBox", inputBBoxes[0].m_val,
                               int(inputBBoxes.size()), 4);

    // Perform transform feedback
    const GLsizeiptr varyingSizes[] = {sizeof(::RectF)};
    GLvoid *bufs[]                  = {bboxF.m_val};

    context.transformFeedback(1, varyingSizes, bufs);
  }

  // Finalize output
  bbox = (bboxF == infiniteRectF) ? TConsts::infiniteRectD : TRectD(bboxF);
  return true;
}

//-------------------------------------------------------------------

bool ShaderFx::canHandle(const TRenderSettings &info, double frame) {
  return (m_shaderInterface->hwtType() == ShaderInterface::ANY)
             ? true
             : isAlmostIsotropic(info.m_affine);
}

//-------------------------------------------------------------------

QOpenGLShaderProgram *ShaderFx::touchShaderProgram(
    const ShaderInterface::ShaderData &sd, ShadingContext &context,
    int varyingsCount, const GLchar **varyings) {
  typedef std::pair<QOpenGLShaderProgram *, QDateTime> CompiledShader;

  struct locals {
    inline static void logCompilation(QOpenGLShaderProgram *program) {
      // Log shaders - observe that we'll look into the program's *children*,
      // not its
      // shaders. This is necessary as uncompiled shaders are not added to the
      // program.
      const QObjectList &children = program->children();

      int c, cCount = children.size();
      for (c = 0; c != cCount; ++c) {
        if (QOpenGLShader *shader =
                dynamic_cast<QOpenGLShader *>(children[c])) {
          const QString &log = shader->log();
          if (!log.isEmpty()) DVGui::info(log);
        }
      }

      // ShaderProgram linking logs
      const QString &log = program->log();
      if (!log.isEmpty()) DVGui::info(log);
    }
  };  // locals

  // ShadingContext& context =
  // ShadingContextManager::instance()->shadingContext();

  CompiledShader cs = context.shaderData(sd.m_name);
  if (!cs.first || ::isObsolete(sd.m_path, cs.second)) {
    cs = m_shaderInterface->makeProgram(sd, varyingsCount, varyings);
    context.addShaderProgram(sd.m_name, cs.first, cs.second);

    locals::logCompilation(cs.first);
  }

  assert(cs.first);
  return cs.first;
}

//-------------------------------------------------------------------

void ShaderFx::bindParameters(QOpenGLShaderProgram *program, double frame) {
  // Bind fx parameters
  const std::vector<ShaderInterface::Parameter> &siParams =
      m_shaderInterface->parameters();

  assert(siParams.size() == m_params.size());

  int p, pCount = int(siParams.size());
  for (p = 0; p != pCount; ++p) {
    const ShaderInterface::Parameter &siParam = siParams[p];

    switch (siParam.m_type) {
    case ShaderInterface::BOOL: {
      const TBoolParamP &param =
          *boost::unsafe_any_cast<TBoolParamP>(&m_params[p]);
      program->setUniformValue(siParam.m_name.toUtf8().data(),
                               (GLboolean)param->getValue());
      break;
    }

    case ShaderInterface::FLOAT: {
      const TDoubleParamP &param =
          *boost::unsafe_any_cast<TDoubleParamP>(&m_params[p]);
      program->setUniformValue(siParam.m_name.toUtf8().data(),
                               (GLfloat)param->getValue(frame));
      break;
    }

    case ShaderInterface::VEC2: {
      const TPointParamP &param =
          *boost::unsafe_any_cast<TPointParamP>(&m_params[p]);

      const TPointD &value = param->getValue(frame);
      program->setUniformValue(siParam.m_name.toUtf8().data(), (GLfloat)value.x,
                               (GLfloat)value.y);
      break;
    }

    case ShaderInterface::INT: {
      const TIntParamP &param =
          *boost::unsafe_any_cast<TIntParamP>(&m_params[p]);
      program->setUniformValue(siParam.m_name.toUtf8().data(),
                               (GLint)param->getValue());
      break;
    }

    case ShaderInterface::RGBA:
    case ShaderInterface::RGB: {
      const TPixelParamP &param =
          *boost::unsafe_any_cast<TPixelParamP>(&m_params[p]);

      const TPixel32 &value = param->getValue(frame);
      program->setUniformValue(
          siParam.m_name.toUtf8().data(), (GLfloat)value.r / 255.0f,
          (GLfloat)value.g / 255.0f, (GLfloat)value.b / 255.0f,
          (GLfloat)value.m / 255.0f);
      break;
    }
    }
  }
}

//-------------------------------------------------------------------

void ShaderFx::bindWorldTransform(QOpenGLShaderProgram *program,
                                  const TAffine &worldToDst) {
// Bind transformation affine
#if QT_VERSION >= 0x050500
  float qwToD[9] = {static_cast<float>(worldToDst.a11),
                    static_cast<float>(worldToDst.a12),
                    static_cast<float>(worldToDst.a13),
                    static_cast<float>(worldToDst.a21),
                    static_cast<float>(worldToDst.a22),
                    static_cast<float>(worldToDst.a23),
                    0.0f,
                    0.0f,
                    1.0f};
#else
  qreal qwToD[9] = {worldToDst.a11,
                    worldToDst.a12,
                    worldToDst.a13,
                    worldToDst.a21,
                    worldToDst.a22,
                    worldToDst.a23,
                    0.0,
                    0.0,
                    1.0};
#endif
  program->setUniformValue("worldToOutput", QMatrix3x3(qwToD));

  const TAffine &dToW = worldToDst.inv();
#if QT_VERSION >= 0x050500
  float qdToW[9] = {static_cast<float>(dToW.a11),
                    static_cast<float>(dToW.a12),
                    static_cast<float>(dToW.a13),
                    static_cast<float>(dToW.a21),
                    static_cast<float>(dToW.a22),
                    static_cast<float>(dToW.a23),
                    0.0f,
                    0.0f,
                    1.0f};
#else
  qreal qdToW[9] = {dToW.a11, dToW.a12, dToW.a13, dToW.a21, dToW.a22,
                    dToW.a23, 0.0,      0.0,      1.0};
#endif
  program->setUniformValue("outputToWorld", QMatrix3x3(qdToW));
}

//-------------------------------------------------------------------

void ShaderFx::getInputData(const TRectD &rect, double frame,
                            const TRenderSettings &ri,
                            std::vector<TRectD> &inputRects,
                            std::vector<TAffine> &inputAffines,
                            ShadingContext &context) {
  struct locals {
    static inline void addNames(std::vector<std::string> &names,
                                const char *prefix, int pCount) {
      for (int p = 0; p != pCount; ++p)
        names.push_back((prefix + QString("[%1]").arg(p)).toStdString());
    }
  };

  const ShaderInterface::ShaderData &sd = m_shaderInterface->inputPortsShader();
  if (!sd.isValid()) {
    inputRects.resize(getInputPortCount());
    std::fill(inputRects.begin(), inputRects.end(), rect);

    inputAffines.resize(getInputPortCount());
    std::fill(inputAffines.begin(), inputAffines.end(), ri.m_affine);

    return;
  }

  // ShadingContext& context =
  // ShadingContextManager::instance()->shadingContext();

  std::vector<GLfloat> buf;
  int pCount = getInputPortCount();

  // Build the varyings data
  QOpenGLShaderProgram *prog = 0;
  {
    // Unsubscripted varying arrays on transform feedback seems to be
    // unsupported
    // on ATI cards. We have to declare EACH array name - e.g. inputRect[0],
    // intputRect[1], etc..

    const GLchar *varyingPrefixes[] = {"inputRect", "worldToInput"};
    const int varyingsCount = sizeof(varyingPrefixes) / sizeof(GLchar *);

    std::vector<std::string> varyingStrings;
    varyingStrings.reserve(varyingsCount);

    for (int v = 0; v != varyingsCount; ++v)
      locals::addNames(varyingStrings, varyingPrefixes[v], pCount);

#if defined(__APPLE_CC__)
    /* OSX10.8 の clang -stdlib=libc++ だと link 時 &std::string::c_str が
     * undefined になってしまう */
    std::vector<const GLchar *> varyingNames(varyingStrings.size());
    auto conv = [](const std::string &i) { return i.c_str(); };
    std::transform(varyingStrings.begin(), varyingStrings.end(),
                   varyingNames.begin(), conv);
#else
    std::vector<const GLchar *> varyingNames(
        boost::make_transform_iterator(varyingStrings.begin(),
                                       std::mem_fun_ref(&std::string::c_str)),
        boost::make_transform_iterator(varyingStrings.end(),
                                       std::mem_fun_ref(&std::string::c_str)));
#endif
    prog = touchShaderProgram(sd, context, int(varyingNames.size()),
                              &varyingNames[0]);
  }

  {
    ProgramBinder progBinder(prog);

    // Build varying buffers
    int bufFloatsCount =
        pCount * (sizeof(RectF) + sizeof(AffineF)) / sizeof(GLfloat);
    buf.resize(bufFloatsCount);

    // Bind uniform parameters
    bindParameters(prog, frame);
    bindWorldTransform(prog, ri.m_affine);

    prog->setUniformValue("outputRect", (GLfloat)rect.x0, (GLfloat)rect.y0,
                          (GLfloat)rect.x1, (GLfloat)rect.y1);

    // Perform transform feedback
    const GLsizeiptr varyingSizes[] = {
        static_cast<GLsizeiptr>(bufFloatsCount * sizeof(GLfloat))};
    GLvoid *bufs[] = {&buf[0]};

    context.transformFeedback(1, varyingSizes, bufs);

#ifdef TRANSFORM_FEEDBACK_COUT
    std::cout << "trFeedback: ";
    for (int f = 0; f != bufFloatsCount; ++f) std::cout << buf[f] << " ";
    std::cout << "\n" << std::endl;
#endif
  }

  // Finalize output
  const RectF *rBufBegin(reinterpret_cast<const RectF *>(&buf[0])),
      *rBufEnd(rBufBegin + pCount);
  std::copy(rBufBegin, rBufEnd, &inputRects[0]);

  const AffineF *aBufBegin(reinterpret_cast<const AffineF *>(rBufEnd)),
      *aBufEnd(aBufBegin + pCount);
  std::copy(aBufBegin, aBufEnd, &inputAffines[0]);
}

//-------------------------------------------------------------------

void ShaderFx::doCompute(TTile &tile, double frame,
                         const TRenderSettings &info) {
  struct locals {
    struct TexturesStorage {
      ShadingContext &m_ctx;
      std::vector<GLuint> m_texIds;

      TexturesStorage(ShadingContext &ctx, int pCount) : m_ctx(ctx) {
        m_texIds.reserve(pCount);
      }

      ~TexturesStorage() {
        typedef tcg::function<void (ShadingContext::*)(GLuint),
                              &ShadingContext::unloadTexture>
            UnloadFunc;

        std::for_each(m_texIds.begin(), m_texIds.end(),
                      tcg::bind1st(UnloadFunc(), m_ctx));
      }

      void load(const TRasterP &ras, GLuint texUnit) {
        if (ras) m_texIds.push_back(m_ctx.loadTexture(ras, texUnit));
      }
    };

    inline static QOpenGLFramebufferObjectFormat makeFormat(int bpp) {
      QOpenGLFramebufferObjectFormat fmt;
      if (bpp == 64) fmt.setInternalTextureFormat(GL_RGBA16);
      return fmt;
    }

    inline static void touchOutputSize(ShadingContext &context,
                                       const TDimension &size, int bpp) {
      const QOpenGLFramebufferObjectFormat &fmt = makeFormat(bpp);

      const TDimension &currentSize                    = context.size();
      const QOpenGLFramebufferObjectFormat &currentFmt = context.format();

      if (currentSize.lx < size.lx || currentSize.ly < size.ly ||
          currentFmt != fmt)
        context.resize(std::max(size.lx, currentSize.lx),
                       std::max(size.ly, currentSize.ly), fmt);
    }
  };  // locals

  ShadingContextManager *manager = ShadingContextManager::instance();
  if (manager->touchSupport() != ShadingContext::OK) return;

  QMutexLocker mLocker(
      manager->mutex());  // As GPU access can be considered sequential anyway,
                          // lock the full-scale mutex
  std::shared_ptr<ShadingContext> shadingContextPtr(
      new ShadingContext(manager->getSurface()));
  ShadingContext &context = *shadingContextPtr.get();
  // ShadingContext& context = manager->shadingContext();

  int pCount = getInputPortCount();

  const TRectD &tileRect = ::tileRect(tile);

  std::vector<TRectD> inputRects(pCount);
  std::vector<TAffine> inputAffines(pCount);

  // Calculate input tiles
  ::ContextLocker cLocker(context);

  tcg::unique_ptr<TTile[]> inTiles(
      new TTile[pCount]);  // NOTE: Input tiles must be STORED - they cannot
  // be passed immediately to OpenGL, since *other shader
  if (pCount > 0)  // fxs*, with the very same host context, could lie
  {                // inside this fx's input branches...
    getInputData(tileRect, frame, info, inputRects, inputAffines, context);

    // Release context and mutex
    cLocker.unlock();
    mLocker.unlock();

    for (int p = 0; p != pCount; ++p) {
      TRasterFxPort &port = m_inputPorts[p];
      if (port.isConnected()) {
        // Compute input tile
        TRectD &inRect = inputRects[p];
        if (inRect.getLx() > 0.0 && inRect.getLy() > 0.0) {
          ::ceilRect(inRect);

          TRenderSettings inputInfo(info);
          inputInfo.m_affine = inputAffines[p];

#ifdef TRANSFORM_FEEDBACK_COUT
          const TAffine &inAff = inputAffines[p];
          std::cout << "inRect " << p << ": " << inRect.x0 << " " << inRect.y0
                    << " " << inRect.x1 << " " << inRect.y1 << "\n";
          std::cout << "inAff  " << p << ": " << inAff.a11 << " " << inAff.a12
                    << " " << inAff.a13 << "\n";
          std::cout << "          " << inAff.a21 << " " << inAff.a22 << " "
                    << inAff.a23 << "\n"
                    << std::endl;
#endif

          port->allocateAndCompute(
              inTiles[p], inRect.getP00(),
              TDimension(tround(inRect.getLx()), tround(inRect.getLy())),
              tile.getRaster(), frame, inputInfo);
        }
      }
    }

    // Load input tiles on the GPU as textures
    mLocker.relock();
    cLocker.relock();

    // Input tiles are NOT supplied to OpenGL here - but rather just before
    // drawing.
    // It's probably beacuse a uniform integer variable must have already been
    // bound
    // to prepare the associated sampler variable in the linkes program...
  }

  // Perform the actual fragment shading
  {
    locals::touchOutputSize(context, tile.getRaster()->getSize(), info.m_bpp);

    QOpenGLShaderProgram *program =
        touchShaderProgram(m_shaderInterface->mainShader(), context);
    {
      ProgramBinder binder(program);

      // Bind parameters and textures
      bindParameters(program, frame);
      bindWorldTransform(program, TTranslation(-tile.m_pos) * info.m_affine);

      // Setup input data, if any
      locals::TexturesStorage texStorage(context, pCount);

      if (pCount > 0) {
        std::vector<GLint> inputs(pCount);
        std::vector<QMatrix3x3> screenToInput(pCount);
        std::vector<QMatrix3x3> inputToScreen(pCount);

        for (int p = 0; p != pCount; ++p) {
          TAffine iToS(
              TTranslation(-tile.m_pos) *             // Output to Screen
              info.m_affine *                         // World to Output
              inputAffines[p].inv() *                 // Input to World
              TTranslation(inputRects[p].getP00()) *  // Texture to Input
              TScale(inputRects[p].getLx(), inputRects[p].getLy()));  //

          TAffine sToI(iToS.inv());

#if QT_VERSION >= 0x050500
          float qiToS[9] = {static_cast<float>(iToS.a11),
                            static_cast<float>(iToS.a12),
                            static_cast<float>(iToS.a13),
                            static_cast<float>(iToS.a21),
                            static_cast<float>(iToS.a22),
                            static_cast<float>(iToS.a23),
                            0.0f,
                            0.0f,
                            1.0f};
          float qsToI[9] = {static_cast<float>(sToI.a11),
                            static_cast<float>(sToI.a12),
                            static_cast<float>(sToI.a13),
                            static_cast<float>(sToI.a21),
                            static_cast<float>(sToI.a22),
                            static_cast<float>(sToI.a23),
                            0.0f,
                            0.0f,
                            1.0f};
#else
          qreal qiToS[9] = {iToS.a11, iToS.a12, iToS.a13, iToS.a21, iToS.a22,
                            iToS.a23, 0.0,      0.0,      1.0};

          qreal qsToI[9] = {sToI.a11, sToI.a12, sToI.a13, sToI.a21, sToI.a22,
                            sToI.a23, 0.0,      0.0,      1.0};
#endif
          inputs[p] = p, screenToInput[p] = QMatrix3x3(qsToI),
          inputToScreen[p] = QMatrix3x3(qiToS);
        }

        program->setUniformValueArray("inputImage", &inputs[0], pCount);
        program->setUniformValueArray("outputToInput", &screenToInput[0],
                                      pCount);
        program->setUniformValueArray("inputToOutput", &inputToScreen[0],
                                      pCount);

        // Load textures
        for (int p = 0; p != pCount; ++p)
          texStorage.load(inTiles[p].getRaster(), p);
      }

#ifdef DIAGNOSTICS
      DIAGNOSTICS_TIMER("Shader Overall Times | " +
                        m_shaderInterface->m_mainShader.m_name);
#endif

      context.draw(tile.getRaster());
    }
  }
}

//-------------------------------------------------------------------

void ShaderFx::doDryCompute(TRectD &rect, double frame,
                            const TRenderSettings &info) {
  ShadingContextManager *manager = ShadingContextManager::instance();
  if (manager->touchSupport() != ShadingContext::OK) return;

  QMutexLocker mLocker(manager->mutex());

  // ShadingContext& context = manager->shadingContext();
  std::shared_ptr<ShadingContext> shadingContextPtr(
      new ShadingContext(manager->getSurface()));
  ShadingContext &context = *shadingContextPtr.get();

  int pCount = getInputPortCount();
  if (pCount > 0) {
    ::ContextLocker cLocker(context);

    std::vector<TRectD> inputRects(pCount);
    std::vector<TAffine> inputAffines(pCount);

    getInputData(rect, frame, info, inputRects, inputAffines, context);

    for (int p = 0; p != pCount; ++p) {
      TRasterFxPort &port = m_inputPorts[p];
      if (port.isConnected()) {
        TRectD &inRect = inputRects[p];
        if (inRect.getLx() > 0.0 && inRect.getLy() > 0.0) {
          ::ceilRect(inRect);

          TRenderSettings inputInfo(info);
          inputInfo.m_affine = inputAffines[p];

          cLocker.unlock();
          mLocker.unlock();

          port->dryCompute(inRect, frame, inputInfo);

          mLocker.relock();
          cLocker.relock();
        }
      }
    }
  }
}

//------------------------------------------------------------------

const TPersistDeclaration *ShaderFx::getDeclaration() const {
  FxDeclarationsMap::iterator it =
      ::l_shaderFxDeclarations.find(m_shaderInterface->mainShader().m_name);

  return (it == ::l_shaderFxDeclarations.end()) ? 0 : it->second;
}

//****************************************************************************
//    Shader Interfaces  loading function
//****************************************************************************

void loadShaderInterfaces(const TFilePath &shadersFolder) {
  // Scan the shaders folder for xml (shader interface) files
  QDir shadersDir(QString::fromStdWString(shadersFolder.getWideString()));

  QStringList namesFilter("*.xml");
  QStringList files = shadersDir.entryList(namesFilter, QDir::Files,
                                           QDir::Name | QDir::LocaleAware);

  int f, fCount = files.size();
  for (f = 0; f != fCount; ++f) {
    TIStream is(shadersFolder + TFilePath(files[f].toStdWString()));

    // Try to load a ShaderInterface instance for the file
    ShaderInterface shaderInterface;
    is >> shaderInterface;

    if (shaderInterface.isValid()) {
      // Store a ShaderFx factory for the interface
      ::l_shaderFxDeclarations.insert(
          std::make_pair(shaderInterface.mainShader().m_name,
                         new ShaderFxDeclaration(shaderInterface)));
    }
  }
}
