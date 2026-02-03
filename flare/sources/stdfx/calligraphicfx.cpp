

#include "stdfx.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#ifdef _WIN32
#include "windows.h"
#endif
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>
#include <math.h>

#include "tfxparam.h"
#include "tdoubleparam.h"
#include "ttzpimagefx.h"

//===================================================================

class CalligraphicFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(CalligraphicFx)
  TRasterFxPort m_input;
  TStringParamP m_colorIndex;
  TDoubleParamP m_thickness;
  TDoubleParamP m_horizontal;
  TDoubleParamP m_upWDiagonal;
  TDoubleParamP m_vertical;
  TDoubleParamP m_doWDiagonal;
  TDoubleParamP m_accuracy;
  TDoubleParamP m_noise;

public:
  CalligraphicFx()
      : m_colorIndex(L"1,2,3")
      , m_thickness(5.0)
      , m_horizontal(100.0)
      , m_upWDiagonal(50.0)
      , m_vertical(0.0)
      , m_doWDiagonal(50.0)
      , m_accuracy(50.0)
      , m_noise(0.0) {
    m_thickness->setMeasureName("fxLength");
    addInputPort("Source", m_input);
    bindParam(this, "Color_Index", m_colorIndex);
    bindParam(this, "Thickness", m_thickness);
    bindParam(this, "Accuracy", m_accuracy);
    bindParam(this, "Noise", m_noise);
    bindParam(this, "Horizontal", m_horizontal);
    bindParam(this, "upWDiagonal", m_upWDiagonal);
    bindParam(this, "Vertical", m_vertical);
    bindParam(this, "doWDiagonal", m_doWDiagonal);
    m_thickness->setValueRange(0.0, 60.0);
    m_horizontal->setValueRange(0.0, 100.0);
    m_upWDiagonal->setValueRange(0.0, 100.0);
    m_vertical->setValueRange(0.0, 100.0);
    m_doWDiagonal->setValueRange(0.0, 100.0);
    m_accuracy->setValueRange(0.0, 100.0);
    m_noise->setValueRange(0.0, 100.0);
  }

  ~CalligraphicFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected()) {
      TRenderSettings info2(info);

      int shrink = tround((info.m_shrinkX + info.m_shrinkY) / 2.0);
      int argc   = 8;
      const char *argv[8];
      argv[0] = strsave(::to_string(m_colorIndex->getValue()).c_str());
      getValues(argv, argc, frame);
      SandorFxRenderData *calligraphicData =
          new SandorFxRenderData(Calligraphic, argc, argv, 0, shrink);
      CalligraphicParams &params = calligraphicData->m_callParams;
      params.m_accuracy          = m_accuracy->getValue(frame);
      params.m_horizontal        = m_horizontal->getValue(frame);
      params.m_colorIndex        = m_colorIndex->getValue();
      params.m_upWDiagonal       = m_upWDiagonal->getValue(frame);
      params.m_noise             = m_noise->getValue(frame);
      params.m_doWDiagonal       = m_doWDiagonal->getValue(frame);
      params.m_thickness         = m_thickness->getValue(frame) * 0.5;
      params.m_vertical          = m_vertical->getValue(frame);
      info2.m_data.push_back(calligraphicData);

      return m_input->doGetBBox(frame, bBox, info2);
    } else {
      bBox = TRectD();
      return false;
    }
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  bool allowUserCacheOnPort(int port) override { return false; }

