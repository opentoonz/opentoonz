#include "igs_os_type.h"
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include <stdexcept>
namespace {

#ifndef _pri_h_
#define _pri_h_

/* Windowsではstdint.hが見付からない */
#if defined _MSC_VER
typedef int int32_t;
#else
#include <stdint.h> /* for int32_t */
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern void pri_funct_cv_start(int32_t i32_ys);
extern void pri_funct_cv_run(int32_t i32_y);
extern void pri_funct_cv_end(void);

extern void pri_funct_set_cp_title(const char *cp_title);
extern void pri_funct_msg_ttvr(const char *fmt, ...);
extern void pri_funct_msg_vr(const char *fmt, ...);
extern void pri_funct_err_bttvr(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* !_pri_h_ */
#include <stdio.h>

#include "igs_line_blur.h"  // "pri.h"

static int32_t pri_param_i32_ysize, pri_param_i32_pos_before;

/* カウントダウン開始 */
void pri_funct_cv_start(int32_t i32_ys) {
  /* 目盛表示 */
  (void)fprintf(stdout,
                "0%%  10   20   30   40   50   60   70   80   90   100%%\n");
  (void)fprintf(stdout, "....v....v....v....v....v....v....v....v....v....v\n");

  /* パラメータ初期設定 */
  pri_param_i32_ysize      = i32_ys;
  pri_param_i32_pos_before = 0;
}
/* カウントダウン実行中 */
void pri_funct_cv_run(int32_t i32_y) {
  int32_t i32_pos_current, ii;

  /* 目盛上の現在位置(1...50) */
  i32_pos_current = 50 * (i32_y + 1) / pri_param_i32_ysize;

  /* 一回で45度くるくるカウンター */
  /******switch (i32_y%4) {
  case 0: (void)fprintf(stdout, "-\b"  ); break;
  case 1: (void)fprintf(stdout, "\\\b" ); break;
  case 2: (void)fprintf(stdout, "|\b"  ); break;
  case 3: (void)fprintf(stdout, "/\b"  ); break;
  }
  (void)fflush(stdout);******/

  /* 一回前の目盛位置と同じなら抜ける */
  if (i32_pos_current == pri_param_i32_pos_before) return;

  /* 目盛が進んだとき */
  for (ii = pri_param_i32_pos_before; ii < i32_pos_current; ++ii) {
    /* 一目盛以上進んだときのすき間 */
    if ((ii + 1) < i32_pos_current) {
      (void)fprintf(stdout, ".");
    }
    /* 一目盛分 */
    else {
      (void)fprintf(stdout, "^");
    }
  }
  (void)fflush(stdout);

  /* 現在位置を記憶する */
  pri_param_i32_pos_before = i32_pos_current;
}
/* カウントダウン終了 */
void pri_funct_cv_end(void) {
  /* 改行 */
  (void)fprintf(stdout, "\n");
}

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "igs_line_blur.h"  // "pri.h"

/* Windowsではvsnprintf()の頭にアンダーバーが付く!!! */
#if defined _WIN32
#define vsnprintf(buf, len, fmt, ap) _vsnprintf(buf, len, fmt, ap)
#endif

static const char *pri_param_cp_com_name = "#";

void pri_funct_set_cp_title(const char *cp_title) {
  pri_param_cp_com_name = cp_title;
}

/* print  Valiable_argument, Return_code */
void pri_funct_msg_vr(const char *fmt, ...) {
  int i_ret;
  va_list ap;
  char buf[FILENAME_MAX]; /* redhat9 : stdio.h --> bits/stdio_lim.h */

  va_start(ap, fmt);
  i_ret = vsnprintf(buf, FILENAME_MAX, fmt, ap);
  va_end(ap);
  if (i_ret < 0) {
    (void)strcpy(buf, "bad argument for fprintf stdout");
  }

  /* 可変引数,改行 */
  (void)fprintf(stdout, "%s\n", buf);
  (void)fflush(stdout);
}

/* print  Title, Time_stamp, Valiable_argument, Return_code */
void pri_funct_msg_ttvr(const char *fmt, ...) {
  time_t tt;
  char *cc;

  int i_ret;
  va_list ap;
  char buf[FILENAME_MAX]; /* redhat9 : stdio.h --> bits/stdio_lim.h */

  tt     = time(NULL);
  cc     = asctime(localtime(&tt)); /* 注意：ccはstatic変数 */
  cc[24] = '\0'; /* 26-character Example : "Fri Sep 13 00:00:00 1986\n\0" */

  va_start(ap, fmt);
  i_ret = vsnprintf(buf, FILENAME_MAX, fmt, ap);
  va_end(ap);
  if (i_ret < 0) {
    (void)strcpy(buf, "bad argument for fprintf stdout");
  }

  /* ベル,タイトル,日時,可変引数,改行 */
  (void)fprintf(stdout, "%s  %s  %s\n", pri_param_cp_com_name, cc, buf);
  (void)fflush(stdout);
}
/* print  Bell, Title, Time_stamp, Valiable_argument, Return_code */
void pri_funct_err_bttvr(const char *fmt, ...) {
  time_t tt;
  char *cc;

  int i_ret;
  va_list ap;
  char buf[FILENAME_MAX];

  tt     = time(NULL);
  cc     = asctime(localtime(&tt)); /* 注意：ccはstatic変数 */
  cc[24] = '\0'; /* 26-character Example : "Fri Sep 13 00:00:00 1986\n\0" */

  va_start(ap, fmt);
  i_ret = vsnprintf(buf, FILENAME_MAX, fmt, ap);
  va_end(ap);
  if (i_ret < 0) {
    (void)strcpy(buf, "bad argument for fprintf stderr");
  }

  /* ベル,タイトル,日時,可変引数,改行 */
  (void)fprintf(stderr, "\007%s  %s  %s\n", pri_param_cp_com_name, cc, buf);
  (void)fflush(stderr);
}

#ifndef _list_node_h_
#define _list_node_h_

#include <stdio.h>

class list_node {
public:
  list_node() {
    this->_clp_previous = NULL;
    this->_clp_next     = NULL;
  }

  list_node *get_clp_next(void) { return this->_clp_next; }
  list_node *get_clp_previous(void) { return this->_clp_previous; }

  void set_clp_next(list_node *clp) { this->_clp_next = clp; }
  void set_clp_previous(list_node *clp) { this->_clp_previous = clp; }

private:
  list_node *_clp_previous, *_clp_next;
};

#endif /* !_list_node_h_ */
#ifndef _list_root_h_
#define _list_root_h_

#include <stdio.h>

/* Windowsではstdint.hが見つからない */
#if defined _MSC_VER
typedef int int32_t;
typedef unsigned short uint16_t;
#else
#include <stdint.h> /* for int32_t, uint16_t */
#endif

#include "igs_line_blur.h"  // "list_node.h"

class list_root {
public:
  list_root() {
    this->_clp_first = NULL;
    this->_clp_last  = NULL;
    this->_i32_count = 0;
  }

  list_node *push(list_node *clp_previous, list_node *clp_new);
  list_node *pop(list_node *clp_old);

  list_node *get_clp_first(void) { return this->_clp_first; }
  list_node *get_clp_last(void) { return this->_clp_last; }

  int32_t get_i32_count(void) { return this->_i32_count; }

private:
  list_node *_clp_first, *_clp_last;
  int32_t _i32_count;

  void _set_clp_first(list_node *clp_) { this->_clp_first = clp_; }
  void _set_clp_last(list_node *clp_) { this->_clp_last = clp_; }

  void _connect(list_node *clp_previous, list_node *clp_next);
};

#endif              /* !_list_root_h_ */
#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "list_root.h"

/* ピクセルリストを前後でつなげるメソッド */
void list_root::_connect(list_node *clp_previous, list_node *clp_next) {
  /* リスト初期化 */
  if ((NULL == clp_previous) && (NULL == clp_next)) {
    this->_clp_first = NULL;
    this->_clp_last  = NULL;
  }
  /* (clp_previous)をリストの最後部に追加 */
  else if ((NULL != clp_previous) && (NULL == clp_next)) {
    this->_clp_last = clp_previous;
    clp_previous->set_clp_next(NULL);
  }
  /* (clp_next)をリストの最前部に挿入 */
  else if ((NULL == clp_previous) && (NULL != clp_next)) {
    this->_clp_first = clp_next;
    clp_next->set_clp_previous(NULL);
  }
  /* (clp_previous)と、(clp_next)をつなげる */
  else if ((NULL != clp_previous) && (NULL != clp_next)) {
    clp_next->set_clp_previous(clp_previous);
    clp_previous->set_clp_next(clp_next);
  }
}

list_node *list_root::push(list_node *clp_previous, list_node *clp_new) {
  /* あってはならないプログラムバグのチェック */
  assert(NULL != clp_new);

  /* リスト上で、clp_newの後ろを接続 */
  if (NULL != clp_previous) { /* clp_previousの後ろに接続 */
    this->_connect(clp_new, clp_previous->get_clp_next());
  } else { /* clp_previousがNULLなら、リストの頭に挿入 */
    this->_connect(clp_new, this->get_clp_first());
  }

  /* リスト上で、clp_newの前を接続 */
  this->_connect(clp_previous, clp_new);

  /* 子リストの数のカウント */
  ++(this->_i32_count);

  return clp_new;
}
list_node *list_root::pop(list_node *clp_old) {
  /* あってはならないプログラムバグのチェック */
  assert(NULL != clp_old);

  /* はずすノードの前後をつなげる */
  this->_connect(clp_old->get_clp_previous(), clp_old->get_clp_next());

  /* 子リストの数のカウント */
  --(this->_i32_count);

  /* あってはならないプログラムバグのチェック */
  assert(0 <= (this->_i32_count));

  return clp_old;
}

#ifndef _brush_curve_blur_h_
#define _brush_curve_blur_h_

#include <stdio.h>

/* Windowsではstdint.hが見つからない */
#if defined _MSC_VER
typedef int int32_t;
#else
#include <stdint.h> /* for int32_t */
#endif

#ifndef OK
#define OK (0)
#endif
#ifndef NG
#define NG (-1)
#endif

#ifndef CHANNEL_COUNT
#define CHANNEL_COUNT (4)
#endif

class brush_curve_blur {
public:
  /* constructer */
  brush_curve_blur(void) {
    int32_t ii;

    this->_i_mv_sw = false;
    this->_i_pv_sw = false;
    this->_i_cv_sw = false;

    this->_i32_count            = 51;
    this->_i32_subpixel_divide  = 2;
    this->_d_effect_area_radius = 25.0;
    this->_d_power              = 1.0;

    this->_dp_ratio      = NULL;
    this->_dp_linepixels = NULL;
    this->_dp_xp         = NULL;
    this->_dp_yp         = NULL;
    this->_dp_subpixel   = NULL;
    for (ii = 0; ii < CHANNEL_COUNT; ++ii) {
      this->_da_pixel[ii] = 0.0;
    }
  }
  /* constructer */
  ~brush_curve_blur(void) { this->mem_free(); }

  /* パラメータ設定 */
  void set_i_mv_sw(bool ii) { this->_i_mv_sw = ii; }
  void set_i_pv_sw(bool ii) { this->_i_pv_sw = ii; }
  void set_i_cv_sw(bool ii) { this->_i_cv_sw = ii; }

  /* ぼかし線のポイント数 */
  void set_i32_count(int32_t ii) { this->_i32_count = ii; }
  int32_t get_i32_count(void) { return this->_i32_count; }

  void set_i32_subpixel_divide(int32_t ii) { this->_i32_subpixel_divide = ii; }
  int32_t get_i32_subpixel_divide(void) { return this->_i32_subpixel_divide; }

  void set_d_effect_area_radius(double dd) { this->_d_effect_area_radius = dd; }
  double get_d_effect_area_radius(void) { return this->_d_effect_area_radius; }

  void set_d_power(double dd) { this->_d_power = dd; }
  double get_d_power(void) { return this->_d_power; }

  /* get for using */
  double *get_dp_linepixels(void) { return this->_dp_linepixels; }
  double *get_dp_xp(void) { return this->_dp_xp; }
  double *get_dp_yp(void) { return this->_dp_yp; }
  double *get_dp_pixel(void) { return this->_da_pixel; }

  void mem_free(void);
  int mem_alloc(void);
  void init_ratio_array(void);
  void set_subpixel_value(int32_t i32_x_sub, int32_t i32_y_sub);
  void set_pixel_value(void);

  int save(double d_xp, double d_yp, const char *cp_fname);
  void debug_print(void);

private:
  bool _i_mv_sw; /* Method    Verbose */
  bool _i_pv_sw; /* Parameter Verbose */
  bool _i_cv_sw; /* Counter   Verbose */

  int32_t _i32_count;
  int32_t _i32_subpixel_divide;
  double _d_effect_area_radius;
  double _d_power;
  double *_dp_ratio, *_dp_linepixels, *_dp_xp, *_dp_yp, *_dp_subpixel,
      _da_pixel[CHANNEL_COUNT];
};

#endif              /* !_brush_curve_blur_h_ */
#include <math.h>   /* pow() */
#include <stdlib.h> /* free(), calloc() */
#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "brush_curve_blur.h"

/* メモリ開放 */
void brush_curve_blur::mem_free(void) {
  if (NULL != this->_dp_ratio) {
    if (this->_i_mv_sw) {
      pri_funct_msg_ttvr("brush_curve_blur::mem_free()");
    }
    free(this->_dp_ratio);
    this->_dp_ratio      = NULL;
    this->_dp_linepixels = NULL;
    this->_dp_xp         = NULL;
    this->_dp_yp         = NULL;
    this->_dp_subpixel   = NULL;
  }
}

/* メモリ確保 */
int brush_curve_blur::mem_alloc(void) {
  /* 以前のメモリが残っていたら開放する */
  this->mem_free();

  /* ユーザー指定がゼロなら動作キャンセル */
  if (this->_i32_count <= 0) {
    return OK;
  }

  /* 処理ごとのメッセージ */
  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("brush_curve_blur::mem_alloc()");
  }
  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr(
        "alloc brush_curve_blur memory ((%d * %d) + %d) x %d bytes",
        this->_i32_count, (1 + CHANNEL_COUNT + 2), /* ratio,accum,xp,yp */
        this->_i32_subpixel_divide * this->_i32_subpixel_divide *
            CHANNEL_COUNT, /* pixel(rgba) */
        sizeof(double));
  }

  this->_dp_ratio = (double *)calloc(
      /* ratio,linepixels,xp,yp */
      this->_i32_count * (1 + CHANNEL_COUNT + 2) +
          this->_i32_subpixel_divide * this->_i32_subpixel_divide *
              CHANNEL_COUNT, /* pixel(rgba) */
      sizeof(double));

  if (NULL == this->_dp_ratio) {
    pri_funct_err_bttvr("Error : calloc(-) returns NULL.");
    return NG;
  }

  this->_dp_linepixels = this->_dp_ratio + this->_i32_count;
  this->_dp_xp       = this->_dp_linepixels + this->_i32_count * CHANNEL_COUNT;
  this->_dp_yp       = this->_dp_xp + this->_i32_count;
  this->_dp_subpixel = this->_dp_yp + this->_i32_count;

  return OK;
}

/********************************************************************/

void brush_curve_blur::init_ratio_array(void) {
  int32_t ii;
  double d_total;

  /* ユーザー指定がゼロなら動作キャンセル */
  if (this->_i32_count <= 0) {
    return;
  }

  /* 前半の変化、ゼロからリニア増 */
  for (ii = 1; ii < this->_i32_count / 2; ++ii) {
    this->_dp_ratio[ii] = ii;
  }
  /* 後半の変化、ゼロへリニア減 */
  for (ii = this->_i32_count / 2; ii < this->_i32_count; ++ii) {
    this->_dp_ratio[ii] = this->_i32_count - ii;
  }

  /* 正規化(0...1)する */
  /****for (ii = 0; ii < this->_i32_count; ++ii) {
          this->_dp_ratio[ii] /=
                  (double)(this->_i32_count - this->_i32_count/2);
  }****/
  /* 影響の強さの設定 */
  for (ii = 0; ii < this->_i32_count; ++ii) {
    this->_dp_ratio[ii] = pow(this->_dp_ratio[ii], 1.0 / this->_d_power);
  }

  /* 総計 */
  d_total = 0.0;
  for (ii = 0; ii < this->_i32_count; ++ii) {
    d_total += this->_dp_ratio[ii];
  }
  /* 総計で割って、総計を1にする */
  for (ii = 0; ii < this->_i32_count; ++ii) {
    this->_dp_ratio[ii] /= d_total;
  }
}

/********************************************************************/

/* 線分から取り込んだピクセル値をサブピクセル値として合成 */
void brush_curve_blur::set_subpixel_value(int32_t i32_x_sub,
                                          int32_t i32_y_sub) {
  int32_t ii, zz;
  double d_accum;

  /* ユーザー指定がゼロなら動作キャンセル */
  if (this->_i32_count <= 0) {
    return;
  }

  /* チャネルごと */
  for (zz = 0; zz < CHANNEL_COUNT; ++zz) {
    /* 加算の前にゼロ初期化 */
    this->_dp_subpixel[(this->_i32_subpixel_divide * i32_y_sub + i32_x_sub) *
                           CHANNEL_COUNT +
                       zz] = 0.0;
    /* ピクセル値を加算 */
    d_accum = 0.0;
    for (ii = 0; ii < this->_i32_count; ++ii) {
      /* 画像の範囲外 */
      if (this->_dp_linepixels[ii * CHANNEL_COUNT + zz] < 0.0) {
        continue;
      }
      /* ピクセル値に比を掛けて加算 */
      this->_dp_subpixel[(this->_i32_subpixel_divide * i32_y_sub + i32_x_sub) *
                             CHANNEL_COUNT +
                         zz] +=
          this->_dp_linepixels[ii * CHANNEL_COUNT + zz] * this->_dp_ratio[ii];
      d_accum += this->_dp_ratio[ii];
    }
    /* 画像をスキャンしているのだから、
    画像の範囲となる値はかならず一つ以上ある */
    assert(0.0 < d_accum);

    /* 値の計算 */
    this->_dp_subpixel[(this->_i32_subpixel_divide * i32_y_sub + i32_x_sub) *
                           CHANNEL_COUNT +
                       zz] /= d_accum;
  }
}

/* サブピクセル値を平均してピクセル値を得る */
void brush_curve_blur::set_pixel_value(void) {
  int32_t xx, yy, zz;

  /* ユーザー指定がゼロなら動作キャンセル */
  if (this->_i32_count <= 0) {
    return;
  }

  for (zz = 0; zz < CHANNEL_COUNT; ++zz) {
    this->_da_pixel[zz] = 0.0;
    for (yy = 0; yy < this->_i32_subpixel_divide; ++yy) {
      for (xx = 0; xx < this->_i32_subpixel_divide; ++xx) {
        this->_da_pixel[zz] +=
            this->_dp_subpixel[(this->_i32_subpixel_divide * yy + xx) *
                                   CHANNEL_COUNT +
                               zz];
      }
    }
    this->_da_pixel[zz] /=
        this->_i32_subpixel_divide * this->_i32_subpixel_divide;
  }
}

#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "brush_curve_blur.h"

int brush_curve_blur::save(double d_xp, double d_yp, const char *cp_fname) {
  FILE *fp;
  int32_t ii;

  /* ファイル開く */
  fp = fopen(cp_fname, "w");
  if (NULL == fp) {
    pri_funct_err_bttvr("Error : fopen(%s,w) returns NULL", cp_fname);
    return NG;
  }

  /* 選択数保存 */
  if (fprintf(fp, "# curve blur count %d\n", this->get_i32_count()) < 0) {
    pri_funct_err_bttvr("Error : fprintf(# curve blur count) returns minus");
    fclose(fp);
    return NG;
  }

  for (ii = 0; ii < this->get_i32_count(); ++ii) {
    /* ピクセル位置から近点位置保存 */
    if (fprintf(fp, "%g %g\n", d_xp + this->_dp_xp[ii],
                d_yp + this->_dp_yp[ii]) < 0) {
      pri_funct_err_bttvr("Error : fprintf(avarage x y) returns minus");
      fclose(fp);
      return NG;
    }
  }

  /* ファイル閉じる */
  fclose(fp);

  return OK;
}

#ifndef __brush_smudge_circle_h__
#define __brush_smudge_circle_h__

#include <stdio.h>

/* Windowsではstdint.hが見つからない */
#if defined _MSC_VER
typedef int int32_t;
typedef unsigned short uint16_t;
#else
#include <stdint.h> /* for int32_t, uint16_t */
#endif

#ifndef OK
#define OK (0)
#endif
#ifndef NG
#define NG (-1)
#endif

class brush_smudge_circle {
public:
  brush_smudge_circle() {
    this->_i_mv_sw = false;
    this->_i_pv_sw = false;
    this->_i_cv_sw = false;

    this->_i32_size_by_pixel   = 7; /* 画像上の線の最大幅 */
    this->_i32_subpixel_divide = 4;
    this->_d_ratio             = 0.85;

    this->_dp_brush          = NULL;
    this->_dp_subpixel_image = NULL;
    this->_dp_pixel_image    = NULL;
  }
  ~brush_smudge_circle() { this->mem_free(); }

  /* パラメータ設定 */
  void set_i_mv_sw(bool ii) { this->_i_mv_sw = ii; }
  void set_i_pv_sw(bool ii) { this->_i_pv_sw = ii; }
  void set_i_cv_sw(bool ii) { this->_i_cv_sw = ii; }

  void set_i32_size_by_pixel(int32_t ii) { this->_i32_size_by_pixel = ii; }
  void set_i32_subpixel_divide(int32_t ii) { this->_i32_subpixel_divide = ii; }
  void set_d_ratio(double dd) { this->_d_ratio = dd; }

  /* パラメータを得る */
  int32_t get_i32_size_by_pixel(void) { return this->_i32_size_by_pixel; }
  int32_t get_i32_subpixel_divide(void) { return this->_i32_subpixel_divide; }
  double get_d_ratio(void) { return this->_d_ratio; }

  void get_dp_area(double d_xp, double d_yp, double *dp_x1, double *dp_y1,
                   double *dp_x2, double *dp_y2);

  /* メモリ確保 */
  int mem_alloc(void);

  /* メモリへのポインターを得る */
  double *get_dp_brush(void) { return this->_dp_brush; }
  double *get_dp_subpixel_image(void) { return this->_dp_subpixel_image; }
  double *get_dp_pixel_image(void) { return this->_dp_pixel_image; }

  /* 実行 */
  void set_brush_circle(void);
  void copy_to_brush_from_image(void);
  void exec(void);
  void to_subpixel_from_pixel(double d_x1, double d_y1, double d_x2,
                              double d_y2);
  void to_pixel_from_subpixel(double d_x1, double d_y1, double d_x2,
                              double d_y2);

  /* メモリ開放 */
  void mem_free(void);

private:
  bool _i_mv_sw; /* Method    Verbose */
  bool _i_pv_sw; /* Parameter Verbose */
  bool _i_cv_sw; /* Counter   Verbose */

  int32_t _i32_size_by_pixel, _i32_subpixel_divide;
  double _d_ratio;

  double *_dp_brush, *_dp_subpixel_image, *_dp_pixel_image;
};

#endif              /* !__brush_smudge_circle_h__ */
#include <stdlib.h> /* free(), calloc() */
#include <math.h>   /* sqrt() */
#include <string.h> /* memset() */
#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "brush_smudge_circle.h"

/* メモリ開放 */
void brush_smudge_circle::mem_free(void) {
  if (NULL != this->_dp_brush) {
    if (this->_i_mv_sw) {
      pri_funct_msg_ttvr("brush_smudge_circle::mem_free()");
    }
    free(this->_dp_brush);
    this->_dp_brush          = NULL;
    this->_dp_pixel_image    = NULL;
    this->_dp_subpixel_image = NULL;
  }
}

/* データ設定とメモリ確保 */
int brush_smudge_circle::mem_alloc(void) {
  int32_t i32_sz;

  /* もし以前のメモリがあるなら開放する */
  this->mem_free();

  /* メモリ確保のためのサイズの1単位 */
  i32_sz = this->_i32_size_by_pixel * this->_i32_subpixel_divide;

  /* 処理ごとのメッセージ */
  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("brush_smudge_circle::mem_alloc()");
  }

  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr("calloc((%d x %d + %d x %d + %d x %d) x (%d x %d))",
                       i32_sz, i32_sz, i32_sz, i32_sz,
                       (this->_i32_size_by_pixel + 1),
                       (this->_i32_size_by_pixel + 1), sizeof(double), 5);
  }

  this->_dp_brush = (double *)calloc(
      i32_sz * i32_sz + i32_sz * i32_sz +
          (this->_i32_size_by_pixel + 1) * (this->_i32_size_by_pixel + 1),
      sizeof(double) * 5);
  if (NULL == this->_dp_brush) {
    pri_funct_err_bttvr("Error : calloc(-) returns NULL.");
    return NG;
  }

  this->_dp_subpixel_image = this->_dp_brush + i32_sz * i32_sz * 5;
  this->_dp_pixel_image    = this->_dp_subpixel_image + i32_sz * i32_sz * 5;

  return OK;
}

/********************************************************************/

/* 画像上に置いたブラシの範囲
        +-------+-------+ --> *dp_y2
        |       |       |
        |   +   |   +   |
        |       |       |
        +-------+-------+ <-- d_yp
        |       |       |
        |   +   |   +   |
        |       |       |
        +-------+-------+ --> *dp_y2
        |       ^       |
        v       |       v
        *dp_x1  d_xp    *dp_x2
            0       1     <---- d_xp,d_yp is pixel position
                             |
                             | +0.5
                             v
        0       1       2 <---- *dp_x1,*dp_y1,*dp_x2,*dp_y2 is image position
*/
void brush_smudge_circle::get_dp_area(double d_xp, double d_yp, double *dp_x1,
                                      double *dp_y1, double *dp_x2,
                                      double *dp_y2) {
  *dp_x1 = d_xp + 0.5 - this->_i32_size_by_pixel / 2.0;
  *dp_x2 = *dp_x1 + this->_i32_size_by_pixel;
  *dp_y1 = d_yp + 0.5 - this->_i32_size_by_pixel / 2.0;
  *dp_y2 = *dp_y1 + this->_i32_size_by_pixel;
}

/********************************************************************/

/* ブラシの形を、有効フラグの設定で決める
   例
        pixel_size      3 x 3
        subpixel_divide 2 x 2
        +-------+-------+-------+   ^
        | +   + | +   + | +   + | 5 |
        |       |       |       |   |
        | +   + | +   + | +   + | 4 |
        +-------+-------+-------+   |
        | +   + | +   + | +   + | 3 |
center->|       |       |       |  i32_size(3*2)
        | +   + | +   + | +   + | 2 |
        +-------+-------+-------+   |
        | +   + | +   + | +   + | 1 |
        |       |       |       |   |
        | +   + | +   + | +   + | 0 |
        +-------+-------+-------+ ^ v
          0   1   2   3   4   5 <-+---- subpixel count
        0   1   2   3   4   5   6  <----- position
        <---- i32_size(3*2) ---->
                    ^
                    |
                   center
*/
void brush_smudge_circle::set_brush_circle(void) {
  double *dp_brush, d_radius, d_tmp;
  int32_t xx, yy, i32_size;

  dp_brush = this->_dp_brush;
  i32_size = this->_i32_size_by_pixel * this->_i32_subpixel_divide;

  d_radius = i32_size / 2.0;

  for (yy = 0; yy < i32_size; ++yy) {
    for (xx = 0; xx < i32_size; ++xx, dp_brush += 5) {
      d_tmp = sqrt((xx + 0.5 - d_radius) * (xx + 0.5 - d_radius) +
                   (yy + 0.5 - d_radius) * (yy + 0.5 - d_radius));
      if (d_tmp < d_radius) {
        dp_brush[4] = 1.0;
      } else {
        dp_brush[4] = 0.0;
      }
    }
  }
}

/* 切り取った画像をブラシにセット */
void brush_smudge_circle::copy_to_brush_from_image(void) {
  double *dp_brush, *dp_image;
  int32_t xx, yy, zz, i32_size;

  dp_brush = this->_dp_brush;
  dp_image = this->_dp_subpixel_image;
  i32_size = this->_i32_size_by_pixel * this->_i32_subpixel_divide;

  for (yy = 0; yy < i32_size; ++yy) {
    for (xx = 0; xx < i32_size; ++xx, dp_brush += 5, dp_image += 5) {
      if (0.0 < dp_brush[4]) {
        /* rgbaの4チャンネルをコピーする */
        for (zz = 0; zz < 4; ++zz) {
          dp_brush[zz] = dp_image[zz];
        }
      }
    }
  }
}

/* こする
        以前の場所での画像と指定の比率で差分を画像に足し、その画像をブラシに保存
 */
void brush_smudge_circle::exec(void) {
  double *dp_brush, *dp_image;
  int32_t xx, yy, zz, i32_size;

  dp_brush = this->_dp_brush;
  dp_image = this->_dp_subpixel_image;
  i32_size = this->_i32_size_by_pixel * this->_i32_subpixel_divide;

  for (yy = 0; yy < i32_size; ++yy) {
    for (xx = 0; xx < i32_size; ++xx, dp_brush += 5, dp_image += 5) {
      if (0.0 < dp_brush[4]) {
        /* rgbaの4チャンネルで実行 */
        for (zz = 0; zz < 4; ++zz) {
          dp_image[zz] += (dp_brush[zz] - dp_image[zz]) * this->_d_ratio;
          dp_brush[zz] = dp_image[zz];
        }
      }
    }
  }
}

