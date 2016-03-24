#include "plugin_param_interface.h"
#include "plugin_utilities.h"
#include "tparamset.h"
#include "../include/ttonecurveparam.h"
#include "../include/toonzqt/fxsettings.h"
#include "plugin_param_traits.h"

/* 公開インターフェイスからは削除 */
enum toonz_param_value_type_enum {
	/* deprecated */
	TOONZ_PARAM_VALUE_TYPE_CHAR = 1,
	TOONZ_PARAM_VALUE_TYPE_INT = 4,
	TOONZ_PARAM_VALUE_TYPE_DOUBLE = 8,

	TOONZ_PARAM_VALUE_TYPE_NB,
	TOONZ_PARAM_VALUE_TYPE_MAX = 0x7FFFFFFF
};

enum toonz_value_unit_enum {
	TOONZ_PARAM_UNIT_NONE,
	TOONZ_PARAM_UNIT_LENGTH,
	TOONZ_PARAM_UNIT_ANGLE,
	TOONZ_PARAM_UNIT_SCALE,
	TOONZ_PARAM_UNIT_PERCENTAGE,
	TOONZ_PARAM_UNIT_PERCENTAGE2,
	TOONZ_PARAM_UNIT_SHEAR,
	TOONZ_PARAM_UNIT_COLOR_CHANNEL,

	TOONZ_PARAM_UNIT_NB,
	TOONZ_PARAM_UNIT_MAX = 0x7FFFFFFF
};

static int set_value_unit(TDoubleParamP param, toonz_value_unit_enum unit);

static int set_value_unit(TDoubleParamP param, toonz_value_unit_enum unit)
{
	switch (unit) {
	case TOONZ_PARAM_UNIT_NONE:
		break;
	case TOONZ_PARAM_UNIT_LENGTH:
		param->setMeasureName("fxLength");
		break;
	case TOONZ_PARAM_UNIT_ANGLE:
		param->setMeasureName("angle");
		break;
	case TOONZ_PARAM_UNIT_SCALE:
		param->setMeasureName("scale");
		break;
	case TOONZ_PARAM_UNIT_PERCENTAGE:
		param->setMeasureName("percentage");
		break;
	case TOONZ_PARAM_UNIT_PERCENTAGE2:
		param->setMeasureName("percentage2");
		break;
	case TOONZ_PARAM_UNIT_SHEAR:
		param->setMeasureName("shear");
		break;
	case TOONZ_PARAM_UNIT_COLOR_CHANNEL:
		param->setMeasureName("colorChannel");
		break;
	default:
		printf("invalid param unit");
		return TOONZ_ERROR_INVALID_VALUE;
	}

	return TOONZ_OK;
}

int get_type(toonz_node_handle_t node, toonz_param_handle_t param, double frame, int *ptype, int *pcount)
{
	/* size はほとんどの型で自明なので返さなくてもいいよね? */
	if (!node || !ptype || !pcount)
		return TOONZ_ERROR_NULL;

	TFx* fx = reinterpret_cast<TFx *>(node);
	if (Param *p = reinterpret_cast<Param *>(param)) {
		toonz_param_type_enum e = toonz_param_type_enum(p->desc()->traits_tag);
		if (e >= TOONZ_PARAM_TYPE_NB)
			return TOONZ_ERROR_NOT_IMPLEMENTED;
		size_t v;
		if (parameter_type_check(p->param(fx).getPointer(), p->desc(), v)) {
			*ptype = p->desc()->traits_tag;
			if (e == TOONZ_PARAM_TYPE_STRING) {
				TStringParam *r = reinterpret_cast<TStringParam *>(p->param(fx).getPointer());
				const std::string str = QString::fromStdWString(r->getValue()).toStdString();
				*pcount = str.length() + 1;
			} else if (e == TOONZ_PARAM_TYPE_TONECURVE) {
				TToneCurveParam *r = reinterpret_cast<TToneCurveParam *>(p->param(fx).getPointer());
				auto lst = r->getValue(frame);
				*pcount = lst.size();
			} else {
				*pcount = 1; //static_cast< int >(v);
			}
			return TOONZ_OK;
		} else
			return TOONZ_ERROR_NOT_IMPLEMENTED;
	}
	return TOONZ_ERROR_INVALID_HANDLE;
}

int get_value(toonz_node_handle_t node, toonz_param_handle_t param, double frame, int *psize_in_bytes, void *pvalue)
{
	if (!node || !psize_in_bytes) {
		return TOONZ_ERROR_NULL;
	}

	if (!pvalue) {
		int type = 0;
		return get_type(node, param, frame, &type, psize_in_bytes);
	}

	TFx* fx = reinterpret_cast<TFx *>(node);
	if (Param *p = reinterpret_cast<Param *>(param)) {
		toonz_param_type_enum e = toonz_param_type_enum(p->desc()->traits_tag);
		if (e >= TOONZ_PARAM_TYPE_NB)
			return TOONZ_ERROR_NOT_IMPLEMENTED;
		size_t v;
		int icounts = *psize_in_bytes;
		if (parameter_read_value(p->param(fx).getPointer(), p->desc(), pvalue, frame, icounts, v)) {
			*psize_in_bytes = v;
			return TOONZ_OK;
		} else
			return TOONZ_ERROR_NOT_IMPLEMENTED;
	}
	return TOONZ_ERROR_INVALID_HANDLE;
}

int get_string_value(toonz_node_handle_t node, toonz_param_handle_t param, int *wholesize, int rcvbufsize, char *pvalue)
{
	if (!node || !pvalue)
		return TOONZ_ERROR_NULL;

	TFx* fx = reinterpret_cast<TFx *>(node);
	if (Param *p = reinterpret_cast<Param *>(param)) {
		const toonz_param_desc_t *desc = p->desc();
		toonz_param_type_enum e = toonz_param_type_enum(desc->traits_tag);
		size_t vsz;
		TParam *pp = p->param(fx).getPointer();
		if (param_type_check_<tpbind_str_t>(pp, desc, vsz)) {
			size_t isize = rcvbufsize;
			size_t osize = 0;
			if (param_read_value_<tpbind_str_t>(pp, desc, pvalue, 0, isize, osize)) {
				if (wholesize)
					*wholesize = static_cast<int>(osize);
				return TOONZ_OK;
			}
		}
	}
	return TOONZ_ERROR_INVALID_HANDLE;
}

int get_spectrum_value(toonz_node_handle_t node, toonz_param_handle_t param, double frame, double x, toonz_param_spectrum_t *pvalue)
{
	if (!node || !pvalue)
		return TOONZ_ERROR_NULL;

	TFx* fx = reinterpret_cast<TFx *>(node);
	if (Param *p = reinterpret_cast<Param *>(param)) {
		const toonz_param_desc_t *desc = p->desc();
		toonz_param_type_enum e = toonz_param_type_enum(desc->traits_tag);
		size_t vsz;
		TParam *pp = p->param(fx).getPointer();
		if (param_type_check_<tpbind_spc_t>(pp, desc, vsz)) {
			size_t isize = 1;
			size_t osize = 0;
			pvalue->w = x;
			if (param_read_value_<tpbind_spc_t>(pp, desc, pvalue, frame, isize, osize)) {
				return TOONZ_OK;
			}
		}
	}
	return TOONZ_ERROR_INVALID_HANDLE;
}

