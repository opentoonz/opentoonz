#pragma once

#ifndef PLASTICDEFORMERFX_H
#define PLASTICDEFORMERFX_H

// TnzLib includes
#include "toonz/txshsimplelevel.h"
#include "toonz/tstageobject.h"

// TnzBase includes
#include "trasterfx.h"

//=====================================================

//    Forward declarations

class TXsheet;

//=====================================================

//***************************************************************************************************
//    PlasticDeformerFx  declaration
//***************************************************************************************************

//! PlasticDeformerFx is a hidden fx used to inject data about a plastic deformation just above a
//! TLevelColumnFx instance.
class PlasticDeformerFx : public TRasterFx
{
	FX_DECLARATION(PlasticDeformerFx)

public:
	TXsheet *m_xsh;			//!< The Fx's enclosing Xsheet
	int m_col;				//!< The input column index
	TAffine m_texPlacement; //!< Texture to mesh reference transform

	TRasterFxPort m_port; //!< Input port

public:
	PlasticDeformerFx();

	TFx *clone(bool recursive = true) const;

	bool canHandle(const TRenderSettings &info, double frame);

	std::string getAlias(double frame, const TRenderSettings &info) const;
	bool doGetBBox(double frame, TRectD &bbox, const TRenderSettings &info);

	void doCompute(TTile &tile, double frame, const TRenderSettings &info);
	void doDryCompute(TRectD &rect, double frame, const TRenderSettings &info);

	std::string getPluginId() const { return std::string(); }

private:
	void buildRenderSettings(double, TRenderSettings &);

	bool buildTextureData(double, TRenderSettings &, TAffine &);
	bool buildTextureDataSl(double, TRenderSettings &, TAffine &);

	// not implemented
	PlasticDeformerFx(const PlasticDeformerFx &);
	PlasticDeformerFx &operator=(const PlasticDeformerFx &);
};

#endif // PLASTICDEFORMERFX_H