/********************************************************************/

void brush_smudge_circle::to_subpixel_from_pixel(double d_x1, double d_y1,
                                                 double d_x2, double d_y2) {
  double d_subsize;
  int32_t i32_xsize;
  double *dp_image, *dp_save;
  double d_x, d_y, d_x1floor, d_y1floor, d_xsave, d_ysave;
  int32_t zz;

  d_subsize = 1.0 / this->_i32_subpixel_divide;

  /* 保存(復元)位置 */
  d_x1floor = floor(d_x1 + d_subsize / 2.0);
  d_y1floor = floor(d_y1 + d_subsize / 2.0);
  i32_xsize = (int32_t)floor(d_x2 - d_subsize / 2.0) - (int32_t)d_x1floor + 1;

  dp_image = this->_dp_subpixel_image;
  dp_save  = this->_dp_pixel_image;

  /* d_x,d_yでループ、d_xsave,d_ysaveで位置 */
  for (d_y = d_y1 + d_subsize / 2.0; d_y < d_y2; d_y += d_subsize) {
    for (d_x = d_x1 + d_subsize / 2.0; d_x < d_x2;
         d_x += d_subsize, dp_image += 5) {
      d_xsave = d_x - d_x1floor;
      d_ysave = d_y - d_y1floor;

      assert(0 <= (int32_t)d_xsave);
      assert(0 <= (int32_t)d_ysave);
      assert((int32_t)d_xsave < (this->_i32_size_by_pixel + 1));
      assert((int32_t)d_ysave < (this->_i32_size_by_pixel + 1));

      for (zz = 0; zz < 5; ++zz) {
        dp_image[zz] = dp_save[(int32_t)d_ysave * i32_xsize * 5 +
                               (int32_t)d_xsave * 5 + zz];
      }
    }
  }
}

/* 切取ったサブピクセル画像を、(元画像へ)保存のためピクセルサイズに縮小
        例えば、元画像の2 x 2ピクセルの任意の位置から切り取ってあるなら、
        復元に3 x 3ピクセルサイズ必要となる。
        +-------+-------+-------+
        |     +-|-----+-|-----+ |
        |   + | |   + | |   + | |
        |     | |     | |     | |
        +-------+-------+-------+
        |     +-|-----+ |-----+ |
        |   + | |   + | |   + | |
        |     | |     | |     | |
        +-------+-------+-------+
        |     +-|-----+-|-----+ |
        |   +   |   +   |   +   |
        |       |       |       |
        +-------+-------+-------+
*/
void brush_smudge_circle::to_pixel_from_subpixel(double d_x1, double d_y1,
                                                 double d_x2, double d_y2) {
  double d_subsize;
  int32_t i32_xsize;
  double *dp_image, *dp_save;
  double d_x, d_y, d_x1floor, d_y1floor, d_xsave, d_ysave;
  int32_t zz;
  int32_t ii, jj;

  d_subsize = 1.0 / this->_i32_subpixel_divide;

  /* 初期化 */
  dp_save = this->_dp_pixel_image;
  for (ii = 0; ii < this->_i32_size_by_pixel + 1; ++ii) {
    for (jj = 0; jj < this->_i32_size_by_pixel + 1; ++jj, dp_save += 5) {
      for (zz = 0; zz < 5; ++zz) {
        dp_save[zz] = 0.0;
      }
    }
  }

  /* 保存(復元)位置 */
  d_x1floor = floor(d_x1 + d_subsize / 2.0);
  d_y1floor = floor(d_y1 + d_subsize / 2.0);
  i32_xsize = (int32_t)floor(d_x2 - d_subsize / 2.0) - (int32_t)d_x1floor + 1;

  dp_image = this->_dp_subpixel_image;
  dp_save  = this->_dp_pixel_image;

  /* d_x,d_yでループ、d_xsave,d_ysaveで位置 */
  for (d_y = d_y1 + d_subsize / 2.0; d_y < d_y2; d_y += d_subsize) {
    for (d_x = d_x1 + d_subsize / 2.0; d_x < d_x2;
         d_x += d_subsize, dp_image += 5) {
      d_xsave = d_x - d_x1floor;
      d_ysave = d_y - d_y1floor;

      assert(0 <= (int32_t)d_xsave);
      assert(0 <= (int32_t)d_ysave);
      assert((int32_t)d_xsave < (this->_i32_size_by_pixel + 1));
      assert((int32_t)d_ysave < (this->_i32_size_by_pixel + 1));

      for (zz = 0; zz < 5; ++zz) {
        dp_save[(int32_t)d_ysave * i32_xsize * 5 + (int32_t)d_xsave * 5 + zz] +=
            dp_image[zz];
      }
    }
  }

  /* ピクセル値に */
  dp_save = this->_dp_pixel_image;
  for (ii = 0; ii < this->_i32_size_by_pixel + 1; ++ii) {
    for (jj = 0; jj < this->_i32_size_by_pixel + 1; ++jj, dp_save += 5) {
      for (zz = 0; zz < 5; ++zz) {
        dp_save[zz] /= this->_i32_subpixel_divide * this->_i32_subpixel_divide;
      }
    }
  }
}

#ifndef __calculator_geometry_h__
#define __calculator_geometry_h__

class calculator_geometry {
public:
  double get_d_radian(double d_xv, double d_yv);
  double get_d_radian_by_2_vector(double d_xv1, double d_yv1, double d_xv2,
                                  double d_yv2);
  void get_dd_rotate(double d_xp1, double d_yp1, double d_radian,
                     double *dp_xp2, double *dp_yp2);
  void get_dd_mirror(double d_xp1, double d_yp1, double d_mirror_xc,
                     double d_mirror_yc, double d_mirror_radian, double *dp_xp2,
                     double *dp_yp2);
  void get_dd_rotate_by_pos(double d_xp1, double d_yp1, double d_xpos,
                            double d_ypos, double d_radian, double *dp_xp2,
                            double *dp_yp2);

private:
};

#endif            /* !__calculator_geometry_h__ */
#include <math.h> /* sin(),cos(),atan(), M_PI */

#include "igs_line_blur.h"  // "pri.h" "calculator_geometry.h"

/* WindowsではM_PIが見つからない */
#if defined _MSC_VER
#define M_PI 3.14159265358979323846
#endif

/* ベクトルの角度を"0"から"2*PI"で返す */
double calculator_geometry::get_d_radian(double d_xv, double d_yv) {
  double d_radian;

  /* ゼロエラー */
  if ((0.0 == d_xv) && (0.0 == d_yv)) {
    pri_funct_err_bttvr(
        "Warning : calculator_geometry::get_d_radian(d_xv,d_yv is zero).");
    return 0.0;
  }
  /* 第1象限 (0 <= angle < 90)*/
  else if ((0.0 < d_xv) && (0.0 <= d_yv)) {
    d_radian = atan(d_yv / d_xv);
  }
  /* 第2象限 (第1象限に置き換えて... 0 <= angle < 90) */
  else if ((d_xv <= 0.0) && (0.0 < d_yv)) {
    d_radian = atan(-d_xv / d_yv) + M_PI_2;
  }
  /* 第3象限 (第1象限に置き換えて... 0 <= angle < 90) */
  else if ((d_xv < 0.0) && (d_yv <= 0.0)) {
    d_radian = atan(d_yv / d_xv) + M_PI;
  }
  /* 第4象限 (第1象限に置き換えて... 0 <= angle < 90) */
  else if ((0.0 <= d_xv) && (d_yv < 0.0)) {
    d_radian = atan(d_xv / -d_yv) + M_PI + M_PI_2;
  }
  return d_radian;
}

/* 2つのベクトルのなす角度を反時計回りに調べる */
double calculator_geometry::get_d_radian_by_2_vector(double d_xv1, double d_yv1,
                                                     double d_xv2,
                                                     double d_yv2) {
  double d_radian_start, d_radian_end;

  if ((0.0 == d_xv1) && (0.0 == d_yv1)) {
    pri_funct_err_bttvr(
        "Warning : calculator_geometry::get_d_radian_by_2_vector(d_xv1,d_yv1 "
        "is zero).");
  }
  if ((0.0 == d_xv2) && (0.0 == d_yv2)) {
    pri_funct_err_bttvr(
        "Warning : calculator_geometry::get_d_radian_by_2_vector(d_xv2,d_yv2 "
        "is zero).");
  }

  d_radian_start = this->get_d_radian(d_xv1, d_yv1);
  d_radian_end   = this->get_d_radian(d_xv2, d_yv2);
  if (d_radian_end < d_radian_start) {
    d_radian_end += M_PI * 2.0;
  }
  return d_radian_end - d_radian_start;
}

/* x,y座標の回転 */
void calculator_geometry::get_dd_rotate(double d_xp1, double d_yp1,
                                        double d_radian, double *dp_xp2,
                                        double *dp_yp2) {
  *dp_xp2 = d_xp1 * cos(d_radian) - d_yp1 * sin(d_radian);
  *dp_yp2 = d_xp1 * sin(d_radian) + d_yp1 * cos(d_radian);
}

/* x,y座標を線(鏡)対象に移動する */
void calculator_geometry::get_dd_mirror(double d_xp1, double d_yp1,
                                        double d_mirror_xpos,
                                        double d_mirror_ypos,
                                        double d_mirror_radian, double *dp_xp2,
                                        double *dp_yp2) {
  d_xp1 -= d_mirror_xpos;
  d_yp1 -= d_mirror_ypos;
  this->get_dd_rotate(d_xp1, d_yp1, -d_mirror_radian, &d_xp1, &d_yp1);
  this->get_dd_rotate(d_xp1, -d_yp1, d_mirror_radian, &d_xp1, &d_yp1);
  *dp_xp2 = d_xp1 + d_mirror_xpos;
  *dp_yp2 = d_yp1 + d_mirror_ypos;
}

/* 指定座標を中心に回転する */
void calculator_geometry::get_dd_rotate_by_pos(double d_xp1, double d_yp1,
                                               double d_xpos, double d_ypos,
                                               double d_radian, double *dp_xp2,
                                               double *dp_yp2) {
  d_xp1 -= d_xpos;
  d_yp1 -= d_ypos;
  this->get_dd_rotate(d_xp1, d_yp1, d_radian, &d_xp1, &d_yp1);
  *dp_xp2 = d_xp1 + d_xpos;
  *dp_yp2 = d_yp1 + d_ypos;
}

#ifndef _pixel_point_node_h_
#define _pixel_point_node_h_

#include <stdio.h>

/* Windowsではstdint.hが見つからない */
#if defined _MSC_VER
typedef int int32_t;
typedef unsigned short uint16_t;
#else
#include <stdint.h> /* for int32_t, uint16_t */
#endif

#include "igs_line_blur.h"  // "list_node.h"

#ifndef OK
#define OK (0)
#endif
#ifndef NG
#define NG (-1)
#endif

#ifndef LINK_NEAR_COUNT
#define LINK_NEAR_COUNT 4
#endif

/* x,yポイント座標のリストノード、画素連結、及び、線分連結、機能付き */
class pixel_point_node final : public list_node {
public:
  pixel_point_node() {
    int32_t ii;

    this->_i32_xp   = 0;
    this->_i32_yp   = 0;
    this->_d_xp_tgt = 0.0;
    this->_d_yp_tgt = 0.0;

    for (ii = 0; ii < LINK_NEAR_COUNT; ++ii) {
      this->_clpa_link_near[ii] = NULL;
    }
    this->_clp_previous_point = NULL;
    this->_clp_next_point     = NULL;
  }

  /* 位置のセットと、ゲット */
  void set_i32_xp(int32_t i32) { this->_i32_xp = i32; }
  void set_i32_yp(int32_t i32) { this->_i32_yp = i32; }
  void set_d_xp_tgt(double dd) { this->_d_xp_tgt = dd; }
  void set_d_yp_tgt(double dd) { this->_d_yp_tgt = dd; }

  int32_t get_i32_xp(void) { return this->_i32_xp; }
  int32_t get_i32_yp(void) { return this->_i32_yp; }
  double get_d_xp_tgt(void) { return this->_d_xp_tgt; }
  double get_d_yp_tgt(void) { return this->_d_yp_tgt; }

  /* 第１のリンク : list_nodeの継承による全データ検索用リスト */
  /* void set_clp_next( pixel_point_node *clp ); */
  /* void set_clp_previous( pixel_point_node *clp ); */
  /* pixel_point_node *get_clp_next( void ); */
  /* pixel_point_node *get_clp_previous( void ); */

  /* 第２のリンク : 画像上の(上下左右斜め)ピクセル連結 */
  int link_near(pixel_point_node *clp_);
  pixel_point_node *get_clp_link_near(int32_t i32);

  /* 第３のリンク : 線分を表すポイントリスト */
  void set_clp_next_point(pixel_point_node *clp) {
    this->_clp_next_point = clp;
  }
  void set_clp_previous_point(pixel_point_node *clp) {
    this->_clp_previous_point = clp;
  }
  pixel_point_node *get_clp_next_point(void) { return this->_clp_next_point; }
  pixel_point_node *get_clp_previous_point(void) {
    return this->_clp_previous_point;
  }

  /* 連結情報表示 */
  void print_xy_around(void);

private:
  int32_t _i32_xp, _i32_yp;    /* source(元)画像上の位置 */
  double _d_xp_tgt, _d_yp_tgt; /* target(最終)画像上の位置 */

  pixel_point_node *_clpa_link_near[LINK_NEAR_COUNT];
  pixel_point_node *_clp_previous_point, *_clp_next_point;
};

#endif              /* !_pixel_point_node_h_ */
#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "pixel_point_node.h"

int pixel_point_node::link_near(pixel_point_node *clp_) {
  int32_t ii;

  for (ii = 0; ii < LINK_NEAR_COUNT; ++ii) {
    if (NULL == this->_clpa_link_near[ii]) {
      this->_clpa_link_near[ii] = clp_;
      return ii;
    }
  }

  pri_funct_err_bttvr("Error : no link_near point, over %d.",
                      LINK_NEAR_COUNT - 1);
  pri_funct_err_bttvr("this   x %d y %d", this->get_i32_xp(),
                      this->get_i32_yp());
  for (ii = 0; ii < LINK_NEAR_COUNT; ++ii) {
    assert(NULL != this->_clpa_link_near[ii]);
    pri_funct_err_bttvr("link_near %d x %d y %d", ii,
                        this->_clpa_link_near[ii]->get_i32_xp(),
                        this->_clpa_link_near[ii]->get_i32_yp());
  }

  return NG;
}

pixel_point_node *pixel_point_node::get_clp_link_near(int32_t i32) {
  if (LINK_NEAR_COUNT <= i32) return NULL;
  return this->_clpa_link_near[i32];
}

void pixel_point_node::print_xy_around(void) {
  int32_t ii;

  pri_funct_msg_ttvr(
      "pixel_point_node::debug_print_xy_around() : self address <0x%lx>", this);
  pri_funct_msg_ttvr(" self    x %d y %d", this->get_i32_xp(),
                     this->get_i32_yp());

  for (ii = 0; ii < LINK_NEAR_COUNT; ++ii) {
    if (NULL != this->get_clp_link_near(ii)) {
      pri_funct_msg_ttvr(" link_near %d  x %d y %d", ii,
                         this->get_clp_link_near(ii)->get_i32_xp(),
                         this->get_clp_link_near(ii)->get_i32_yp());
    } else {
      pri_funct_msg_ttvr(" link_near %ld is not exist(NULL).", ii);
    }
  }
}

#ifndef _pixel_point_root_h_
#define _pixel_point_root_h_

#include "igs_line_blur.h"  // "list_root.h" "pixel_point_node.h"

class pixel_point_root final : public list_root {
public:
  pixel_point_root(void) {
    this->_i_mv_sw = false;
    this->_i_cv_sw = false;
    this->_i_pv_sw = false;
  }
  ~pixel_point_root(void) { this->mem_free(); }

  void set_i_mv_sw(bool sw) { this->_i_mv_sw = sw; }
  void set_i_cv_sw(bool sw) { this->_i_cv_sw = sw; }
  void set_i_pv_sw(bool sw) { this->_i_pv_sw = sw; }

  /* 16Bits 1channel画像からゼロ以上のピクセルをプロットする */
  int alloc_mem_and_list_node(int32_t i32_xs, int32_t i32_ys,
                              uint16_t *ui16p_src);

  /* リストの追加 */
  pixel_point_node *append(pixel_point_node *clp_previous);

  /* for debug */
  int save(const char *cp_fname);

  void mem_free(void);

private:
  bool _i_mv_sw, _i_cv_sw, _i_pv_sw;

  /* リストの削除 */
  void _remove(pixel_point_node *clp_target);
};

#endif              /* !_pixel_point_root_h_ */
#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "pixel_point_root.h"

pixel_point_node *pixel_point_root::append(pixel_point_node *clp_previous) {
  pixel_point_node *clp_new;

  clp_new = new pixel_point_node;
  if (NULL == clp_new) {
    pri_funct_err_bttvr("Error : 'new pixel_point_node' returns NULL.");
    return NULL;
  }
  clp_new = (pixel_point_node *)this->push(clp_previous, clp_new);

  return clp_new;
}

void pixel_point_root::_remove(pixel_point_node *clp_old) {
  assert(NULL != clp_old); /* あってはならないプログラムバグのチェック */
  this->pop(clp_old);
  delete clp_old;
}

void pixel_point_root::mem_free(void) {
  pixel_point_node *clp_;
  int32_t ii;

  if (NULL != this->get_clp_last()) {
    if (this->_i_mv_sw) {
      pri_funct_msg_ttvr("pixel_point_root::mem_free()");
    }
    ii = 0;
    while (NULL != (clp_ = (pixel_point_node *)this->get_clp_last())) {
      this->_remove(clp_);
      ++ii;
    }
    if (this->_i_pv_sw) {
      pri_funct_msg_ttvr("free point node %d", ii);
    }
  }
}

int pixel_point_root::alloc_mem_and_list_node(int32_t i32_xs, int32_t i32_ys,
                                              uint16_t *ui16p_src) {
  int32_t xx, yy;
  pixel_point_node *clp_pp;

  /* 処理実行表示 */
  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("pixel_point_root::alloc_mem_and_list_node()");
  }

  /* カウントダウン表示始め */
  if (this->_i_cv_sw) {
    pri_funct_cv_start(i32_ys);
  }

  clp_pp = NULL;
  for (yy = 0; yy < i32_ys; ++yy) {
    /* カウントダウン表示中 */
    if (this->_i_cv_sw) {
      pri_funct_cv_run(yy);
    }

    for (xx = 0; xx < i32_xs; ++xx, ++ui16p_src) {
      if (0 < (*ui16p_src)) {
        clp_pp = this->append(clp_pp);
        if (NULL == clp_pp) {
          pri_funct_err_bttvr("Error : this->append(clp_pp) returns NULL.");
          return NG;
        }
        clp_pp->set_i32_xp(xx);
        clp_pp->set_i32_yp(yy);
      }
    }
  }
  /* カウントダウン表示終了 */
  if (this->_i_cv_sw) {
    pri_funct_cv_end();
  }

  /* 処理結果表示 */
  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr("alloc and list %d points", this->get_i32_count());
  }

  return OK;
}

int pixel_point_root::save(const char *cp_fname) {
  FILE *fp;
  int32_t ii;
  pixel_point_node *clp_pp_node;

  /* あってはならないプログラムバグのチェック */
  assert(NULL != cp_fname);

  /* ファイル開く */
  fp = fopen(cp_fname, "w");
  if (NULL == fp) {
    pri_funct_err_bttvr("Error : fopen(%s) returns NULL.", cp_fname);
    return NG;
  }

  /* データ数保存 */
  if (fprintf(fp, "# count %d\n", this->get_i32_count()) < 0) {
    pri_funct_err_bttvr("Error : fprintf(count) returns minus.");
    return NG;
  }

  /* データ保存 */
  for (clp_pp_node = (pixel_point_node *)this->get_clp_first(), ii = 0L;
       (NULL != clp_pp_node) && (ii < this->get_i32_count());
       clp_pp_node = (pixel_point_node *)clp_pp_node->get_clp_next(), ++ii) {
    if (fprintf(fp, "%d %d\n", clp_pp_node->get_i32_xp(),
                clp_pp_node->get_i32_yp()) < 0) {
      pri_funct_err_bttvr("Error : fprintf(count xp yp) returns minus.");
      return NG;
    }
  }
  /* ファイル閉じる */
  fclose(fp);

  return OK;
}

#ifndef _pixel_line_node_h_
#define _pixel_line_node_h_

#include "igs_line_blur.h"  // "calculator_geometry.h" "list_node.h" "pixel_point_root.h"

#ifndef OK
#define OK (0)
#endif
#ifndef NG
#define NG (-1)
#endif

class pixel_line_node final : public list_node {
public:
  pixel_line_node() {
    this->_i32_point_count           = 0;
    this->_d_same_way_radian_one     = 0.0;
    this->_d_same_way_radian_another = 0.0;
    this->_d_bbox_x_min              = 0.0;
    this->_d_bbox_x_max              = 0.0;
    this->_d_bbox_y_min              = 0.0;
    this->_d_bbox_y_max              = 0.0;

    this->_clpa_link[0] = NULL;
    this->_clpa_link[1] = NULL;
    this->_clpa_link[2] = NULL;
    this->_clpa_link[3] = NULL;
    this->_clpa_link[4] = NULL;
  }

  /* 値の参照 */
  int32_t get_i32_point_count(void) { return this->_i32_point_count; }
  void set_d_same_way_radian_one(double dd) {
    this->_d_same_way_radian_one = dd;
  }
  void set_d_same_way_radian_another(double dd) {
    this->_d_same_way_radian_another = dd;
  }
  double get_d_same_way_radian_one(void) {
    return this->_d_same_way_radian_one;
  }
  double get_d_same_way_radian_another(void) {
    return this->_d_same_way_radian_another;
  }
  double get_d_bbox_x_min(void) { return this->_d_bbox_x_min; }
  double get_d_bbox_x_max(void) { return this->_d_bbox_x_max; }
  double get_d_bbox_y_min(void) { return this->_d_bbox_y_min; }
  double get_d_bbox_y_max(void) { return this->_d_bbox_y_max; }

  /* 対象(pixel_point_node)へのリンク機能 */
  void link_one(pixel_point_node *clp) { this->_clpa_link[0] = clp; }
  void link_another(pixel_point_node *clp) { this->_clpa_link[1] = clp; }

  void link_middle(pixel_point_node *clp) { this->_clpa_link[2] = clp; }
  void link_one_expand(pixel_point_node *clp) { this->_clpa_link[3] = clp; }
  void link_another_expand(pixel_point_node *clp) { this->_clpa_link[4] = clp; }

  pixel_point_node *get_clp_link_one(void) { return this->_clpa_link[0]; }
  pixel_point_node *get_clp_link_another(void) { return this->_clpa_link[1]; }

  pixel_point_node *get_clp_link_middle(void) { return this->_clpa_link[2]; }
  pixel_point_node *get_clp_link_one_expand(void) {
    return this->_clpa_link[3];
  }
  pixel_point_node *get_clp_link_another_expand(void) {
    return this->_clpa_link[4];
  }

  void set_middle(void);
  void int2double_body(void);
  void smooth_body(int32_t i32_smooth_retry);
  void smooth_expand(int32_t i32_smooth_retry);
  void link_line(pixel_point_node *clp_crnt, pixel_point_node *clp_next,
                 int32_t i32_count);
  void get_near_point(double d_xp, double d_yp, int32_t *i32p_pos,
                      pixel_point_node **clpp_point, double *dp_length);

  pixel_point_node *get_next_point_by_count(pixel_point_node *clp_point,
                                            int32_t i32_count);
  pixel_point_node *get_prev_point_by_count(pixel_point_node *clp_point,
                                            int32_t i32_count);
  void set_bbox(void);

  int save_line(FILE *fp);
  int save_one_point(FILE *fp);
  int save_middle_point(FILE *fp);
  int save_another_point(FILE *fp);

  int expand_line(pixel_point_root *clp_pixel_point_root);

  int save_expand_line(FILE *fp);
  int save_expand_vector(FILE *fp);
  int save_one_expand_point(FILE *fp);
  int save_another_expand_point(FILE *fp);

private:
  int32_t _i32_point_count;
  double _d_same_way_radian_one, _d_same_way_radian_another;
  double _d_bbox_x_min, _d_bbox_x_max, _d_bbox_y_min, _d_bbox_y_max;

  pixel_point_node *_clpa_link[5];
  calculator_geometry _cl_cal_geom;

  int _expand_line_from_one(pixel_point_root *clp_pp_root,
                            int32_t i32_body_point_count,
                            pixel_point_node *clp_one,
                            pixel_point_node *clp_another, double d_radian);
  int _expand_line_from_another(pixel_point_root *clp_pp_root,
                                int32_t i32_body_point_count,
                                pixel_point_node *clp_one,
                                pixel_point_node *clp_another, double d_radian);
  void _smooth_point(double d_x1, double d_y1, double d_x2, double d_y2,
                     double d_x3, double d_y3, double *dp_x, double *dp_y);

  void _get_link_line_selecter_vector(pixel_point_node *clp_crnt,
                                      pixel_point_node *clp_next, double *dp_xv,
                                      double *dp_yv, int32_t i32_count);
  pixel_point_node *_get_link_line_selecter(double d_xv, double d_yv,
                                            pixel_point_node *clp_crnt,
                                            int32_t i32_count);
};

#endif /* !_pixel_line_node_h_ */
#include <assert.h>

#include "igs_line_blur.h"  // "pri.h" "pixel_line_node.h"

int pixel_line_node::_expand_line_from_one(pixel_point_root *clp_pp_root,
                                           int32_t i32_body_point_count,
                                           pixel_point_node *clp_one,
                                           pixel_point_node *clp_another,
                                           double d_radian) {
  pixel_point_node *clp_last, *clp_before, *clp_exist;
  double d_xp, d_yp;
  int32_t ii;

  clp_before = clp_one;

  /* ライン開始点(clp_one)から後ろへたどる、
     始まりは開始点でなくその一つ後ろから */
  for (ii = 1, clp_exist = clp_one->get_clp_next_point(); NULL != clp_exist;
       ++ii, clp_exist   = clp_exist->get_clp_next_point()) {
    /* 偽の場合、たぶん無限ループ */
    assert(ii < i32_body_point_count);

    /* 点node生成とデータリンク(free用) */
    clp_last =
        clp_pp_root->append((pixel_point_node *)(clp_pp_root->get_clp_last()));
    if (NULL == clp_last) {
      pri_funct_err_bttvr(
          "Error : ii %d : this->append(clp_last) returns NULL.", ii);
      return NG;
    }

    /* ラインとしてのリンク */
    clp_before->set_clp_previous_point(clp_last);
    clp_last->set_clp_next_point(clp_before);
    this->link_one_expand(clp_last);
    ++this->_i32_point_count;

    /* 位置を鏡面反転 */
    this->_cl_cal_geom.get_dd_mirror(
        clp_exist->get_d_xp_tgt(), clp_exist->get_d_yp_tgt(),
        clp_one->get_d_xp_tgt(), clp_one->get_d_yp_tgt(), d_radian, &d_xp,
        &d_yp);

    /* 位置をセット */
    clp_last->set_d_xp_tgt(d_xp);
    clp_last->set_d_yp_tgt(d_yp);

    /* 接続のため前の点を記憶 */
    clp_before = clp_last;

    /* 終了 */
    if (clp_another == clp_exist) {
      break;
    }
  }
  return OK;
}
int pixel_line_node::_expand_line_from_another(pixel_point_root *clp_pp_root,
                                               int32_t i32_body_point_count,
                                               pixel_point_node *clp_one,
                                               pixel_point_node *clp_another,
                                               double d_radian) {
  pixel_point_node *clp_last, *clp_before, *clp_exist;
  double d_xp, d_yp;
  int32_t ii;

  clp_before = clp_another;

  /* ライン最終点(clp_another)から前へたどる、
     始まりは最終点でなくその一つ前から */
  for (ii = 1, clp_exist = clp_another->get_clp_previous_point();
       NULL != clp_exist;
       ++ii, clp_exist = clp_exist->get_clp_previous_point()) {
    /* 偽の場合、たぶん無限ループ */
    assert(ii < i32_body_point_count);

    /* 点node生成とデータリンク(free用) */
    clp_last =
        clp_pp_root->append((pixel_point_node *)(clp_pp_root->get_clp_last()));
    if (NULL == clp_last) {
      pri_funct_err_bttvr(
          "Error : ii %d : this->append(clp_last) returns NULL.", ii);
      return NG;
    }

    /* ラインとしてのリンク */
    clp_before->set_clp_next_point(clp_last);
    clp_last->set_clp_previous_point(clp_before);
    this->link_another_expand(clp_last);
    ++this->_i32_point_count;

    /* 位置を鏡面反転 */
    this->_cl_cal_geom.get_dd_mirror(
        clp_exist->get_d_xp_tgt(), clp_exist->get_d_yp_tgt(),
        clp_another->get_d_xp_tgt(), clp_another->get_d_yp_tgt(), d_radian,
        &d_xp, &d_yp);

    /* 位置をセット */
    clp_last->set_d_xp_tgt(d_xp);
    clp_last->set_d_yp_tgt(d_yp);

    /* 接続のため前の点を記憶 */
    clp_before = clp_last;

    /* 終了 */
    if (clp_one == clp_exist) {
      break;
    }
  }
  return OK;
}

