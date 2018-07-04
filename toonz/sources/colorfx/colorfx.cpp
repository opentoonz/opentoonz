


#include "strokestyles.h"
#include "regionstyles.h"
#include "rasterstyles.h"
#include "colorfx.h"

// static TPluginInfo info("ColorFxPlugin");

//-------------------------------------------------------------------

namespace {

//-------------------------------------------------------------------

void add(TColorStyle *s) { TColorStyle::declare(s); }

//-------------------------------------------------------------------

}  // namespace

//-------------------------------------------------------------------

void initColorFx() {
  // add(new TFriezeStrokeStyle);

  add(new TRopeStrokeStyle);

  add(new TChainStrokeStyle);
  add(new TFurStrokeStyle);
  // add(new TChalkStrokeStyle);
  // add(new TBumpStrokeStyle);
  // add(new TBlendStrokeStyle);

  add(new TDottedLineStrokeStyle);

  add(new TBraidStrokeStyle);
  add(new TSketchStrokeStyle);
  add(new TBubbleStrokeStyle);
  add(new TGraphicPenStrokeStyle);
  add(new TCrystallizeStrokeStyle);
  add(new TSprayStrokeStyle);
  add(new TTissueStrokeStyle);
  // add(new TMultiLineStrokeStyle);
  add(new TBiColorStrokeStyle);
  add(new TNormal2StrokeStyle);
  // add(new TNormalStrokeStyle);
  // add(new TLongBlendStrokeStyle);
  add(new TChalkStrokeStyle2);
  // add(new TDualColorStrokeStyle);

  add(new TBlendStrokeStyle2);
  add(new TTwirlStrokeStyle);

  add(new TMultiLineStrokeStyle2);
  add(new TZigzagStrokeStyle);  // non funziona su linux, rivedere
  add(new TSinStrokeStyle);

  add(new TFriezeStrokeStyle2);
  add(new TDualColorStrokeStyle2());  // non funziona (massimo)  su linux,
                                      // rivedere
  add(new TLongBlendStrokeStyle2());

  add(new TMatrioskaStrokeStyle());

#ifdef _DEBUG
  add(new OutlineViewerStyle());
#endif

  add(new MovingSolidColor(TPixel32::Blue, TPointD(10, 10)));  // ok
  // add(new
  // MovingTexture(readTexture("chessboard.bmp"),TTextureStyle::NONE,TPointD(10,10)
  // ));

  add(new ShadowStyle(TPixel32::White, TPixel32::Black));
  add(new ShadowStyle2(TPixel32::Yellow, TPixel32::Magenta));

  add(new TRubberFillStyle(TPixel32(255, 0, 255, 127), 25.0));
  add(new TPointShadowFillStyle(TPixel32(255, 255, 200), TPixel32(215, 0, 0)));

  add(new TDottedFillStyle(TPixel32::Green));

  add(new TCheckedFillStyle(TPixel32(255, 0, 0, 128)));
  add(new ArtisticSolidColor(TPixel32(0, 130, 255), TPointD(10, 10), 100));

  add(new TChalkFillStyle(TPixel32::White, TPixel32::Black));  // non funziona
  add(new TChessFillStyle(TPixel32::Red));

  add(new TSawToothStrokeStyle);
  add(new TStripeFillStyle(TPixel32::Green));

  add(new TLinGradFillStyle(TPixel32::Green));
  add(new TRadGradFillStyle(TPixel32::Green));
  add(new TCircleStripeFillStyle(TPixel32::Green));
  add(new TMosaicFillStyle(TPixel32::Red));
  add(new TPatchFillStyle(TPixel32::Blue));
  add(new TAirbrushRasterStyle(TPixel32::Black, 10));
  add(new TBlendRasterStyle(TPixel32::Black, 10));
  add(new TNoColorRasterStyle());
}
