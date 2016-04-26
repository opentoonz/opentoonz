

// TnzCore includes
#include "trop.h"
#include "tpixelutils.h"

// TnzBase includes
#include "tparamset.h"
#include "tdoubleparam.h"
#include "tzeraryfx.h"
#include "tfxparam.h"
#include "tparamuiconcept.h"

#include "tbasefx.h"
#include "tzeraryfx.h"

//==================================================================

class ColorCardFx : public TBaseZeraryFx
{
	FX_DECLARATION(ColorCardFx)

	TPixelParamP m_color;

  public:
	ColorCardFx() : m_color(TPixel32::Green)
	{
		bindParam(this, "color", m_color);
		m_color->setDefaultValue(TPixel32::Green);
		setName(L"ColorCardFx");
	}

	bool canHandle(const TRenderSettings &info, double frame) { return true; }

	bool doGetBBox(double frame, TRectD &bBox, const TRenderSettings &info)
	{
		bBox = TConsts::infiniteRectD;
		return true;
	}

	void doCompute(TTile &tile, double frame, const TRenderSettings &)
	{
		TRaster32P raster32 = tile.getRaster();
		if (raster32)
			raster32->fill(m_color->getPremultipliedValue(frame));
		else {
			TRaster64P ras64 = tile.getRaster();
			if (ras64)
				ras64->fill(toPixel64(m_color->getPremultipliedValue(frame)));
			else
				throw TException("ColorCardFx unsupported pixel type");
		}
	};
};

//==================================================================

class CheckBoardFx : public TBaseZeraryFx
{
	FX_DECLARATION(CheckBoardFx)

	TPixelParamP m_color1, m_color2;
	TDoubleParamP m_size;

  public:
	CheckBoardFx() : m_color1(TPixel32::Black), m_color2(TPixel32::White), m_size(50)
	{
		m_size->setMeasureName("fxLength");
		bindParam(this, "color1", m_color1);
		bindParam(this, "color2", m_color2);
		bindParam(this, "size", m_size);
		m_color1->setDefaultValue(TPixel32::Black);
		m_color2->setDefaultValue(TPixel32::White);
		m_size->setValueRange(1, 1000);
		m_size->setDefaultValue(50);
		setName(L"CheckBoardFx");
	}

	bool canHandle(const TRenderSettings &info, double frame) { return false; }

	bool doGetBBox(double, TRectD &bBox, const TRenderSettings &info)
	{
		bBox = TConsts::infiniteRectD;
		return true;
	}

	void doCompute(TTile &tile, double frame, const TRenderSettings &info)
	{
		const TPixel32 &c1 = m_color1->getValue(frame);
		const TPixel32 &c2 = m_color2->getValue(frame);

		double size = m_size->getValue(frame);

		assert(info.m_shrinkX == info.m_shrinkY);
		size *= info.m_affine.a11 / info.m_shrinkX;

		TDimensionD dim(size, size);
		TRop::checkBoard(tile.getRaster(), c1, c2, dim, tile.m_pos);
	}

	void getParamUIs(TParamUIConcept *&concepts, int &length)
	{
		concepts = new TParamUIConcept[length = 1];

		concepts[0].m_type = TParamUIConcept::SIZE;
		concepts[0].m_label = "Size";
		concepts[0].m_params.push_back(m_size);
	}
};

//==================================================================

FX_IDENTIFIER(ColorCardFx, "colorCardFx")
FX_IDENTIFIER(CheckBoardFx, "checkBoardFx")