/********************************************************************/

int pixel_line_node::expand_line(pixel_point_root *clp_pixel_point_root) {
  pixel_point_node *clp_one, *clp_middle, *clp_another;
  double d_radian, d_radian_one, d_radian_another;
  int32_t i32_body_point_count;

  /* 3点より少ない、短すぎる線分は伸ばすことができない */
  if (this->_i32_point_count < 3) {
    return OK;
  }

  /* 両端点と中点を得る */
  clp_one     = this->get_clp_link_one();
  clp_another = this->get_clp_link_another();
  clp_middle  = this->get_clp_link_middle();

  assert(clp_one != clp_another);
  assert(clp_another != clp_middle);
  assert(clp_middle != clp_one);

  /* 2つのベクトルの角度を反時計回りに求める */
  assert((clp_one->get_i32_xp() != clp_middle->get_i32_xp()) ||
         (clp_one->get_i32_yp() != clp_middle->get_i32_yp()));
  assert((clp_another->get_i32_xp() != clp_middle->get_i32_xp()) ||
         (clp_another->get_i32_yp() != clp_middle->get_i32_yp()));
  d_radian = this->_cl_cal_geom.get_d_radian_by_2_vector(
      clp_one->get_i32_xp() - clp_middle->get_i32_xp(),
      clp_one->get_i32_yp() - clp_middle->get_i32_yp(),
      clp_another->get_i32_xp() - clp_middle->get_i32_xp(),
      clp_another->get_i32_yp() - clp_middle->get_i32_yp());

  /* 半分にして */
  d_radian /= 2.0;

  /* 始点から中間点へのベクトルの角度 */
  d_radian_one = this->_cl_cal_geom.get_d_radian(
      clp_middle->get_i32_xp() - clp_one->get_i32_xp(),
      clp_middle->get_i32_yp() - clp_one->get_i32_yp());

  /* 始点における、線分の線対象な線の角度 */
  d_radian_one -= d_radian;

  /* 終点から中間点へのベクトルの角度 */
  d_radian_another = this->_cl_cal_geom.get_d_radian(
      clp_middle->get_i32_xp() - clp_another->get_i32_xp(),
      clp_middle->get_i32_yp() - clp_another->get_i32_yp());

  /* 終点における、線分の線対象な線の角度 */
  d_radian_another += d_radian;

  /* 始点が端点ならば先へ伸ばす */
  i32_body_point_count = this->_i32_point_count;
  if (NULL == clp_one->get_clp_link_near(1)) { /* 2点目がないなら端点 */
    if (OK !=
        this->_expand_line_from_one(clp_pixel_point_root, i32_body_point_count,
                                    this->get_clp_link_one(),
                                    this->get_clp_link_another(),
                                    d_radian_one)) {
      pri_funct_err_bttvr(
          "Error : this->_expand_line_from_one(-) returns NULL.");
      return NG;
    }
  }

  /* 終点が端点ならば先へ伸ばす */
  if (NULL == clp_another->get_clp_link_near(1)) { /* 2点目がないなら端点 */
    if (OK !=
        this->_expand_line_from_another(
            clp_pixel_point_root, i32_body_point_count,
            this->get_clp_link_one(), this->get_clp_link_another(),
            d_radian_another)) {
      pri_funct_err_bttvr(
          "Error : this->_expand_line_from_another(-) returns NULL.");
      return NG;
    }
  }

  return OK;
}

#include <math.h>   /* sqrt() */
#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pixel_line_node.h"

void pixel_line_node::get_near_point(double d_xp, double d_yp,
                                     int32_t *i32p_pos,
                                     pixel_point_node **clpp_point,
                                     double *dp_length) {
  pixel_point_node *clp_loop;
  int32_t ii;
  double d_length;

  assert(NULL != this->get_clp_link_one_expand());

  /* 各ポイントを探索 */
  *dp_length = 1000.0;
  clp_loop   = this->get_clp_link_one_expand();
  for (ii = 0; NULL != clp_loop;
       ++ii, clp_loop = clp_loop->get_clp_next_point()) {
    /* 偽の場合、たぶん無限ループ */
    assert(ii < this->_i32_point_count);

    /* 距離 */
    d_length = sqrt(
        (clp_loop->get_d_xp_tgt() - d_xp) * (clp_loop->get_d_xp_tgt() - d_xp) +
        (clp_loop->get_d_yp_tgt() - d_yp) * (clp_loop->get_d_yp_tgt() - d_yp));
    /* 近いものならセットする */
    if (d_length < (*dp_length)) {
      *i32p_pos   = ii;
      *clpp_point = clp_loop;
      *dp_length  = d_length;
    }
  }
}

#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pixel_line_node.h"

void pixel_line_node::int2double_body(void) {
  pixel_point_node *clp_1;
  int32_t ii;

  /* 浮動小数化 */
  clp_1 = this->get_clp_link_one();
  for (ii = 0; NULL != clp_1; clp_1 = clp_1->get_clp_next_point(), ++ii) {
    /* 偽の場合、たぶん無限ループ */
    assert(ii < this->_i32_point_count);

    /* x,y座標をint(整数値)からdouble(浮動小数点)に変換し格納 */
    clp_1->set_d_xp_tgt((double)clp_1->get_i32_xp());
    clp_1->set_d_yp_tgt((double)clp_1->get_i32_yp());
  }
}

#include <math.h>   /* M_PI */
#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "pixel_line_node.h"

/* WindowsではM_PIが見つからない */
#if defined _MSC_VER
#define M_PI 3.14159265358979323846
#endif

void pixel_line_node::_get_link_line_selecter_vector(pixel_point_node *clp_crnt,
                                                     pixel_point_node *clp_next,
                                                     double *dp_xv,
                                                     double *dp_yv,
                                                     int32_t i32_count) {
  pixel_point_node *clp_start;
  int32_t ii;

  clp_start = clp_crnt;

  /* 次への端点を求める */
  for (ii = 0; (NULL != clp_next) && (ii < i32_count); ++ii) {
    /* 次の点が線分としてリンク済みのとき */
    if ((NULL != clp_next->get_clp_next_point()) ||
        (NULL != clp_next->get_clp_previous_point())) {
      break;
    }

    /* もう一方の端点に到着し終端(リンク2点目がないので) */
    if ((NULL == clp_next->get_clp_link_near(1))) {
      clp_crnt = clp_next;
      break;
    }

    /* 分岐点(リンク3点目があるので) */
    if (NULL != clp_next->get_clp_link_near(2)) {
      clp_crnt = clp_next;
      break;
    }
    /* 次へのリンクをたどる */
    if (clp_crnt == clp_next->get_clp_link_near(0)) {
      clp_crnt = clp_next;
      clp_next = clp_next->get_clp_link_near(1);
    } else if (clp_crnt == clp_next->get_clp_link_near(1)) {
      clp_crnt = clp_next;
      clp_next = clp_next->get_clp_link_near(0);
    }
    /* リンクが壊れている */
    else {
      pri_funct_err_bttvr("Error : bad link");
      assert(0);
    }
  }
  *dp_xv = clp_crnt->get_i32_xp() - clp_start->get_i32_xp();
  *dp_yv = clp_crnt->get_i32_yp() - clp_start->get_i32_yp();
}

pixel_point_node *pixel_line_node::_get_link_line_selecter(
    double d_xv, double d_yv, pixel_point_node *clp_crnt, int32_t i32_count) {
  int32_t ii, i32_pos;
  double da_xv[LINK_NEAR_COUNT], da_yv[LINK_NEAR_COUNT],
      da_radian[LINK_NEAR_COUNT], d_radian;

  /* あってはならないプログラムバグのチェック */
  assert((0.0 != d_xv) || (0.0 != d_yv));
  assert(NULL != clp_crnt);

  /* 各分岐の方向ベクトルを得る */
  for (ii = 0; ii < LINK_NEAR_COUNT; ++ii) {
    if (NULL == clp_crnt->get_clp_link_near(ii)) {
      break;
    }
    this->_get_link_line_selecter_vector(clp_crnt,
                                         clp_crnt->get_clp_link_near(ii),
                                         &da_xv[ii], &da_yv[ii], i32_count);
  }

  /* 各分岐線の方向ベクトルから、手前の線との角度を求める */
  for (ii = 0; ii < LINK_NEAR_COUNT; ++ii) {
    if (NULL == clp_crnt->get_clp_link_near(ii)) {
      break;
    }
    if ((0.0 != da_xv[ii]) || (0.0 != da_yv[ii])) {
      da_radian[ii] = this->_cl_cal_geom.get_d_radian_by_2_vector(
          d_xv, d_yv, da_xv[ii], da_yv[ii]);
      if (M_PI < da_radian[ii]) {
        da_radian[ii] = M_PI - (da_radian[ii] - M_PI);
      }
    } else {
      da_radian[ii] = M_PI;
    }
  }

  /* もっとも直線と見なされる(角度の小さい)分岐を返す */
  d_radian = M_PI * 2.0;
  for (ii = 0; ii < LINK_NEAR_COUNT; ++ii) {
    if (NULL == clp_crnt->get_clp_link_near(ii)) {
      break;
    }
    if (da_radian[ii] < d_radian) {
      i32_pos  = ii;
      d_radian = da_radian[ii];
    }
  }
  /* もっとも直線と見なされる(角度の小さい)といっても
  90度以上も曲がってたらだめ */
  if ((M_PI / 2.0 <= d_radian) && (d_radian <= M_PI * 3.0 / 2.0)) return NULL;

  return clp_crnt->get_clp_link_near(i32_pos);
}

void pixel_line_node::link_line(pixel_point_node *clp_crnt,
                                pixel_point_node *clp_next, int32_t i32_count) {
  int32_t ii;
  pixel_point_node *clp_tmp1, *clp_tmp2;
  double d_xv, d_yv;

  /* あってはならないプログラムバグのチェック */
  assert(NULL != clp_crnt);
  assert(NULL != clp_next);

  /* スタート点をセット */
  this->link_one(clp_crnt);

  ++(this->_i32_point_count);

  clp_tmp1 = clp_tmp2 = this->get_clp_link_one();
  d_xv = d_yv = 0.0;

  for (ii = 0; (NULL != clp_next) && (ii < i32_count); ++ii) {
    /* 次の点が線分としてリンク済みのとき */
    if ((NULL != clp_next->get_clp_next_point()) ||
        (NULL != clp_next->get_clp_previous_point())) {
      /* カレント点をゴール点としてセット */
      this->link_another(clp_crnt);
      return;
    }

    /* 線分としてのリンクをする */
    clp_crnt->set_clp_next_point(clp_next);
    clp_next->set_clp_previous_point(clp_crnt);
    ++(this->_i32_point_count);

    /* もう一方の端点に到着し終端(リンク2点目がないので) */
    if ((NULL == clp_next->get_clp_link_near(1))) {
      /* 次の点をゴール点としてセット */
      this->link_another(clp_next);
      return;
    }

    /* 分岐点(リンク3点目があるので)から次の点へ */
    if (NULL != clp_next->get_clp_link_near(2)) {
      /* それまでの線分の方向を保存しておく */
      clp_tmp2 = clp_tmp1;
      clp_tmp1 = clp_next;
      assert(clp_tmp1 != clp_tmp2);

      d_xv = clp_tmp1->get_i32_xp() - clp_tmp2->get_i32_xp();
      d_yv = clp_tmp1->get_i32_yp() - clp_tmp2->get_i32_yp();
      assert((0.0 != d_xv) || (0.0 != d_yv));

      /* 各分岐線をたどり、方向の最も合う線を探す */
      clp_crnt = clp_next;
      clp_next = this->_get_link_line_selecter(d_xv, d_yv, clp_next, i32_count);

      /* たどる線がすでにたどっているものの場合、
      そこで、終端 */
      if (NULL == clp_next) {
        /* ゴール点をセット */
        this->link_another(clp_crnt);
        return;
      }
    }
    /* 線分構成点(リンク3点目がない)から次の点へ */
    else {
      /* 次へのリンクをたどる */
      if (clp_crnt == clp_next->get_clp_link_near(0)) {
        clp_crnt = clp_next;
        clp_next = clp_next->get_clp_link_near(1);
      } else if (clp_crnt == clp_next->get_clp_link_near(1)) {
        clp_crnt = clp_next;
        clp_next = clp_next->get_clp_link_near(0);
      }
      /* リンクが壊れている */
      else {
        pri_funct_err_bttvr("Error : bad link");
        assert(0);
      }
    }
  }
  /* リンクが不自然に長すぎる */
  pri_funct_err_bttvr("Error : too long link");
  assert(0);
  return;
}

#include "igs_line_blur.h"  // "pixel_line_node.h"

/*
pixel_select_curve_blur.cxx
の、
void pixel_select_curve_blur_root::exec( double d_xp, double d_yp,
pixel_line_node *clp_line_first, int32_t i32_count, int32_t i32_blur_count )
で使用。
*/

pixel_point_node *pixel_line_node::get_next_point_by_count(
    pixel_point_node *clp_point, int32_t i32_count) {
  for (; (0 < i32_count) && (NULL != clp_point); --i32_count) {
    clp_point = (pixel_point_node *)clp_point->get_clp_next_point();
  };
  return clp_point;
}

pixel_point_node *pixel_line_node::get_prev_point_by_count(
    pixel_point_node *clp_point, int32_t i32_count) {
  for (; (0 < i32_count) && (NULL != clp_point); --i32_count) {
    clp_point = (pixel_point_node *)clp_point->get_clp_previous_point();
  };
  return clp_point;
}

#include <assert.h>         /* assert() */
#include "igs_line_blur.h"  // "pri.h" "pixel_line_node.h"

int pixel_line_node::save_line(FILE *fp) {
  pixel_point_node *clp_crnt;
  int32_t ii;

  clp_crnt = this->get_clp_link_one();
  for (ii       = 0; NULL != clp_crnt;
       clp_crnt = clp_crnt->get_clp_next_point(), ++ii) {
    /* 偽の場合、たぶん無限ループ */
    assert(ii < this->_i32_point_count);

    /* 次の点(シード2含む)の座標値保存 */
    if (fprintf(fp, "%d %d\n", clp_crnt->get_i32_xp(), clp_crnt->get_i32_yp()) <
        0) {
      pri_funct_err_bttvr("Error : point %d : fprintf(%d %d) returns minus", ii,
                          clp_crnt->get_i32_xp(), clp_crnt->get_i32_yp());
      return NG;
    }
  }
  return OK;
}

/********************************************************************/

int pixel_line_node::save_one_point(FILE *fp) {
  pixel_point_node *clp_crnt;

  /* シード1の座標値保存 */
  clp_crnt = this->get_clp_link_one();

  if (NULL != clp_crnt) {
    if (fprintf(fp, "%d %d\n", clp_crnt->get_i32_xp(), clp_crnt->get_i32_yp()) <
        0) {
      pri_funct_err_bttvr("Error : one point : fprintf(%d %d) returns minus",
                          clp_crnt->get_i32_xp(), clp_crnt->get_i32_yp());
      return NG;
    }
  }
  return OK;
}

int pixel_line_node::save_middle_point(FILE *fp) {
  pixel_point_node *clp_crnt;

  /* シード1の座標値保存 */
  clp_crnt = this->get_clp_link_middle();

  if (NULL != clp_crnt) {
    if (fprintf(fp, "%d %d\n", clp_crnt->get_i32_xp(), clp_crnt->get_i32_yp()) <
        0) {
      pri_funct_err_bttvr("Error : middle point : fprintf(%d %d) returns minus",
                          clp_crnt->get_i32_xp(), clp_crnt->get_i32_yp());
      return NG;
    }
  }
  return OK;
}

int pixel_line_node::save_another_point(FILE *fp) {
  pixel_point_node *clp_crnt;

  /* シード2の座標値保存 */
  clp_crnt = this->get_clp_link_another();

  if (NULL != clp_crnt) {
    if (fprintf(fp, "%d %d\n", clp_crnt->get_i32_xp(), clp_crnt->get_i32_yp()) <
        0) {
      pri_funct_err_bttvr(
          "Error : another point : fprintf(%d %d) returns minus",
          clp_crnt->get_i32_xp(), clp_crnt->get_i32_yp());
      return NG;
    }
  }

  return OK;
}

#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "pixel_line_node.h"

int pixel_line_node::save_expand_line(FILE *fp) {
  pixel_point_node *clp_crnt;
  int32_t ii;

  clp_crnt = this->get_clp_link_one_expand();
  for (ii       = 0; NULL != clp_crnt;
       clp_crnt = clp_crnt->get_clp_next_point(), ++ii) {
    /* 伸長した後なので3倍-2の長さ。偽の場合、たぶん無限ループ */
    /***assert( ii < this->_i32_point_count * 3 - 2 );***/
    assert(ii < this->_i32_point_count);

    /* 次の点(シード2含む)の座標値保存 */
    if (fprintf(fp, "%g %g\n", clp_crnt->get_d_xp_tgt(),
                clp_crnt->get_d_yp_tgt()) < 0) {
      pri_funct_err_bttvr("Error : point %d : fprintf(%g %g) returns minus", ii,
                          clp_crnt->get_d_xp_tgt(), clp_crnt->get_d_yp_tgt());
      return NG;
    }
  }
  return OK;
}

/********************************************************************/

int pixel_line_node::save_one_expand_point(FILE *fp) {
  pixel_point_node *clp_crnt;

  clp_crnt = this->get_clp_link_one_expand();

  if (NULL != clp_crnt) {
    if (fprintf(fp, "%g %g\n", clp_crnt->get_d_xp_tgt(),
                clp_crnt->get_d_yp_tgt()) < 0) {
      pri_funct_err_bttvr(
          "Error : one expand point : fprintf(%g %g) returns minus",
          clp_crnt->get_d_xp_tgt(), clp_crnt->get_d_yp_tgt());
      return NG;
    }
  }
  return OK;
}

int pixel_line_node::save_another_expand_point(FILE *fp) {
  pixel_point_node *clp_crnt;

  clp_crnt = this->get_clp_link_another_expand();

  if (NULL != clp_crnt) {
    if (fprintf(fp, "%g %g\n", clp_crnt->get_d_xp_tgt(),
                clp_crnt->get_d_yp_tgt()) < 0) {
      pri_funct_err_bttvr(
          "Error : another expand point : fprintf(%g %g) returns minus",
          clp_crnt->get_d_xp_tgt(), clp_crnt->get_d_yp_tgt());
      return NG;
    }
  }

  return OK;
}

#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "pixel_line_node.h"

int pixel_line_node::save_expand_vector(FILE *fp) {
  pixel_point_node *clp_term, *clp_expa;

  clp_term = this->get_clp_link_one();
  clp_expa = this->get_clp_link_one_expand();

  if ((NULL != clp_term) && (NULL != clp_expa)) {
    (void)fprintf(fp, "%g %g %g %g\n", clp_term->get_d_xp_tgt(),
                  clp_term->get_d_yp_tgt(),
                  clp_expa->get_d_xp_tgt() - clp_term->get_d_xp_tgt(),
                  clp_expa->get_d_yp_tgt() - clp_term->get_d_yp_tgt());
    if (ferror(fp)) {
      pri_funct_err_bttvr("Error : fprintf(one and one_expand xp and yp)");
      return NG;
    }
  }

  clp_term = this->get_clp_link_another();
  clp_expa = this->get_clp_link_another_expand();

  if ((NULL != clp_term) && (NULL != clp_expa)) {
    (void)fprintf(fp, "%g %g %g %g\n", clp_term->get_d_xp_tgt(),
                  clp_term->get_d_yp_tgt(),
                  clp_expa->get_d_xp_tgt() - clp_term->get_d_xp_tgt(),
                  clp_expa->get_d_yp_tgt() - clp_term->get_d_yp_tgt());
    if (ferror(fp)) {
      pri_funct_err_bttvr(
          "Error : fprintf(another and another_expand xp and yp)");
      return NG;
    }
  }

  return OK;
}

#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pixel_line_node.h"

void pixel_line_node::set_bbox(void) {
  pixel_point_node *clp_point;
  int32_t ii;

  /* 浮動小数化 */
  clp_point = this->get_clp_link_one_expand();
  if (NULL == clp_point) {
    clp_point = this->get_clp_link_one();
  }
  for (ii        = 0; NULL != clp_point;
       clp_point = clp_point->get_clp_next_point(), ++ii) {
    /* 偽の場合、たぶん無限ループ */
    assert(ii < this->_i32_point_count);

    if (0 == ii) {
      this->_d_bbox_x_min = clp_point->get_d_xp_tgt();
      this->_d_bbox_x_max = clp_point->get_d_xp_tgt();
      this->_d_bbox_y_min = clp_point->get_d_yp_tgt();
      this->_d_bbox_y_max = clp_point->get_d_yp_tgt();
    } else {
      if (clp_point->get_d_xp_tgt() < this->_d_bbox_x_min) {
        this->_d_bbox_x_min = clp_point->get_d_xp_tgt();
      } else if (this->_d_bbox_x_max < clp_point->get_d_xp_tgt()) {
        this->_d_bbox_x_max = clp_point->get_d_xp_tgt();
      }
      if (clp_point->get_d_yp_tgt() < this->_d_bbox_y_min) {
        this->_d_bbox_y_min = clp_point->get_d_yp_tgt();
      } else if (this->_d_bbox_y_max < clp_point->get_d_yp_tgt()) {
        this->_d_bbox_y_max = clp_point->get_d_yp_tgt();
      }
    }
  }
}

#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pixel_line_node.h"

void pixel_line_node::set_middle(void) {
  int32_t ii;
  pixel_point_node *clp_middle;

  /* すでにラインデータとしてのリンクがあること */
  assert(NULL != this->get_clp_link_one());

  /* 中間点を得る(nodeが偶数個のときはendよりのnode) */
  for (ii = 0, clp_middle = this->get_clp_link_one();
       (ii < (this->_i32_point_count) / 2) && (NULL != clp_middle); ++ii) {
    if (NULL != clp_middle->get_clp_next_point()) {
      clp_middle = clp_middle->get_clp_next_point();
    }
  }
  /* 短すぎる線は選択していないはず */
  assert(clp_middle != this->get_clp_link_one());
  assert(clp_middle != this->get_clp_link_another());

  /* 中間点を登録 */
  this->link_middle(clp_middle);
}

#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pixel_line_node.h"

/* 3点から線分がスムースになるような中点を計算する
        * p1
        |
        |
        |
        |       +
        |
        |   * <-- new p2
        |
        *---------------*
        p2              |p3
                    *   |
                        |
                +	|       +
                        |
                        |   *
                        |
                        *----------------*
*/
void pixel_line_node::_smooth_point(double d_x1, double d_y1, double d_x2,
                                    double d_y2, double d_x3, double d_y3,
                                    double *dp_x, double *dp_y) {
  *dp_x = (d_x2 + ((d_x1 + d_x3) / 2.0)) / 2.0;
  *dp_y = (d_y2 + ((d_y1 + d_y3) / 2.0)) / 2.0;
}

void pixel_line_node::smooth_body(int32_t i32_smooth_retry) {
  pixel_point_node *clp_1, *clp_2, *clp_3;
  int32_t kk, ii;
  double d_x, d_y, d_x1, d_y1, d_x2 = 0, d_y2 = 0, d_x3, d_y3;

  /* スムース化 */
  for (kk = 0; kk < i32_smooth_retry; ++kk) {
    clp_1 = this->get_clp_link_one();
    clp_2 = clp_3 = NULL;
    for (ii = 0; NULL != clp_1; ++ii) {
      /* 偽の場合、たぶん無限ループ */
      assert(ii < this->_i32_point_count);

      d_x1 = clp_1->get_d_xp_tgt();
      d_y1 = clp_1->get_d_yp_tgt();

      /* スムース化 */
      if (NULL != clp_3) {
        this->_smooth_point(d_x1, d_y1, d_x2, d_y2, d_x3, d_y3, &d_x, &d_y);
        clp_2->set_d_xp_tgt(d_x);
        clp_2->set_d_yp_tgt(d_y);
      }

      /* 細線化部分の終点で終り */
      if (this->get_clp_link_another() == clp_1) break;

      /* 次の3ポイントへずらす */
      clp_3 = clp_2;
      d_x3  = d_x2;
      d_y3  = d_y2;
      clp_2 = clp_1;
      d_x2  = d_x1;
      d_y2  = d_y1;
      clp_1 = clp_1->get_clp_next_point();
    }
  }
}

void pixel_line_node::smooth_expand(int32_t i32_smooth_retry) {
  pixel_point_node *clp_1, *clp_2, *clp_3;
  int32_t kk, ii;
  double d_x, d_y, d_x1, d_y1, d_x2, d_y2, d_x3, d_y3;

  /* スムース化 */
  for (kk = 0; kk < i32_smooth_retry; ++kk) {
    clp_1 = this->get_clp_link_one_expand();
    clp_2 = clp_3 = NULL;
    for (ii = 0; NULL != clp_1; ++ii) {
      /* 偽の場合、たぶん無限ループ */
      /***assert( ii < (this->_i32_point_count * 3) );***/
      assert(ii < (this->_i32_point_count));

      d_x1 = clp_1->get_d_xp_tgt();
      d_y1 = clp_1->get_d_yp_tgt();

      /* スムース化 */
      if (NULL != clp_3) {
        this->_smooth_point(d_x1, d_y1, d_x2, d_y2, d_x3, d_y3, &d_x, &d_y);
        clp_2->set_d_xp_tgt(d_x);
        clp_2->set_d_yp_tgt(d_y);
      }

      /* 伸ばした終点で終り */
      if (this->get_clp_link_another_expand() == clp_1) break;

      /* 次の3ポイントへずらす */
      clp_3 = clp_2;
      d_x3  = d_x2;
      d_y3  = d_y2;
      clp_2 = clp_1;
      d_x2  = d_x1;
      d_y2  = d_y1;
      clp_1 = clp_1->get_clp_next_point();
    }
  }
}

#ifndef _pixel_select_same_way_h_
#define _pixel_select_same_way_h_

#include <stdio.h>

#include "igs_line_blur.h"  // "list_root.h" "calculator_geometry.h" "pixel_point_root.h" "pixel_line_root.h"

class pixel_select_same_way_node final : public list_node {
public:
  pixel_select_same_way_node(void) {
    this->clp_point_middle = NULL;
    this->clp_point_term   = NULL;
    this->clp_point_expand = NULL;
    this->d_length         = -1.0;
  }
  void copy(pixel_select_same_way_node *clp) {
    this->clp_point_middle = clp->clp_point_middle;
    this->clp_point_term   = clp->clp_point_term;
    this->clp_point_expand = clp->clp_point_expand;
    this->d_length         = clp->d_length;
  }
  pixel_point_node *clp_point_middle;
  pixel_point_node *clp_point_term;
  pixel_point_node *clp_point_expand;
  double d_length;

private:
};

class pixel_select_same_way_root final : public list_root {
public:
  pixel_select_same_way_root(void) {
    this->_i_mv_sw = false;
    this->_i_cv_sw = false;
    this->_i_pv_sw = false;

    this->_i32_count_max = 4;
    this->_d_length_max  = 160;
  }
  ~pixel_select_same_way_root(void) { this->mem_free(); }
  void set_i_mv_sw(bool sw) { this->_i_mv_sw = sw; }
  void set_i_cv_sw(bool sw) { this->_i_cv_sw = sw; }
  void set_i_pv_sw(bool sw) { this->_i_pv_sw = sw; }

  int32_t get_i32_count_max(void) { return this->_i32_count_max; }
  void set_i32_count_max(int32_t ii) { this->_i32_count_max = ii; }
  double get_d_length_max(void) { return this->_d_length_max; }
  void set_d_length_max(double dd) { this->_d_length_max = dd; }

  void exec(pixel_line_node *clp_line_first, int32_t i32_count,
            pixel_point_node *clp_point_middle,
            pixel_point_node *clp_point_term,
            pixel_point_node *clp_point_expand);
  void get_vector(double *dp_xv, double *dp_yv);

  void mem_free(void);

private:
  bool _i_mv_sw, _i_cv_sw, _i_pv_sw;

  int32_t _i32_count_max;
  double _d_length_max;

  calculator_geometry _cl_cal_geom;

  pixel_select_same_way_node *_append(pixel_select_same_way_node *clp_previous);
  void _remove(pixel_select_same_way_node *clp_old);

  int _sort_append(pixel_select_same_way_node *clp_src);
  double _term_length(pixel_point_node *clp_middle1,
                      pixel_point_node *clp_term1,
                      pixel_point_node *clp_middle2,
                      pixel_point_node *clp_term2);
};

#endif              /* !_pixel_select_same_way_h_ */
#include <math.h>   /* sqrt() */
#include <stdlib.h> /* free(), calloc() */
#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "pixel_line_root.h" "pixel_select_same_way.h"

/* WindowsではM_PIが見つからない */
#if defined _MSC_VER
#define M_PI 3.14159265358979323846
#endif

