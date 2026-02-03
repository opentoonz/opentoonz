#pragma once

#ifndef TMACROFX_INCLUDED
#define TMACROFX_INCLUDED

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

#ifdef _MSC_VER
#pragma warning(disable : 4251)
#endif

//===================================================================

class DVAPI TMacroFx final : public TRasterFx {
  FX_DECLARATION(TMacroFx)

  TRasterFxP m_root;
  std::vector<TFxP> m_fxs;
  bool m_isEditing;

  bool isaLeaf(TFx *fx) const;

  bool m_isLoading          = false;
  TMacroFx *m_waitingLinkFx = nullptr;

public:
  static bool analyze(const std::vector<TFxP> &fxs, TFxP &root,
                      std::vector<TFxP> &roots, std::vector<TFxP> &leafs);

  static bool analyze(const std::vector<TFxP> &fxs);

  static TMacroFx *create(const std::vector<TFxP> &fxs);

  TMacroFx();
  ~TMacroFx();

  TFx *clone(bool recursive = true) const override;

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;
  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

  TFxTimeRegion getTimeRegion() const override;

  std::string getPluginId() const override;

  void setRoot(TFx *root);
  TFx *getRoot() const;
  //! Returns the Fx identified by \b id.
  //! Returns 0 if the macro does not contains an Fx with fxId equals ti \b id.
  TFx *getFxById(const std::wstring &id) const;

  // restituisce un riferimento al vettore contenente gli effetti contenuti nel
  // macroFx
  const std::vector<TFxP> &getFxs() const;

  std::string getMacroFxType() const;

  bool canHandle(const TRenderSettings &info, double frame) override;

  std::string getAlias(double frame,
                       const TRenderSettings &info) const override;

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;
  bool isEditing() { return m_isEditing; }
  void editMacro(bool edit) { m_isEditing = edit; }

  void compatibilityTranslatePort(int majorVersion, int minorVersion,
                                  std::string &portName) override;

  void linkParams(TFx *src) override;
  bool isLoading() { return m_isLoading; }
  void setWaitingLinkFx(TMacroFx *linkFx) { m_waitingLinkFx = linkFx; }

private:
  // non implementati
  TMacroFx(const TMacroFx &);
  void operator=(const TMacroFx &);
};

//-------------------------------------------------------------------

#endif
