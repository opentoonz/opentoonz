#pragma once

#ifndef TEXTERNFX_H
#define TEXTERNFX_H

#include "tbasefx.h"
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

//---------------------------------------------------------

class DVAPI TExternFx : public TBaseRasterFx {
public:
  TExternFx() { setName(L"ExternFx"); }

  static TExternFx *create(std::string name);
  void getNames(std::vector<std::string> &names);
};

//---------------------------------------------------------

class DVAPI TExternalProgramFx final : public TExternFx {
  FX_DECLARATION(TExternalProgramFx)

  class Port {
  public:
    std::string m_name;
    TRasterFxPort *m_port;  // n.b. la porta di output ha m_port=0
    std::string m_ext;  // estensione del file in cui si legge/scrive l'immagine
    Port() : m_name(), m_port(0), m_ext() {}
    Port(std::string name, std::string ext, TRasterFxPort *port)
        : m_name(name), m_port(port), m_ext(ext) {}
  };

  std::map<std::string, Port> m_ports;
  // std::map<std::string, TParamP> m_params;
  std::vector<TParamP> m_params;

  TFilePath m_executablePath;
  std::string m_args;
  std::string m_externFxName;

public:
  TExternalProgramFx(std::string name);
  TExternalProgramFx();
  ~TExternalProgramFx();

  void initialize(std::string name);

  TFx *clone(bool recursive = true) const override;

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return false;
  }

  void setExecutable(TFilePath fp, std::string args) {
    m_executablePath = fp;
    m_args           = args;
  }

  void addPort(std::string portName, std::string ext, bool isInput);
  // void addParam(string paramName, const TParamP &param);

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

  void setExternFxName(std::string name) { m_externFxName = name; }
  std::string getExternFxName() const { return m_externFxName; };

private:
  // not implemented
  TExternalProgramFx(const TExternalProgramFx &);
  TExternalProgramFx &operator=(const TExternalProgramFx &);
};

#endif