pixel_select_same_way_node *pixel_select_same_way_root::_append(
    pixel_select_same_way_node *clp_previous) {
  pixel_select_same_way_node *clp_new;

  clp_new = new pixel_select_same_way_node;
  if (NULL == clp_new) {
    pri_funct_err_bttvr(
        "Error : 'new pixel_select_same_way_node' returns NULL.");
    return NULL;
  }
  clp_new = (pixel_select_same_way_node *)this->push(clp_previous, clp_new);

  return clp_new;
}

void pixel_select_same_way_root::_remove(pixel_select_same_way_node *clp_old) {
  assert(NULL != clp_old); /* あってはならないプログラムバグのチェック */
  this->pop(clp_old);
  delete clp_old;
}

void pixel_select_same_way_root::mem_free(void) {
  pixel_select_same_way_node *clp_;
  int32_t ii;

  if (NULL != this->get_clp_last()) {
    if (this->_i_mv_sw) {
      pri_funct_msg_ttvr("pixel_select_same_way_root::mem_free()");
    }

    ii = 0;
    while (NULL !=
           (clp_ = (pixel_select_same_way_node *)this->get_clp_last())) {
      this->_remove(clp_);
      ++ii;
    }
    if (this->_i_pv_sw) {
      pri_funct_msg_ttvr("free select same way node %d", ii);
    }
  }
}

/********************************************************************/

/* lengthの値を見て、小さい値順になるように追加する
        他の選択より小さいなら先頭
        他の選択より大きいなら最後に追加
        他の選択の値の間入るならそこに追加
*/
int pixel_select_same_way_root::_sort_append(
    pixel_select_same_way_node *clp_src) {
  pixel_select_same_way_node *clp_loop, *clp_prev;
  int32_t ii;

  assert(0.0 < clp_src->d_length);

  clp_loop = (pixel_select_same_way_node *)this->get_clp_first();

  /* まだなにも選択されていないとき */
  if (NULL == clp_loop) {
    clp_prev = NULL;
  }
  /* 1つだけ選択されているとき */
  else if (NULL == clp_loop->get_clp_next()) {
    /* 既選択より値が小さいので先頭位置を示す */
    if (clp_src->d_length < clp_loop->d_length) {
      clp_prev = NULL;
    }
    /* 既選択より値が大きいので既選択を示す */
    else {
      clp_prev = clp_loop;
    }
  }
  /* 2つ以上の選択があるとき */
  else {
    /* 既選択より値が大きいときのため最後尾を示しておく */
    clp_prev = (pixel_select_same_way_node *)this->get_clp_last();
    /* 既選択より値が小さい場所があればその手前を示す */
    for (ii      = 0; NULL != clp_loop; ++ii,
        clp_loop = (pixel_select_same_way_node *)clp_loop->get_clp_next()) {
      assert(ii < this->get_i32_count());

      if (clp_src->d_length < clp_loop->d_length) {
        clp_prev = (pixel_select_same_way_node *)clp_loop->get_clp_previous();
        break;
      }
    }
  }

  /* 示した場所の後ろに追加 */
  clp_prev = this->_append(clp_prev);
  if (NULL == clp_prev) {
    pri_funct_err_bttvr("Error : this->_append(clp_prev) returns NULL.");
    return NG;
  }

  /* 情報をコピー */
  clp_prev->copy(clp_src);

  return OK;
}

/********************************************************************/

double pixel_select_same_way_root::_term_length(pixel_point_node *clp_middle1,
                                                pixel_point_node *clp_term1,
                                                pixel_point_node *clp_middle2,
                                                pixel_point_node *clp_term2) {
  double d_length, d_radian;

  /* 2つの端点の距離 */
  assert(clp_term2 != clp_term1);
  d_length = sqrt((clp_term2->get_d_xp_tgt() - clp_term1->get_d_xp_tgt()) *
                      (clp_term2->get_d_xp_tgt() - clp_term1->get_d_xp_tgt()) +
                  (clp_term2->get_d_yp_tgt() - clp_term1->get_d_yp_tgt()) *
                      (clp_term2->get_d_yp_tgt() - clp_term1->get_d_yp_tgt()));

  /* 同方向のものでなければならない
          2つの線分の角度をしらべ...
  */
  assert(clp_term1 != clp_middle1);
  assert(clp_term2 != clp_middle2);
  d_radian = this->_cl_cal_geom.get_d_radian_by_2_vector(
      clp_term1->get_d_xp_tgt() - clp_middle1->get_d_xp_tgt(),
      clp_term1->get_d_yp_tgt() - clp_middle1->get_d_yp_tgt(),
      clp_term2->get_d_xp_tgt() - clp_middle2->get_d_xp_tgt(),
      clp_term2->get_d_yp_tgt() - clp_middle2->get_d_yp_tgt());
  /* 線分が逆方向ならだめ */
  if ((M_PI / 2.0 <= d_radian) && (d_radian <= M_PI * 3.0 / 2.0)) return -1.0;

  /* 端点が離れすぎならだめ */
  if (this->_d_length_max <= d_length) return -1.0;

  /* 距離を返す */
  return d_length;
}

/********************************************************************/

/* 同方向で、一定の距離の範囲にある端点を選択する */
void pixel_select_same_way_root::exec(pixel_line_node *clp_line_first,
                                      int32_t i32_count,
                                      pixel_point_node *clp_point_middle,
                                      pixel_point_node *clp_point_term,
                                      pixel_point_node *clp_point_expand) {
  pixel_line_node *clp_line;
  int32_t ii;
  double d_length;
  pixel_select_same_way_node cl_select;

  assert(clp_point_middle != clp_point_term);
  assert(clp_point_term != clp_point_expand);
  assert(clp_point_expand != clp_point_middle);

  /* 選択リストをクリア */
  this->mem_free();

  /* 自身以外の全端点と比較 */
  for (clp_line = clp_line_first, ii = 0; NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    assert(ii < i32_count);

    /* 端点1が自分以外のときで、
       線分が短すぎて(1点以下)真中指定と端点1が同じではない */
    if ((clp_line->get_clp_link_one() != clp_point_term) &&
        (clp_line->get_clp_link_middle() != clp_line->get_clp_link_one())) {
      assert(NULL != clp_line->get_clp_link_middle());
      assert(NULL != clp_line->get_clp_link_one());
      d_length = this->_term_length(clp_point_middle, clp_point_term,
                                    clp_line->get_clp_link_middle(),
                                    clp_line->get_clp_link_one());
      /* 同方向で一定の距離の範囲内にあるなら */
      if ((0.0 < d_length) && (NULL != clp_line->get_clp_link_one_expand())) {
        cl_select.clp_point_middle = clp_line->get_clp_link_middle();
        cl_select.clp_point_term   = clp_line->get_clp_link_one();
        cl_select.clp_point_expand = clp_line->get_clp_link_one_expand();
        cl_select.d_length         = d_length;
        /* 登録 */
        this->_sort_append(&cl_select);
      }
    }

    /* 端点2が自分以外のときで、
       線分が短すぎて(2点以下)真中指定と端点2が同じではない */
    if ((clp_line->get_clp_link_another() != clp_point_term) &&
        (clp_line->get_clp_link_middle() != clp_line->get_clp_link_another())) {
      assert(NULL != clp_line->get_clp_link_middle());
      assert(NULL != clp_line->get_clp_link_another());
      /* 端点(another)を調べて */
      d_length = this->_term_length(clp_point_middle, clp_point_term,
                                    clp_line->get_clp_link_middle(),
                                    clp_line->get_clp_link_another());
      /* 同方向で一定の距離の範囲内にあるなら */
      if ((0.0 < d_length) &&
          (NULL != clp_line->get_clp_link_another_expand())) {
        cl_select.clp_point_middle = clp_line->get_clp_link_middle();
        cl_select.clp_point_term   = clp_line->get_clp_link_another();
        cl_select.clp_point_expand = clp_line->get_clp_link_another_expand();
        cl_select.d_length         = d_length;
        /* 登録 */
        this->_sort_append(&cl_select);
      }
    }
  }
}

/********************************************************************/

/* 距離の近い順(lengthの小さい値)に指定個数分のベクトルを合成して返す
        先頭から
        this->_i32_count_max数個分
        のベクトルを合成する
*/
void pixel_select_same_way_root::get_vector(double *dp_xv, double *dp_yv) {
  pixel_select_same_way_node *clp_loop;
  int32_t ii;

  clp_loop = (pixel_select_same_way_node *)this->get_clp_first();
  for (ii      = 0; (NULL != clp_loop) && (ii < this->_i32_count_max); ++ii,
      clp_loop = (pixel_select_same_way_node *)clp_loop->get_clp_next()) {
    assert(ii < this->get_i32_count());
    assert(NULL != clp_loop->clp_point_expand);
    assert(NULL != clp_loop->clp_point_term);

    *dp_xv += clp_loop->clp_point_expand->get_d_xp_tgt() -
              clp_loop->clp_point_term->get_d_xp_tgt();
    *dp_yv += clp_loop->clp_point_expand->get_d_yp_tgt() -
              clp_loop->clp_point_term->get_d_yp_tgt();
  }
}

#ifndef _pixel_line_root_h_
#define _pixel_line_root_h_

#include <stdio.h>

#include "igs_line_blur.h"  // "list_root.h" "calculator_geometry.h" "pixel_point_root.h" "pixel_line_node.h" "pixel_select_same_way.h"

class pixel_line_root final : public list_root {
public:
  pixel_line_root(void) {
    this->_i_mv_sw = false;
    this->_i_cv_sw = false;
    this->_i_pv_sw = false;

    this->_d_bbox_x_min = 0.0;
    this->_d_bbox_x_max = 0.0;
    this->_d_bbox_y_min = 0.0;
    this->_d_bbox_y_max = 0.0;

    this->_i32_smooth_retry   = 100;
    this->_i_same_way_exec_sw = true;
  }
  ~pixel_line_root(void) { this->mem_free(); }
  void set_i_mv_sw(bool sw) { this->_i_mv_sw = sw; }
  void set_i_cv_sw(bool sw) { this->_i_cv_sw = sw; }
  void set_i_pv_sw(bool sw) { this->_i_pv_sw = sw; }

  double get_d_bbox_x_min(void) { return this->_d_bbox_x_min; }
  double get_d_bbox_x_max(void) { return this->_d_bbox_x_max; }
  double get_d_bbox_y_min(void) { return this->_d_bbox_y_min; }
  double get_d_bbox_y_max(void) { return this->_d_bbox_y_max; }

  void set_i32_smooth_retry(int32_t i32) { this->_i32_smooth_retry = i32; }
  int32_t get_i32_smooth_retry(void) { return this->_i32_smooth_retry; }
  void set_i_same_way_exec_sw(bool sw) { this->_i_same_way_exec_sw = sw; }

  int exec01020304(pixel_point_root *clp_pixel_point_root);
  void exec05_set_middle(void);
  void exec06_int2double_body(void);
  void exec07_smooth_body(void);
  int exec08_expand_lines(pixel_point_root *clp_pixel_point_root);
  void exec09_same_way_expand(pixel_select_same_way_root *clp_select);
  void exec10_smooth_expand(void);
  void exec11_set_bbox(void);

  int save_not_include(pixel_point_root *clp_pixel_point_root,
                       const char *cp_fname);
  int save_lines(const char *cp_fname);
  int save_one_point(const char *cp_fname);
  int save_middle_point(const char *cp_fname);
  int save_another_point(const char *cp_fname);

  int save_expand_lines(const char *cp_fname);
  int save_one_expand_point(const char *cp_fname);
  int save_another_expand_point(const char *cp_fname);
  int save_expand_vector(const char *cp_fname);

  void mem_free(void);

private:
  bool _i_mv_sw, _i_cv_sw, _i_pv_sw;

  double _d_bbox_x_min, _d_bbox_x_max, _d_bbox_y_min, _d_bbox_y_max;

  int32_t _i32_smooth_retry;
  bool _i_same_way_exec_sw;

  calculator_geometry _cl_cal_geom;

  pixel_line_node *_append(pixel_line_node *clp_previous);
  void _remove(pixel_line_node *clp_old);

  int _exec01_link_left_right(pixel_point_root *clp_pixel_point_root);
  int _exec02_link_up_down(pixel_point_root *clp_pixel_point_root);
  int _exec03_link_slant(pixel_point_root *clp_pixel_point_root);
  int _exec04_grouping(pixel_point_root *clp_pixel_point_root);

  double _same_way_expand_radian_diff(pixel_point_node *clp_point_middle,
                                      pixel_point_node *clp_point_term,
                                      pixel_point_node *clp_point_term_expand,
                                      pixel_select_same_way_root *clp_select);
};

#endif                      /* !_pixel_line_root_h_ */
#include "igs_line_blur.h"  // "pri.h" "pixel_line_root.h"

int pixel_line_root::exec01020304(pixel_point_root *clp_pixel_point_root) {
  /* ピクセルノードの左右リンク */
  if (OK != this->_exec01_link_left_right(clp_pixel_point_root)) {
    pri_funct_err_bttvr("Error : this->_exec01_link_left_right() returns NG.");
    return NG;
  }
  /* ピクセルノードの上下リンク */
  if (OK != this->_exec02_link_up_down(clp_pixel_point_root)) {
    pri_funct_err_bttvr("Error : this->_exec02_link_up_down() returns NG.");
    return NG;
  }
  /* ピクセルノードの斜めリンク */
  if (OK != this->_exec03_link_slant(clp_pixel_point_root)) {
    pri_funct_err_bttvr("Error : this->_exec03_link_slant() returns NG.");
    return NG;
  }

  /* リンクしたものを、グループ化 */
  if (OK != this->_exec04_grouping(clp_pixel_point_root)) {
    pri_funct_err_bttvr("Error : this->_exec04_grouping() returns NG.");
    return NG;
  }

  return OK;
}

/********************************************************************/

int pixel_line_root::_exec01_link_left_right(
    pixel_point_root *clp_pixel_point_root) {
  pixel_point_node *clp_point, *clp_point2;
  int32_t ii;

  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("pixel_line_root::_exec01_link_left_right()");
  }

  ii         = 0;
  clp_point2 = NULL;
  for (clp_point = (pixel_point_node *)(clp_pixel_point_root->get_clp_first());
       NULL != clp_point;
       clp_point = (pixel_point_node *)clp_point->get_clp_next()) {
    /* 隣同士の２ポイントで */
    if ((NULL != clp_point2)
        /* 同じスキャンラインで */
        && (clp_point->get_i32_yp() == clp_point2->get_i32_yp())
        /* 左右隣同士なら */
        && (((clp_point2->get_i32_xp()) + 1) == (clp_point->get_i32_xp()))) {
      /* 双方向リンクする */
      if (NG == clp_point->link_near(clp_point2)) {
        pri_funct_err_bttvr(
            "Error : count %d : clp_point->link_near() returns NG.", ii);
        return NG;
      }
      if (NG == clp_point2->link_near(clp_point)) {
        pri_funct_err_bttvr(
            "Error : count %d : clp_point2->link_near() returns NG.", ii);
        return NG;
      }
      ++ii;
    }
    clp_point2 = clp_point;
  }

  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr(" link left right %d", ii);
  }

  return OK;
}
int pixel_line_root::_exec02_link_up_down(
    pixel_point_root *clp_pixel_point_root) {
  pixel_point_node *clp_point, *clp_point2;
  int32_t ii;

  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("pixel_line_root::_exec02_link_up_down()");
  }

  ii = 0;

  for (clp_point = (pixel_point_node *)(clp_pixel_point_root->get_clp_first());
       NULL != clp_point;
       clp_point = (pixel_point_node *)clp_point->get_clp_next()) {
    for (clp_point2 = (pixel_point_node *)clp_point->get_clp_next();
         (NULL != clp_point2)
         /* １つ上のスキャンラインまでを見る */
         && (clp_point2->get_i32_yp() <= (1 + clp_point->get_i32_yp()));
         clp_point2 = (pixel_point_node *)clp_point2->get_clp_next()) {
      if (
          /* １つ上のスキャンラインで */
          (clp_point2->get_i32_yp() == (1 + (clp_point->get_i32_yp())))
          /* 上下隣接同士なら */
          && (clp_point->get_i32_xp() == clp_point2->get_i32_xp())) {
        /* 双方向リンクする */
        if (NG == clp_point->link_near(clp_point2)) {
          pri_funct_err_bttvr(
              "Error : count %d : clp_point->link_near() returns NG.", ii);
          return NG;
        }
        if (NG == clp_point2->link_near(clp_point)) {
          pri_funct_err_bttvr(
              "Error : count %d : clp_point2->link_near() returns NG.", ii);
          return NG;
        }
        ++ii;

        /* 一個つなげたら抜ける(上下一致するのは一か所だけ)*/
        break;
      }
    }
  }

  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr(" link up down %d", ii);
  }

  return OK;
}

int pixel_line_root::_exec03_link_slant(
    pixel_point_root *clp_pixel_point_root) {
  pixel_point_node *clp_point, *clp_point2, *clp_point_link;
  int32_t ii, jj;

  bool i_left_link_sw, i_right_link_sw, i_up_link_sw;

  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("pixel_line_root::_exec03_link_slant()");
  }

  ii = 0;

  for (clp_point = (pixel_point_node *)(clp_pixel_point_root->get_clp_first());
       NULL != clp_point;
       clp_point = (pixel_point_node *)clp_point->get_clp_next()) {
    /* LINK_NEAR_COUNT個所リンクしているところは、
       これ以上リンクできないので、次へ */
    if (NULL != clp_point->get_clp_link_near(LINK_NEAR_COUNT - 1)) {
      continue;
    }

    i_left_link_sw  = false;
    i_right_link_sw = false;
    i_up_link_sw    = false;

    /* 全リンクの調査 */
    for (jj = 0; jj < LINK_NEAR_COUNT; ++jj) {
      clp_point_link = clp_point->get_clp_link_near(jj);
      /* リンクの終了なので抜ける */
      if (NULL == clp_point_link) break;
      /* 上につながっている */
      if (((clp_point_link->get_i32_xp()) == (clp_point->get_i32_xp())) &&
          ((clp_point_link->get_i32_yp()) == (clp_point->get_i32_yp() + 1))) {
        i_up_link_sw = true;
      }
      /* 左につながっている */
      if (((clp_point_link->get_i32_xp()) == (clp_point->get_i32_xp() - 1)) &&
          ((clp_point_link->get_i32_yp()) == (clp_point->get_i32_yp()))) {
        i_left_link_sw = true;
      }
      /* 右につながっている */
      if (((clp_point_link->get_i32_xp()) == (clp_point->get_i32_xp() + 1)) &&
          ((clp_point_link->get_i32_yp()) == (clp_point->get_i32_yp()))) {
        i_right_link_sw = true;
      }
    }

    /* 左右両方につながりあるなら、斜めにはつなげず、次へ */
    if (i_left_link_sw && i_right_link_sw) continue;
    /* 上につながっているなら、斜めにはつなげず、次へ */
    if (i_up_link_sw) continue;

    for (clp_point2 = (pixel_point_node *)clp_point->get_clp_next();
         (NULL != clp_point2)
         /* １つ上のスキャンラインまでを見る */
         && (clp_point2->get_i32_yp() <= (1 + clp_point->get_i32_yp()));
         clp_point2 = (pixel_point_node *)clp_point2->get_clp_next()) {
      /* １つ上のスキャンラインでないなら次へ */
      if ((clp_point2->get_i32_yp()) != (1 + (clp_point->get_i32_yp()))) {
        continue;
      }

      /* 左斜め上につながるものあり */
      if (!i_left_link_sw &&
          ((clp_point2->get_i32_xp()) == (clp_point->get_i32_xp() - 1)) &&
          ((clp_point2->get_i32_yp()) == (clp_point->get_i32_yp() + 1))) {
        /* 双方向リンクする */
        if (NG == clp_point->link_near(clp_point2)) {
          pri_funct_err_bttvr(
              "Error : count %d : clp_point->link_near() returns NG.", ii);
          return NG;
        }
        if (NG == clp_point2->link_near(clp_point)) {
          pri_funct_err_bttvr(

              "Error : count %d : clp_point2->link_near() returns NG.", ii);
          return NG;
        }
        ++ii;
      }
      /* 右斜め上につながるものあり */
      if (!i_right_link_sw &&
          ((clp_point2->get_i32_xp()) == (clp_point->get_i32_xp() + 1)) &&
          ((clp_point2->get_i32_yp()) == (clp_point->get_i32_yp() + 1))) {
        /* 双方向リンクする */
        if (NG == clp_point->link_near(clp_point2)) {
          pri_funct_err_bttvr(
              "Error : count %d : clp_point->link_near() returns NG.", ii);
          return NG;
        }
        if (NG == clp_point2->link_near(clp_point)) {
          pri_funct_err_bttvr(
              "Error : count %d : clp_point2->link_near() returns NG.", ii);
          return NG;
        }
        ++ii;
      }
    }
  }

  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr(" link slant %d", ii);
  }

  return OK;
}

int pixel_line_root::_exec04_grouping(pixel_point_root *clp_pixel_point_root) {
  pixel_point_node *clp_point;
  pixel_line_node *clp_line_before, *clp_line_crnt;
  int32_t ii;

  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("pixel_line_root::_exec04_grouping()");
  }

  ii              = 0;
  clp_line_before = NULL;

  for (clp_point = (pixel_point_node *)(clp_pixel_point_root->get_clp_first());
       NULL != clp_point;
       clp_point = (pixel_point_node *)clp_point->get_clp_next()) {
    /* 線分としてリンク済みのは飛ばして次へ */
    if ((NULL != clp_point->get_clp_next_point()) ||
        (NULL != clp_point->get_clp_previous_point())) {
      continue;
    }

    /* 端点をみつけた */
    if ((NULL != clp_point->get_clp_link_near(0)) &&
        (NULL == clp_point->get_clp_link_near(1))) {
      clp_line_crnt = this->_append(clp_line_before);
      if (NULL == clp_line_crnt) {
        pri_funct_err_bttvr("Error : this->_append() returns NULL.");
        return NG;
      }

      /* 線分としてリンクし、両端点をセットする */
      clp_line_crnt->link_line(clp_point, clp_point->get_clp_link_near(0),
                               clp_pixel_point_root->get_i32_count());
      /* リンクした線分は3点以上でないとだめ */
      if (clp_line_crnt->get_i32_point_count() < 3) {
        this->_remove(clp_line_crnt);
      }
      /* 選択が3点以上ならOKなので、次のラインへ */
      else {
        clp_line_before = clp_line_crnt;
      }

      ++ii;
    }
  }

  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr(" make %d lines", this->get_i32_count());
  }

  return OK;
}

#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "pixel_line_root.h"

void pixel_line_root::exec05_set_middle(void) {
  pixel_line_node *clp_line;
  int32_t ii;

  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("pixel_line_root::exec05_set_middle()");
  }

  for (clp_line = (pixel_line_node *)this->get_clp_first(), ii = 0;
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    assert(ii < this->get_i32_count());

    clp_line->set_middle();
  }
  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr(" set middle point about %d lines", ii);
  }
}

#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "pixel_line_root.h"

void pixel_line_root::exec06_int2double_body(void) {
  pixel_line_node *clp_line;
  int32_t ii;

  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("pixel_line_root::exec06_int2double_body()");
  }

  for (clp_line = (pixel_line_node *)this->get_clp_first(), ii = 0;
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    assert(ii < this->get_i32_count());

    clp_line->int2double_body();
  }

  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr(" int to double %d lines", ii);
  }
}
#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "pixel_line_root.h"

void pixel_line_root::exec07_smooth_body(void) {
  pixel_line_node *clp_line;
  int32_t ii;

  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("pixel_line_root::exec07_smooth_body()");
  }

  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr(" smooth retry %u", this->_i32_smooth_retry);
  }

  /* カウントダウン表示始め */
  if (this->_i_cv_sw) {
    pri_funct_cv_start(this->get_i32_count());
  }

  for (clp_line = (pixel_line_node *)this->get_clp_first(), ii = 0;
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    assert(ii < this->get_i32_count());

    /* カウントダウン表示中 */
    if (this->_i_cv_sw) {
      pri_funct_cv_run(ii);
    }

    clp_line->smooth_body(this->_i32_smooth_retry);
  }
  /* カウントダウン表示終了 */
  if (this->_i_cv_sw) {
    pri_funct_cv_end();
  }

  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr(" smooth %d lines", ii);
  }
}

void pixel_line_root::exec10_smooth_expand(void) {
  pixel_line_node *clp_line;
  int32_t ii;

  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("pixel_line_root::exec10_smooth_expand()");
  }
  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr(" smooth retry %u", this->_i32_smooth_retry);
  }

  /* カウントダウン表示始め */
  if (this->_i_cv_sw) {
    pri_funct_cv_start(this->get_i32_count());
  }

  for (clp_line = (pixel_line_node *)this->get_clp_first(), ii = 0;
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    assert(ii < this->get_i32_count());

    /* カウントダウン表示中 */
    if (this->_i_cv_sw) {
      pri_funct_cv_run(ii);
    }

    clp_line->smooth_expand(this->_i32_smooth_retry);
  }
  /* カウントダウン表示終了 */
  if (this->_i_cv_sw) {
    pri_funct_cv_end();
  }

  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr(" smooth %d lines", ii);
  }
}

#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "pixel_line_root.h"

int pixel_line_root::exec08_expand_lines(
    pixel_point_root *clp_pixel_point_root) {
  pixel_line_node *clp_line;
  int32_t ii;

  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("pixel_line_root::exec08_expand_lines()");
  }

  for (clp_line = (pixel_line_node *)this->get_clp_first(), ii = 0;
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    assert(ii < this->get_i32_count());

    if (OK != clp_line->expand_line(clp_pixel_point_root)) {
      pri_funct_err_bttvr(
          "Error : line number %d : clp_line->expand_line() returns NG", ii);
      return NG;
    }
  }

  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr(" expand %d lines", ii);
  }

  return OK;
}

#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "pixel_line_root.h"

double pixel_line_root::_same_way_expand_radian_diff(
    pixel_point_node *clp_point_middle, pixel_point_node *clp_point_term,
    pixel_point_node *clp_point_term_expand,
    pixel_select_same_way_root *clp_select) {
  double d_xv, d_yv;

  assert(clp_point_middle != clp_point_term);
  assert(clp_point_term != clp_point_term_expand);
  assert(clp_point_term_expand != clp_point_middle);

  /* 自身以外の全端点と比較 */
  clp_select->exec((pixel_line_node *)this->get_clp_first(),
                   this->get_i32_count(), clp_point_middle, clp_point_term,
                   clp_point_term_expand);

  if (clp_select->get_i32_count() <= 0) {
    return 0.0;
  }

  /* ベクトルを付加合成 */
  d_xv = 0.0;
  d_yv = 0.0;
  clp_select->get_vector(&d_xv, &d_yv);

  /* ベクトルがゼロの場合
     --> 何も選択していない
         --> 角度ゼロを返す
  */
  if ((0.0 == d_xv) && (0.0 == d_yv)) {
    return 0.0;
  }

  /* 自分自身のベクトル、他より優位生を持たせるため2倍する */
  d_xv +=
      (clp_point_term_expand->get_d_xp_tgt() - clp_point_term->get_d_xp_tgt()) *
      2.0;
  d_yv +=
      (clp_point_term_expand->get_d_yp_tgt() - clp_point_term->get_d_yp_tgt()) *
      2.0;

  /* 元の延長線との角度の差を返す */
  return this->_cl_cal_geom.get_d_radian_by_2_vector(
      clp_point_term_expand->get_d_xp_tgt() - clp_point_term->get_d_xp_tgt(),
      clp_point_term_expand->get_d_yp_tgt() - clp_point_term->get_d_yp_tgt(),
      d_xv, d_yv);
}

/********************************************************************/