private:
  void getValues(const char *argv[], int argc, double frame) {
    double values[8];
    values[1] = m_noise->getValue(frame);
    values[2] = m_accuracy->getValue(frame);
    values[3] = m_doWDiagonal->getValue(frame);
    values[4] = m_vertical->getValue(frame);
    values[5] = m_upWDiagonal->getValue(frame);
    values[6] = m_horizontal->getValue(frame);
    values[7] = m_thickness->getValue(frame) * 0.5;
    convertParam(values, argv, argc);
  }
  //----------------------------------------------------------------------

  char *strsave(const char *t) {
    char *s;
    s = (char *)malloc(strlen(t) + 1);
    strcpy(s, t);
    return s;
  }

  //-----------------------------------------------------------------------

  void convertParam(double param[], const char *cParam[], int cParamLen) {
    std::string app;
    for (int i = 1; i < cParamLen; i++) {
      app       = std::to_string(param[i]);
      cParam[i] = strsave(app.c_str());
    }
  }

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &ri) override {
    if (!m_input.isConnected()) return;

    int shrink = tround((ri.m_shrinkX + ri.m_shrinkY) / 2.0);
    int argc   = 8;
    const char *argv[8];
    argv[0] = strsave(::to_string(m_colorIndex->getValue()).c_str());
    getValues(argv, argc, frame);
    TRenderSettings ri2(ri);
    SandorFxRenderData *calligraphicData =
        new SandorFxRenderData(Calligraphic, argc, argv, 0, shrink);
    CalligraphicParams &params = calligraphicData->m_callParams;
    params.m_accuracy          = m_accuracy->getValue(frame);
    params.m_horizontal        = m_horizontal->getValue(frame);
    params.m_colorIndex        = m_colorIndex->getValue();
    params.m_upWDiagonal       = m_upWDiagonal->getValue(frame);
    params.m_noise             = m_noise->getValue(frame);
    params.m_doWDiagonal       = m_doWDiagonal->getValue(frame);
    params.m_thickness         = m_thickness->getValue(frame) * 0.5;
    params.m_vertical          = m_vertical->getValue(frame);
    ri2.m_data.push_back(calligraphicData);
    ri2.m_userCachable = false;
    m_input->compute(tile, frame, ri2);
  }

  void transform(double frame, int port, const TRectD &rectOnOutput,
                 const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                 TRenderSettings &infoOnInput) override {
    rectOnInput = rectOnOutput;
    infoOnInput = infoOnOutput;

    int shrink =
        tround((infoOnOutput.m_shrinkX + infoOnOutput.m_shrinkY) / 2.0);
    int argc = 8;
    const char *argv[8];
    argv[0] = strsave(::to_string(m_colorIndex->getValue()).c_str());
    getValues(argv, argc, frame);
    SandorFxRenderData *calligraphicData =
        new SandorFxRenderData(Calligraphic, argc, argv, 0, shrink);
    CalligraphicParams &params = calligraphicData->m_callParams;
    params.m_accuracy          = m_accuracy->getValue(frame);
    params.m_horizontal        = m_horizontal->getValue(frame);
    params.m_colorIndex        = m_colorIndex->getValue();
    params.m_upWDiagonal       = m_upWDiagonal->getValue(frame);
    params.m_noise             = m_noise->getValue(frame);
    params.m_doWDiagonal       = m_doWDiagonal->getValue(frame);
    params.m_thickness         = m_thickness->getValue(frame);
    params.m_vertical          = m_vertical->getValue(frame);
    infoOnInput.m_data.push_back(calligraphicData);
    infoOnInput.m_userCachable = false;
  }
};

//------------------------------------------------------------------

//===================================================================
class OutBorderFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(OutBorderFx)
  TRasterFxPort m_input;
  TDoubleParamP m_thickness;
  TDoubleParamP m_horizontal;
  TDoubleParamP m_upWDiagonal;
  TDoubleParamP m_vertical;
  TDoubleParamP m_doWDiagonal;
  TDoubleParamP m_accuracy;
  TDoubleParamP m_noise;

public:
  OutBorderFx()
      : m_thickness(5.0)
      , m_horizontal(100.0)
      , m_upWDiagonal(100.0)
      , m_vertical(100.0)
      , m_doWDiagonal(100.0)
      , m_accuracy(50.0)
      , m_noise(0.0) {
    m_thickness->setMeasureName("fxLength");
    addInputPort("Source", m_input);
    bindParam(this, "Thickness", m_thickness);
    bindParam(this, "Accuracy", m_accuracy);
    bindParam(this, "Noise", m_noise);
    bindParam(this, "Horizontal", m_horizontal);
    bindParam(this, "upWDiagonal", m_upWDiagonal);
    bindParam(this, "Vertical", m_vertical);
    bindParam(this, "doWDiagonal", m_doWDiagonal);
    m_thickness->setValueRange(0.0, 30.0);
    m_horizontal->setValueRange(0.0, 100.0);
    m_upWDiagonal->setValueRange(0.0, 100.0);
    m_vertical->setValueRange(0.0, 100.0);
    m_doWDiagonal->setValueRange(0.0, 100.0);
    m_accuracy->setValueRange(0.0, 100.0);
    m_noise->setValueRange(0.0, 100.0);
  }

  ~OutBorderFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected()) {
      TRenderSettings info2(info);

      int shrink = tround((info.m_shrinkX + info.m_shrinkY) / 2.0);
      int argc   = 8;
      const char *argv[8];
      argv[0] = "-1";
      getValues(argv, argc, frame);
      SandorFxRenderData *outBorderData =
          new SandorFxRenderData(OutBorder, argc, argv, 0, shrink);
      CalligraphicParams &params = outBorderData->m_callParams;
      params.m_accuracy          = m_accuracy->getValue(frame);
      params.m_horizontal        = m_horizontal->getValue(frame);
      params.m_colorIndex        = L"-1";
      params.m_upWDiagonal       = m_upWDiagonal->getValue(frame);
      params.m_noise             = m_noise->getValue(frame);
      params.m_doWDiagonal       = m_doWDiagonal->getValue(frame);
      params.m_thickness         = m_thickness->getValue(frame);
      params.m_vertical          = m_vertical->getValue(frame);
      info2.m_data.push_back(outBorderData);

      return m_input->doGetBBox(frame, bBox, info2);
    } else {
      bBox = TRectD();
      return false;
    }
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

