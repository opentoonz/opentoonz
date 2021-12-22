#include <stdio.h>
#include <toonz_plugin.h>
#include <toonz_hostif.h>
#include <toonz_params.h>
#include <memory>
#include <cmath>
#include <limits>
#include <utils/rect.hpp>
#include <utils/affine.hpp>
#include <utils/interf_holder.hpp>
#include <utils/param_traits.hpp>

#ifdef _MSC_VER
#define RESTRICT
#else
#define RESTRICT __restrict
#endif

extern "C" {
TOONZ_EXPORT int toonz_plugin_init(toonz::host_interface_t *hostif);
TOONZ_EXPORT void toonz_plugin_exit();
}

toonz::host_interface_t *ifactory_ = NULL;

int toonz_plugin_init(toonz::host_interface_t *hostif)
{
	printf("toonz_plugin_init()\n");
	ifactory_ = hostif;
	return TOONZ_OK;
}

void toonz_plugin_exit()
{
	printf("toonz_plugin_exit()\n");
}

void do_compute(toonz::node_handle_t node, const toonz::rendering_setting_t *rs, double frame, toonz::tile_handle_t tile)
{
	printf("do_compute(): node:%p tile:%p frame:%g\n", node, tile, frame);

	auto nodeif = grab_interf<toonz::node_interface_t>(TOONZ_UUID_NODE);
	if (nodeif) {
		void *param = NULL;
		int r = nodeif->get_param(node, "p3rd", &param);
		if (r == TOONZ_OK) {
			printf("do_compute: get param: p:%p\n", param);
			auto pif = grab_interf<toonz::param_interface_t>(TOONZ_UUID_PARAM);
			if (pif) {
				int type, counts;
				pif->get_type(param, frame, &type, &counts);
				printf("do_compute: get value type:%d\n", type);
				toonz_param_traits_point_t::iovaluetype v;
				pif->get_value(param, frame, &counts, &v);

				printf("do_compute: value:[%g, %g]\n", v.x, v.y);
			}
		} else {
			printf("do_compute: error to get param:0x%x\n", r);
		}
	}

	auto tileif = grab_interf<toonz::tile_interface_t>(TOONZ_UUID_TILE);
	if (tileif) {
		toonz::rect_t rect;
		tileif->get_rectangle(tile, &rect);

		/* evaluate the fx */
		int lx = rect.x1 - rect.x0;
		int ly = rect.y1 - rect.y0;

		ToonzAffine mat(rs->affine);
		ToonzAffine invmat = mat.inv();
		ToonzPoint p = invmat * ToonzPoint(rect.x0, rect.y0);

		int dstride = 0;
		tileif->get_raw_stride(tile, &dstride);
		int delement_type = 0;
		tileif->get_element_type(tile, &delement_type);
		uint8_t *daddr = NULL;
		tileif->get_raw_address_unsafe(tile, reinterpret_cast<void **>(&daddr));

		printf("-+ lx:%d ly:%d\n", lx, ly);
		printf("-+ px:%g py:%g\n", p.x, p.y);
		printf(" + dst rect(%g, %g, %g, %g)\n", rect.x0, rect.y0, rect.x1, rect.y1);
		printf(" + dst tile: addr:%p stride:%d type:%d\n", daddr, dstride, delement_type);

		// need compatibility check for formats
		for (int i = 0; i < ly; i++) {
			uint32_t *ptr = reinterpret_cast<uint32_t *>(daddr + i * dstride);
			/* [rx, ry] は effect の面内を描くベクトル. 半径 r 円の中心が [0, 0] で、 [-lx/2, -ly/2]-[lx/2, ly/2]
			   [j, i] は dst のスキャンラインのベクトルで [0, 0]-[lx, ly] */
			double rx = p.x;
			double ry = p.y;
			for (int j = 0; j < lx; j++) {
				double r = std::sqrt(rx * rx + ry * ry);
				double k = 0;
				if (r < 64)
					k = 1;
				uint8_t c = (uint8_t)(k * 255);
				*(ptr + j) = 0xff000000 | (c << 16) | (c << 8) | c;
				/* hor advance */
				rx += invmat.a11;
				ry += invmat.a21;
			}
			/* vert advance */
			p.x += invmat.a12;
			p.y += invmat.a22;
		}
	}
}

int do_get_bbox(toonz::node_handle_t node, const toonz::rendering_setting_t *rs, double frame, toonz::rect_t *rect)
{
	printf("do_get_bbox(): node:%p\n", node);
	bool modified = false;
/* 円の周辺の黒いマスク部分が bbox に相当する:
	   コンサバには無限に大きな矩形を返すのが最もコンサバな方法だが、 rendering_setting_t の affine 変換行列を用いてよりよい予測をすることもできる.
	 */
#if 0
	/* too concervative */
	rect->x0 = -std::numeric_limits< double >::max();
	rect->y0 = -std::numeric_limits< double >::max();
	rect->x1 = std::numeric_limits< double >::max();
	rect->y1 = std::numeric_limits< double >::max();
#endif
#if 0
	/* too aggressive */
	rect->x0 = -64;
	rect->y0 = -64;
	rect->x1 = 64;
	rect->y1 = 64;
#endif
	/* more reasonable than above */
	ToonzRect r(-64, -64, 64, 64);
	ToonzRect bbox = ToonzAffine(rs->affine) * r;
	rect->x0 = bbox.x0;
	rect->y0 = bbox.y0;
	rect->x1 = bbox.x1;
	rect->y1 = bbox.y1;
	return modified;
}