void pixel_line_root::exec09_same_way_expand(
    pixel_select_same_way_root *clp_select) {
  pixel_point_node *clp_point_start, *clp_point_tmp;
  pixel_line_node *clp_line;
  int32_t ii;
  double d_radian, d_x, d_y;
  int32_t i32_count_one, i32_count_another;

  /* 同方向曲げをしないときはここで抜けてしまう */
  if (!this->_i_same_way_exec_sw) {
    return;
  }

  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("pixel_line_root::exec09_same_way_expand()");
  }
  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr(" select max length %g count %d",
                       clp_select->get_d_length_max(),
                       clp_select->get_i32_count_max());
  }

  /* 各端点(one,anohter)ごと
     同方向線分となる端点の近点を探し、
     それらの合成ベクトルによる角度との差を一時保管する */
  for (clp_line = (pixel_line_node *)this->get_clp_first(), ii = 0;
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    assert(ii < this->get_i32_count());

    if (NULL != clp_line->get_clp_link_one_expand()) {
      d_radian = this->_same_way_expand_radian_diff(
          clp_line->get_clp_link_middle(), clp_line->get_clp_link_one(),
          clp_line->get_clp_link_one_expand(), clp_select);
      clp_line->set_d_same_way_radian_one(d_radian);
    }

    if (NULL != clp_line->get_clp_link_another_expand()) {
      d_radian = this->_same_way_expand_radian_diff(
          clp_line->get_clp_link_middle(), clp_line->get_clp_link_another(),
          clp_line->get_clp_link_another_expand(), clp_select);
      clp_line->set_d_same_way_radian_another(d_radian);
    }
  }

  /* 各端点(one,anohter)ごと
     伸ばした線分を、一時保管した角度に曲げる */
  i32_count_one     = 0;
  i32_count_another = 0;
  for (clp_line = (pixel_line_node *)this->get_clp_first(), ii = 0;
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    assert(ii < this->get_i32_count());

    if (0.0 < clp_line->get_d_same_way_radian_one()) {
      clp_point_start = clp_line->get_clp_link_one();
      clp_point_tmp   = clp_point_start->get_clp_previous_point();
      for (; NULL != clp_point_tmp;
           clp_point_tmp = clp_point_tmp->get_clp_previous_point()) {
        this->_cl_cal_geom.get_dd_rotate_by_pos(
            clp_point_tmp->get_d_xp_tgt(), clp_point_tmp->get_d_yp_tgt(),
            clp_point_start->get_d_xp_tgt(), clp_point_start->get_d_yp_tgt(),
            clp_line->get_d_same_way_radian_one(), &d_x, &d_y);
        clp_point_tmp->set_d_xp_tgt(d_x);
        clp_point_tmp->set_d_yp_tgt(d_y);
      }
      ++i32_count_one;
    }
    if (0.0 < clp_line->get_d_same_way_radian_another()) {
      clp_point_start = clp_line->get_clp_link_another();
      clp_point_tmp   = clp_point_start->get_clp_next_point();
      for (; NULL != clp_point_tmp;
           clp_point_tmp = clp_point_tmp->get_clp_next_point()) {
        this->_cl_cal_geom.get_dd_rotate_by_pos(
            clp_point_tmp->get_d_xp_tgt(), clp_point_tmp->get_d_yp_tgt(),
            clp_point_start->get_d_xp_tgt(), clp_point_start->get_d_yp_tgt(),
            clp_line->get_d_same_way_radian_another(), &d_x, &d_y);
        clp_point_tmp->set_d_xp_tgt(d_x);
        clp_point_tmp->set_d_yp_tgt(d_y);
      }
      ++i32_count_another;
    }
  }

  clp_select->mem_free();

  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr(" same way expand  one %d another %d", i32_count_one,
                       i32_count_another);
  }
}

#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "pixel_line_root.h"

void pixel_line_root::exec11_set_bbox(void) {
  pixel_line_node *clp_line;
  int32_t ii;

  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("pixel_line_root::exec11_set_bbox()");
  }

  for (clp_line = (pixel_line_node *)this->get_clp_first(), ii = 0;
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    assert(ii < this->get_i32_count());

    clp_line->set_bbox();

    if (0 == ii) {
      this->_d_bbox_x_min = clp_line->get_d_bbox_x_min();
      this->_d_bbox_x_max = clp_line->get_d_bbox_x_max();
      this->_d_bbox_y_min = clp_line->get_d_bbox_y_min();
      this->_d_bbox_y_max = clp_line->get_d_bbox_y_max();
    } else {
      if (clp_line->get_d_bbox_x_min() < this->_d_bbox_x_min) {
        this->_d_bbox_x_min = clp_line->get_d_bbox_x_min();
      } else if (this->_d_bbox_x_max < clp_line->get_d_bbox_x_max()) {
        this->_d_bbox_x_max = clp_line->get_d_bbox_x_max();
      }
      if (clp_line->get_d_bbox_y_min() < this->_d_bbox_y_min) {
        this->_d_bbox_y_min = clp_line->get_d_bbox_y_min();
      } else if (this->_d_bbox_y_max < clp_line->get_d_bbox_y_max()) {
        this->_d_bbox_y_max = clp_line->get_d_bbox_y_max();
      }
    }
  }

  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr(" set bbox %d lines : min x %g y %g : max x %g y %g", ii,
                       this->_d_bbox_x_min, this->_d_bbox_y_min,
                       this->_d_bbox_x_max, this->_d_bbox_y_max);
  }
}
#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "pixel_line_root.h"

pixel_line_node *pixel_line_root::_append(pixel_line_node *clp_previous) {
  pixel_line_node *clp_new;

  clp_new = new pixel_line_node;
  if (NULL == clp_new) {
    pri_funct_err_bttvr("Error : 'new pixel_line_node' returns NULL.");
    return NULL;
  }
  clp_new = (pixel_line_node *)this->push(clp_previous, clp_new);

  return clp_new;
}

void pixel_line_root::_remove(pixel_line_node *clp_old) {
  assert(NULL != clp_old); /* あってはならないプログラムバグのチェック */
  this->pop(clp_old);
  delete clp_old;
}

void pixel_line_root::mem_free(void) {
  pixel_line_node *clp_;
  int32_t ii;

  if (NULL != this->get_clp_last()) {
    if (this->_i_mv_sw) {
      pri_funct_msg_ttvr("pixel_line_root::mem_free()");
    }

    ii = 0;
    while (NULL != (clp_ = (pixel_line_node *)this->get_clp_last())) {
      this->_remove(clp_);
      ++ii;
    }
    if (this->_i_pv_sw) {
      pri_funct_msg_ttvr("free line node %d", ii);
    }
  }
}

#include "igs_line_blur.h"  // "pri.h" "pixel_line_root.h"

int pixel_line_root::save_not_include(pixel_point_root *clp_pixel_point_root,
                                      const char *cp_fname) {
  FILE *fp;
  int32_t ii;
  pixel_point_node *clp_point;
  int32_t i32_not_include_line;

  /* ファイル開く */
  fp = fopen(cp_fname, "w");
  if (NULL == fp) {
    pri_funct_err_bttvr("Error : fopen(%s,w) returns NULL", cp_fname);
    return NG;
  }

  /* 全ポイント数保存 */
  if (fprintf(fp, "# all point count %d\n",
              clp_pixel_point_root->get_i32_count()) < 0) {
    pri_funct_err_bttvr("Error : fprintf(# all point count) returns minus");
    fclose(fp);
    return NG;
  }

  i32_not_include_line = 0;
  for (clp_point = (pixel_point_node *)(clp_pixel_point_root->get_clp_first()),
      ii         = 0L;
       NULL != clp_point;
       clp_point = (pixel_point_node *)clp_point->get_clp_next(), ++ii) {
    if ((NULL == clp_point->get_clp_next_point()) &&
        (NULL == clp_point->get_clp_previous_point())) {
      if (fprintf(fp, "%d %d\n", clp_point->get_i32_xp(),
                  clp_point->get_i32_yp()) < 0) {
        pri_funct_err_bttvr("Error : fprintf(%d %d) returns minus",
                            clp_point->get_i32_xp(), clp_point->get_i32_yp());
        fclose(fp);
        return NG;
      }
      ++i32_not_include_line;
    }
  }
  /* 独立ポイント数保存 */
  if (fprintf(fp, "# not include is %d\n", i32_not_include_line) < 0) {
    pri_funct_err_bttvr("Error : fprintf(# not include is %d) returns minus",
                        i32_not_include_line);
    fclose(fp);
    return NG;
  }

  /* ファイル閉じる */
  fclose(fp);

  return OK;
}

/********************************************************************/

int pixel_line_root::save_lines(const char *cp_fname) {
  FILE *fp;
  int32_t ii;
  pixel_line_node *clp_line;

  /* ファイル開く */
  fp = fopen(cp_fname, "w");
  if (NULL == fp) {
    pri_funct_err_bttvr("Error : fopen(%s,w) returns NULL", cp_fname);
    return NG;
  }

  /* グループ(ライン)数保存 */
  if (fprintf(fp, "# lines count %d\n", this->get_i32_count()) < 0) {
    pri_funct_err_bttvr("Error : fprintf(# group count) returns minus");
    fclose(fp);
    return NG;
  }

  /* データ保存 */
  for (clp_line = (pixel_line_node *)this->get_clp_first(), ii = 0;
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    /* グループ(ライン)番号保存 */
    if (fprintf(fp, "# line number %d  and points count %d\n", ii,
                clp_line->get_i32_point_count()) < 0) {
      pri_funct_err_bttvr("Error : fprintf(# group number %d) returns minus",
                          ii);
      fclose(fp);
      return NG;
    }

    /* ポイント保存 */
    if (OK != clp_line->save_line(fp)) {
      pri_funct_err_bttvr("Error : clp_line->save_line() returns NG", ii);
      fclose(fp);
      return NG;
    }

    if (NULL != clp_line->get_clp_next()) {
      /* ２行の改行(for gnuplot)保存 */
      if (fprintf(fp, "\n\n") < 0) {
        pri_funct_err_bttvr("Error : fprintf(LF,LF) returns minus");
        fclose(fp);
        return NG;
      }
    }
  }

  /* ファイル閉じる */
  fclose(fp);

  return OK;
}

/********************************************************************/

int pixel_line_root::save_one_point(const char *cp_fname) {
  FILE *fp;
  int32_t ii;
  pixel_line_node *clp_line;

  /* ファイル開く */
  fp = fopen(cp_fname, "w");
  if (NULL == fp) {
    pri_funct_err_bttvr("Error : fopen(%s,w) returns NULL", cp_fname);
    return NG;
  }

  /* グループ(ライン)数保存 */
  if (fprintf(fp, "# line count %d\n", this->get_i32_count()) < 0) {
    pri_funct_err_bttvr("Error : fprintf(# group count) returns minus");
    fclose(fp);
    return NG;
  }

  /* データ保存 */
  for (clp_line = (pixel_line_node *)this->get_clp_first(), ii = 0;
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    /* グループ(ライン)番号保存 */
    if (fprintf(fp, "# line number %d ... start point\n", ii) < 0) {
      pri_funct_err_bttvr("Error : fprintf(# group number %d) returns minus",
                          ii);
      fclose(fp);
      return NG;
    }

    /* ポイント保存 */
    if (OK != clp_line->save_one_point(fp)) {
      pri_funct_err_bttvr("Error : clp_line->save_one_point() returns NG", ii);
      fclose(fp);
      return NG;
    }
  }

  /* ファイル閉じる */
  fclose(fp);

  return OK;
}

int pixel_line_root::save_middle_point(const char *cp_fname) {
  FILE *fp;
  int32_t ii;
  pixel_line_node *clp_line;

  /* ファイル開く */
  fp = fopen(cp_fname, "w");
  if (NULL == fp) {
    pri_funct_err_bttvr("Error : fopen(%s,w) returns NULL", cp_fname);
    return NG;
  }

  /* グループ(ライン)数保存 */
  if (fprintf(fp, "# line count %d\n", this->get_i32_count()) < 0) {
    pri_funct_err_bttvr("Error : fprintf(# group count) returns minus");
    fclose(fp);
    return NG;
  }

  /* データ保存 */
  for (clp_line = (pixel_line_node *)this->get_clp_first(), ii = 0;
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    /* グループ(ライン)番号保存 */
    if (fprintf(fp, "# line number %d ... start point\n", ii) < 0) {
      pri_funct_err_bttvr("Error : fprintf(# group number %d) returns minus",
                          ii);
      fclose(fp);
      return NG;
    }

    /* ポイント保存 */
    if (OK != clp_line->save_middle_point(fp)) {
      pri_funct_err_bttvr("Error : clp_line->save_middle_point() returns NG",
                          ii);
      fclose(fp);
      return NG;
    }
  }

  /* ファイル閉じる */
  fclose(fp);

  return OK;
}

int pixel_line_root::save_another_point(const char *cp_fname) {
  FILE *fp;
  int32_t ii;
  pixel_line_node *clp_line;

  /* ファイル開く */
  fp = fopen(cp_fname, "w");
  if (NULL == fp) {
    pri_funct_err_bttvr("Error : fopen(%s,w) returns NULL", cp_fname);
    return NG;
  }

  /* グループ(ライン)数保存 */
  if (fprintf(fp, "# line count %d\n", this->get_i32_count()) < 0) {
    pri_funct_err_bttvr("Error : fprintf(# group count) returns minus");
    fclose(fp);
    return NG;
  }

  /* データ保存 */
  for (clp_line = (pixel_line_node *)this->get_clp_first(), ii = 0;
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    /* グループ(ライン)番号保存 */
    if (fprintf(fp, "# line number %d ... end point\n", ii) < 0) {
      pri_funct_err_bttvr("Error : fprintf(# group number %d) returns minus",
                          ii);
      fclose(fp);
      return NG;
    }

    /* ポイント保存 */
    if (OK != clp_line->save_another_point(fp)) {
      pri_funct_err_bttvr("Error : clp_line->save_another_point() returns NG",
                          ii);
      fclose(fp);
      return NG;
    }
  }

  /* ファイル閉じる */
  fclose(fp);

  return OK;
}

#include "igs_line_blur.h"  // "pri.h" "pixel_line_root.h"

int pixel_line_root::save_expand_lines(const char *cp_fname) {
  FILE *fp;
  int32_t ii;
  pixel_line_node *clp_line;

  /* ファイル開く */
  fp = fopen(cp_fname, "w");
  if (NULL == fp) {
    pri_funct_err_bttvr("Error : fopen(%s,w) returns NULL", cp_fname);
    return NG;
  }

  /* グループ(ライン)数保存 */
  if (fprintf(fp, "# lines count %d\n", this->get_i32_count()) < 0) {
    pri_funct_err_bttvr("Error : fprintf(# group count) returns minus");
    fclose(fp);
    return NG;
  }

  /* データ保存 */
  for (clp_line = (pixel_line_node *)this->get_clp_first(), ii = 0;
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    /* グループ(ライン)番号保存 */
    if (fprintf(fp, "# line number %d  and points count %d\n", ii,
                clp_line->get_i32_point_count()) < 0) {
      pri_funct_err_bttvr("Error : fprintf(# group number %d) returns minus",
                          ii);
      fclose(fp);
      return NG;
    }

    /* ポイント保存 */
    if (OK != clp_line->save_expand_line(fp)) {
      pri_funct_err_bttvr("Error : clp_line->save_line() returns NG", ii);
      fclose(fp);
      return NG;
    }

    if (NULL != clp_line->get_clp_next()) {
      /* ２行の改行(for gnuplot)保存 */
      if (fprintf(fp, "\n\n") < 0) {
        pri_funct_err_bttvr("Error : fprintf(LF,LF) returns minus");
        fclose(fp);
        return NG;
      }
    }
  }

  /* ファイル閉じる */
  fclose(fp);

  return OK;
}

/********************************************************************/

int pixel_line_root::save_one_expand_point(const char *cp_fname) {
  FILE *fp;
  int32_t ii;
  pixel_line_node *clp_line;

  /* ファイル開く */
  fp = fopen(cp_fname, "w");
  if (NULL == fp) {
    pri_funct_err_bttvr("Error : fopen(%s,w) returns NULL", cp_fname);
    return NG;
  }

  /* グループ(ライン)数保存 */
  if (fprintf(fp, "# line count %d\n", this->get_i32_count()) < 0) {
    pri_funct_err_bttvr("Error : fprintf(# group count) returns minus");
    fclose(fp);
    return NG;
  }

  /* データ保存 */
  for (clp_line = (pixel_line_node *)this->get_clp_first(), ii = 0;
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    /* グループ(ライン)番号保存 */
    if (fprintf(fp, "# line number %d ... one expand point\n", ii) < 0) {
      pri_funct_err_bttvr("Error : fprintf(# group number %d) returns minus",
                          ii);
      fclose(fp);
      return NG;
    }

    /* ポイント保存 */
    if (OK != clp_line->save_one_expand_point(fp)) {
      pri_funct_err_bttvr(
          "Error : clp_line->save_one_expand_point() returns NG", ii);
      fclose(fp);
      return NG;
    }
  }

  /* ファイル閉じる */
  fclose(fp);

  return OK;
}

int pixel_line_root::save_another_expand_point(const char *cp_fname) {
  FILE *fp;
  int32_t ii;
  pixel_line_node *clp_line;

  /* ファイル開く */
  fp = fopen(cp_fname, "w");
  if (NULL == fp) {
    pri_funct_err_bttvr("Error : fopen(%s,w) returns NULL", cp_fname);
    return NG;
  }

  /* グループ(ライン)数保存 */
  if (fprintf(fp, "# line count %d\n", this->get_i32_count()) < 0) {
    pri_funct_err_bttvr("Error : fprintf(# group count) returns minus");
    fclose(fp);
    return NG;
  }

  /* データ保存 */
  for (clp_line = (pixel_line_node *)this->get_clp_first(), ii = 0;
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    /* グループ(ライン)番号保存 */
    if (fprintf(fp, "# line number %d ... another expand point\n", ii) < 0) {
      pri_funct_err_bttvr("Error : fprintf(# group number %d) returns minus",
                          ii);
      fclose(fp);
      return NG;
    }

    /* ポイント保存 */
    if (OK != clp_line->save_another_expand_point(fp)) {
      pri_funct_err_bttvr(
          "Error : clp_line->save_another_expand_point() returns NG", ii);
      fclose(fp);
      return NG;
    }
  }

  /* ファイル閉じる */
  fclose(fp);

  return OK;
}

#include "igs_line_blur.h"  // "pri.h" "pixel_line_root.h"

int pixel_line_root::save_expand_vector(const char *cp_fname) {
  FILE *fp;
  int32_t ii;
  pixel_line_node *clp_line;

  /* ファイル開く */
  fp = fopen(cp_fname, "w");
  if (NULL == fp) {
    pri_funct_err_bttvr("Error : fopen(%s,w) returns NULL", cp_fname);
    return NG;
  }

  /* グループ(ライン)数保存 */
  if (fprintf(fp, "# lines count %d\n", this->get_i32_count()) < 0) {
    pri_funct_err_bttvr("Error : fprintf(# group count) returns minus");
    fclose(fp);
    return NG;
  }

  /* データ保存 */
  for (clp_line = (pixel_line_node *)this->get_clp_first(), ii = 0;
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    /* グループ(ライン)番号保存 */
    if (fprintf(fp, "# line number %d  and points count %d\n", ii,
                clp_line->get_i32_point_count()) < 0) {
      pri_funct_err_bttvr("Error : fprintf(# line number %d) returns minus",
                          ii);
      fclose(fp);
      return NG;
    }

    /* ポイント保存 */
    if (OK != clp_line->save_expand_vector(fp)) {
      pri_funct_err_bttvr("Error : clp_line->save_expand_vector() returns NG");
      fclose(fp);
      return NG;
    }
  }

  /* ファイル閉じる */
  fclose(fp);

  return OK;
}

#ifndef _pixel_select_curve_blur_h_
#define _pixel_select_curve_blur_h_

#include <stdio.h>

#include "igs_line_blur.h"  // "list_root.h" "calculator_geometry.h" "pixel_point_root.h" "pixel_line_root.h"

class pixel_select_curve_blur_node final : public list_node {
public:
  pixel_select_curve_blur_node(void) {
    this->clp_line           = NULL;
    this->clp_start_point    = NULL;
    this->clp_near_point     = NULL;
    this->i32_near_point_pos = 0;
    this->d_length           = -1.0;
    this->i_reverse_sw       = false;
  }
  void copy(pixel_select_curve_blur_node *clp) {
    this->clp_line           = clp->clp_line;
    this->clp_start_point    = clp->clp_start_point;
    this->clp_near_point     = clp->clp_near_point;
    this->i32_near_point_pos = clp->i32_near_point_pos;
    this->d_length           = clp->d_length;
    this->i_reverse_sw       = clp->i_reverse_sw;
  }
  pixel_line_node *clp_line;
  pixel_point_node *clp_start_point;
  pixel_point_node *clp_near_point;
  int32_t i32_near_point_pos;
  double d_length;
  bool i_reverse_sw;

private:
};

class pixel_select_curve_blur_root final : public list_root {
public:
  pixel_select_curve_blur_root(void) {
    this->_i_mv_sw = false;
    this->_i_cv_sw = false;
    this->_i_pv_sw = false;

    this->_i32_count_max = 4;
    this->_d_length_max  = 160;
  }
  ~pixel_select_curve_blur_root(void) { this->mem_free(); }
  void set_i_mv_sw(bool sw) { this->_i_mv_sw = sw; }
  void set_i_cv_sw(bool sw) { this->_i_cv_sw = sw; }
  void set_i_pv_sw(bool sw) { this->_i_pv_sw = sw; }

  int32_t get_i32_count_max(void) { return this->_i32_count_max; }
  void set_i32_count_max(int32_t ii) { this->_i32_count_max = ii; }
  double get_d_length_max(void) { return this->_d_length_max; }
  void set_d_length_max(double dd) { this->_d_length_max = dd; }

  void exec(double d_xp, double d_yp, pixel_line_root *clp_pixel_line_root,
            int32_t i32_blur_count, double d_effect_length_radius);
  /******void exec( double d_xp, double d_yp, pixel_line_node *clp_line_first,
   * int32_t i32_count, int32_t i32_blur_count, double d_effect_length_radius
   * );******/
  int get_line(int32_t i32_blur_count, double *dp_xv, double *dp_yv);

  int save(double d_xp, double d_yp, int32_t i32_blur_count,
           const char *cp_fname);

  void mem_free(void);

private:
  bool _i_mv_sw, _i_cv_sw, _i_pv_sw;

  int32_t _i32_count_max;
  double _d_length_max;

  calculator_geometry _cl_cal_geom;

  pixel_select_curve_blur_node *_append(
      pixel_select_curve_blur_node *clp_previous);
  void _remove(pixel_select_curve_blur_node *clp_old);
  int _sort_append(pixel_select_curve_blur_node *clp_src);

  pixel_point_node *_get_next_points(pixel_point_node *clp_point,
                                     int32_t i32_count);
  pixel_point_node *_get_prev_points(pixel_point_node *clp_point,
                                     int32_t i32_count);

  double _get_line_accum_count(pixel_select_curve_blur_node *clp_select,
                               int32_t i32_blur_count);
};

#endif              /* !_pixel_select_curve_blur_h_ */
#include <stdlib.h> /* exit() */
#include <math.h>   /* M_PI */

/* WindowsではM_PIが見つからない */
#if defined _MSC_VER
#define M_PI 3.14159265358979323846
#endif

#include <assert.h>         /* assert() */
#include "igs_line_blur.h"  // "pri.h" "pixel_line_root.h" "pixel_select_curve_blur.h"

pixel_select_curve_blur_node *pixel_select_curve_blur_root::_append(
    pixel_select_curve_blur_node *clp_previous) {
  pixel_select_curve_blur_node *clp_new;

  clp_new = new pixel_select_curve_blur_node;
  if (NULL == clp_new) {
    pri_funct_err_bttvr(
        "Error : 'new pixel_select_curve_blur_node' returns NULL.");
    return NULL;
  }
  clp_new = (pixel_select_curve_blur_node *)this->push(clp_previous, clp_new);

  return clp_new;
}

void pixel_select_curve_blur_root::_remove(
    pixel_select_curve_blur_node *clp_old) {
  assert(NULL != clp_old); /* あってはならないプログラムバグのチェック */
  this->pop(clp_old);
  delete clp_old;
}

void pixel_select_curve_blur_root::mem_free(void) {
  int32_t ii;

  if (NULL != this->get_clp_last()) {
    if (this->_i_mv_sw) {
      pri_funct_msg_ttvr("pixel_select_curve_blur_root::mem_free()");
    }

    ii = 0;
    while (NULL != this->get_clp_last()) {
      this->_remove((pixel_select_curve_blur_node *)this->get_clp_last());
      ++ii;
    }
    if (this->_i_pv_sw) {
      pri_funct_msg_ttvr("free select curve blur node %d", ii);
    }
  }
}

/********************************************************************/

/* lengthの値を見て、距離の近い(小さい値)順になるように追加する
        すべての選択リストより小さいなら先頭
        すべての選択リストより大きいなら最後に追加
        値から選択リストの間に入るならそこに追加
*/
int pixel_select_curve_blur_root::_sort_append(
    pixel_select_curve_blur_node *clp_src) {
  pixel_select_curve_blur_node *clp_prev;
  int32_t ii;

  assert(0.0 < clp_src->d_length);

  /* リストの最後から距離を比較し、より小さい場所 */
  clp_prev = (pixel_select_curve_blur_node *)this->get_clp_last();
  for (ii      = 0; NULL != clp_prev; ++ii,
      clp_prev = (pixel_select_curve_blur_node *)clp_prev->get_clp_previous()) {
    assert(ii < this->get_i32_count());
    if (clp_prev->d_length < clp_src->d_length) {
      break;
    }
  }

  /* 示した場所の後ろに追加 */
  clp_prev = this->_append(clp_prev);
  if (NULL == clp_prev) {
    pri_funct_err_bttvr("Error : this->_append(clp_prev) returns NULL.");
    return NG;
  }

  /* 情報をコピー */
  clp_prev->copy(clp_src);

  return OK;
}

/********************************************************************/
#define NOT_USE_PARAMETER_VAL (-10000.0)

/* 同方向で、一定の距離の範囲にある線とその位置を選択する */
void pixel_select_curve_blur_root::exec(double d_xp, double d_yp,
                                        pixel_line_root *clp_pixel_line_root,
                                        int32_t i32_blur_count,
                                        double d_effect_area_radius) {
  pixel_line_node *clp_line;
  int32_t ii, i32_pos;
  bool i_reverse_sw;
  pixel_point_node *clp_near_point, *clp_start_point;
  double d_length, d_radius, d_radius_1st;
  pixel_select_curve_blur_node cl_select;

  /* 選択リストをクリア */
  this->mem_free();

  /* 全体の bbox を見る */
  if ((d_xp <
       (clp_pixel_line_root->get_d_bbox_x_min() - d_effect_area_radius)) ||
      ((clp_pixel_line_root->get_d_bbox_x_max() + d_effect_area_radius) <
       d_xp) ||
      (d_yp <
       (clp_pixel_line_root->get_d_bbox_y_min() - d_effect_area_radius)) ||
      ((clp_pixel_line_root->get_d_bbox_y_max() + d_effect_area_radius) <
       d_yp)) {
    return;
  }

  d_radius_1st = NOT_USE_PARAMETER_VAL;

  /* 全ライン */
  ii = 0;
  for (clp_line = (pixel_line_node *)clp_pixel_line_root->get_clp_first();
       NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    /* 無限ループ? */
    assert(ii < clp_pixel_line_root->get_i32_count());
    /* 選択してない? */
    assert(NULL != clp_line->get_clp_link_middle());
    assert(NULL != clp_line->get_clp_link_one());
    assert(NULL != clp_line->get_clp_link_another());
    /* 線が短すぎ? */
    assert(clp_line->get_clp_link_middle() != clp_line->get_clp_link_one());
    assert(clp_line->get_clp_link_middle() != clp_line->get_clp_link_another());

    /* 各ラインの bbox を見る */
    if ((d_xp < (clp_line->get_d_bbox_x_min() - d_effect_area_radius)) ||
        ((clp_line->get_d_bbox_x_max() + d_effect_area_radius) < d_xp) ||
        (d_yp < (clp_line->get_d_bbox_y_min() - d_effect_area_radius)) ||
        ((clp_line->get_d_bbox_y_max() + d_effect_area_radius) < d_yp)) {
      continue;
    }

    /* ラインとの近点を探す */
    clp_line->get_near_point(d_xp, d_yp, &i32_pos, &clp_near_point, &d_length);

    /* 距離が遠すぎる */
    if (this->_d_length_max < d_length) {
      continue;
    }

    /* 線分の端で必要な長さがとれない
            画面の端の場合、問題あり!!!!!!
     */
    if ((i32_pos < (i32_blur_count / 2)) ||
        ((clp_line->get_i32_point_count() - 1 - i32_pos) <
         (i32_blur_count / 2))) {
      continue;
    }

    /* 逆方向スイッチoffしておく */
    i_reverse_sw = false;

    /* 対応する部分のスタート点 */
    clp_start_point =
        clp_line->get_prev_point_by_count(clp_near_point, i32_blur_count / 2);

    /* 近点からスタート点へのベクトル角度 */
    d_radius = this->_cl_cal_geom.get_d_radian(
        clp_start_point->get_d_xp_tgt() - clp_near_point->get_d_xp_tgt(),
        clp_start_point->get_d_yp_tgt() - clp_near_point->get_d_yp_tgt());

    /* 1番始めは線分の方向を記憶しておく */
    if (NOT_USE_PARAMETER_VAL == d_radius_1st) {
      d_radius_1st = d_radius;
    }
    /* 2番目以後は記憶した方向との比較し
       線の対応部分の、方向と開始点を最設定 */
    else {
      /* 1番目の方向との角度差を出す */
      if (d_radius < d_radius_1st) {
        d_radius += M_PI * 2.0;
      }
      d_radius -= d_radius_1st;
      /* 反対方向なら、逆方向スイッチをonし、スタート点を最設定 */
      if ((M_PI / 2.0 < d_radius) && (d_radius < M_PI * 3.0 / 2.0)) {
        clp_start_point = clp_line->get_next_point_by_count(clp_near_point,
                                                            i32_blur_count / 2);
        i_reverse_sw = true;
      }
    }

    /* 選択リストに追加(近い順に) */
    cl_select.clp_line           = clp_line;
    cl_select.clp_start_point    = clp_start_point;
    cl_select.clp_near_point     = clp_near_point;
    cl_select.i32_near_point_pos = i32_pos;
    cl_select.d_length           = d_length;
    cl_select.i_reverse_sw       = i_reverse_sw;
    /* 登録 */
    this->_sort_append(&cl_select);
  }
  /*****if ((NULL != this->get_clp_first()) && (510.0 < d_xp) && (100.0 < d_yp))
{
//if (NULL != this->get_clp_first()) {
this->save( d_xp, d_yp, i32_blur_count,"tmp19_select.txt" ); exit(1); }
******/
}