private:
  void getValues(const char *argv[], int argc, double frame) {
    double values[8];
    values[1] = m_noise->getValue(frame);
    values[2] = m_accuracy->getValue(frame);
    values[3] = m_doWDiagonal->getValue(frame);
    values[4] = m_vertical->getValue(frame);
    values[5] = m_upWDiagonal->getValue(frame);
    values[6] = m_horizontal->getValue(frame);
    values[7] = m_thickness->getValue(frame);
    convertParam(values, argv, argc);
  }
  //----------------------------------------------------------------------

  char *strsave(const char *t) {
    char *s;
    s = (char *)malloc(strlen(t) + 1);
    strcpy(s, t);
    return s;
  }

  //-----------------------------------------------------------------------

  void convertParam(double param[], const char *cParam[], int cParamLen) {
    std::string app;
    for (int i = 1; i < cParamLen; i++) {
      app       = std::to_string(param[i]);
      cParam[i] = strsave(app.c_str());
    }
  }

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &ri) override {
    if (!m_input.isConnected()) return;

    int shrink = tround((ri.m_shrinkX + ri.m_shrinkY) / 2.0);
    int argc   = 8;
    const char *argv[8];
    argv[0] = "-1";
    getValues(argv, argc, frame);
    TRenderSettings ri2(ri);
    SandorFxRenderData *outBorderData =
        new SandorFxRenderData(OutBorder, argc, argv, 0, shrink);
    CalligraphicParams &params = outBorderData->m_callParams;
    params.m_accuracy          = m_accuracy->getValue(frame);
    params.m_horizontal        = m_horizontal->getValue(frame);
    params.m_colorIndex        = L"-1";
    params.m_upWDiagonal       = m_upWDiagonal->getValue(frame);
    params.m_noise             = m_noise->getValue(frame);
    params.m_doWDiagonal       = m_doWDiagonal->getValue(frame);
    params.m_thickness         = m_thickness->getValue(frame);
    params.m_vertical          = m_vertical->getValue(frame);
    ri2.m_data.push_back(outBorderData);
    m_input->compute(tile, frame, ri2);
  }

  void transform(double frame, int port, const TRectD &rectOnOutput,
                 const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                 TRenderSettings &infoOnInput) override {
    rectOnInput = rectOnOutput;
    infoOnInput = infoOnOutput;

    int shrink =
        tround((infoOnOutput.m_shrinkX + infoOnOutput.m_shrinkY) / 2.0);
    int argc = 8;
    const char *argv[8];
    argv[0] = "-1";
    getValues(argv, argc, frame);
    SandorFxRenderData *outBorderData =
        new SandorFxRenderData(OutBorder, argc, argv, 0, shrink);
    CalligraphicParams &params = outBorderData->m_callParams;
    params.m_accuracy          = m_accuracy->getValue(frame);
    params.m_horizontal        = m_horizontal->getValue(frame);
    params.m_colorIndex        = L"-1";
    params.m_upWDiagonal       = m_upWDiagonal->getValue(frame);
    params.m_noise             = m_noise->getValue(frame);
    params.m_doWDiagonal       = m_doWDiagonal->getValue(frame);
    params.m_thickness         = m_thickness->getValue(frame);
    params.m_vertical          = m_vertical->getValue(frame);
    infoOnInput.m_data.push_back(outBorderData);
  }
};

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(CalligraphicFx, "calligraphicFx");

FX_PLUGIN_IDENTIFIER(OutBorderFx, "outBorderFx");