param_desc_t params0_[] = {
	param_desc_ctor<traits_double_t>("p1st", "", {1.0 /* default */, -1.0 /* min */, 1.0 /* max */}),
	param_desc_ctor<traits_range_t>("p2nd", "", {{48.0, 64.0} /* default */, {0.0, 180.0} /* min-max */}),
	param_desc_ctor<traits_point_t>("p3rd", "", {{0, 0} /* default */, {-10, 10} /* min */, {-200, +200} /* max */}),
};

param_desc_t params1_[] = {
	param_desc_ctor<traits_double_t>("g1st", "", {1.0 /* default */, -1.0 /* min */, 1.0 /* max */}),
	param_desc_ctor<traits_range_t>("g2nd", "", {{48.0, 64.0} /* default */, {0.0, 180.0} /* min-max */}),
	param_desc_ctor<traits_point_t>("g3rd", "", {{1, 1} /* default */, {-10, 10} /* min */, {-200, +200} /* max */}),
};

param_desc_t params2_[] = {
	param_desc_ctor<traits_double_t>("pp1st", "", {1.0 /* default */, -1.0 /* min */, 1.0 /* max */}),
	param_desc_ctor<traits_range_t>("pp2nd", "", {{48.0, 64.0} /* default */, {0.0, 90.0} /* min-max */}),
	param_desc_ctor<traits_point_t>("pp3rd", "", {{1, 1} /* default */, {-10, 10} /* min */, {-200, +200} /* max */}),
};

toonz_param_group_t groups0_[] = {
	param_group_ctor("First Group", sizeof(params0_) / sizeof(param_desc_t), params0_),
	param_group_ctor("Second Group", sizeof(params1_) / sizeof(param_desc_t), params1_),
};

toonz_param_group_t groups1_[] = {
	param_group_ctor("Third Group", sizeof(params2_) / sizeof(param_desc_t), params2_),
};

toonz_param_page_t pages_[] = {
	param_page_ctor("Main Page", sizeof(groups0_) / sizeof(toonz_param_group_t), groups0_),
	param_page_ctor("Sub Page", sizeof(groups1_) / sizeof(toonz_param_group_t), groups1_),
};

int node_setup(toonz::node_handle_t node)
{
	auto setup = grab_interf<toonz::setup_interface_t>(TOONZ_UUID_SETUP);
	if (setup) {
		int errcode = 0;
		void *entry = NULL;
		int ret = setup->set_parameter_pages_with_error(node, 2, pages_, &errcode, &entry);
		if (ret) {
			printf("setup error:0x%x reason:0x%x entry:%p\n", ret, errcode, entry);
		}

		setup->add_input_port(node, "IPort", TOONZ_PORT_TYPE_RASTER);
	}
	return 0;
}

int node_create(toonz::node_handle_t node)
{
	printf("create(): node:%p\n", node);
	return TOONZ_OK;
}

int node_destroy(toonz::node_handle_t node)
{
	printf("destroy():node:%p\n", node);
	return TOONZ_OK;
}

int can_handle(toonz_node_handle_t node, const toonz_rendering_setting_t *rs, double frame)
{
	printf("%s:\n", __FUNCTION__);
	/* geometric plugin のように Bounding Box が自明の場合、 Skew などの変換の影響でフルスクリーン効果のような大きな Bounding Box が
	   過剰に大きくなってしまうことがある. これを防ぐには非ゼロを返すこと.
	 */
	return 1; /* non zero is 'true' */
}

size_t get_memory_requirement(toonz_node_handle_t node, const toonz_rendering_setting_t *rs, double frame, const toonz_rect_t *rect)
{
	printf("%s:\n", __FUNCTION__);
	return TOONZ_OK;
}

void on_new_frame(toonz_node_handle_t node, const toonz_rendering_setting_t *rs, double frame)
{
	printf("%s:\n", __FUNCTION__);
}

void on_end_frame(toonz_node_handle_t node, const toonz_rendering_setting_t *rs, double frame)
{
	printf("%s:\n", __FUNCTION__);
}

toonz_nodal_rasterfx_handler_t_ toonz_plugin_node_handler =
	{
		{1, 0},					//	ver
		do_compute,				//	do_compute
		do_get_bbox,			//	do_get_bbox
		can_handle,				//	can_handle
		get_memory_requirement, //	get_memory_requirement
		on_new_frame,			//	on_new_frame
		on_end_frame,			//	on_end_frame
		node_create,			//	create
		node_destroy,			//	destroy
		node_setup};

extern "C" {
TOONZ_PLUGIN_PROBE_BEGIN(TOONZ_IF_VER(1, 0))
TOONZ_PLUGIN_PROBE_DEFINE(TOONZ_PLUGIN_VER(1, 0), "geom-plugin" /* name */, "dwango" /* vendor */, "geom.plugin" /* identifier */, "a geom plugin for test", "http://dwango.co.jp/", TOONZ_PLUGIN_CLASS_POSTPROCESS_SLAB | TOONZ_PLUGIN_CLASS_MODIFIER_GEOMETRIC, &toonz_plugin_node_handler)
TOONZ_PLUGIN_PROBE_END;
}