/********************************************************************/

double pixel_select_curve_blur_root::_get_line_accum_count(
    pixel_select_curve_blur_node *clp_select, int32_t i32_blur_count) {
  int32_t i32_pos, i32_count;
  double d_ratio;

  i32_pos   = clp_select->i32_near_point_pos - (i32_blur_count / 2);
  i32_count = clp_select->clp_line->get_i32_point_count() -
              (i32_blur_count / 2) - (i32_blur_count / 2);

  assert(0 <= i32_pos);
  assert(0 < i32_count);
  assert(i32_pos < i32_count);

  /**if (i32_pos < (i32_count/2)) {
          d_ratio = (double)(i32_pos+1) / (i32_count/2);
  } else {
          d_ratio = (double)(i32_count-i32_pos) / (i32_count/2);
  }**/

  /* ゼロから１の間の値(ゼロと１はだめ) */
  d_ratio = ((double)i32_pos + 0.5) / (double)i32_count;
  /* sinカーブ
          --> 0より大きく1より小さい(d_ratio)
          --> 0-(PI/2)より大きく、(2*PI-(PI/2))より小さい
          --> sin()
          --> -1より大きく1より小さい
          -->  0より大きく2より小さい */
  d_ratio = 1.0 + sin(2.0 * M_PI * d_ratio - M_PI / 2);

  assert(0.0 < d_ratio);

  return d_ratio;
}

/* 距離の近い順(lengthの小さい値)に指定個数分のベクトルを合成して返す
        先頭から
        this->_i32_count_max数個分
        のベクトルを合成する
*/
int pixel_select_curve_blur_root::get_line(int32_t i32_blur_count,
                                           double *dp_xv, double *dp_yv) {
  int32_t ii, jj, i32_count;
  double d_xp, d_yp;
  double d_ratio, d_accum;
  pixel_select_curve_blur_node *clp_select;
  pixel_point_node *clp_point;

  for (ii = 0; ii < i32_blur_count; ++ii) {
    dp_xv[ii] = 0.0;
    dp_yv[ii] = 0.0;
  }

  i32_count = 0;
  d_accum   = 0.0;

  clp_select = (pixel_select_curve_blur_node *)this->get_clp_first();
  for (ii = 0; (NULL != clp_select) && (ii < this->_i32_count_max); ++ii,
      ++i32_count,
      clp_select = (pixel_select_curve_blur_node *)clp_select->get_clp_next()) {
    assert(ii < this->get_i32_count());
    assert(NULL != clp_select->clp_line);
    assert(NULL != clp_select->clp_start_point);
    assert(NULL != clp_select->clp_near_point);

    d_ratio = this->_get_line_accum_count(clp_select, i32_blur_count);
    d_accum += d_ratio;

    d_xp      = clp_select->clp_near_point->get_d_xp_tgt();
    d_yp      = clp_select->clp_near_point->get_d_yp_tgt();
    clp_point = clp_select->clp_start_point;

    /* 線分合成(いきなりの変化をへらす) */
    if (!clp_select->i_reverse_sw) {
      for (jj = 0; jj < i32_blur_count; ++jj) {
        dp_xv[jj] += (clp_point->get_d_xp_tgt() - d_xp) * d_ratio;
        dp_yv[jj] += (clp_point->get_d_yp_tgt() - d_yp) * d_ratio;
        clp_point = clp_point->get_clp_next_point();
      }
    } else {
      for (jj = 0; jj < i32_blur_count; ++jj) {
        dp_xv[jj] += (clp_point->get_d_xp_tgt() - d_xp) * d_ratio;
        dp_yv[jj] += (clp_point->get_d_yp_tgt() - d_yp) * d_ratio;
        clp_point = clp_point->get_clp_previous_point();
      }
    }
  }

  if (i32_count <= 0) {
    return NG;
  }

  assert(0.0 < d_accum);
  for (ii = 0; ii < i32_blur_count; ++ii) {
    dp_xv[ii] /= d_accum;
    dp_yv[ii] /= d_accum;
  }

  return OK;
}

#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "pixel_select_curve_blur.h"

int pixel_select_curve_blur_root::save(double d_xp, double d_yp,
                                       int32_t i32_blur_count,
                                       const char *cp_fname) {
  FILE *fp;
  int32_t ii, jj;
  pixel_select_curve_blur_node *clp_loop;
  pixel_point_node *clp_point;

  /* ファイル開く */
  fp = fopen(cp_fname, "w");
  if (NULL == fp) {
    pri_funct_err_bttvr("Error : fopen(%s,w) returns NULL", cp_fname);
    return NG;
  }

  /* ライン長さ(ポイント数)保存 */
  if (fprintf(fp, "# blur point count %d\n", i32_blur_count) < 0) {
    pri_funct_err_bttvr("Error : fprintf(# blur point count) returns minus");
    fclose(fp);
    return NG;
  }
  /* 選択数保存 */
  if (fprintf(fp, "# select count %d in %d\n", this->_i32_count_max,
              this->get_i32_count()) < 0) {
    pri_funct_err_bttvr("Error : fprintf(# select count) returns minus");
    fclose(fp);
    return NG;
  }

  clp_loop = (pixel_select_curve_blur_node *)this->get_clp_first();
  for (ii      = 0; (NULL != clp_loop) && (ii < this->_i32_count_max); ++ii,
      clp_loop = (pixel_select_curve_blur_node *)clp_loop->get_clp_next()) {
    assert(ii < this->get_i32_count());
    assert(NULL != clp_loop);
    assert(NULL != clp_loop->clp_line);
    assert(NULL != clp_loop->clp_start_point);
    assert(NULL != clp_loop->clp_near_point);

    /* グループ(ライン)番号保存 */
    if (fprintf(fp, "# selct number %d : reverse sw is %s\n", ii,
                (!clp_loop->i_reverse_sw ? "off" : "on")) < 0) {
      pri_funct_err_bttvr("Error : fprintf(# line number %d) returns minus",
                          ii);
      fclose(fp);
      return NG;
    }

    clp_point = clp_loop->clp_near_point;
    /* ピクセル位置から近点位置保存 */
    if (fprintf(fp, "%g %g\n", clp_point->get_d_xp_tgt(),
                clp_point->get_d_yp_tgt()) < 0) {
      pri_funct_err_bttvr("Error : fprintf(near point) returns minus");
      fclose(fp);
      return NG;
    }
    if (fprintf(fp, "%g %g\n\n\n", d_xp, d_yp) < 0) {
      pri_funct_err_bttvr("Error : fprintf(pixel point) returns minus");
      fclose(fp);
      return NG;
    }

    clp_point = clp_loop->clp_start_point;
    if (!clp_loop->i_reverse_sw) {
      for (jj = 0; jj < i32_blur_count; ++jj) {
        /* グループ(ライン)番号保存 */
        if (fprintf(fp, "%g %g\n", clp_point->get_d_xp_tgt(),
                    clp_point->get_d_yp_tgt()) < 0) {
          pri_funct_err_bttvr("Error : fprintf(x,y) returns minus");
          fclose(fp);
          return NG;
        }
        clp_point = clp_point->get_clp_next_point();
      }
    } else {
      for (jj = 0; jj < i32_blur_count; ++jj) {
        /* グループ(ライン)番号保存 */
        if (fprintf(fp, "%g %g\n", clp_point->get_d_xp_tgt(),
                    clp_point->get_d_yp_tgt()) < 0) {
          pri_funct_err_bttvr("Error : fprintf(x,y) returns minus");
          fclose(fp);
          return NG;
        }
        clp_point = clp_point->get_clp_previous_point();
      }
    }
    if (fprintf(fp, "\n\n") < 0) {
      pri_funct_err_bttvr("Error : fprintf(EnterEnter) returns minus");
      fclose(fp);
      return NG;
    }
  }

  /* ファイル閉じる */
  fclose(fp);

  return OK;
}

#ifndef __thinnest_ui16_image_h__
#define __thinnest_ui16_image_h__

#include <stdio.h>

/* Windowsではstdint.hが見つからない */
#if defined _MSC_VER
typedef int int32_t;
typedef unsigned short uint16_t;
#else
#include <stdint.h> /* for int32_t, uint16_t */
#endif

#ifndef UINT16_MAX
#define UINT16_MAX (65535)
#endif

#ifndef OK
#define OK (0)
#endif
#ifndef NG
#define NG (-1)
#endif

class thinnest_ui16_image {
public:
  thinnest_ui16_image() {
    this->_i_mv_sw = false;
    this->_i_pv_sw = false;
    this->_i_cv_sw = false;

    this->_i32_xs              = 0;
    this->_i32_ys              = 0;
    this->_i32_xd              = 1;
    this->_i32_yd              = 1;
    this->_ui16_threshold      = (uint16_t)0x4000; /* =64*256 */
    this->_i32_exec_loop_count = 100;

    this->_ui16p_channel[0] = NULL;
    this->_ui16p_channel[1] = NULL;
    this->memory_free_this_ = NULL;
  }
  ~thinnest_ui16_image() { this->mem_free(); }

  /* パラメータ設定 */
  void set_i_mv_sw(bool ii) { this->_i_mv_sw = ii; }
  void set_i_pv_sw(bool ii) { this->_i_pv_sw = ii; }
  void set_i_cv_sw(bool ii) { this->_i_cv_sw = ii; }

  void set_i32_xs(int32_t ii) { this->_i32_xs = ii; }
  void set_i32_ys(int32_t ii) { this->_i32_ys = ii; }
  void set_i32_xd(int32_t ii) { this->_i32_xd = ii; }
  void set_i32_yd(int32_t ii) { this->_i32_yd = ii; }
  void set_ui16_threshold(uint16_t ii) { this->_ui16_threshold = ii; }
  void set_i32_exec_loop_count(int32_t ii) { this->_i32_exec_loop_count = ii; }

  /* パラメータを得る */
  int32_t get_i32_xs(void) { return this->_i32_xs; }
  int32_t get_i32_ys(void) { return this->_i32_ys; }
  int32_t get_i32_xd(void) { return this->_i32_xd; }
  int32_t get_i32_yd(void) { return this->_i32_yd; }
  uint16_t get_ui16_threshold(void) { return this->_ui16_threshold; }
  int32_t get_i32_exec_loop_count(void) { return this->_i32_exec_loop_count; }

  /* メモリ確保 */
  int mem_alloc(void);

  /* 実行 */
  int32_t exec01_fill_noise_pixel(void);
  void exec02_scale_add_edge_pixel(void);
  void exec03_scale_liner(void);
  void exec04_bw(void);
  int exec05_thin(void);

  /* メモリへのポインターを得る */
  uint16_t *get_ui16p_src_channel(void) { return this->_ui16p_channel[0]; }
  uint16_t *get_ui16p_tgt_channel(void) { return this->_ui16p_channel[1]; }

  /* メモリ開放 */
  void mem_free(void);

private:
  bool _i_mv_sw; /* Method    Verbose */
  bool _i_pv_sw; /* Parameter Verbose */
  bool _i_cv_sw; /* Counter   Verbose */

  int32_t _i32_xs, _i32_ys,     /* Size */
      _i32_xd, _i32_yd;         /* sub Divide */
  uint16_t _ui16_threshold;     /* bw_by_threshold */
  int32_t _i32_exec_loop_count; /* thinnest root */

  uint16_t *memory_free_this_;
  uint16_t *_ui16p_channel[2];

  void _swap_channel(void) {
    uint16_t *_ui16p_tmp;

    _ui16p_tmp              = this->_ui16p_channel[0];
    this->_ui16p_channel[0] = this->_ui16p_channel[1];
    this->_ui16p_channel[1] = _ui16p_tmp;
  }

  int32_t _exec01_fill_noise_pixel_scanline(uint16_t *ui16p_y1,
                                            uint16_t *ui16p_y2,
                                            uint16_t *ui16p_y3);
  int32_t _exec01_fill_noise_pixel_pixel(
      uint16_t *ui16p_x1a, uint16_t *ui16p_x1b, uint16_t *ui16p_x1c,
      uint16_t *ui16p_x2a, uint16_t *ui16p_x2b, uint16_t *ui16p_x2c,
      uint16_t *ui16p_x3a, uint16_t *ui16p_x3b, uint16_t *ui16p_x3c);

  int32_t _one_side_thinner(void);
  int32_t _one_side_thinner_scanline(uint16_t *ui16p_src_y1,
                                     uint16_t *ui16p_src_y2,
                                     uint16_t *ui16p_src_y3,
                                     uint16_t *ui16p_tgt);
  int32_t _one_side_thinner_pixel(
      uint16_t *ui16p_src_x1a, uint16_t *ui16p_src_x1b, uint16_t *ui16p_src_x1c,
      uint16_t *ui16p_src_x2a, uint16_t *ui16p_src_x2b, uint16_t *ui16p_src_x2c,
      uint16_t *ui16p_src_x3a, uint16_t *ui16p_src_x3b, uint16_t *ui16p_src_x3c,
      uint16_t *ui16p_tgt);

  void _rot90_by_clockwork(void);
};

#endif /* !__thinnest_ui16_image_h__ */

#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "thinnest_ui16_image.h"

/* 閾値より大きい白線上の独立点を潰す */
int32_t thinnest_ui16_image::exec01_fill_noise_pixel(void) {
  uint16_t *ui16p_y1, *ui16p_y2, *ui16p_y3;
  int32_t yy, i32_fill_count;

  /* 処理ごとのメッセージ */
  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("thinnest_ui16_image::exec01_fill_noise_pixel()");
  }
  /* カウントダウン表示始め */
  if (this->_i_cv_sw) {
    pri_funct_cv_start(this->_i32_ys);
  }

  /* 始めのスキャンライン位置(画像から外れているものはNULLをいれる) */
  ui16p_y1 = this->get_ui16p_src_channel() + this->_i32_xs;
  ui16p_y2 = this->get_ui16p_src_channel();
  ui16p_y3 = NULL;

  /* 画像を縦にループ */
  i32_fill_count = 0;
  for (yy = 0; yy < this->_i32_ys; ++yy) {
    /* カウントダウン表示中 */
    if (this->_i_cv_sw) {
      pri_funct_cv_run(yy);
    }

    /* スキャンライン(とその前後のスキャンライン)毎の処理 */
    i32_fill_count +=
        this->_exec01_fill_noise_pixel_scanline(ui16p_y1, ui16p_y2, ui16p_y3);

    /* 次のスキャンラインへ進める(画像から外れたらNULLをいれる) */
    ui16p_y3 = ui16p_y2;
    ui16p_y2 = ui16p_y1;
    if ((yy + 1) < (this->_i32_ys)) {
      ui16p_y1 += this->_i32_xs;
    } else
      ui16p_y1 = NULL;
  }

  /* カウントダウン表示終了 */
  if (this->_i_cv_sw) {
    pri_funct_cv_end();
  }

  /* 終了 */
  return i32_fill_count;
}

int32_t thinnest_ui16_image::_exec01_fill_noise_pixel_scanline(
    uint16_t *ui16p_y1, uint16_t *ui16p_y2, uint16_t *ui16p_y3) {
  uint16_t *ui16p_x1a, *ui16p_x1b, *ui16p_x1c, *ui16p_x2a, *ui16p_x2b,
      *ui16p_x2c, *ui16p_x3a, *ui16p_x3b, *ui16p_x3c;
  int32_t xx, i32_fill_count;

  /* 始めのピクセル位置(画像から外れているものはNULLをいれる) */
  ui16p_x1a = NULL;
  ui16p_x1b = NULL;
  ui16p_x1c = NULL;
  ui16p_x2a = NULL;
  ui16p_x2b = NULL;
  ui16p_x2c = NULL;
  ui16p_x3a = NULL;
  ui16p_x3b = NULL;
  ui16p_x3c = NULL;
  if (NULL != ui16p_y1) {
    ui16p_x1a = ui16p_y1 + 1;
    ui16p_x1b = ui16p_y1;
  }
  if (NULL != ui16p_y2) {
    ui16p_x2a = ui16p_y2 + 1;
    ui16p_x2b = ui16p_y2;
  }
  if (NULL != ui16p_y3) {
    ui16p_x3a = ui16p_y3 + 1;
    ui16p_x3b = ui16p_y3;
  }

  /* 画像を横にループ */
  i32_fill_count = 0;
  for (xx = 0; xx < this->_i32_xs; ++xx) {
    i32_fill_count += this->_exec01_fill_noise_pixel_pixel(
        ui16p_x1a, ui16p_x1b, ui16p_x1c, ui16p_x2a, ui16p_x2b, ui16p_x2c,
        ui16p_x3a, ui16p_x3b, ui16p_x3c);

    /* 次のピクセルへ進める(画像から外れたらNULLをいれる) */
    ui16p_x1c = ui16p_x1b;
    ui16p_x1b = ui16p_x1a;
    ui16p_x2c = ui16p_x2b;
    ui16p_x2b = ui16p_x2a;
    ui16p_x3c = ui16p_x3b;
    ui16p_x3b = ui16p_x3a;
    if ((xx + 1) < (this->_i32_xs)) {
      if (NULL != (ui16p_x1a)) ++ui16p_x1a;
      if (NULL != (ui16p_x2a)) ++ui16p_x2a;
      if (NULL != (ui16p_x3a)) ++ui16p_x3a;
    } else {
      ui16p_x1a = NULL;
      ui16p_x2a = NULL;
      ui16p_x3a = NULL;
    }
  }

  /* 終了 */
  return i32_fill_count;
}

int32_t thinnest_ui16_image::_exec01_fill_noise_pixel_pixel(
    uint16_t *ui16p_x1a, uint16_t *ui16p_x1b, uint16_t *ui16p_x1c,
    uint16_t *ui16p_x2a, uint16_t *ui16p_x2b, uint16_t *ui16p_x2c,
    uint16_t *ui16p_x3a, uint16_t *ui16p_x3b, uint16_t *ui16p_x3c) {
  int32_t i32_fill_count;
  int32_t i32_exist_count, i32_eqgt_count;

  /* ui16p_x2bがNULLであってはならない */
  assert(NULL != ui16p_x2b);

  /* パラメータの初期値 */
  i32_fill_count  = 0;
  i32_eqgt_count  = 0L;
  i32_exist_count = 0L;

  /* 時計回り(右回り)に見ていく */
  /* 始め */
  if (NULL != (ui16p_x1a)) {
    if (this->_ui16_threshold <= (*ui16p_x1a)) {
      ++i32_eqgt_count;
    }
    ++i32_exist_count;
  }
  /* 次 ... */
  if (NULL != (ui16p_x1b)) {
    if (this->_ui16_threshold <= (*ui16p_x1b)) {
      ++i32_eqgt_count;
    }
    ++i32_exist_count;
  }
  if (NULL != (ui16p_x1c)) {
    if (this->_ui16_threshold <= (*ui16p_x1c)) {
      ++i32_eqgt_count;
    }
    ++i32_exist_count;
  }
  if (NULL != (ui16p_x2c)) {
    if (this->_ui16_threshold <= (*ui16p_x2c)) {
      ++i32_eqgt_count;
    }
    ++i32_exist_count;
  }
  if (NULL != (ui16p_x3c)) {
    if (this->_ui16_threshold <= (*ui16p_x3c)) {
      ++i32_eqgt_count;
    }
    ++i32_exist_count;
  }
  if (NULL != (ui16p_x3b)) {
    if (this->_ui16_threshold <= (*ui16p_x3b)) {
      ++i32_eqgt_count;
    }
    ++i32_exist_count;
  }
  if (NULL != (ui16p_x3a)) {
    if (this->_ui16_threshold <= (*ui16p_x3a)) {
      ++i32_eqgt_count;
    }
    ++i32_exist_count;
  }
  if (NULL != (ui16p_x2a)) {
    if (this->_ui16_threshold <= (*ui16p_x2a)) {
      ++i32_eqgt_count;
    }
    ++i32_exist_count;
  }

  /* 回りが全部閾値より大きかったら白線上の独立点なので潰す */
  if (i32_exist_count == i32_eqgt_count) {
    *ui16p_x2b = UINT16_MAX;
    ++i32_fill_count;
  }

  /* 終了 */
  return i32_fill_count;
}
#include "igs_line_blur.h"  // "pri.h" "thinnest_ui16_image.h"

void thinnest_ui16_image::exec02_scale_add_edge_pixel(void) {
  int32_t xx, yy, i32_tmp1, i32_tmp2;
  uint16_t *ui16p_src, *ui16p_src1, *ui16p_src2, *ui16p_tgt, *ui16p_tgt1,
      *ui16p_tgt2;

  /* 画像サイズが足りないか、分割数が指定されていない、ときは実行キャンセル */
  if ((this->_i32_xs < 2) || (this->_i32_ys < 2) || (this->_i32_xd < 2) ||
      (this->_i32_yd < 2)) {
    return;
  }

  /* 処理ごとのメッセージ */
  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("thinnest_ui16_image::exec02_scale_add_edge_pixel()");
  }
  /* カウントダウン表示始め */
  if (this->_i_cv_sw) {
    pri_funct_cv_start(this->_i32_ys);
  }

  ui16p_src  = this->get_ui16p_src_channel();
  ui16p_tgt1 = this->get_ui16p_tgt_channel() + (this->_i32_xs + 2) + 1;

  /* 縁以外の部分のコピー */
  for (yy = 0; yy < this->_i32_ys; ++yy) {
    /* カウントダウン表示中 */
    if (this->_i_cv_sw) {
      pri_funct_cv_run(yy);
    }

    ui16p_tgt2 = ui16p_tgt1;
    for (xx = 0; xx < this->_i32_xs; ++xx) {
      *ui16p_tgt2 = *ui16p_src;
      ++ui16p_src;
      ++ui16p_tgt2;
    }
    ui16p_tgt1 += (this->_i32_xs + 2);
  }
  /* カウントダウン表示終了 */
  if (this->_i_cv_sw) {
    pri_funct_cv_end();
  }

  /* 縁の上部分 */
  ui16p_src1 = this->get_ui16p_src_channel();
  ui16p_src2 = this->get_ui16p_src_channel() + this->_i32_xs;
  ui16p_tgt  = this->get_ui16p_tgt_channel() + 1;
  for (xx = 0; xx < this->_i32_xs; ++xx) {
    i32_tmp1 = (int32_t)(*ui16p_src1); /* 隣接値 */
    i32_tmp2 = (int32_t)(*ui16p_src2); /* さらに一つ隣の値 */
    i32_tmp1 *= 2;                     /* 隣接値２倍 */
    i32_tmp1 -= i32_tmp2;              /* カレントの値を得る */
    if (i32_tmp1 < 0) {
      *ui16p_tgt = 0;
    } else if ((int32_t)UINT16_MAX < i32_tmp1) {
      *ui16p_tgt = UINT16_MAX;
    } else {
      *ui16p_tgt = (uint16_t)i32_tmp1;
    }

    ++ui16p_src1;
    ++ui16p_src2;
    ++ui16p_tgt;
  }
  /* 縁の下部分 */
  ui16p_src1 =
      this->get_ui16p_src_channel() + ((this->_i32_ys - 1) * this->_i32_xs);
  ui16p_src2 =
      this->get_ui16p_src_channel() + ((this->_i32_ys - 2) * this->_i32_xs);
  ui16p_tgt = this->get_ui16p_tgt_channel() +
              (this->_i32_ys + 2 - 1) * (this->_i32_xs + 2) + 1;
  for (xx = 0; xx < this->_i32_xs; ++xx) {
    i32_tmp1 = (int32_t)(*ui16p_src1); /* 隣接値 */
    i32_tmp2 = (int32_t)(*ui16p_src2); /* さらに一つ隣の値 */
    i32_tmp1 *= 2;                     /* 隣接値２倍 */
    i32_tmp1 -= i32_tmp2;              /* カレントの値を得る */
    if (i32_tmp1 < 0) {
      *ui16p_tgt = 0;
    } else if ((int32_t)UINT16_MAX < i32_tmp1) {
      *ui16p_tgt = UINT16_MAX;
    } else {
      *ui16p_tgt = (uint16_t)i32_tmp1;
    }

    ++ui16p_src1;
    ++ui16p_src2;
    ++ui16p_tgt;
  }
  /* 縁の左部分 */
  ui16p_src1 = this->get_ui16p_src_channel();
  ui16p_src2 = this->get_ui16p_src_channel() + 1;
  ui16p_tgt  = this->get_ui16p_tgt_channel() + (this->_i32_xs + 2);
  for (yy = 0; yy < this->_i32_ys; ++yy) {
    i32_tmp1 = (int32_t)(*ui16p_src1); /* 隣接値 */
    i32_tmp2 = (int32_t)(*ui16p_src2); /* さらに一つ隣の値 */
    i32_tmp1 *= 2;                     /* 隣接値２倍 */
    i32_tmp1 -= i32_tmp2;              /* カレントの値を得る */
    if (i32_tmp1 < 0) {
      *ui16p_tgt = 0;
    } else if ((int32_t)UINT16_MAX < i32_tmp1) {
      *ui16p_tgt = UINT16_MAX;
    } else {
      *ui16p_tgt = (uint16_t)i32_tmp1;
    }

    ui16p_src1 += this->_i32_xs;
    ui16p_src2 += this->_i32_xs;
    ui16p_tgt += (this->_i32_xs + 2);
  }
  /* 縁の右部分 */
  ui16p_src1 = this->get_ui16p_src_channel() + this->_i32_xs - 1;
  ui16p_src2 = this->get_ui16p_src_channel() + this->_i32_xs - 1 - 1;
  ui16p_tgt  = this->get_ui16p_tgt_channel() + (this->_i32_xs + 2) * 2 - 1;
  for (yy = 0; yy < this->_i32_ys; ++yy) {
    i32_tmp1 = (int32_t)(*ui16p_src1); /* 隣接値 */
    i32_tmp2 = (int32_t)(*ui16p_src2); /* さらに一つ隣の値 */
    i32_tmp1 *= 2;                     /* 隣接値２倍 */
    i32_tmp1 -= i32_tmp2;              /* カレントの値を得る */
    if (i32_tmp1 < 0) {
      *ui16p_tgt = 0;
    } else if ((int32_t)UINT16_MAX < i32_tmp1) {
      *ui16p_tgt = UINT16_MAX;
    } else {
      *ui16p_tgt = (uint16_t)i32_tmp1;
    }

    ui16p_src1 += this->_i32_xs;
    ui16p_src2 += this->_i32_xs;
    ui16p_tgt += (this->_i32_xs + 2);
  }

  /* 画像サイズを縁１ピクセル増やす */
  this->_i32_xs += 2;
  this->_i32_ys += 2;

  /* 処理終了したらsrc,tgt画像交換 */
  this->_swap_channel();
}

#include <math.h> /* floor(), ceil() */

#include "igs_line_blur.h"  // "pri.h" "thinnest_ui16_image.h"

