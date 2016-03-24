#include <stdio.h>
#include <toonz_plugin.h>
#include <toonz_hostif.h>
#include <memory>
#include <cmath>
#include <vector>
#include "pixelop.hpp"
#include "toonz_plugin_helper_rect.h"

#include <utils/interf_holder.hpp>
#include <utils/param_traits.hpp>

#ifdef _MSC_VER
//	Visual Studio is not supported __restrict for reference
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

ToonzRect conv2rect(toonz::rect_t &&r)
{
	return ToonzRect(r.x0, r.y0, r.x1, r.y1);
}

toonz::rect_t conv2rect(ToonzRect &&r)
{
	return toonz::rect_t{r.x0, r.y0, r.x1, r.y1};
}

ToonzRect conv2rect(const toonz::rect_t &r)
{
	return ToonzRect(r.x0, r.y0, r.x1, r.y1);
}

toonz::rect_t conv2rect(const ToonzRect &r)
{
	return toonz::rect_t{r.x0, r.y0, r.x1, r.y1};
}

template <typename T>
void dump_value(const T &v)
{
	printf("value: type:%s {%d}\n", typeid(T).name(), v);
}

template <>
void dump_value<toonz_param_traits_double_t::iovaluetype>(const toonz_param_traits_double_t::iovaluetype &v)
{
	printf("value: type:%s {%g}\n", typeid(toonz_param_traits_double_t::iovaluetype).name(), v);
}

template <>
void dump_value<toonz_param_traits_range_t::iovaluetype>(const toonz_param_traits_range_t::iovaluetype &v)
{
	printf("value: type:%s {%g, %g}\n", typeid(toonz_param_traits_range_t::iovaluetype).name(), v.a, v.b);
}

template <>
void dump_value<toonz_param_traits_point_t::iovaluetype>(const toonz_param_traits_point_t::iovaluetype &v)
{
	printf("value: type:%s {%g, %g}\n", typeid(toonz_param_traits_point_t::iovaluetype).name(), v.x, v.y);
}

template <>
void dump_value<toonz_param_traits_color_t::iovaluetype>(const toonz_param_traits_color_t::iovaluetype &v)
{
	printf("value: type:%s {%d, %d, %d, %d}\n", typeid(toonz_param_traits_color_t::iovaluetype).name(), v.c0, v.c1, v.c2, v.m);
}

template <>
void dump_value<toonz_param_traits_spectrum_t::iovaluetype>(const toonz_param_traits_spectrum_t::iovaluetype &v)
{
	printf("value: type:%s {%g, %g, %g, %g, %g}\n", typeid(toonz_param_traits_spectrum_t::iovaluetype).name(), v.w, v.c0, v.c1, v.c2, v.m);
}

template <>
void dump_value<toonz_param_traits_tonecurve_t::iovaluetype>(const toonz_param_traits_tonecurve_t::iovaluetype &v)
{
	printf("value: type:%s {%g, %g, %d, %d}\n", typeid(toonz_param_traits_tonecurve_t::iovaluetype).name(), v.x, v.y, v.channel, v.interp);
}

template <typename T>
bool get_and_check(toonz::node_handle_t node, const char *nm, double frame, double opt = 0)
{
	auto nodeif = grab_interf<toonz::node_interface_t>(TOONZ_UUID_NODE);
	auto pif = grab_interf<toonz::param_interface_t>(TOONZ_UUID_PARAM);
	toonz_param_handle_t p1;
	int ret;
	ret = nodeif->get_param(node, nm, &p1);
	printf("get_param(%s): ret:%d\n", nm, ret);

	int type = 0;
	int cnt = 0;
	pif->get_type(node, p1, frame, &type, &cnt);
	printf("parameter(ret:%d): name:%s (type:%d count:%d) expect:(type:%d) typesize:%ld\n", ret, nm, type, cnt, T::E, sizeof(typename T::iovaluetype));
	if (type != T::E)
		return false;

	typename T::iovaluetype v;
	ret = pif->get_value(node, p1, frame, &cnt, &v);
	printf("get_value: ret:%d count:%d\n", ret, cnt);
	dump_value<typename T::iovaluetype>(v);
	return true;
}

