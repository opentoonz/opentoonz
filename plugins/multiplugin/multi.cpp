#include <stdio.h>
#include <toonz_plugin.h>
#include <toonz_hostif.h>
#include <memory>
#include <cmath>
#include <limits>
#include <utils/rect.hpp>
#include <utils/affine.hpp>
#include <utils/interf_holder.hpp>
#include <utils/param_traits.hpp>

#ifdef _MSC_VER
//	Visual Studio is not supported __restrict for reference
#define RESTRICT
#else
#define RESTRICT __restrict
#endif

extern "C" {
TOONZ_EXPORT void toonz_plugin_probe(toonz_plugin_probe_t **begin, toonz_plugin_probe_t **end);
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

const char *strtbl[] = {"tokyo", "osaka", "kobe"};
param_desc_t params0_[] = {
	param_desc_ctor<traits_int_t>("p1st", "integer", {2 /* default */, -10 /* min */, 10 /* max */}, "this is integer"),
	param_desc_ctor<traits_enum_t>("p2nd", "enumuration", {1, 3, strtbl}),
	param_desc_ctor<traits_bool_t>("p3rd", "boolean", {1}),
	param_desc_ctor<traits_string_t>("p4th", "string", {"a message"}),
};

toonz_param_traits_spectrum_t::valuetype specs[] = {{0.0, 1.0, 0.0, 0.0, 1.0},
													{0.5, 0.5, 0.0, 0.5, 1.0},
													{1.0, 0.0, 0.0, 0.5, 1.0}};

param_desc_t params1_[] = {
	param_desc_ctor<traits_color_t>("p1st", "color", {0, 0, 255 /* blue */, 255}),
	param_desc_ctor<traits_spectrum_t>("p2nd", "spectrum", {0.1, 3, specs}),
	param_desc_ctor<traits_tonecurve_t>("p3rd", "tone curve", {}),
};

toonz_param_group_t groups0_[] = {
	param_group_ctor("Group", sizeof(params0_) / sizeof(param_desc_t), params0_),
};

toonz_param_group_t groups1_[] = {
	param_group_ctor("Group", sizeof(params1_) / sizeof(param_desc_t), params1_),
};

toonz_param_page_t pages0_[] = {
	param_page_ctor("1st plugin", sizeof(groups0_) / sizeof(toonz_param_group_t), groups0_),
};

toonz_param_page_t pages1_[] = {
	param_page_ctor("2nd plugin", sizeof(groups1_) / sizeof(toonz_param_group_t), groups1_),
};

int node_setup_a(toonz::node_handle_t node)
{
	printf("plugin A: setup(): node:%p\n", node);
	auto setup = grab_interf<toonz::setup_interface_t>(TOONZ_UUID_SETUP);
	if (setup) {
		int errcode = 0;
		void *position = NULL;
		int err = setup->set_parameter_pages_with_error(node, 1, pages0_, &errcode, &position);
		if (err == TOONZ_ERROR_INVALID_VALUE) {
			printf("setup err: reason:0x%x address:%p\n", errcode, position);
		}
	}
	return 0;
}

int node_setup_b(toonz::node_handle_t node)
{
	printf("plugin B: setup(): node:%p\n", node);
	auto setup = grab_interf<toonz::setup_interface_t>(TOONZ_UUID_SETUP);
	if (setup) {
		int errcode = 0;
		void *position = NULL;
		int err = setup->set_parameter_pages_with_error(node, 1, pages1_, &errcode, &position);
		if (err == TOONZ_ERROR_INVALID_VALUE) {
			printf("setup err: reason:0x%x address:%p\n", errcode, position);
		}
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

toonz_nodal_rasterfx_handler_t_ toonz_plugin_node_handler0 = {
	TOONZ_IF_VER(1, 0),		//	type ver
	do_compute,				//	do_compute
	do_get_bbox,			//	do_get_bbox
	can_handle,				//	can_handle
	get_memory_requirement, //	get_memory_requirement
	on_new_frame,			//	on_new_frame
	on_end_frame,			//	on_end_frame
	node_create,			//	create
	node_destroy,			//	destroy
	node_setup_a};

toonz_nodal_rasterfx_handler_t_ toonz_plugin_node_handler1 = {
	TOONZ_IF_VER(1, 0),		//	type ver
	do_compute,				//	do_compute
	do_get_bbox,			//	do_get_bbox
	can_handle,				//	can_handle
	get_memory_requirement, //	get_memory_requirement
	on_new_frame,			//	on_new_frame
	on_end_frame,			//	on_end_frame
	node_create,			//	create
	node_destroy,			//	destroy
	node_setup_b};

/**
 *    Memory
 *
 *   |//////////////////////|
 *   +----------------------+ <- toonz_plugin_info_list.begin
 *   |  1st plugin          |
 *   +----------------------+
 *   |  2nd plugin          |
 *   +----------------------+
 *   |                      |
 *             :
 *   |                      |
 *   +----------------------+ <- toonz_plugin_info_list.end
 *   |  empty(all zeroed)   |
 *   +----------------------+
 *   |//// unused area /////|
 * 
 * 1. the array of plugins should be lain in the continuous area on a memory.
 * 2. an empty element must be zero-ed all (including 'ver').
 * 3. 'ver' fields must be same each other.
 */

extern "C" {
TOONZ_PLUGIN_PROBE_BEGIN(TOONZ_IF_VER(1, 0))
TOONZ_PLUGIN_PROBE_DEFINE(TOONZ_PLUGIN_VER(1, 0), "multi1-plugin" /* name */, "dwango" /* vendor */, "multi1.plugin" /* identifier */, "1st plugin from the single module", "http://dwango.co.jp/", TOONZ_PLUGIN_CLASS_POSTPROCESS_SLAB | TOONZ_PLUGIN_CLASS_MODIFIER_GEOMETRIC, &toonz_plugin_node_handler0)
,
	TOONZ_PLUGIN_PROBE_DEFINE(TOONZ_PLUGIN_VER(1, 0), "multi2-plugin" /* name */, "dwango" /* vendor */, "multi2.plugin" /* identifier */, "2nd plugin from the single module", "http://dwango.co.jp/", TOONZ_PLUGIN_CLASS_POSTPROCESS_SLAB | TOONZ_PLUGIN_CLASS_MODIFIER_GEOMETRIC, &toonz_plugin_node_handler1)
		TOONZ_PLUGIN_PROBE_END;
}

const toonz_plugin_probe_list_t_ *toonz_plugin_probe()
{
	printf("toonz_plugin_probe()\n");
	return &toonz_plugin_info_list;
}