void thinnest_ui16_image::exec03_scale_liner(void) {
  int32_t xx, yy, i32_tgt_xs, i32_tgt_ys;
  uint16_t *ui16p_src, *ui16p_tgt;
  double d_src_xp, d_src_yp, d_tgt_xp, d_tgt_yp;
  int32_t i32_src_xpos1, i32_src_ypos1, i32_src_xpos2, i32_src_ypos2;
  double d_src_xratio1, d_src_yratio1, d_src_xratio2, d_src_yratio2;
  double d_tmp;

  /* 画像サイズが足りないか、分割数が指定されていない、ときは実行キャンセル */
  if ((this->_i32_xs < 2) || (this->_i32_ys < 2) || (this->_i32_xd < 2) ||
      (this->_i32_yd < 2)) {
    return;
  }

  /* 処理ごとのメッセージ */
  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("thinnest_ui16_image::exec03_scale_liner()");
  }
  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("thi : Scale %d x %d", this->_i32_xd, this->_i32_yd);
  }
  /* カウントダウン表示始め */
  if (this->_i_cv_sw) {
    pri_funct_cv_start((this->_i32_ys - 2) * this->_i32_yd);
  }

  ui16p_src = this->get_ui16p_src_channel();
  ui16p_tgt = this->get_ui16p_tgt_channel();

  i32_tgt_ys = (this->_i32_ys - 2) * this->_i32_yd;
  i32_tgt_xs = (this->_i32_xs - 2) * this->_i32_xd;

  /* ターゲットの大きさでループ */
  for (yy = 0; yy < i32_tgt_ys; ++yy) {
    for (xx = 0; xx < i32_tgt_xs; ++xx, ++ui16p_tgt) {
      /* カウントダウン表示中 */
      if (this->_i_cv_sw) {
        pri_funct_cv_run(yy);
      }

      /*
画像scaleのピクセルの対応図
(3倍拡大の場合)
+-------+
|       |
|       |
|       |
|       |
|       |
|   +   |外周付加
|       |ピクセル
|       |
|       |
|       |
|       |
+-------+	+-------+
|       |	|       |
|       |  -->  |   +   |
|       |	|       |
|       |	+-------+
|       |	|       |
|   +   |  -->  |   +   |
|       |	|       |
|       |	+-------+
|       |	|       |
|       |  -->  |   +   |
|       |	|       |
+-------+	+-------+
|       |	|       |
|       |  -->  |   +   |
|       |	|       |
|       |	+-------+
|       |	|       |
|   +   |  -->  |   +   |
|       |	|       |
|       |	+-------+
|       |	|       |
|       |  -->  |   +   |
|       |	|       |
+-------+	+-------+
|       |
|       |
|       |
|       |
|       |
|   +   |外周付加
|       |ピクセル
|       |
|       |
|       |
|       |
+-------+
*/
      /* ターゲット上の正規化された位置 */
      d_tgt_xp = ((double)xx + 0.5) / (double)i32_tgt_xs;
      d_tgt_yp = ((double)yy + 0.5) / (double)i32_tgt_ys;

      /* ソース画像上の正規化された位置 */
      d_src_xp = ((double)(this->_i32_xs) - 2.0) * d_tgt_xp + 0.5;
      d_src_yp = ((double)(this->_i32_ys) - 2.0) * d_tgt_yp + 0.5;

      /* ソース画像上の正規化された上下、左右の位置 */
      i32_src_xpos1 = (int32_t)floor(d_src_xp);
      i32_src_ypos1 = (int32_t)floor(d_src_yp);
      i32_src_xpos2 = (int32_t)ceil(d_src_xp);
      i32_src_ypos2 = (int32_t)ceil(d_src_yp);
      /* ソース画像上の正規化された上下、左右の比率 */
      if (i32_src_xpos1 == i32_src_xpos2) {
        d_src_xratio1 = 0.0;
        d_src_xratio2 = 1.0;
      } else {
        d_src_xratio1 = d_src_xp - floor(d_src_xp);
        d_src_xratio2 = ceil(d_src_xp) - d_src_xp;
      }
      if (i32_src_ypos1 == i32_src_ypos2) {
        d_src_yratio1 = 0.0;
        d_src_yratio2 = 1.0;
      } else {
        d_src_yratio1 = d_src_yp - floor(d_src_yp);
        d_src_yratio2 = ceil(d_src_yp) - d_src_yp;
      }

      /* 比率でその場所の値を計算する */
      d_tmp = ((ui16p_src[this->_i32_xs * i32_src_ypos1 + i32_src_xpos1] *
                    d_src_xratio2 +
                ui16p_src[this->_i32_xs * i32_src_ypos1 + i32_src_xpos2] *
                    d_src_xratio1) *
                   d_src_yratio2 +
               (ui16p_src[this->_i32_xs * i32_src_ypos2 + i32_src_xpos1] *
                    d_src_xratio2 +
                ui16p_src[this->_i32_xs * i32_src_ypos2 + i32_src_xpos2] *
                    d_src_xratio1) *
                   d_src_yratio1);
      if (UINT16_MAX <= d_tmp) {
        ui16p_tgt[0] = (uint16_t)UINT16_MAX;
      } else {
        ui16p_tgt[0] = (uint16_t)d_tmp;
      }
    }
  }
  /* カウントダウン表示終了 */
  if (this->_i_cv_sw) {
    pri_funct_cv_end();
  }

  /* 画像サイズの設定 */
  this->_i32_xs = (this->_i32_xs - 2) * this->_i32_xd;
  this->_i32_ys = (this->_i32_ys - 2) * this->_i32_yd;

  /* 処理終了したらsrc,tgt画像交換 */
  this->_swap_channel();
}

#include "igs_line_blur.h"  // "pri.h" "thinnest_ui16_image.h"

/* データB/W化 */
void thinnest_ui16_image::exec04_bw(void) {
  int32_t xx, yy;
  uint16_t *ui16p_src, *ui16p_tgt;

  /* 処理実行表示 */
  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("thinnest_ui16_image::exec04_bw()");
  }
  /* パラメータ表示 */
  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr("thi : threshold %u", this->_ui16_threshold);
  }
  /* カウントダウン表示始め */
  if (this->_i_cv_sw) {
    pri_funct_cv_start(this->_i32_ys);
  }

  ui16p_src = this->get_ui16p_src_channel();
  ui16p_tgt = this->get_ui16p_tgt_channel();

  for (yy = 0L; yy < this->_i32_ys; ++yy) {
    /* カウントダウン表示中 */
    if (this->_i_cv_sw) {
      pri_funct_cv_run(yy);
    }

    for (xx = 0L; xx < this->_i32_xs; ++xx, ++ui16p_src, ++ui16p_tgt) {
      if (this->_ui16_threshold <= *ui16p_src) {
        *ui16p_tgt = UINT16_MAX;
      } else {
        *ui16p_tgt = 0;
      }
    }
  }
  /* カウントダウン表示終了 */
  if (this->_i_cv_sw) {
    pri_funct_cv_end();
  }

  /* 処理終了したらsrc,tgt画像交換 */
  this->_swap_channel();
}

#include "igs_line_blur.h"  // "pri.h" "thinnest_ui16_image.h"

int thinnest_ui16_image::exec05_thin(void) {
  int32_t ii, jj, i32_pixel_count_total, i32_pixel_count_one_round,
      i32_pixel_count_one_side_tmp, i32_pixel_count_one_side[4];

  /* 処理ごとのメッセージ */
  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("thinnest_ui16_image::exec05_thin()");
  }

  /* 削るところがなくなるまでループする */
  i32_pixel_count_total       = 0;
  i32_pixel_count_one_side[0] = 0;
  i32_pixel_count_one_side[1] = 0;
  i32_pixel_count_one_side[2] = 0;
  i32_pixel_count_one_side[3] = 0;
  for (ii = 0;; ++ii) {
    if (this->get_i32_exec_loop_count() <= ii) {
      pri_funct_err_bttvr("Error : loop counter over %ld.",
                          this->get_i32_exec_loop_count());
      return NG;
    }

    /* 4方向に1pixelずつ細線化処理を行なう */
    i32_pixel_count_one_round = 0;
    for (jj = 0; jj < 4; ++jj) {
      /* 右方向への細線化 */
      i32_pixel_count_one_side_tmp = this->_one_side_thinner();

      /* 排除したpixel数のカウント */
      i32_pixel_count_one_round += i32_pixel_count_one_side_tmp;
      i32_pixel_count_total += i32_pixel_count_one_side_tmp;
      i32_pixel_count_one_side[jj] += i32_pixel_count_one_side_tmp;

      /* 時計方向(右)へ画像を90度回転する */
      this->_rot90_by_clockwork();

      /* カウントダウン表示(上下左右ひとつずつピリオド表示) */
      if (this->_i_cv_sw) {
        (void)fprintf(stdout, ".");
        (void)fflush(stdout);
      }
    }
    /* カウントダウン表示(上下左右やって"....|"となる) */
    if (this->_i_cv_sw) {
      (void)fprintf(stdout, "|");
      (void)fflush(stdout);
      /* 5 x 10カラムで改行 */
      if (9 == (ii % 10)) {
        (void)fprintf(stdout, "\n");
        (void)fflush(stdout);
      }
    }

    if (i32_pixel_count_one_round <= 0) break;
  }
  /* カウントダウン表示終了 */
  if (this->_i_cv_sw) {
    (void)fprintf(stdout, "\nthin line ... end.\n");
    (void)fflush(stdout);
  }

  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr("thi : total %d loop, and %ld pixel deleted", ii,
                       i32_pixel_count_total);
    pri_funct_msg_ttvr(
        "thi : and each r<%ld>+ t<%ld>+ l<%ld>+ b<%ld> pixel deleted",
        i32_pixel_count_one_side[0], i32_pixel_count_one_side[1],
        i32_pixel_count_one_side[2], i32_pixel_count_one_side[3]);
  }

  /* 正常終了 */
  return OK;
}
#include <stdlib.h> /* free(), calloc() */

#include "igs_line_blur.h"  // "pri.h" "thinnest_ui16_image.h"

/* メモリ開放 */
void thinnest_ui16_image::mem_free(void) {
#if 0
	if (NULL !=	this->_ui16p_channel[0]) {
		if (this->_i_mv_sw) {
			pri_funct_msg_ttvr( "thinnest_ui16_image::mem_free()" );
		}

		free(	this->_ui16p_channel[0]);/* ここで落ちる2014-5-16 */
			this->_ui16p_channel[0] = NULL;
			this->_ui16p_channel[1] = NULL;
	}
#endif
  if (NULL != this->memory_free_this_) {
    if (this->_i_mv_sw) {
      pri_funct_msg_ttvr("thinnest_ui16_image::mem_free() <%x>",
                         this->memory_free_this_);
    }

    free(this->memory_free_this_); /* これだと落ちない2014-5-16 */
    this->memory_free_this_ = NULL;
    this->_ui16p_channel[0] = NULL;
    this->_ui16p_channel[1] = NULL;
  }
}

/* データ設定とメモリ確保 */
int thinnest_ui16_image::mem_alloc(void) {
  /* 以前のメモリが残っていたら開放する */
  this->mem_free();

  /* 処理ごとのメッセージ */
  if (this->_i_mv_sw) {
    pri_funct_msg_ttvr("thinnest_ui16_image::mem_alloc()");
  }
  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr(
        "alloc ui16_image memory (%d+%d) x (%d+%d) x %d x %d bytes",
        this->_i32_xs * this->_i32_xd, 2, this->_i32_ys * this->_i32_yd, 2, 2,
        sizeof(uint16_t));
  }

  this->memory_free_this_ = this->_ui16p_channel[0] =
      (uint16_t *)calloc((this->_i32_xs * this->_i32_xd + 2) *
                             (this->_i32_ys * this->_i32_yd + 2) * 2,
                         sizeof(uint16_t));
  if (this->_i_pv_sw) {
    pri_funct_msg_ttvr("thinnest_ui16_image::mem_alloc() memory <%x>",
                       this->memory_free_this_);
  }
  if (NULL == this->_ui16p_channel[0]) {
    pri_funct_err_bttvr("Error : calloc(-) returns NULL.");
    return NG;
  }

  this->_ui16p_channel[1] =
      this->_ui16p_channel[0] +
      (this->_i32_xs * this->_i32_xd + 2) * (this->_i32_ys * this->_i32_yd + 2);

  return OK;
}

#include <assert.h> /* assert() */

#include "igs_line_blur.h"  // "pri.h" "thinnest_ui16_image.h"

int32_t thinnest_ui16_image::_one_side_thinner(void) {
  uint16_t *ui16p_src_y1, *ui16p_src_y2, *ui16p_src_y3, *ui16p_tgt;
  int32_t yy, i32_delete_count;

  /* 処理ごとのメッセージ */
  /******if (this->_i_mv_sw) {
          pri_funct_msg_ttvr( "thinnest_ui16_image::_one_side_thinner()" );
  }******/
  /* カウントダウン表示始め */
  // if (this->_i_cv_sw) { pri_funct_cv_start( this->_i32_ys ); }

  /* 始めのスキャンライン位置(画像から外れているものはNULLをいれる) */
  ui16p_src_y1 = this->get_ui16p_src_channel() + this->_i32_xs;
  ui16p_src_y2 = this->get_ui16p_src_channel();
  ui16p_src_y3 = NULL;
  ui16p_tgt    = this->get_ui16p_tgt_channel();

  /* 画像を縦にループ */
  i32_delete_count = 0;
  for (yy = 0; yy < this->_i32_ys; ++yy) {
    /* カウントダウン表示中 */
    // if (this->_i_cv_sw) { pri_funct_cv_run(yy); }

    /* スキャンライン(とその前後のスキャンライン)毎の処理 */
    i32_delete_count += this->_one_side_thinner_scanline(
        ui16p_src_y1, ui16p_src_y2, ui16p_src_y3, ui16p_tgt);

    /* 次のスキャンラインへ進める(画像から外れたらNULLをいれる) */
    ui16p_src_y3 = ui16p_src_y2;
    ui16p_src_y2 = ui16p_src_y1;
    if ((yy + 1) < (this->_i32_ys)) {
      ui16p_src_y1 += this->_i32_xs;
    } else
      ui16p_src_y1 = NULL;
    ui16p_tgt += this->_i32_xs;
  }

  /* カウントダウン表示終了 */
  // if (this->_i_cv_sw) { pri_funct_cv_end(); }

  /* 処理終了したらsrc,tgt画像交換 */
  this->_swap_channel();

  /* 終了 */
  return i32_delete_count;
}

int32_t thinnest_ui16_image::_one_side_thinner_scanline(uint16_t *ui16p_src_y1,
                                                        uint16_t *ui16p_src_y2,
                                                        uint16_t *ui16p_src_y3,
                                                        uint16_t *ui16p_tgt) {
  uint16_t *ui16p_src_x1a, *ui16p_src_x1b, *ui16p_src_x1c, *ui16p_src_x2a,
      *ui16p_src_x2b, *ui16p_src_x2c, *ui16p_src_x3a, *ui16p_src_x3b,
      *ui16p_src_x3c, ui16_src_x2c_before, ui16_src_x2b_before;
  int32_t xx, i32_delete_count;

  /* 始めのピクセル位置(画像から外れているものはNULLをいれる) */
  ui16p_src_x1a = NULL;
  ui16p_src_x1b = NULL;
  ui16p_src_x1c = NULL;
  ui16p_src_x2a = NULL;
  ui16p_src_x2b = NULL;
  ui16p_src_x2c = NULL;
  ui16p_src_x3a = NULL;
  ui16p_src_x3b = NULL;
  ui16p_src_x3c = NULL;
  if (NULL != ui16p_src_y1) {
    ui16p_src_x1a = ui16p_src_y1 + 1;
    ui16p_src_x1b = ui16p_src_y1;
  }
  if (NULL != ui16p_src_y2) {
    ui16p_src_x2a = ui16p_src_y2 + 1;
    ui16p_src_x2b = ui16p_src_y2;
  }
  if (NULL != ui16p_src_y3) {
    ui16p_src_x3a = ui16p_src_y3 + 1;
    ui16p_src_x3b = ui16p_src_y3;
  }
  ui16_src_x2c_before = 0;
  ui16_src_x2b_before = 0;

  /* 画像を横にループ */
  i32_delete_count = 0;
  for (xx = 0; xx < this->_i32_xs; ++xx) {
    /* ui16p_src_x2bがNULLであってはならない */
    assert(NULL != ui16p_src_x2b);

    /* 処理前に、次のループで参照のため、値をとっておく */
    ui16_src_x2b_before = (*ui16p_src_x2b);

    /*	ピクセル位置が一番左はじ、あるいは、
            前のピクセルがゼロ値(黒)で、
    カレントピクセルがゼロ以外(白)のとき
    */
    if (((NULL == ui16p_src_x2c) || (0 == ui16_src_x2c_before)) &&
        (0 < (*ui16p_src_x2b))) {
      i32_delete_count += this->_one_side_thinner_pixel(
          ui16p_src_x1a, ui16p_src_x1b, ui16p_src_x1c, ui16p_src_x2a,
          ui16p_src_x2b, ui16p_src_x2c, ui16p_src_x3a, ui16p_src_x3b,
          ui16p_src_x3c, ui16p_tgt);
    }
    /* 細線化に関係ない部分は単にコピーする */
    else {
      *ui16p_tgt = *ui16p_src_x2b;
    }

    /* 次のピクセルへ進める(画像から外れたらNULLをいれる) */
    ui16_src_x2c_before = ui16_src_x2b_before;
    ui16p_src_x1c       = ui16p_src_x1b;
    ui16p_src_x1b       = ui16p_src_x1a;
    ui16p_src_x2c       = ui16p_src_x2b;
    ui16p_src_x2b       = ui16p_src_x2a;
    ui16p_src_x3c       = ui16p_src_x3b;
    ui16p_src_x3b       = ui16p_src_x3a;
    if ((xx + 1) < (this->_i32_xs)) {
      if (NULL != (ui16p_src_x1a)) ++ui16p_src_x1a;
      if (NULL != (ui16p_src_x2a)) ++ui16p_src_x2a;
      if (NULL != (ui16p_src_x3a)) ++ui16p_src_x3a;
    } else {
      ui16p_src_x1a = NULL;
      ui16p_src_x2a = NULL;
      ui16p_src_x3a = NULL;
    }
    ++ui16p_tgt;
  }

  /* 終了 */
  return i32_delete_count;
}

int32_t thinnest_ui16_image::_one_side_thinner_pixel(
    uint16_t *ui16p_src_x1a, uint16_t *ui16p_src_x1b, uint16_t *ui16p_src_x1c,
    uint16_t *ui16p_src_x2a, uint16_t *ui16p_src_x2b, uint16_t *ui16p_src_x2c,
    uint16_t *ui16p_src_x3a, uint16_t *ui16p_src_x3b, uint16_t *ui16p_src_x3c,
    uint16_t *ui16p_tgt) {
  int32_t i32_delete_count;
  long l_off_count, l_white_count;
  bool i_sw, i_sw2;

  /* ui16p_src_x2bがNULLであってはならない */
  assert(NULL != ui16p_src_x2b);

  /* パラメータの初期値 */
  i32_delete_count = 0;
  l_off_count      = 0L;
  l_white_count    = 0L;
  i_sw = i_sw2 = false;

  /* 時計回り(右回り)に見ていく */
  /* 始め */
  if (NULL != (ui16p_src_x1a)) {
    i_sw = 0 < (*ui16p_src_x1a);
    if (i_sw) ++l_white_count;
    if (!i_sw && i_sw2) ++l_off_count;
    i_sw2 = i_sw;
  }
  /* 次 ... */
  if (NULL != (ui16p_src_x1b)) {
    i_sw = 0 < (*ui16p_src_x1b);
    if (i_sw) ++l_white_count;
    if (!i_sw && i_sw2) ++l_off_count;
    i_sw2 = i_sw;
  }
  if (NULL != (ui16p_src_x1c)) {
    i_sw = 0 < (*ui16p_src_x1c);
    if (i_sw) ++l_white_count;
    if (!i_sw && i_sw2) ++l_off_count;
    i_sw2 = i_sw;
  }
  if (NULL != (ui16p_src_x2c)) {
    i_sw = 0 < (*ui16p_src_x2c);
    if (i_sw) ++l_white_count;
    if (!i_sw && i_sw2) ++l_off_count;
    i_sw2 = i_sw;
  }
  if (NULL != (ui16p_src_x3c)) {
    i_sw = 0 < (*ui16p_src_x3c);
    if (i_sw) ++l_white_count;
    if (!i_sw && i_sw2) ++l_off_count;
    i_sw2 = i_sw;
  }
  if (NULL != (ui16p_src_x3b)) {
    i_sw = 0 < (*ui16p_src_x3b);
    if (i_sw) ++l_white_count;
    if (!i_sw && i_sw2) ++l_off_count;
    i_sw2 = i_sw;
  }
  if (NULL != (ui16p_src_x3a)) {
    i_sw = 0 < (*ui16p_src_x3a);
    if (i_sw) ++l_white_count;
    if (!i_sw && i_sw2) ++l_off_count;
    i_sw2 = i_sw;
  }
  if (NULL != (ui16p_src_x2a)) {
    i_sw = 0 < (*ui16p_src_x2a);
    if (i_sw) ++l_white_count;
    if (!i_sw && i_sw2) ++l_off_count;
    i_sw2 = i_sw;
  }
  /* 調査の開始点のみがゼロのときのため(l_off_count)開始点を再度調査する */
  if (NULL != (ui16p_src_x1a)) {
    i_sw = 0 < (*ui16p_src_x1a);
    if (!i_sw && i_sw2) ++l_off_count;
    i_sw2 = i_sw;
  }

  /* 回りが全部黒だったら独立点なので消す */
  if (l_white_count <= 0L) {
    *ui16p_tgt = 0;
    ++i32_delete_count;
  }
  /* ３近傍以上のときのエッジを消す(１近傍、２近傍は端点として残す) */
  else if ((1L == l_off_count) && (3 <= l_white_count)) {
    *ui16p_tgt = 0;
    ++i32_delete_count;
  }
  /* １ピクセル幅の線分上(両端含む)のとき */
  else {
    *ui16p_tgt = *ui16p_src_x2b;
  }

  /* 終了 */
  return i32_delete_count;
}

#include "igs_line_blur.h"  // "pri.h" "thinnest_ui16_image.h"

/* 時計回りに90度回転 */
void thinnest_ui16_image::_rot90_by_clockwork(void) {
  int32_t i32_tmp;
  int32_t xx, yy;
  uint16_t *ui16p_src, *ui16p_tgt_y, *ui16p_tgt_x;

  /* 処理ごとのメッセージ */
  /******if (this->_i_mv_sw) {
          pri_funct_msg_ttvr( "thinnest_ui16_image::_rot90_by_clockwork()" );
  }******/
  /* カウントダウン表示始め */
  // if (this->_i_cv_sw) { pri_funct_cv_start( this->_i32_ys ); }

  ui16p_src   = this->get_ui16p_src_channel();
  ui16p_tgt_y = this->get_ui16p_tgt_channel() + (this->_i32_ys - 1);

  for (yy = 0L; yy < this->_i32_ys; ++yy) {
    /* カウントダウン表示中 */
    // if (this->_i_cv_sw) { pri_funct_cv_run(yy); }

    ui16p_tgt_x = ui16p_tgt_y;
    for (xx = 0L; xx < this->_i32_xs; ++xx) {
      *ui16p_tgt_x = *ui16p_src;
      ++ui16p_src;
      ui16p_tgt_x += this->_i32_ys;
    }
    --ui16p_tgt_y;
  }
  /* カウントダウン表示終了 */
  // if (this->_i_cv_sw) { pri_funct_cv_end(); }

  /* 横と縦のサイズを交換 */
  i32_tmp       = this->_i32_xs;
  this->_i32_xs = this->_i32_ys;
  this->_i32_ys = i32_tmp;

  i32_tmp       = this->_i32_xd;
  this->_i32_xd = this->_i32_yd;
  this->_i32_yd = i32_tmp;

  /* 処理終了したらsrc,tgt画像交換 */
  this->_swap_channel();
}
#include <stdexcept>

#include "igs_line_blur.h"  // "brush_curve_blur.h" "igs_line_blur.h"

template <class T>
void igs_line_blur_brush_curve_point_put_image_template_(
    double *dp_pixel, int xp, int yp, const int height  // no_margin
    ,
    const int width  // no_margin
    ,
    const int channels, T *image_top  // no_margin
    ) {
  for (int zz = 0; zz < channels; ++zz) {
    image_top[yp * channels * width + xp * channels + zz] =
        static_cast<T>(dp_pixel[zz]);
  }
}

/*
int xp, int yp
 --> ピクセル位置
*/
void igs_line_blur_brush_curve_point_put_image_(
    brush_curve_blur &cl_brush_curve_blur, int xp, int yp,
    const int height  // no_margin
    ,
    const int width  // no_margin
    ,
    const int channels, const int bits, void *out  // no_margin
    ) {
  if ((xp < 0) && (width <= xp) && (yp < 0) && (height <= yp)) {
    throw std::domain_error(
        "Error : igs::line_blur::_brush_curve_point_put_image(-)");
  }

  if (16 == bits) {
    igs_line_blur_brush_curve_point_put_image_template_(
        cl_brush_curve_blur.get_dp_pixel(), xp, yp, height, width, channels,
        static_cast<unsigned short *>(out));
  } else if (8 == bits) {
    igs_line_blur_brush_curve_point_put_image_template_(
        cl_brush_curve_blur.get_dp_pixel(), xp, yp, height, width, channels,
        static_cast<unsigned char *>(out));
  }
}
#include "igs_line_blur.h"  // "brush_curve_blur.h"

template <class T>
void igs_line_blur_brush_curve_line_get_image_template_(
    const T *image_top, int height, int width, int channels, int i32_blur_count,
    double *xp_array, double *yp_array, double *linepixels_array, double xp,
    double yp) {
  for (int ii = 0; ii < i32_blur_count; ++ii) {
    const int xx = (int)(xp_array[ii] + xp + 0.5);
    const int yy = (int)(yp_array[ii] + yp + 0.5);
    if ((0 <= xx) && (xx < width) && (0 <= yy) && (yy < height)) {
      for (int zz = 0; zz < channels; ++zz) {
        linepixels_array[ii * CHANNEL_COUNT + zz] =
            (double)(image_top[yy * channels * width + xx * channels + zz]) +
            0.999999;
      }
    } else {
      for (int zz = 0; zz < channels; ++zz) {
        linepixels_array[ii * CHANNEL_COUNT + zz] = -1.0;
      }
    }
  }
}

/*
this->cl_brush_curve_blur.get_dp_xp()
this->cl_brush_curve_blur.get_dp_yp()
double xp, double yp
 --> ピクセルの真中が原点
*/
void igs_line_blur_brush_curve_line_get_image_(
    brush_curve_blur &cl_brush_curve_blur, const void *in  // no_margin
    ,
    const int height  // no_margin
    ,
    const int width  // no_margin
    ,
    const int channels, const int bits, double xp, double yp) {
  if (16 == bits) {
    igs_line_blur_brush_curve_line_get_image_template_(
        static_cast<const unsigned short *>(in), height, width, channels,
        cl_brush_curve_blur.get_i32_count(), cl_brush_curve_blur.get_dp_xp(),
        cl_brush_curve_blur.get_dp_yp(),
        cl_brush_curve_blur.get_dp_linepixels(), xp, yp);
  } else if (8 == bits) {
    igs_line_blur_brush_curve_line_get_image_template_(
        static_cast<const unsigned char *>(in), height, width, channels,
        cl_brush_curve_blur.get_i32_count(), cl_brush_curve_blur.get_dp_xp(),
        cl_brush_curve_blur.get_dp_yp(),
        cl_brush_curve_blur.get_dp_linepixels(), xp, yp);
  }
}
#include <iostream>
#include <string.h> /* memcpy() */

#include "igs_line_blur.h"  // "pri.h" "brush_curve_blur.h" "pixel_select_curve_blur.h" "pixel_line_root.h" "igs_line_blur.h"

int igs_line_blur_brush_curve_blur_subpixel_(
    brush_curve_blur &cl_brush_curve_blur,
    pixel_select_curve_blur_root &cl_pixel_select_curve_blur_root,
    pixel_line_root &cl_pixel_line_root

    ,
    const void *in  // no_margin
    ,
    const int height  // no_margin
    ,
    const int width  // no_margin
    ,
    const int channels, const int bits, int32_t xx, int32_t yy) {
  int32_t i32_subpixel, xsub, ysub;

  i32_subpixel = cl_brush_curve_blur.get_i32_subpixel_divide();

  for (ysub = 0; ysub < i32_subpixel; ++ysub) {
    for (xsub = 0; xsub < i32_subpixel; ++xsub) {
      /* 近い線分の、近い部分を選択 */
      cl_pixel_select_curve_blur_root.exec(
          (double)xx + (double)xsub / i32_subpixel - 0.5,
          (double)yy + (double)ysub / i32_subpixel - 0.5, &(cl_pixel_line_root),
          cl_brush_curve_blur.get_i32_count(),
          cl_brush_curve_blur.get_d_effect_area_radius());

      /* 選択できなかったらサブピクセルループを抜けて次のピクセルへ */
      if (cl_pixel_select_curve_blur_root.get_i32_count() <= 0) {
        return NG;
      }

      /* 合成線を生成 */
      cl_pixel_select_curve_blur_root.get_line(
          cl_brush_curve_blur.get_i32_count(), cl_brush_curve_blur.get_dp_xp(),
          cl_brush_curve_blur.get_dp_yp());

      /* 線分の各ピクセル値を取得 */
      igs_line_blur_brush_curve_line_get_image_(
          cl_brush_curve_blur, in, height, width, channels, bits,
          (double)xx + (double)xsub / i32_subpixel - 0.5,
          (double)yy + (double)ysub / i32_subpixel - 0.5);
      /* サブピクセル値を計算 */
      cl_brush_curve_blur.set_subpixel_value(xsub, ysub);
    }
  }
  return OK;
}

int igs_line_blur_brush_curve_blur_all_(
    bool mv_sw, bool pv_sw, bool cv_sw,
    brush_curve_blur &cl_brush_curve_blur,
    pixel_select_curve_blur_root &cl_pixel_select_curve_blur_root,
    pixel_line_root &cl_pixel_line_root

    ,
    const void *in  // no_margin
    ,
    const int height  // no_margin
    ,
    const int width  // no_margin
    ,
    const int channels, const int bits, void *out  // no_margin
    ) {
  /* 処理ごとのメッセージ */
  if (mv_sw) {
    std::cout << "igs::line_blur::_brush_curve_blur_all()" << std::endl;
  }
  if (pv_sw) {
    std::cout << " curve blur points count is " << std::endl
              << cl_brush_curve_blur.get_i32_count() << std::endl
              << " power is " << std::endl
              << cl_brush_curve_blur.get_d_power() << std::endl
              << " subpixel divide is " << std::endl
              << cl_brush_curve_blur.get_i32_subpixel_divide() << std::endl
              << " clip area for speedup is " << std::endl
              << cl_brush_curve_blur.get_d_effect_area_radius() << std::endl;
  }

  /* ブラシメモリの確保 */
  if (OK != cl_brush_curve_blur.mem_alloc()) {
    throw std::domain_error(
        "Error : cl_brush_curve_blur.mem_alloc() returns NG");
  }

  /* ブラシの線ぼかし変化比率の設定 */
  cl_brush_curve_blur.init_ratio_array();

  /* 画像をinからoutへコピーしておく */
  (void)memcpy(out, in, height * width * channels * ((16 == bits) ? 2 : 1));
  /* カウントダウン表示始め */
  if (cv_sw) {
    pri_funct_cv_start(height);
  }

  for (int yy = 0; yy < height; ++yy) {
    /* カウントダウン表示中 */
    if (cv_sw) {
      pri_funct_cv_run(yy);
    }

    for (int xx = 0; xx < width; ++xx) {
      if (OK == igs_line_blur_brush_curve_blur_subpixel_(
                    cl_brush_curve_blur, cl_pixel_select_curve_blur_root,
                    cl_pixel_line_root, in, height, width, channels, bits, xx,
                    yy)) {
        /* ピクセル値を計算 */
        cl_brush_curve_blur.set_pixel_value();

        /* 結果をピクセルへ置く
(取ったものと別の画像におくこと) */
        igs_line_blur_brush_curve_point_put_image_(
            cl_brush_curve_blur, xx, yy, height, width, channels, bits, out);
      }
    }
  }

  /* カウントダウン表示終了 */
  if (cv_sw) {
    pri_funct_cv_end();
  }

  /* ブラシメモリの開放 */
  cl_brush_curve_blur.mem_free();

  return OK;
}