template <>
bool get_and_check<toonz_param_traits_string_t>(toonz::node_handle_t node, const char *nm, double frame, double opt)
{
	auto nodeif = grab_interf<toonz::node_interface_t>(TOONZ_UUID_NODE);
	auto pif = grab_interf<toonz::param_interface_t>(TOONZ_UUID_PARAM);
	toonz_param_handle_t p1;
	int ret;
	ret = nodeif->get_param(node, nm, &p1);
	printf("get_param(%s): ret:%d\n", nm, ret);

	int type = 0;
	int cnt = 0;
	ret = pif->get_type(node, p1, frame, &type, &cnt);
	printf("parameter(ret:%d): name:%s (type:%d count:%d) expect:(type:%d) typesize:-\n", ret, nm, type, cnt, toonz_param_traits_string_t::E);
	if (type != toonz_param_traits_string_t::E)
		return false;

	std::vector<toonz_param_traits_string_t::iovaluetype> v(cnt);
	ret = pif->get_value(node, p1, frame, &cnt, v.data());
	printf("get_value: ret:%d count:%d\n", ret, cnt);
	//dump_value< toonz_param_traits_string_t::iovaluetype* >(v);
	printf("value: type:%s {%s}\n", typeid(toonz_param_traits_string_t::iovaluetype).name(), v.begin());
	return true;
}

template <>
bool get_and_check<toonz_param_traits_spectrum_t>(toonz::node_handle_t node, const char *nm, double frame, double opt)
{
	auto nodeif = grab_interf<toonz::node_interface_t>(TOONZ_UUID_NODE);
	auto pif = grab_interf<toonz::param_interface_t>(TOONZ_UUID_PARAM);
	toonz_param_handle_t p1;
	int ret;
	ret = nodeif->get_param(node, nm, &p1);
	printf("get_param(%s): ret:%d\n", nm, ret);
	int type = 0;
	int cnt = 0;
	ret = pif->get_type(node, p1, frame, &type, &cnt);
	printf("parameter(ret:%d): name:%s (type:%d count:%d) expect:(type:%d) typesize:-\n", ret, nm, type, cnt, toonz_param_traits_spectrum_t::E);
	if (type != toonz_param_traits_spectrum_t::E)
		return false;

	toonz_param_traits_spectrum_t::iovaluetype v;
	v.w = opt;
	ret = pif->get_value(node, p1, frame, &cnt, &v);
	printf("get_value: ret:%d count:%d\n", ret, cnt);
	dump_value<toonz_param_traits_spectrum_t::iovaluetype>(v);
	return true;
}

template <>
bool get_and_check<toonz_param_traits_tonecurve_t>(toonz::node_handle_t node, const char *nm, double frame, double opt)
{
	auto nodeif = grab_interf<toonz::node_interface_t>(TOONZ_UUID_NODE);
	auto pif = grab_interf<toonz::param_interface_t>(TOONZ_UUID_PARAM);
	toonz_param_handle_t p1;
	int ret;
	ret = nodeif->get_param(node, nm, &p1);
	printf("get_param(%s): ret:%d\n", nm, ret);

	int type = 0;
	int cnt = 0;
	ret = pif->get_type(node, p1, frame, &type, &cnt);
	printf("parameter)(ret:%d): name:%s (type:%d count:%d) expect:(type:%d) sz:-\n", ret, nm, type, cnt, toonz_param_traits_tonecurve_t::E);
	if (type != toonz_param_traits_tonecurve_t::E)
		return false;

	std::vector<toonz_param_traits_tonecurve_t::iovaluetype> v(cnt);
	ret = pif->get_value(node, p1, frame, &cnt, v.data());
	printf("get_value: ret:%d count:%d\n", ret, cnt);
	for (int i = 0; i < cnt; i++) {
		dump_value<toonz_param_traits_tonecurve_t::iovaluetype>(v[i]);
	}
	return true;
}

