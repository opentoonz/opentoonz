#pragma once

#ifndef TRENDERRESOURCEMANAGER_INCLUDED
#define TRENDERRESOURCEMANAGER_INCLUDED

#include "trasterfx.h"

#undef DVAPI
#undef DVVAR
#ifdef TFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//===============================================================================

class TRenderer;
class TRenderResourceManager;
class TRenderResourceManagerGenerator;

//===============================================================================

//=================================
//    Render Resource Managers
//---------------------------------

/*!
The TRenderResourceManager is a base class used to develop resource
handlers for Toonz rendering contexts.
\a Resources are hereby intended as data which possess special scope and
duration with respect to one or more rendering processes. A most notable
example of this is the internal Fxs cache, which is specialized to
maintain a group of intermediate rendered tiles which may be shared in input
by multiple fxs.
\n \n
Creation of a resource manager requires the implementation of both the
manager itself, and that of a TRenderResourceManagerGenerator class which
is used internally to allocate manager instances at the most appropriate times.
The manager's generator needs to be instanced at <b> file scope <\b> to
be correctly registered by Toonz.
\n \n
Observe that the default scope of a resource manager is limited to a TRenderer
instance; specifically meaning that the swatch viewer, preview fx command,
camstand preview and render/preview commands build different managers.
*/

class TRenderResourceManager {
public:
  TRenderResourceManager() {}
  virtual ~TRenderResourceManager() {}

  virtual void onRenderInstanceStart(unsigned long id) {}
  virtual void onRenderInstanceEnd(unsigned long id) {}

  virtual void onRenderFrameStart(double f) {}
  virtual void onRenderFrameEnd(double f) {}

  virtual void onRenderStatusStart(int renderStatus) {}
  virtual void onRenderStatusEnd(int renderStatus) {}

  virtual bool renderHasOwnership() { return true; }
};

//===============================================================================

//===========================================
//    Render Resource Manager Generators
//-------------------------------------------

/*!
The TRenderResourceManagerGenerator class is used to construct resource managers
that will be used by TRenderers.
Please refer to the TRenderResourceManager documentation for the details.

\note Resource managers may be constructed with a <i> render instance <\i> scope
rather
than the default render context (ie TRenderer) scope. This can be achieved by
passing
\c true at the generator's constructor, proving especially useful considering
that the same TRenderer may virtually start multiple render instances at the
same time.
*/

class DVAPI TRenderResourceManagerGenerator {
  int m_managerIndex;
  bool m_instanceScope;

public:
  TRenderResourceManagerGenerator(bool renderInstanceScope = false);
  ~TRenderResourceManagerGenerator() {}

  int getGeneratorIndex() const { return m_managerIndex; }
  static std::vector<TRenderResourceManagerGenerator *> &generators();
  static std::vector<TRenderResourceManagerGenerator *> &generators(
      bool instanceScope);

  virtual TRenderResourceManager *operator()(void) = 0;

  bool hasInstanceScope() const { return m_instanceScope; }

  TRenderResourceManager *getManager(const TRenderer &renderer) const;
  TRenderResourceManager *getManager(unsigned long renderId) const;
};

//===============================================================================

//===========================
//    Declaration macros
//---------------------------

/*!  \def MANAGER_DECLARATION(managerClass)
Declares a TRenderResourceManager class, it must be used inside the manager
definition.
For example:
\code
class MyManager final : public TRenderResourceManager
{
  T_RENDER_RESOURCE_MANAGER

  ...
};
\endcode
In particular, this macro is used to decorate the manager definition with the
static method \c gen(), which can be used to access the manager's generator.
The method declaration is:
\code
static TRenderResourceManagerGenerator* gen();
\endcode
*/

#define T_RENDER_RESOURCE_MANAGER                                              \
  \
public:                                                                        \
  static TRenderResourceManagerGenerator *gen();                               \
  static TRenderResourceManagerGenerator *deps();                              \
  \
private:

//--------------------------------------------------------------------------------

/*!  \def MANAGER_FILESCOPE_DECLARATION(managerClass, generatorClass)
This macro must be used at file scope to bind the manager to its generator
instance - just place it after the generator definition.
\note In case the manager is dependant on some other manager, use the
\c MANAGER_FILESCOPE_DECLARATION_DEP macro instead.
*/

#define MANAGER_FILESCOPE_DECLARATION(managerClass, generatorClass)            \
                                                                               \
  TRenderResourceManagerGenerator *managerClass::gen() {                       \
    static generatorClass theInstance;                                         \
    return &theInstance;                                                       \
  }                                                                            \
  TRenderResourceManagerGenerator *managerClass::deps() {                      \
    return managerClass::gen();                                                \
  }                                                                            \
                                                                               \
  namespace {                                                                  \
  generatorClass *generatorClass##Instance =                                   \
      (generatorClass *)managerClass::deps();                                  \
  }

//--------------------------------------------------------------------------------

/*!  \def MANAGER_FILESCOPE_DECLARATION_DEP(managerClass, generatorClass,
depsList)
A variation of the MANAGER_FILESCOPE_DECLARATION macro used to declare
dependencies between
resource managers. A manager always receives start notifications \a after those
from which
it depends, and viceversa for end notifications. The same applies for
constructors and destructors.
\n \n
The dependencies declarations list is a sequence of static \c
<DependencyResourceManager>::deps()
calls separated by a comma.
Here is an example:
\code
MANAGER_FILESCOPE_DECLARATION_DEP(
  MyResourceManager,
  MyResourceManagerGenerator,
  TPredictiveCacheManager::deps(); TOfflineGLManager::deps(); \\etc...
)
\endcode
*/

#define MANAGER_FILESCOPE_DECLARATION_DEP(managerClass, generatorClass,        \
                                          depsList)                            \
                                                                               \
  TRenderResourceManagerGenerator *managerClass::gen() {                       \
    static generatorClass theInstance;                                         \
    return &theInstance;                                                       \
  }                                                                            \
  TRenderResourceManagerGenerator *managerClass::deps() {                      \
    depsList;                                                                  \
    return managerClass::gen();                                                \
  }                                                                            \
                                                                               \
  namespace {                                                                  \
  generatorClass *generatorClass##Instance =                                   \
      (generatorClass *)managerClass::deps();                                  \
  }

//===============================================================================

#endif  // TRENDERRESOURCEMANAGER_INCLUDED