#include <math.h> /* floor() */

#include "igs_line_blur.h"  // "brush_smudge_circle.h"

template <class T>
void igs_line_blur_brush_smudge_get_image_template_(
    T *in, int height, int width, int channels, double x1, double y1, double x2,
    double y2, double d_subsize, double *dp_image) {
  /* 保存(復元)位置 */
  int x1p = (int)floor(x1 + d_subsize / 2.0);
  int x2p = (int)floor(x2 - d_subsize / 2.0);
  int y1p = (int)floor(y1 + d_subsize / 2.0);
  int y2p = (int)floor(y2 - d_subsize / 2.0);

  for (int yy = y1p; yy <= y2p; ++yy) {
    for (int xx = x1p; xx <= x2p; ++xx, dp_image += 5) {
      if ((0 <= xx) && (xx < width) && (0 <= yy) && (yy < height)) {
        for (int zz = 0; zz < 4; ++zz) {
          if (zz < channels) {
            dp_image[zz] =
                (double)(in[yy * channels * width + xx * channels + zz]) +
                0.999999;
          } else {
            /* 本来来てはいけない条件なので意味のない値にしとく */
            dp_image[zz] = 0.0;
          }
        }
        dp_image[4] = 1.0;
      } else {
        dp_image[4] = 0.0;
      }
    }
  }
}

void igs_line_blur_brush_smudge_get_image_(
    brush_smudge_circle &cl_brush_smudge_circle, const void *in  // no_margin
    ,
    const int height  // no_margin
    ,
    const int width  // no_margin
    ,
    const int channels, const int bits, double d_xp, double d_yp) {
  /* 画像上に置いたブラシの範囲 */
  double x1, y1, x2, y2;
  cl_brush_smudge_circle.get_dp_area(d_xp, d_yp, &x1, &y1, &x2, &y2);

  if (16 == bits) {
    igs_line_blur_brush_smudge_get_image_template_(
        static_cast<const unsigned short *>(in), height, width, channels, x1,
        y1, x2, y2, 1.0 / cl_brush_smudge_circle.get_i32_subpixel_divide(),
        cl_brush_smudge_circle.get_dp_pixel_image());
  } else if (8 == bits) {
    igs_line_blur_brush_smudge_get_image_template_(
        static_cast<const unsigned char *>(in), height, width, channels, x1, y1,
        x2, y2, 1.0 / cl_brush_smudge_circle.get_i32_subpixel_divide(),
        cl_brush_smudge_circle.get_dp_pixel_image());
  }
}
#include <math.h> /* floor() */

#include "igs_line_blur.h"  // "brush_smudge_circle.h"

template <class T>
void igs_line_blur_brush_smudge_put_image_template_(
    double x1, double y1, double x2, double y2, double d_subsize,
    double *dp_image, int height, int width, int channels, T *out) {
  /* 保存(復元)位置 */
  int x1p = (int)floor(x1 + d_subsize / 2.0);
  int x2p = (int)floor(x2 - d_subsize / 2.0);
  int y1p = (int)floor(y1 + d_subsize / 2.0);
  int y2p = (int)floor(y2 - d_subsize / 2.0);

  for (int yy = y1p; yy <= y2p; ++yy) {
    for (int xx = x1p; xx <= x2p; ++xx, dp_image += 5) {
      if ((0.0 < dp_image[4]) && (0 <= xx) && (xx < width) && (0 <= yy) &&
          (yy < height)) {
        for (int zz = 0; zz < channels; ++zz) {
          out[yy * channels * width + xx * channels + zz] = static_cast<T>(
              (double)out[yy * channels * width + xx * channels + zz] *
                  (1.0 - dp_image[4]) +
              dp_image[zz]);
        }
      }
    }
  }
}

void igs_line_blur_brush_smudge_put_image_(
    brush_smudge_circle &cl_brush_smudge_circle, double d_xp, double d_yp,
    const int height  // no_margin
    ,
    const int width  // no_margin
    ,
    const int channels, const int bits, void *out  // no_margin
    ) {
  /* 画像上に置いたブラシの範囲 */
  double x1, y1, x2, y2;
  cl_brush_smudge_circle.get_dp_area(d_xp, d_yp, &x1, &y1, &x2, &y2);

  if (16 == bits) {
    igs_line_blur_brush_smudge_put_image_template_(
        x1, y1, x2, y2, 1.0 / cl_brush_smudge_circle.get_i32_subpixel_divide(),
        cl_brush_smudge_circle.get_dp_pixel_image(), height, width, channels,
        static_cast<unsigned short *>(out));
  } else if (8 == bits) {
    igs_line_blur_brush_smudge_put_image_template_(
        x1, y1, x2, y2, 1.0 / cl_brush_smudge_circle.get_i32_subpixel_divide(),
        cl_brush_smudge_circle.get_dp_pixel_image(), height, width, channels,
        static_cast<unsigned char *>(out));
  }
}
#include <stdexcept>

#include "igs_line_blur.h"  // "brush_smudge_circle.h"

void igs_line_blur_brush_smudge_line_(
    brush_smudge_circle &cl_brush_smudge_circle, const void *in  // no_margin
    ,
    const int height  // no_margin
    ,
    const int width  // no_margin
    ,
    const int channels, const int bits, void *out  // no_margin
    ,
    pixel_line_node *clp_line) {
  pixel_point_node *clp_one_expand, *clp_another_expand, *clp_crnt;
  int32_t ii;
  double d_x1, d_y1, d_x2, d_y2;

  /* 両端点と中点を得る */
  clp_one_expand     = clp_line->get_clp_link_one_expand();
  clp_another_expand = clp_line->get_clp_link_another_expand();

  /* ブラシマスクを円形にセット */
  cl_brush_smudge_circle.set_brush_circle();

  /* 指先(にじみ)の始点 */
  clp_crnt = clp_line->get_clp_link_middle();
  /* サブピクセルメモリーに画像を得る */
  igs_line_blur_brush_smudge_get_image_(
      cl_brush_smudge_circle, in, height, width, channels, bits,
      clp_crnt->get_d_xp_tgt(), clp_crnt->get_d_yp_tgt());
  /* 画像上に置いたブラシの範囲 */
  cl_brush_smudge_circle.get_dp_area(clp_crnt->get_d_xp_tgt(),
                                     clp_crnt->get_d_yp_tgt(), &d_x1, &d_y1,
                                     &d_x2, &d_y2);
  /* サブピクセル画像にする */
  cl_brush_smudge_circle.to_subpixel_from_pixel(d_x1, d_y1, d_x2, d_y2);
  /* ブラシに画像をセット */
  cl_brush_smudge_circle.copy_to_brush_from_image();

  /* 線の後ろへ向かってこする */
  for (clp_crnt = clp_crnt->get_clp_next_point(), ii = 0; NULL != clp_crnt;
       clp_crnt = clp_crnt->get_clp_next_point(), ++ii) {
    /* 偽の場合、たぶん無限ループ */
    if (clp_line->get_i32_point_count() <= ii) {
      throw std::domain_error("Error : over clp_line->get_i32_point_count()");
    }
    /* 画像上に置いたブラシの範囲 */
    cl_brush_smudge_circle.get_dp_area(clp_crnt->get_d_xp_tgt(),
                                       clp_crnt->get_d_yp_tgt(), &d_x1, &d_y1,
                                       &d_x2, &d_y2);

    /* ブラシをかける */
    if ((0.0 <= d_x2) && (d_x1 < width) && (0.0 <= d_y2) && (d_y1 < height)) {
      /* 画像を得る */
      igs_line_blur_brush_smudge_get_image_(
          cl_brush_smudge_circle, in, height, width, channels, bits,
          clp_crnt->get_d_xp_tgt(), clp_crnt->get_d_yp_tgt());
      /* サブピクセル画像にする */
      cl_brush_smudge_circle.to_subpixel_from_pixel(d_x1, d_y1, d_x2, d_y2);
      /* かすれ処理 */
      cl_brush_smudge_circle.exec();
      /* サブピクセル画像を元ピクセルサイズにする */
      cl_brush_smudge_circle.to_pixel_from_subpixel(d_x1, d_y1, d_x2, d_y2);
      /* 画像を置く */
      igs_line_blur_brush_smudge_put_image_(
          cl_brush_smudge_circle, clp_crnt->get_d_xp_tgt(),
          clp_crnt->get_d_yp_tgt(), height, width, channels, bits, out);
    }
    /* 終点 */
    // if (clp_another_expand == clp_crnt) break;
  }

  /* 指先(にじみ)の始点 */
  clp_crnt = clp_line->get_clp_link_middle();
  /* サブピクセルメモリーに画像を得る */
  igs_line_blur_brush_smudge_get_image_(
      cl_brush_smudge_circle, in, height, width, channels, bits,
      clp_crnt->get_d_xp_tgt(), clp_crnt->get_d_yp_tgt());
  /* 画像上に置いたブラシの範囲 */
  cl_brush_smudge_circle.get_dp_area(clp_crnt->get_d_xp_tgt(),
                                     clp_crnt->get_d_yp_tgt(), &d_x1, &d_y1,
                                     &d_x2, &d_y2);
  /* サブピクセル画像にする */
  cl_brush_smudge_circle.to_subpixel_from_pixel(d_x1, d_y1, d_x2, d_y2);
  /* ブラシに画像をセット */
  cl_brush_smudge_circle.copy_to_brush_from_image();

  /* 線の前へ向かってこする */
  for (clp_crnt = clp_crnt->get_clp_previous_point(), ii = 0; NULL != clp_crnt;
       clp_crnt = clp_crnt->get_clp_previous_point(), ++ii) {
    /* 偽の場合、たぶん無限ループ */
    if (clp_line->get_i32_point_count() <= ii) {
      throw std::domain_error(
          "Error : over clp_line->get_i32_point_count() going front");
    }

    /* 画像上に置いたブラシの範囲 */
    cl_brush_smudge_circle.get_dp_area(clp_crnt->get_d_xp_tgt(),
                                       clp_crnt->get_d_yp_tgt(), &d_x1, &d_y1,
                                       &d_x2, &d_y2);

    /* ブラシをかける */
    if ((0.0 <= d_x2) && (d_x1 < width) && (0.0 <= d_y2) && (d_y1 < height)) {
      /* 画像を得る */
      igs_line_blur_brush_smudge_get_image_(
          cl_brush_smudge_circle, in, height, width, channels, bits,
          clp_crnt->get_d_xp_tgt(), clp_crnt->get_d_yp_tgt());
      /* サブピクセル画像にする */
      cl_brush_smudge_circle.to_subpixel_from_pixel(d_x1, d_y1, d_x2, d_y2);
      /* かすれ処理 */
      cl_brush_smudge_circle.exec();
      /* サブピクセル画像を元ピクセルサイズにする */
      cl_brush_smudge_circle.to_pixel_from_subpixel(d_x1, d_y1, d_x2, d_y2);
      /* 画像を置く */
      igs_line_blur_brush_smudge_put_image_(
          cl_brush_smudge_circle, clp_crnt->get_d_xp_tgt(),
          clp_crnt->get_d_yp_tgt(), height, width, channels, bits, out);
    }
    /* 終点 */
    // if (clp_one_expand == clp_crnt) break;
  }
}

#include <iostream>
#include <string.h> /* memcpy() */

#include "igs_line_blur.h"  // "pri.h"

void igs_line_blur_brush_smudge_all_(
    bool mv_sw, bool pv_sw, bool cv_sw,
    brush_smudge_circle &cl_brush_smudge_circle,
    pixel_line_root &cl_pixel_line_root, const void *in  // no_margin
    ,
    const int height  // no_margin
    ,
    const int width  // no_margin
    ,
    const int channels, const int bits, void *out  // no_margin
    ) {
  /* 処理ごとのメッセージ */
  if (mv_sw) {
    std::cout << "igs::line_expand::_brush_smudge_all()" << std::endl;
  }

  if (pv_sw) {
    std::cout << " smudge ratio " << cl_brush_smudge_circle.get_d_ratio()
              << std::endl
              << " smudge brush size by pixel "
              << cl_brush_smudge_circle.get_i32_size_by_pixel() << std::endl
              << " smudge subpixel divide "
              << cl_brush_smudge_circle.get_i32_subpixel_divide() << std::endl;
  }

  /* ブラシメモリの確保 */
  if (OK != cl_brush_smudge_circle.mem_alloc()) {
    throw std::domain_error(
        "Error : cl_brush_smudge_circle.mem_alloc() returns NG");
  }

  /* 画像をinからoutへコピーしておく */
  (void)memcpy(out, in, height * width * channels * ((16 == bits) ? 2 : 1));

  /* カウントダウン表示始め */
  if (cv_sw) {
    pri_funct_cv_start(cl_pixel_line_root.get_i32_count());
  }

  /* 汚れ線描画 */
  pixel_line_node *clp_line =
      (pixel_line_node *)cl_pixel_line_root.get_clp_first();
  for (int ii   = 0; NULL != clp_line;
       clp_line = (pixel_line_node *)clp_line->get_clp_next(), ++ii) {
    if (cl_pixel_line_root.get_i32_count() <= ii) {
      throw std::domain_error(
          "Error : over cl_pixel_line_root.get_i32_count()");
    }

    /* カウントダウン表示中 */
    if (cv_sw) {
      pri_funct_cv_run(ii);
    }

    igs_line_blur_brush_smudge_line_(cl_brush_smudge_circle, in, height, width,
                                     channels, bits, out, clp_line);
  }
  /* カウントダウン表示終了 */
  if (cv_sw) {
    pri_funct_cv_end();
  }
}

#include <iostream>
#include <stdexcept>

#include "igs_line_blur.h"  // "pri.h"

void igs_line_blur_image_get_(const bool mv_sw, const bool cv_sw,
                              const long reference_channel,
                              thinnest_ui16_image &cl_thinnest_ui16_image

                              ,
                              const void *in  // no_margin
                              ,
                              const int height  // no_margin
                              ,
                              const int width  // no_margin
                              ,
                              const int channels, const int bits) {
  /* 処理ごとのメッセージ */
  if (mv_sw) {
    std::cout << "igs_line_blur_image_get_()" << std::endl
              << "com : reference channel " << reference_channel << std::endl;
  }

  int i32_xs               = cl_thinnest_ui16_image.get_i32_xs();
  int i32_ys               = cl_thinnest_ui16_image.get_i32_ys();
  unsigned short *out_incr = cl_thinnest_ui16_image.get_ui16p_src_channel();

  /* カウントダウン表示始め */
  if (cv_sw) {
    pri_funct_cv_start(i32_ys);
  }

  if (8 == bits) {
    const unsigned char *in_incr = static_cast<const unsigned char *>(in);
    in_incr += reference_channel;
    for (int yy = 0; yy < i32_ys; ++yy) {
      /* カウントダウン表示中 */
      if (cv_sw) {
        pri_funct_cv_run(yy);
      }

      for (int xx = 0; xx < i32_xs; ++xx) {
        /* 8bits -> 16bits変換して格納 */
        *out_incr =
            (((unsigned short)(*in_incr)) << 8) + (unsigned short)(*in_incr);
        /* 参照位置移動 */
        in_incr += channels;
        ++out_incr;
      }
    }
  } else if (16 == bits) {
    const unsigned short *in_incr = static_cast<const unsigned short *>(in);
    in_incr += reference_channel;

    for (int yy = 0; yy < i32_ys; ++yy) {
      /* カウントダウン表示中 */
      if (cv_sw) {
        pri_funct_cv_run(yy);
      }

      for (int xx = 0; xx < i32_xs; ++xx) {
        *out_incr = *in_incr;
        /* 参照位置移動 */
        in_incr += channels;
        ++out_incr;
      }
    }
  } else {
    throw std::domain_error("Error : bits is not 8 or 16");
  }

  /* カウントダウン表示終了 */
  if (cv_sw) {
    pri_funct_cv_end();
  }
}
}

#include <iostream>
#include <stdexcept>
#include "igs_line_blur.h"  // "thinnest_ui16_image.h" "pixel_point_root.h" "pixel_line_root.h" "brush_curve_blur.h" "brush_smudge_circle.h" "pixel_select_same_way.h" "pixel_select_curve_blur.h" "igs_line_blur.h"

void igs::line_blur::convert(
    /* 入出力画像 */
    const void *in  // no_margin
    ,
    void *out  // no_margin

    ,
    const int height  // no_margin
    ,
    const int width  // no_margin
    ,
    const int channels, const int bits

    /* Action */
    ,
    const int b_blur_count /* min=1   def=51   incr=1   max=100  */
    ,
    const double b_blur_power /* min=0.1 def=1    incr=0.1 max=10.0 */
    ,
    const int b_subpixel /* min=1   def=1    incr=1   max=8	 */
    ,
    const int b_blur_near_ref /* min=1   def=5    incr=1   max=100	 */
    ,
    const int b_blur_near_len /* min=1   def=160  incr=1   max=1000 */

    ,
    const int b_smudge_thick /* min=1   def=7    incr=1   max=100	 */
    ,
    const double b_smudge_remain
    /* min=0.0 def=0.85 incr=0.001 max=1.0*/

    ,
    const int v_smooth_retry /* min=0   def=100  incr=1   max=1000 */
    ,
    const int v_near_ref /* min=0   def=4    incr=1   max=100	 */
    ,
    const int v_near_len /* min=2   def=160  incr=1   max=1000 */

    ,
    const bool mv_sw /* false=OFF */
    ,
    const bool pv_sw /* false=OFF */
    ,
    const bool cv_sw /* false=OFF */
    ,
    const long reference_channel /* 3	=Alpha:RGBA orBGRA */
    ,
    const bool debug_save_sw /* false=OFF */
    ,
    const int brush_action /* 0 =Curve Blur ,1=Smudge Brush */
    ) {
  /* --- 動作クラスコンストラクション --- */
  thinnest_ui16_image cl_thinnest_ui16_image;
  pixel_point_root cl_pixel_point_root;
  pixel_line_root cl_pixel_line_root;
  pixel_select_same_way_root cl_pixel_select_same_way_root;
  pixel_select_curve_blur_root cl_pixel_select_curve_blur_root;
  brush_smudge_circle cl_brush_smudge_circle;
  brush_curve_blur cl_brush_curve_blur;

  /* --- 標準出力への動作表示スイッチ --- */
  cl_thinnest_ui16_image.set_i_mv_sw(mv_sw);
  cl_pixel_point_root.set_i_mv_sw(mv_sw);
  cl_pixel_line_root.set_i_mv_sw(mv_sw);
  cl_brush_smudge_circle.set_i_mv_sw(mv_sw);
  cl_brush_curve_blur.set_i_mv_sw(mv_sw);

  cl_thinnest_ui16_image.set_i_pv_sw(pv_sw);
  cl_pixel_point_root.set_i_pv_sw(pv_sw);
  cl_pixel_line_root.set_i_pv_sw(pv_sw);
  cl_brush_smudge_circle.set_i_pv_sw(pv_sw);
  cl_brush_curve_blur.set_i_pv_sw(pv_sw);

  cl_thinnest_ui16_image.set_i_cv_sw(cv_sw);
  cl_pixel_point_root.set_i_cv_sw(cv_sw);
  cl_pixel_line_root.set_i_cv_sw(cv_sw);
  cl_brush_smudge_circle.set_i_cv_sw(cv_sw);
  cl_brush_curve_blur.set_i_cv_sw(cv_sw);

  /* --- 処理ごとのメッセージ --- */
  if (mv_sw) {
    std::cout << "igs::line_blur::convert()" << std::endl;
  }

  /* --- 引数値をセット --- */

  /* 細線化用メモリの大きさを設定 */
  cl_thinnest_ui16_image.set_i32_xs(width);
  cl_thinnest_ui16_image.set_i32_ys(height);

  /* 0: $type : Wheel (min=0, default=0, increment=1, max=1)              */
  /******	if (0 == b_action_mode) {
          this->set_e_brush_smudge_action();
  } else {
          this->set_e_brush_curve_blur_action();
          cl_pixel_line_root.set_i_same_way_exec_sw(false);
  }******/

  /* 1: $b_blur_count   :Wheel(min=1, default=51, increment=1, max=100)   */
  cl_brush_curve_blur.set_i32_count(b_blur_count);
  cl_brush_curve_blur.set_d_effect_area_radius((double)(b_blur_count / 2));

  /* 2: $b_blur_power   :Wheel(min=0.1,default=1,increment=0.1,max=hh0)   */
  cl_brush_curve_blur.set_d_power(b_blur_power);

  /* 3. $b_subpixel : Wheel (min=1, default=1, increment=1, max=8)	*/
  cl_brush_curve_blur.set_i32_subpixel_divide(b_subpixel);

  /* 4: $b_blur_near_ref:Wheel(min=1, default=5, increment=1, max=100)    */
  cl_pixel_select_curve_blur_root.set_i32_count_max(b_blur_near_ref);

  /* 5: $b_blur_near_len:Wheel(min=1, default=160, increment=1, max=1000) */
  cl_pixel_select_curve_blur_root.set_d_length_max(b_blur_near_len);

  /* 6: $b_smudge_thick :Wheel(min=1, default=7, increment=1, max=100)    */
  cl_brush_smudge_circle.set_i32_size_by_pixel(b_smudge_thick);

  /* 7: $b_smudge_remain:Wheel(min=0.0,default=0.85,increment=0.001,max=1.0)*/
  cl_brush_smudge_circle.set_d_ratio(b_smudge_remain);

  /* 8: $v_smooth_retry :Wheel(min=0, default=100, increment=1, max=1000) */
  cl_pixel_line_root.set_i32_smooth_retry(v_smooth_retry);

  /* 9: $v_near_ref     :Wheel(min=0, default=4, increment=1, max=100)    */
  cl_pixel_select_same_way_root.set_i32_count_max(v_near_ref);

  /*10: $v_near_len     :Wheel(min=2, default=160, increment=1, max=1000) */
  cl_pixel_select_same_way_root.set_d_length_max(v_near_len);

  /* 細線化用メモリ確保 */
  if (OK != cl_thinnest_ui16_image.mem_alloc()) {
    throw std::domain_error(
        "Error : cl_thinnest_ui16_image.mem_alloc() returns NG");
  }

  /* 画像情報を細線化用メモリに移す */
  igs_line_blur_image_get_(mv_sw, cv_sw, reference_channel,
                           cl_thinnest_ui16_image, in, height, width, channels,
                           bits);

  /****** 細線化処理 start ******/

  /* 元画像の大きさの時に線分の点ノイズを消す */
  cl_thinnest_ui16_image.exec01_fill_noise_pixel();

  /* 精度をあげるための画像の拡大のための外周１ピクセル付加 */
  cl_thinnest_ui16_image.exec02_scale_add_edge_pixel();

  /* 精度をあげるための画像の拡大 */
  cl_thinnest_ui16_image.exec03_scale_liner();

  /* 白黒２値化 */
  cl_thinnest_ui16_image.exec04_bw();

  /* 細線化処理 */
  if (OK != cl_thinnest_ui16_image.exec05_thin()) {
    throw std::domain_error(
        "Error : cl_thinnest_ui16_image.exec05_thin() returns NG");
  }

  /****** 細線化処理 end ******/

  /****** ベクトルリスト処理 start ******/

  /* 細線化した画像をリストにする */
  if (OK !=
      cl_pixel_point_root.alloc_mem_and_list_node(
          cl_thinnest_ui16_image.get_i32_xs(),
          cl_thinnest_ui16_image.get_i32_ys(),
          cl_thinnest_ui16_image.get_ui16p_src_channel())) {
    throw std::domain_error(
        "Error : cl_pixel_point_root.alloc_mem_and_list_node() returns NG");
  }

  /* 単なるポイントリストを線分リストにする */
  if (OK != cl_pixel_line_root.exec01020304(&(cl_pixel_point_root))) {
    throw std::domain_error(
        "Error : cl_pixel_line_root.exec01020304() returns NG");
  }
  if (debug_save_sw) {
    if (OK != cl_pixel_line_root.save_lines("tmp08_jaggy_lines.txt")) {
      throw std::domain_error(
          "Error : cl_pixel_line_root.save_lines(-) returns NG");
    }
    if (OK != cl_pixel_line_root.save_one_point("tmp09_one_point.txt")) {
      throw std::domain_error(
          "Error : cl_pixel_line_root.save_one_point(-) returns NG");
    }
    if (OK !=
        cl_pixel_line_root.save_another_point("tmp10_another_point.txt")) {
      throw std::domain_error(
          "Error : cl_pixel_line_root.save_another_point(-) returns NG");
    }
    if (OK !=
        cl_pixel_line_root.save_not_include(&(cl_pixel_point_root),
                                            "tmp11_not_include.txt")) {
      throw std::domain_error(
          "Error : cl_pixel_line_root.save_not_include(-) returns NG");
    }
  }

  /* 中点をセットする */
  cl_pixel_line_root.exec05_set_middle();
  if (debug_save_sw) {
    if (OK != cl_pixel_line_root.save_middle_point("tmp12_middle_point.txt")) {
      throw std::domain_error(
          "Error : cl_pixel_line_root.save_middle_point(-) returns NG");
    }
  }

  /* ベクトル化した線分の座標値を整数からdoubleに */
  cl_pixel_line_root.exec06_int2double_body();

  /* ベクトル化した線分をスムースに */
  cl_pixel_line_root.exec07_smooth_body();

  /* 線分を伸長する */
  if (OK != cl_pixel_line_root.exec08_expand_lines(&(cl_pixel_point_root))) {
    throw std::domain_error(
        "Error : cl_pixel_line_root.exec08_expand_lines(-) returns NG");
  }

  /* 伸ばした線分の方向をそろえる */
  if (debug_save_sw) {
    if (OK !=
        cl_pixel_line_root.save_expand_vector("tmp13_expand_vector.txt")) {
      throw std::domain_error(
          "Error : cl_pixel_line_root.save_expand_vector(-) returns NG");
    }
  }
  cl_pixel_line_root.exec09_same_way_expand(&(cl_pixel_select_same_way_root));
  if (debug_save_sw) {
    if (OK !=
        cl_pixel_line_root.save_expand_vector("tmp14_same_way_vector.txt")) {
      throw std::domain_error(
          "Error : cl_pixel_line_root.save_expand_vector(-) returns NG");
    }
  }

  /* 伸ばした線分を再度スムースに */
  cl_pixel_line_root.exec10_smooth_expand();

  if (debug_save_sw) {
    if (OK != cl_pixel_line_root.save_expand_lines("tmp15_expand_lines.txt")) {
      throw std::domain_error(
          "Error : cl_pixel_line_root.save_expand_lines(-) returns NG");
    }
    if (OK !=
        cl_pixel_line_root.save_one_expand_point(
            "tmp16_one_expand_point.txt")) {
      throw std::domain_error(
          "Error : cl_pixel_line_root.save_one_expand_point(-) returns NG");
    }
    if (OK !=
        cl_pixel_line_root.save_another_expand_point(
            "tmp17_another_expand_point.txt")) {
      throw std::domain_error(
          "Error : cl_pixel_line_root.save_another_expand_point(-) returns NG");
    }
  }

  /* 各ラインと全体のバウンダリーボックスをセット */
  cl_pixel_line_root.exec11_set_bbox();

  /****** ベクトルリスト処理 end ******/

  /* 画像の加工 */
  if (0 == brush_action) {
    /* 線分情報からだいたいその方向にぼかす */
    igs_line_blur_brush_curve_blur_all_(
        mv_sw, pv_sw, cv_sw, cl_brush_curve_blur,
        cl_pixel_select_curve_blur_root, cl_pixel_line_root, in, height, width,
        channels, bits, out);
  } else if (1 == brush_action) {
    /* 画像をコピーしてから、指先ツールのようにこする */
    igs_line_blur_brush_smudge_all_(mv_sw, pv_sw, cv_sw, cl_brush_smudge_circle,
                                    cl_pixel_line_root, in, height, width,
                                    channels, bits, out);
  }

  /* 指先ツール用メモリ開放 */
  cl_brush_smudge_circle.mem_free();

  /* 線ぼかしツール用メモリ開放 */
  cl_brush_curve_blur.mem_free();

  /* 線ぼかし方向参照リスト開放 */
  cl_pixel_select_curve_blur_root.mem_free();

  /* 同方向線曲げ方向参照リスト開放 */
  cl_pixel_select_same_way_root.mem_free();

  /* ラインリストメモリ開放 */
  cl_pixel_line_root.mem_free();

  /* ポイントリストメモリ開放 */
  cl_pixel_point_root.mem_free();

  /* 細線化用メモリ開放 */
  cl_thinnest_ui16_image.mem_free();
}