void do_compute(toonz::node_handle_t node, const toonz::rendering_setting_t *rs, double frame, toonz::tile_handle_t tile)
{
	printf("do_compute(): node:%p tile:%p frame:%g\n", node, tile, frame);

	toonz::port_handle_t port = nullptr;
	{
		auto nodeif = grab_interf<toonz::node_interface_t>(TOONZ_UUID_NODE);
		if (nodeif) {
			int ret = nodeif->get_input_port(node, "IPort", &port);
			if (ret)
				printf("do_compute(): get_input_port: ret:%d\n", ret);

			// test
			get_and_check<toonz_param_traits_double_t>(node, "first", frame);
			get_and_check<toonz_param_traits_range_t>(node, "second", frame);
			get_and_check<toonz_param_traits_color_t>(node, "third", frame);
			get_and_check<toonz_param_traits_point_t>(node, "fourth", frame);
			get_and_check<toonz_param_traits_enum_t>(node, "fifth", frame);
			get_and_check<toonz_param_traits_int_t>(node, "sixth", frame);
			get_and_check<toonz_param_traits_bool_t>(node, "seventh", frame);
			get_and_check<toonz_param_traits_string_t>(node, "eighth", frame);
			get_and_check<toonz_param_traits_spectrum_t>(node, "ninth", frame, 0.5);
			get_and_check<toonz_param_traits_tonecurve_t>(node, "tenth", frame);

			get_and_check<toonz_param_traits_tonecurve_t>(node, "unknown", frame);

			toonz::param_handle_t str;
			nodeif->get_param(node, "eighth", &str);
			toonz::param_handle_t notstr;
			nodeif->get_param(node, "seventh", &notstr);
			toonz::param_handle_t spec;
			nodeif->get_param(node, "ninth", &spec);
			auto pif = grab_interf<toonz::param_interface_t>(TOONZ_UUID_PARAM);
			if (pif) {
				int t, l;
				pif->get_type(node, str, frame, &t, &l);
				int sz;
				std::vector<char> shortage_buf(l / 2 + 1);
				int ret = pif->get_string_value(node, str, &sz, l / 2 + 1, shortage_buf.data());
				printf("get_string_value(string): {%s} (length:%d) ret:%d\n", shortage_buf.data(), sz, ret);

				ret = pif->get_string_value(node, notstr, &sz, l / 2 + 1, shortage_buf.data()); // must be error
				printf("get_string_value(not string): sz:%d ret:%d\n", sz, ret);		  // size must be not changed

				pif->get_type(node, spec, frame, &t, &l);
				toonz_param_spectrum_t spc;
				ret = pif->get_spectrum_value(node, spec, frame, 0, &spc);
				printf("get_spectrum_value(spec): ret:%d\n", ret);
				dump_value(spc);
				ret = pif->get_spectrum_value(node, str, frame, 0, &spc);
				printf("get_spectrum_value(str): ret:%d\n", ret); // must be error
			}
		}
	}

	int blur = 1;

	auto tileif = grab_interf<toonz::tile_interface_t>(TOONZ_UUID_TILE);
	auto portif = grab_interf<toonz::port_interface_t>(TOONZ_UUID_PORT);
	if (port && tileif && portif) {
		toonz::fxnode_handle_t fx;
		portif->get_fx(port, &fx);
		auto fxif = grab_interf<toonz::fxnode_interface_t>(TOONZ_UUID_FXNODE);
		if (fxif) {
			/* evaluate upstream's flow */
			toonz::rect_t rect;
			tileif->get_rectangle(tile, &rect);

			toonz::rect_t inRect = conv2rect(conv2rect(rect).enlarge(blur, blur));
			toonz::tile_handle_t result = nullptr;
			tileif->create(&result);
			fxif->compute_to_tile(fx, rs, frame, &inRect, tile, result);

			/* evaluate the fx */
			int x = rect.x0;
			int y = rect.y0;
			int lx = rect.x1 - rect.x0;
			int ly = rect.y1 - rect.y0;

			int sstride, dstride;
			tileif->get_raw_stride(result, &sstride);
			tileif->get_raw_stride(tile, &dstride);
			int selement_type, delement_type;

			tileif->get_element_type(result, &delement_type);
			tileif->get_element_type(tile, &selement_type);
			uint8_t *saddr, *daddr;

			tileif->get_raw_address_unsafe(result, reinterpret_cast<void **>(&saddr));
			tileif->get_raw_address_unsafe(tile, reinterpret_cast<void **>(&daddr));

			printf("%f %f %f %f\n", rect.x0, rect.y0, rect.x1, rect.y1);

			printf("-+ x:%d y:%d lx:%d ly:%d\n", x, y, lx, ly);
			printf(" + src tile: addr:%p stride:%d type:%d\n", saddr, sstride, selement_type);
			printf(" + dst tile: addr:%p stride:%d type:%d\n", daddr, dstride, delement_type);

			// need compatibility check for formats

			hv_kernel<uint8_t, 4>(daddr, saddr + sstride + 4, lx, ly, dstride, sstride, [](uint8_t v[4], const uint8_t *p, int stride, int xinbytes, int yinbytes) {
				uint32_t l00 = *reinterpret_cast<const uint32_t *>(p + -stride - 4);
				uint32_t l10 = *reinterpret_cast<const uint32_t *>(p + -stride);
				uint32_t l20 = *reinterpret_cast<const uint32_t *>(p + -stride + 4);
				uint32_t l01 = *reinterpret_cast<const uint32_t *>(p - 4);
				uint32_t l11 = *reinterpret_cast<const uint32_t *>(p);
				uint32_t l21 = *reinterpret_cast<const uint32_t *>(p + 4);
				uint32_t l02 = *reinterpret_cast<const uint32_t *>(p + stride - 4);
				uint32_t l12 = *reinterpret_cast<const uint32_t *>(p + stride);
				uint32_t l22 = *reinterpret_cast<const uint32_t *>(p + stride + 4);
				uint32_t r = 0, g = 0, b = 0, m = 0;
				//printf("col:0x%08x [%d, %d]\n", l11, xinbytes, yinbytes);

				/* TODO:
						toonz の TPixel は char r, g, b, m; のように持っているのでそのまま uin32_t だとエンディアンが変わる.
						toonz ビルド時の configuration はコンパイル時に参照できるようにしておくべきだ.
						*/

				m += ((l00 >> 24) & 0xff);
				m += ((l10 >> 24) & 0xff);
				m += ((l20 >> 24) & 0xff);
				m += ((l01 >> 24) & 0xff);
				m += ((l11 >> 24) & 0xff);
				m += ((l21 >> 24) & 0xff);
				m += ((l02 >> 24) & 0xff);
				m += ((l12 >> 24) & 0xff);
				m += ((l22 >> 24) & 0xff);

				b += ((l00 >> 16) & 0xff);
				b += ((l10 >> 16) & 0xff);
				b += ((l20 >> 16) & 0xff);
				b += ((l01 >> 16) & 0xff);
				b += ((l11 >> 16) & 0xff);
				b += ((l21 >> 16) & 0xff);
				b += ((l02 >> 16) & 0xff);
				b += ((l12 >> 16) & 0xff);
				b += ((l22 >> 16) & 0xff);

				g += ((l00 >> 8) & 0xff);
				g += ((l10 >> 8) & 0xff);
				g += ((l20 >> 8) & 0xff);
				g += ((l01 >> 8) & 0xff);
				g += ((l11 >> 8) & 0xff);
				g += ((l21 >> 8) & 0xff);
				g += ((l02 >> 8) & 0xff);
				g += ((l12 >> 8) & 0xff);
				g += ((l22 >> 8) & 0xff);

				r += ((l00 >> 0) & 0xff);
				r += ((l10 >> 0) & 0xff);
				r += ((l20 >> 0) & 0xff);
				r += ((l01 >> 0) & 0xff);
				r += ((l11 >> 0) & 0xff);
				r += ((l21 >> 0) & 0xff);
				r += ((l02 >> 0) & 0xff);
				r += ((l12 >> 0) & 0xff);
				r += ((l22 >> 0) & 0xff);

				v[0] = static_cast<uint8_t>(r / 9);
				v[1] = static_cast<uint8_t>(g / 9);
				v[2] = static_cast<uint8_t>(b / 9);
				v[3] = static_cast<uint8_t>(m / 9);
			});
		}
	}
}

int do_get_bbox(toonz::node_handle_t node, const toonz::rendering_setting_t *rs, double frame, toonz::rect_t *rect)
{
	printf("do_get_bbox(): node:%p\n", node);
	bool modified = false;

	toonz::port_handle_t port = nullptr;
	{
		auto nodeif = grab_interf<toonz::node_interface_t>(TOONZ_UUID_NODE);
		if (nodeif) {
			int ret = nodeif->get_input_port(node, "IPort", &port);
			printf("do_get_bbox(): get_input_port:%d\n", ret);
		}
	}

	auto portif = grab_interf<toonz::port_interface_t>(TOONZ_UUID_PORT);
	if (port && portif) {
		int con = 0;
		portif->is_connected(port, &con);
		if (con) {
			toonz::fxnode_handle_t fx;
			portif->get_fx(port, &fx);
			auto fxif = grab_interf<toonz::fxnode_interface_t>(TOONZ_UUID_FXNODE);
			if (fxif) {
				int ret = 0;
				/* この引数の順番つらいなぁ, fx, rs, frame, rect の順だよね */
				fxif->get_bbox(fx, rs, frame, rect, &ret);

				/* 画素単位の値を現在の座標系に合わせる */
				toonz::affine_t a = rs->affine;
				double det = a.a11 * a.a22 - a.a12 * a.a21;
				double scale = sqrt(fabs(det));
				double blur = 1.0 / scale;

				ToonzRect r = conv2rect(*rect);
				r = r.enlarge(blur, blur);
				*rect = conv2rect(std::move(r));
				modified = true;
			}
		}
	}
	return modified;
}

param_desc_t params0_[] = {
	param_desc_ctor<traits_double_t>("first", "1st", {1.0 /* default */, -1.0 /* min */, 1.0 /* max */}, "description of the first param"),
	param_desc_ctor<traits_range_t>("second", "2nd", {{48.0, 64.0} /* default */, {0.0, 180.0} /* min-max */}),
	param_desc_ctor<traits_color_t>("third", "3rd", {0xff, 0xff, 0xff, 0xff} /* white */),
	param_desc_ctor<traits_point_t>("fourth", "4th", {{64, -64} /* default */, {-200, 200} /* min */, {-200, +200} /* max */}),
};

const char *strtbl_[] = {"tokyo", "osaka", "nagoya"};

param_desc_t params1_[] = {
	param_desc_ctor<traits_enum_t>("fifth", "5th", {2, 3, strtbl_}),
	param_desc_ctor<traits_int_t>("sixth", "6th", {2, -10, 10}, "this is integer"),
	param_desc_ctor<traits_bool_t>("seventh", "7th", {1}),
};

toonz_param_traits_spectrum_t::valuetype points_[] = {{0.0, 1.0, 0.0, 0.0, 1.0},
													  {0.5, 0.5, 0.0, 0.5, 1.0},
													  {1.0, 0.0, 0.0, 0.5, 1.0}};

param_desc_t params2_[] = {
	param_desc_ctor<traits_string_t>("eighth", "8th", {"this is sample message"}),
	param_desc_ctor<traits_spectrum_t>("ninth", "9th", {0.1, 3, points_}),
	param_desc_ctor<traits_tonecurve_t>("tenth", "10th", {/* tonecurve has no default */}),
};

toonz_param_group_t groups0_[] = {
	param_group_ctor("Group1", sizeof(params0_) / sizeof(param_desc_t), params0_),
	param_group_ctor("Group2", sizeof(params1_) / sizeof(param_desc_t), params1_),
	param_group_ctor("Group3", sizeof(params2_) / sizeof(param_desc_t), params2_),
};

toonz_param_page_t pages_[] = {
	param_page_ctor("Properties", sizeof(groups0_) / sizeof(toonz_param_group_t), groups0_),
};

int node_setup(toonz::node_handle_t node)
{
	printf("blur: setup(): node:%p\n", node);

	auto setup = grab_interf<toonz::setup_interface_t>(TOONZ_UUID_SETUP);
	if (setup) {
		int errcode = 0;
		void *entry = NULL;
		int ret = setup->set_parameter_pages_with_error(node, sizeof(pages_) / sizeof(toonz_param_page_t), pages_, &errcode, &entry);
		if (ret) {
			printf("setup error:0x%x reason:0x%x entry:%p\n", ret, errcode, entry);
		}
		setup->add_input_port(node, "IPort", TOONZ_PORT_TYPE_RASTER);
	}
	return 0;
}

int node_create(toonz::node_handle_t node)
{
	printf("blur: create(): node:%p\n", node);
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
	return TOONZ_OK;
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
TOONZ_PLUGIN_PROBE_DEFINE(TOONZ_PLUGIN_VER(1, 0), "blur-plugin" /* name */, "dwango" /* vendor */, "libblur.plugin" /* identifier */, "a blur plugin for test", "http://dwango.co.jp/", TOONZ_PLUGIN_CLASS_POSTPROCESS_SLAB, &toonz_plugin_node_handler)
TOONZ_PLUGIN_PROBE_END;
}
