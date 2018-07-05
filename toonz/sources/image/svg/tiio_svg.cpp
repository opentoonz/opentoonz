

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <QTextStream>
#include <QFile>

#include "tiio_svg.h"
#include "tvectorimage.h"
#include "tstroke.h"
#include "tstrokeoutline.h"
#include "tregion.h"
#include "tcurves.h"
#include "tpalette.h"

//=------------------------------------------------------------------------------------------------------------------------------
//=------------------------------------------------------------------------------------------------------------------------------
//=------------------------------------------------------------------------------------------------------------------------------
//=------------------------------------------------------------------------------------------------------------------------------

namespace  // svg_parser
{

struct NSVGpath {
  float *pts;   // Cubic bezier points: x0,y0, [cpx1,cpx1,cpx2,cpy2,x1,y1], ...
  int npts;     // Total number of bezier points.
  char closed;  // Flag indicating if shapes should be treated as closed.
  struct NSVGpath *next;  // Pointer to next path, or NULL if last element.
};

struct NSVGshape {
  unsigned int fillColor;    // Fill color
  unsigned int strokeColor;  // Stroke color
  float strokeWidth;         // Stroke width (scaled)
  char hasFill;              // Flag indicating if fill exists.
  char hasStroke;            // Flag indicating id store exists
  struct NSVGpath *paths;    // Linked list of paths in the image.
  struct NSVGshape *next;    // Pointer to next shape, or NULL if last element.
};

struct NSVGimage {
  float width;               // Width of the image, or -1.0f of not set.
  float height;              // Height of the image, or -1.0f of not set.
  char wunits[8];            // Units of the width attribute
  char hunits[8];            // Units of the height attribute
  struct NSVGshape *shapes;  // Linked list of shapes in the image.
};

#define NSVG_PI 3.14159265358979323846264338327f
#define NSVG_KAPPA90                                                           \
  0.5522847493f  // Length proportional to radius of a cubic bezier handle for
                 // 90deg arcs.

#ifdef _MSC_VER
#pragma warning(disable : 4996)  // Switch off security warnings
#pragma warning(                                                               \
    disable : 4100)  // Switch off unreferenced formal parameter warnings
#ifdef __cplusplus
#define NSVG_INLINE inline
#else
#define NSVG_INLINE
#endif
#else
#define NSVG_INLINE inline
#endif

int nsvg__isspace(char c) { return strchr(" \t\n\v\f\r", c) != 0; }

int nsvg__isdigit(char c) { return strchr("0123456789", c) != 0; }

int nsvg__isnum(char c) { return strchr("0123456789+-.eE", c) != 0; }

NSVG_INLINE float nsvg__maxf(float a, float b) { return a > b ? a : b; }

// Simple XML parser

#define NSVG_XML_TAG 1
#define NSVG_XML_CONTENT 2
#define NSVG_XML_MAX_ATTRIBS 256

void nsvg__parseContent(char *s, void (*contentCb)(void *ud, const char *s),
                        void *ud) {
  // Trim start white spaces
  while (*s && nsvg__isspace(*s)) s++;
  if (!*s) return;

  if (contentCb) (*contentCb)(ud, s);
}

void nsvg__parseElement(char *s, void (*startelCb)(void *ud, const char *el,
                                                   const char **attr),
                        void (*endelCb)(void *ud, const char *el), void *ud) {
  const char *attr[NSVG_XML_MAX_ATTRIBS];
  int nattr = 0;
  char *name;
  int start = 0;
  int end   = 0;

  // Skip white space after the '<'
  while (*s && nsvg__isspace(*s)) s++;

  // Check if the tag is end tag
  if (*s == '/') {
    s++;
    end = 1;
  } else {
    start = 1;
  }

  // Skip comments, data and preprocessor stuff.
  if (!*s || *s == '?' || *s == '!') return;

  // Get tag name
  name = s;
  while (*s && !nsvg__isspace(*s)) s++;
  if (*s) {
    *s++ = '\0';
  }

  // Get attribs
  while (!end && *s && nattr < NSVG_XML_MAX_ATTRIBS - 3) {
    // Skip white space before the attrib name
    while (*s && nsvg__isspace(*s)) s++;
    if (!*s) break;
    if (*s == '/') {
      end = 1;
      break;
    }
    attr[nattr++] = s;
    // Find end of the attrib name.
    while (*s && !nsvg__isspace(*s) && *s != '=') s++;
    if (*s) {
      *s++ = '\0';
    }
    // Skip until the beginning of the value.
    while (*s && *s != '\"') s++;
    if (!*s) break;
    s++;
    // Store value and find the end of it.
    attr[nattr++] = s;
    while (*s && *s != '\"') s++;
    if (*s) {
      *s++ = '\0';
    }
  }

  // List terminator
  attr[nattr++] = 0;
  attr[nattr++] = 0;

  // Call callbacks.
  if (start && startelCb) (*startelCb)(ud, name, attr);
  if (end && endelCb) (*endelCb)(ud, name);
}

int nsvg__parseXML(char *input, void (*startelCb)(void *ud, const char *el,
                                                  const char **attr),
                   void (*endelCb)(void *ud, const char *el),
                   void (*contentCb)(void *ud, const char *s), void *ud) {
  char *s    = input;
  char *mark = s;
  int state  = NSVG_XML_CONTENT;
  while (*s) {
    if (*s == '<' && state == NSVG_XML_CONTENT) {
      // Start of a tag
      *s++ = '\0';
      nsvg__parseContent(mark, contentCb, ud);
      mark  = s;
      state = NSVG_XML_TAG;
    } else if (*s == '>' && state == NSVG_XML_TAG) {
      // Start of a content or new tag.
      *s++ = '\0';
      nsvg__parseElement(mark, startelCb, endelCb, ud);
      mark  = s;
      state = NSVG_XML_CONTENT;
    } else {
      s++;
    }
  }

  return 1;
}

/* Simple SVG parser. */

#define NSVG_MAX_ATTR 128

struct NSVGAttrib {
  float xform[6];
  unsigned int fillColor;
  unsigned int strokeColor;
  float fillOpacity;
  float strokeOpacity;
  float strokeWidth;
  char hasFill;
  char hasStroke;
  char visible;
};

struct NSVGParser {
  struct NSVGAttrib attr[NSVG_MAX_ATTR];
  int attrHead;
  float *pts;
  int npts;
  int cpts;
  struct NSVGpath *plist;
  struct NSVGimage *image;
  char pathFlag;
  char defsFlag;
};

void nsvg__xformSetIdentity(float *t) {
  t[0] = 1.0f;
  t[1] = 0.0f;
  t[2] = 0.0f;
  t[3] = 1.0f;
  t[4] = 0.0f;
  t[5] = 0.0f;
}

void nsvg__xformSetTranslation(float *t, float tx, float ty) {
  t[0] = 1.0f;
  t[1] = 0.0f;
  t[2] = 0.0f;
  t[3] = 1.0f;
  t[4] = tx;
  t[5] = ty;
}

void nsvg__xformSetScale(float *t, float sx, float sy) {
  t[0] = sx;
  t[1] = 0.0f;
  t[2] = 0.0f;
  t[3] = sy;
  t[4] = 0.0f;
  t[5] = 0.0f;
}

void nsvg__xformSetSkewX(float *t, float a) {
  t[0] = 1.0f;
  t[1] = 0.0f;
  t[2] = tanf(a);
  t[3] = 1.0f;
  t[4] = 0.0f;
  t[5] = 0.0f;
}

void nsvg__xformSetSkewY(float *t, float a) {
  t[0] = 1.0f;
  t[1] = tanf(a);
  t[2] = 0.0f;
  t[3] = 1.0f;
  t[4] = 0.0f;
  t[5] = 0.0f;
}

void nsvg__xformSetRotation(float *t, float a) {
  float cs = cosf(a), sn = sinf(a);
  t[0] = cs;
  t[1] = sn;
  t[2] = -sn;
  t[3] = cs;
  t[4] = 0.0f;
  t[5] = 0.0f;
}

void nsvg__xformMultiply(float *t, float *s) {
  float t0 = t[0] * s[0] + t[1] * s[2];
  float t2 = t[2] * s[0] + t[3] * s[2];
  float t4 = t[4] * s[0] + t[5] * s[2] + s[4];
  t[1]     = t[0] * s[1] + t[1] * s[3];
  t[3]     = t[2] * s[1] + t[3] * s[3];
  t[5]     = t[4] * s[1] + t[5] * s[3] + s[5];
  t[0]     = t0;
  t[2]     = t2;
  t[4]     = t4;
}

void nsvg__xformPremultiply(float *t, float *s) {
  float s2[6];
  memcpy(s2, s, sizeof(float) * 6);
  nsvg__xformMultiply(s2, t);
  memcpy(t, s2, sizeof(float) * 6);
}

void nsvg__xformPoint(float *dx, float *dy, float x, float y, float *t) {
  *dx = x * t[0] + y * t[2] + t[4];
  *dy = x * t[1] + y * t[3] + t[5];
}

void nsvg__xformVec(float *dx, float *dy, float x, float y, float *t) {
  *dx = x * t[0] + y * t[2];
  *dy = x * t[1] + y * t[3];
}

struct NSVGParser *nsvg__createParser() {
  struct NSVGParser *p;
  p = (struct NSVGParser *)malloc(sizeof(struct NSVGParser));
  if (p == NULL) goto error;
  memset(p, 0, sizeof(struct NSVGParser));

  p->image = (struct NSVGimage *)malloc(sizeof(struct NSVGimage));
  if (p->image == NULL) goto error;
  memset(p->image, 0, sizeof(struct NSVGimage));
  p->image->width  = -1.0f;
  p->image->height = -1.0f;

  // Init style
  nsvg__xformSetIdentity(p->attr[0].xform);
  p->attr[0].fillColor     = 0;
  p->attr[0].strokeColor   = 0;
  p->attr[0].fillOpacity   = 1;
  p->attr[0].strokeOpacity = 1;
  p->attr[0].strokeWidth   = 1;
  p->attr[0].hasFill       = 0;
  p->attr[0].hasStroke     = 0;
  p->attr[0].visible       = 1;

  return p;

error:
  if (p) {
    if (p->image) free(p->image);
    free(p);
  }
  return NULL;
}

void nsvg__deletePaths(struct NSVGpath *path) {
  while (path) {
    struct NSVGpath *next = path->next;
    if (path->pts != NULL) free(path->pts);
    free(path);
    path = next;
  }
}

void nsvgDelete(struct NSVGimage *image) {
  struct NSVGshape *next, *shape;
  if (image == NULL) return;
  shape = image->shapes;
  while (shape != NULL) {
    next = shape->next;
    nsvg__deletePaths(shape->paths);
    free(shape);
    shape = next;
  }
  free(image);
}

void nsvg__deleteParser(struct NSVGParser *p) {
  if (p != NULL) {
    nsvg__deletePaths(p->plist);
    nsvgDelete(p->image);
    free(p->pts);
    free(p);
  }
}

void nsvg__resetPath(struct NSVGParser *p) { p->npts = 0; }

void nsvg__addPoint(struct NSVGParser *p, float x, float y) {
  if (p->npts + 1 > p->cpts) {
    p->cpts = p->cpts ? p->cpts * 2 : 8;
    p->pts  = (float *)realloc(p->pts, p->cpts * 2 * sizeof(float));
    if (!p->pts) return;
  }
  p->pts[p->npts * 2 + 0] = x;
  p->pts[p->npts * 2 + 1] = y;
  p->npts++;
}

void nsvg__moveTo(struct NSVGParser *p, float x, float y) {
  nsvg__addPoint(p, x, y);
}

void nsvg__lineTo(struct NSVGParser *p, float x, float y) {
  float px, py, dx, dy;
  if (p->npts > 0) {
    px = p->pts[(p->npts - 1) * 2 + 0];
    py = p->pts[(p->npts - 1) * 2 + 1];
    dx = x - px;
    dy = y - py;
    nsvg__addPoint(p, px + dx / 3.0f, py + dy / 3.0f);
    nsvg__addPoint(p, x - dx / 3.0f, y - dy / 3.0f);
    nsvg__addPoint(p, x, y);
  }
}

void nsvg__cubicBezTo(struct NSVGParser *p, float cpx1, float cpy1, float cpx2,
                      float cpy2, float x, float y) {
  nsvg__addPoint(p, cpx1, cpy1);
  nsvg__addPoint(p, cpx2, cpy2);
  nsvg__addPoint(p, x, y);
}

struct NSVGAttrib *nsvg__getAttr(struct NSVGParser *p) {
  return &p->attr[p->attrHead];
}

void nsvg__pushAttr(struct NSVGParser *p) {
  if (p->attrHead < NSVG_MAX_ATTR - 1) {
    p->attrHead++;
    memcpy(&p->attr[p->attrHead], &p->attr[p->attrHead - 1],
           sizeof(struct NSVGAttrib));
  }
}

void nsvg__popAttr(struct NSVGParser *p) {
  if (p->attrHead > 0) p->attrHead--;
}

void nsvg__addShape(struct NSVGParser *p) {
  struct NSVGAttrib *attr = nsvg__getAttr(p);
  float scale             = 1.0f;
  struct NSVGshape *shape, *cur, *prev;

  if (p->plist == NULL) return;

  shape = (struct NSVGshape *)malloc(sizeof(struct NSVGshape));
  if (shape == NULL) goto error;
  memset(shape, 0, sizeof(struct NSVGshape));

  scale              = nsvg__maxf(fabsf(attr->xform[0]), fabsf(attr->xform[3]));
  shape->hasFill     = attr->hasFill;
  shape->hasStroke   = attr->hasStroke;
  shape->strokeWidth = attr->strokeWidth * scale;

  shape->fillColor = attr->fillColor;
  if (shape->hasFill)
    shape->fillColor |= (unsigned int)(attr->fillOpacity * 255) << 24;

  shape->strokeColor = attr->strokeColor;
  if (shape->hasStroke)
    shape->strokeColor |= (unsigned int)(attr->strokeOpacity * 255) << 24;

  shape->paths = p->plist;
  p->plist     = NULL;

  // Add to tail
  prev = NULL;
  cur  = p->image->shapes;
  while (cur != NULL) {
    prev = cur;
    cur  = cur->next;
  }
  if (prev == NULL)
    p->image->shapes = shape;
  else
    prev->next = shape;

  return;

error:
  if (shape) free(shape);
}

void nsvg__addPath(struct NSVGParser *p, char closed) {
  struct NSVGAttrib *attr = nsvg__getAttr(p);
  struct NSVGpath *path   = NULL;
  int i;

  if (p->npts == 0) return;

  if (closed) nsvg__lineTo(p, p->pts[0], p->pts[1]);

  path = (struct NSVGpath *)malloc(sizeof(struct NSVGpath));
  if (path == NULL) goto error;
  memset(path, 0, sizeof(struct NSVGpath));

  path->pts = (float *)malloc(p->npts * 2 * sizeof(float));
  if (path->pts == NULL) goto error;
  path->closed = closed;
  path->npts   = p->npts;

  // Transform path.
  for (i = 0; i < p->npts; ++i)
    nsvg__xformPoint(&path->pts[i * 2], &path->pts[i * 2 + 1], p->pts[i * 2],
                     p->pts[i * 2 + 1], attr->xform);

  path->next = p->plist;
  p->plist   = path;

  return;

error:
  if (path != NULL) {
    if (path->pts != NULL) free(path->pts);
    free(path);
  }
}

const char *nsvg__getNextPathItem(const char *s, char *it) {
  int i = 0;
  it[0] = '\0';
  // Skip white spaces and commas
  while (*s && (nsvg__isspace(*s) || *s == ',')) s++;
  if (!*s) return s;
  if (*s == '-' || *s == '+' || nsvg__isdigit(*s)) {
    // sign
    if (*s == '-' || *s == '+') {
      if (i < 63) it[i++] = *s;
      s++;
    }
    // integer part
    while (*s && nsvg__isdigit(*s)) {
      if (i < 63) it[i++] = *s;
      s++;
    }
    if (*s == '.') {
      // decimal point
      if (i < 63) it[i++] = *s;
      s++;
      // fraction part
      while (*s && nsvg__isdigit(*s)) {
        if (i < 63) it[i++] = *s;
        s++;
      }
    }
    // exponent
    if (*s == 'e' || *s == 'E') {
      if (i < 63) it[i++] = *s;
      s++;
      if (*s == '-' || *s == '+') {
        if (i < 63) it[i++] = *s;
        s++;
      }
      while (*s && nsvg__isdigit(*s)) {
        if (i < 63) it[i++] = *s;
        s++;
      }
    }
    it[i] = '\0';
  } else {
    // Parse command
    it[0] = *s++;
    it[1] = '\0';
    return s;
  }

  return s;
}

#define NSVG_RGB(r, g, b)                                                      \
  (((unsigned int)r) | ((unsigned int)g << 8) | ((unsigned int)b << 16))

unsigned int nsvg__parseColorHex(const char *str) {
  unsigned int c = 0, r = 0, g = 0, b = 0;
  int n = 0;
  str++;  // skip #
  // Calculate number of characters.
  while (str[n] && !nsvg__isspace(str[n])) n++;
  if (n == 6) {
    sscanf(str, "%x", &c);
  } else if (n == 3) {
    sscanf(str, "%x", &c);
    c = (c & 0xf) | ((c & 0xf0) << 4) | ((c & 0xf00) << 8);
    c |= c << 4;
  }
  r = (c >> 16) & 0xff;
  g = (c >> 8) & 0xff;
  b = c & 0xff;
  return NSVG_RGB(r, g, b);
}

unsigned int nsvg__parseColorRGB(const char *str) {
  int r = -1, g = -1, b = -1;
  char s1[32] = "", s2[32] = "";
  sscanf(str + 4, "%d%[%%, \t]%d%[%%, \t]%d", &r, s1, &g, s2, &b);
  if (strchr(s1, '%')) {
    return NSVG_RGB((r * 255) / 100, (g * 255) / 100, (b * 255) / 100);
  } else {
    return NSVG_RGB(r, g, b);
  }
}

struct NSVGNamedColor {
  const char *name;
  unsigned int color;
};

struct NSVGNamedColor nsvg__colors[] = {

    {"red", NSVG_RGB(255, 0, 0)},
    {"green", NSVG_RGB(0, 128, 0)},
    {"blue", NSVG_RGB(0, 0, 255)},
    {"yellow", NSVG_RGB(255, 255, 0)},
    {"cyan", NSVG_RGB(0, 255, 255)},
    {"magenta", NSVG_RGB(255, 0, 255)},
    {"black", NSVG_RGB(0, 0, 0)},
    {"grey", NSVG_RGB(128, 128, 128)},
    {"gray", NSVG_RGB(128, 128, 128)},
    {"white", NSVG_RGB(255, 255, 255)},

    {"aliceblue", NSVG_RGB(240, 248, 255)},
    {"antiquewhite", NSVG_RGB(250, 235, 215)},
    {"aqua", NSVG_RGB(0, 255, 255)},
    {"aquamarine", NSVG_RGB(127, 255, 212)},
    {"azure", NSVG_RGB(240, 255, 255)},
    {"beige", NSVG_RGB(245, 245, 220)},
    {"bisque", NSVG_RGB(255, 228, 196)},
    {"blanchedalmond", NSVG_RGB(255, 235, 205)},
    {"blueviolet", NSVG_RGB(138, 43, 226)},
    {"brown", NSVG_RGB(165, 42, 42)},
    {"burlywood", NSVG_RGB(222, 184, 135)},
    {"cadetblue", NSVG_RGB(95, 158, 160)},
    {"chartreuse", NSVG_RGB(127, 255, 0)},
    {"chocolate", NSVG_RGB(210, 105, 30)},
    {"coral", NSVG_RGB(255, 127, 80)},
    {"cornflowerblue", NSVG_RGB(100, 149, 237)},
    {"cornsilk", NSVG_RGB(255, 248, 220)},
    {"crimson", NSVG_RGB(220, 20, 60)},
    {"darkblue", NSVG_RGB(0, 0, 139)},
    {"darkcyan", NSVG_RGB(0, 139, 139)},
    {"darkgoldenrod", NSVG_RGB(184, 134, 11)},
    {"darkgray", NSVG_RGB(169, 169, 169)},
    {"darkgreen", NSVG_RGB(0, 100, 0)},
    {"darkgrey", NSVG_RGB(169, 169, 169)},
    {"darkkhaki", NSVG_RGB(189, 183, 107)},
    {"darkmagenta", NSVG_RGB(139, 0, 139)},
    {"darkolivegreen", NSVG_RGB(85, 107, 47)},
    {"darkorange", NSVG_RGB(255, 140, 0)},
    {"darkorchid", NSVG_RGB(153, 50, 204)},
    {"darkred", NSVG_RGB(139, 0, 0)},
    {"darksalmon", NSVG_RGB(233, 150, 122)},
    {"darkseagreen", NSVG_RGB(143, 188, 143)},
    {"darkslateblue", NSVG_RGB(72, 61, 139)},
    {"darkslategray", NSVG_RGB(47, 79, 79)},
    {"darkslategrey", NSVG_RGB(47, 79, 79)},
    {"darkturquoise", NSVG_RGB(0, 206, 209)},
    {"darkviolet", NSVG_RGB(148, 0, 211)},
    {"deeppink", NSVG_RGB(255, 20, 147)},
    {"deepskyblue", NSVG_RGB(0, 191, 255)},
    {"dimgray", NSVG_RGB(105, 105, 105)},
    {"dimgrey", NSVG_RGB(105, 105, 105)},
    {"dodgerblue", NSVG_RGB(30, 144, 255)},
    {"firebrick", NSVG_RGB(178, 34, 34)},
    {"floralwhite", NSVG_RGB(255, 250, 240)},
    {"forestgreen", NSVG_RGB(34, 139, 34)},
    {"fuchsia", NSVG_RGB(255, 0, 255)},
    {"gainsboro", NSVG_RGB(220, 220, 220)},
    {"ghostwhite", NSVG_RGB(248, 248, 255)},
    {"gold", NSVG_RGB(255, 215, 0)},
    {"goldenrod", NSVG_RGB(218, 165, 32)},
    {"greenyellow", NSVG_RGB(173, 255, 47)},
    {"honeydew", NSVG_RGB(240, 255, 240)},
    {"hotpink", NSVG_RGB(255, 105, 180)},
    {"indianred", NSVG_RGB(205, 92, 92)},
    {"indigo", NSVG_RGB(75, 0, 130)},
    {"ivory", NSVG_RGB(255, 255, 240)},
    {"khaki", NSVG_RGB(240, 230, 140)},
    {"lavender", NSVG_RGB(230, 230, 250)},
    {"lavenderblush", NSVG_RGB(255, 240, 245)},
    {"lawngreen", NSVG_RGB(124, 252, 0)},
    {"lemonchiffon", NSVG_RGB(255, 250, 205)},
    {"lightblue", NSVG_RGB(173, 216, 230)},
    {"lightcoral", NSVG_RGB(240, 128, 128)},
    {"lightcyan", NSVG_RGB(224, 255, 255)},
    {"lightgoldenrodyellow", NSVG_RGB(250, 250, 210)},
    {"lightgray", NSVG_RGB(211, 211, 211)},
    {"lightgreen", NSVG_RGB(144, 238, 144)},
    {"lightgrey", NSVG_RGB(211, 211, 211)},
    {"lightpink", NSVG_RGB(255, 182, 193)},
    {"lightsalmon", NSVG_RGB(255, 160, 122)},
    {"lightseagreen", NSVG_RGB(32, 178, 170)},
    {"lightskyblue", NSVG_RGB(135, 206, 250)},
    {"lightslategray", NSVG_RGB(119, 136, 153)},
    {"lightslategrey", NSVG_RGB(119, 136, 153)},
    {"lightsteelblue", NSVG_RGB(176, 196, 222)},
    {"lightyellow", NSVG_RGB(255, 255, 224)},
    {"lime", NSVG_RGB(0, 255, 0)},
    {"limegreen", NSVG_RGB(50, 205, 50)},
    {"linen", NSVG_RGB(250, 240, 230)},
    {"maroon", NSVG_RGB(128, 0, 0)},
    {"mediumaquamarine", NSVG_RGB(102, 205, 170)},
    {"mediumblue", NSVG_RGB(0, 0, 205)},
    {"mediumorchid", NSVG_RGB(186, 85, 211)},
    {"mediumpurple", NSVG_RGB(147, 112, 219)},
    {"mediumseagreen", NSVG_RGB(60, 179, 113)},
    {"mediumslateblue", NSVG_RGB(123, 104, 238)},
    {"mediumspringgreen", NSVG_RGB(0, 250, 154)},
    {"mediumturquoise", NSVG_RGB(72, 209, 204)},
    {"mediumvioletred", NSVG_RGB(199, 21, 133)},
    {"midnightblue", NSVG_RGB(25, 25, 112)},
    {"mintcream", NSVG_RGB(245, 255, 250)},
    {"mistyrose", NSVG_RGB(255, 228, 225)},
    {"moccasin", NSVG_RGB(255, 228, 181)},
    {"navajowhite", NSVG_RGB(255, 222, 173)},
    {"navy", NSVG_RGB(0, 0, 128)},
    {"oldlace", NSVG_RGB(253, 245, 230)},
    {"olive", NSVG_RGB(128, 128, 0)},
    {"olivedrab", NSVG_RGB(107, 142, 35)},
    {"orange", NSVG_RGB(255, 165, 0)},
    {"orangered", NSVG_RGB(255, 69, 0)},
    {"orchid", NSVG_RGB(218, 112, 214)},
    {"palegoldenrod", NSVG_RGB(238, 232, 170)},
    {"palegreen", NSVG_RGB(152, 251, 152)},
    {"paleturquoise", NSVG_RGB(175, 238, 238)},
    {"palevioletred", NSVG_RGB(219, 112, 147)},
    {"papayawhip", NSVG_RGB(255, 239, 213)},
    {"peachpuff", NSVG_RGB(255, 218, 185)},
    {"peru", NSVG_RGB(205, 133, 63)},
    {"pink", NSVG_RGB(255, 192, 203)},
    {"plum", NSVG_RGB(221, 160, 221)},
    {"powderblue", NSVG_RGB(176, 224, 230)},
    {"purple", NSVG_RGB(128, 0, 128)},
    {"rosybrown", NSVG_RGB(188, 143, 143)},
    {"royalblue", NSVG_RGB(65, 105, 225)},
    {"saddlebrown", NSVG_RGB(139, 69, 19)},
    {"salmon", NSVG_RGB(250, 128, 114)},
    {"sandybrown", NSVG_RGB(244, 164, 96)},
    {"seagreen", NSVG_RGB(46, 139, 87)},
    {"seashell", NSVG_RGB(255, 245, 238)},
    {"sienna", NSVG_RGB(160, 82, 45)},
    {"silver", NSVG_RGB(192, 192, 192)},
    {"skyblue", NSVG_RGB(135, 206, 235)},
    {"slateblue", NSVG_RGB(106, 90, 205)},
    {"slategray", NSVG_RGB(112, 128, 144)},
    {"slategrey", NSVG_RGB(112, 128, 144)},
    {"snow", NSVG_RGB(255, 250, 250)},
    {"springgreen", NSVG_RGB(0, 255, 127)},
    {"steelblue", NSVG_RGB(70, 130, 180)},
    {"tan", NSVG_RGB(210, 180, 140)},
    {"teal", NSVG_RGB(0, 128, 128)},
    {"thistle", NSVG_RGB(216, 191, 216)},
    {"tomato", NSVG_RGB(255, 99, 71)},
    {"turquoise", NSVG_RGB(64, 224, 208)},
    {"violet", NSVG_RGB(238, 130, 238)},
    {"wheat", NSVG_RGB(245, 222, 179)},
    {"whitesmoke", NSVG_RGB(245, 245, 245)},
    {"yellowgreen", NSVG_RGB(154, 205, 50)},
};

unsigned int nsvg__parseColorName(const char *str) {
  int i, ncolors = sizeof(nsvg__colors) / sizeof(struct NSVGNamedColor);

  for (i = 0; i < ncolors; i++) {
    if (strcmp(nsvg__colors[i].name, str) == 0) {
      return nsvg__colors[i].color;
    }
  }

  return NSVG_RGB(128, 128, 128);
}

unsigned int nsvg__parseColor(const char *str) {
  int len = 0;
  while (*str == ' ') ++str;
  len = (int)strlen(str);
  if (len >= 1 && *str == '#')
    return nsvg__parseColorHex(str);
  else if (len >= 4 && str[0] == 'r' && str[1] == 'g' && str[2] == 'b' &&
           str[3] == '(')
    return nsvg__parseColorRGB(str);
  return nsvg__parseColorName(str);
}

float nsvg__parseFloat(const char *str) {
  while (*str == ' ') ++str;
  return (float)atof(str);
}

int nsvg__parseTransformArgs(const char *str, float *args, int maxNa, int *na) {
  const char *end;
  const char *ptr;

  *na = 0;
  ptr = str;
  while (*ptr && *ptr != '(') ++ptr;
  if (*ptr == 0) return 1;
  end = ptr;
  while (*end && *end != ')') ++end;
  if (*end == 0) return 1;

  while (ptr < end) {
    if (nsvg__isnum(*ptr)) {
      if (*na >= maxNa) return 0;
      args[(*na)++] = (float)atof(ptr);
      while (ptr < end && nsvg__isnum(*ptr)) ++ptr;
    } else {
      ++ptr;
    }
  }
  return (int)(end - str);
}

int nsvg__parseMatrix(struct NSVGParser *p, const char *str) {
  float t[6];
  int na  = 0;
  int len = nsvg__parseTransformArgs(str, t, 6, &na);
  if (na != 6) return len;
  nsvg__xformPremultiply(nsvg__getAttr(p)->xform, t);
  return len;
}

int nsvg__parseTranslate(struct NSVGParser *p, const char *str) {
  float args[2];
  float t[6];
  int na               = 0;
  int len              = nsvg__parseTransformArgs(str, args, 2, &na);
  if (na == 1) args[1] = 0.0;
  nsvg__xformSetTranslation(t, args[0], args[1]);
  nsvg__xformPremultiply(nsvg__getAttr(p)->xform, t);
  return len;
}

int nsvg__parseScale(struct NSVGParser *p, const char *str) {
  float args[2];
  int na = 0;
  float t[6];
  int len              = nsvg__parseTransformArgs(str, args, 2, &na);
  if (na == 1) args[1] = args[0];
  nsvg__xformSetScale(t, args[0], args[1]);
  nsvg__xformPremultiply(nsvg__getAttr(p)->xform, t);
  return len;
}

int nsvg__parseSkewX(struct NSVGParser *p, const char *str) {
  float args[1];
  int na = 0;
  float t[6];
  int len = nsvg__parseTransformArgs(str, args, 1, &na);
  nsvg__xformSetSkewX(t, args[0] / 180.0f * NSVG_PI);
  nsvg__xformPremultiply(nsvg__getAttr(p)->xform, t);
  return len;
}

int nsvg__parseSkewY(struct NSVGParser *p, const char *str) {
  float args[1];
  int na = 0;
  float t[6];
  int len = nsvg__parseTransformArgs(str, args, 1, &na);
  nsvg__xformSetSkewY(t, args[0] / 180.0f * NSVG_PI);
  nsvg__xformPremultiply(nsvg__getAttr(p)->xform, t);
  return len;
}

int nsvg__parseRotate(struct NSVGParser *p, const char *str) {
  float args[3];
  int na = 0;
  float t[6];
  int len              = nsvg__parseTransformArgs(str, args, 3, &na);
  if (na == 1) args[1] = args[2] = 0.0f;

  if (na > 1) {
    nsvg__xformSetTranslation(t, -args[1], -args[2]);
    nsvg__xformPremultiply(nsvg__getAttr(p)->xform, t);
  }

  nsvg__xformSetRotation(t, args[0] / 180.0f * NSVG_PI);
  nsvg__xformPremultiply(nsvg__getAttr(p)->xform, t);

  if (na > 1) {
    nsvg__xformSetTranslation(t, args[1], args[2]);
    nsvg__xformPremultiply(nsvg__getAttr(p)->xform, t);
  }

  return len;
}

void nsvg__parseTransform(struct NSVGParser *p, const char *str) {
  while (*str) {
    if (strncmp(str, "matrix", 6) == 0)
      str += nsvg__parseMatrix(p, str);
    else if (strncmp(str, "translate", 9) == 0)
      str += nsvg__parseTranslate(p, str);
    else if (strncmp(str, "scale", 5) == 0)
      str += nsvg__parseScale(p, str);
    else if (strncmp(str, "rotate", 6) == 0)
      str += nsvg__parseRotate(p, str);
    else if (strncmp(str, "skewX", 5) == 0)
      str += nsvg__parseSkewX(p, str);
    else if (strncmp(str, "skewY", 5) == 0)
      str += nsvg__parseSkewY(p, str);
    else
      ++str;
  }
}

void nsvg__parseStyle(struct NSVGParser *p, const char *str);

int nsvg__parseAttr(struct NSVGParser *p, const char *name, const char *value) {
  struct NSVGAttrib *attr = nsvg__getAttr(p);
  if (!attr) return 0;

  if (strcmp(name, "style") == 0) {
    nsvg__parseStyle(p, value);
  } else if (strcmp(name, "display") == 0) {
    if (strcmp(value, "none") == 0)
      attr->visible = 0;
    else
      attr->visible = 1;
  } else if (strcmp(name, "fill") == 0) {
    if (strcmp(value, "none") == 0) {
      attr->hasFill = 0;
    } else {
      attr->hasFill   = 1;
      attr->fillColor = nsvg__parseColor(value);
    }
  } else if (strcmp(name, "fill-opacity") == 0) {
    attr->fillOpacity = nsvg__parseFloat(value);
  } else if (strcmp(name, "stroke") == 0) {
    if (strcmp(value, "none") == 0) {
      attr->hasStroke = 0;
    } else {
      attr->hasStroke   = 1;
      attr->strokeColor = nsvg__parseColor(value);
    }
  } else if (strcmp(name, "stroke-width") == 0) {
    attr->strokeWidth = nsvg__parseFloat(value);
  } else if (strcmp(name, "stroke-opacity") == 0) {
    attr->strokeOpacity = nsvg__parseFloat(value);
  } else if (strcmp(name, "transform") == 0) {
    nsvg__parseTransform(p, value);
  } else {
    return 0;
  }
  return 1;
}

int nsvg__parseNameValue(struct NSVGParser *p, const char *start,
                         const char *end) {
  const char *str;
  const char *val;
  char name[512];
  char value[512];
  int n;

  str = start;
  while (str < end && *str != ':') ++str;

  val = str;

  // Right Trim
  while (str > start && (*str == ':' || nsvg__isspace(*str))) --str;
  ++str;

  n              = (int)(str - start);
  if (n > 511) n = 511;
  if (n) memcpy(name, start, n);
  name[n] = 0;

  while (val < end && (*val == ':' || nsvg__isspace(*val))) ++val;

  n              = (int)(end - val);
  if (n > 511) n = 511;
  if (n) memcpy(value, val, n);
  value[n] = 0;

  return nsvg__parseAttr(p, name, value);
}

void nsvg__parseStyle(struct NSVGParser *p, const char *str) {
  const char *start;
  const char *end;

  while (*str) {
    // Left Trim
    while (*str && nsvg__isspace(*str)) ++str;
    start = str;
    while (*str && *str != ';') ++str;
    end = str;

    // Right Trim
    while (end > start && (*end == ';' || nsvg__isspace(*end))) --end;
    ++end;

    nsvg__parseNameValue(p, start, end);
    if (*str) ++str;
  }
}

void nsvg__parseAttribs(struct NSVGParser *p, const char **attr) {
  int i;
  for (i = 0; attr[i]; i += 2) {
    if (strcmp(attr[i], "style") == 0)
      nsvg__parseStyle(p, attr[i + 1]);
    else
      nsvg__parseAttr(p, attr[i], attr[i + 1]);
  }
}

int nsvg__getArgsPerElement(char cmd) {
  switch (cmd) {
  case 'v':
  case 'V':
  case 'h':
  case 'H':
    return 1;
  case 'm':
  case 'M':
  case 'l':
  case 'L':
  case 't':
  case 'T':
    return 2;
  case 'q':
  case 'Q':
  case 's':
  case 'S':
    return 4;
  case 'c':
  case 'C':
    return 6;
  case 'a':
  case 'A':
    return 7;
  }
  return 0;
}

void nsvg__pathMoveTo(struct NSVGParser *p, float *cpx, float *cpy, float *args,
                      int rel) {
  if (rel) {
    *cpx += args[0];
    *cpy += args[1];
  } else {
    *cpx = args[0];
    *cpy = args[1];
  }
  nsvg__moveTo(p, *cpx, *cpy);
}

void nsvg__pathLineTo(struct NSVGParser *p, float *cpx, float *cpy, float *args,
                      int rel) {
  if (rel) {
    *cpx += args[0];
    *cpy += args[1];
  } else {
    *cpx = args[0];
    *cpy = args[1];
  }
  nsvg__lineTo(p, *cpx, *cpy);
}

void nsvg__pathHLineTo(struct NSVGParser *p, float *cpx, float *cpy,
                       float *args, int rel) {
  if (rel)
    *cpx += args[0];
  else
    *cpx = args[0];
  nsvg__lineTo(p, *cpx, *cpy);
}

void nsvg__pathVLineTo(struct NSVGParser *p, float *cpx, float *cpy,
                       float *args, int rel) {
  if (rel)
    *cpy += args[0];
  else
    *cpy = args[0];
  nsvg__lineTo(p, *cpx, *cpy);
}

void nsvg__pathCubicBezTo(struct NSVGParser *p, float *cpx, float *cpy,
                          float *cpx2, float *cpy2, float *args, int rel) {
  float x1, y1, x2, y2, cx1, cy1, cx2, cy2;

  x1 = *cpx;
  y1 = *cpy;
  if (rel) {
    cx1 = *cpx + args[0];
    cy1 = *cpy + args[1];
    cx2 = *cpx + args[2];
    cy2 = *cpy + args[3];
    x2  = *cpx + args[4];
    y2  = *cpy + args[5];
  } else {
    cx1 = args[0];
    cy1 = args[1];
    cx2 = args[2];
    cy2 = args[3];
    x2  = args[4];
    y2  = args[5];
  }

  nsvg__cubicBezTo(p, cx1, cy1, cx2, cy2, x2, y2);

  *cpx2 = cx2;
  *cpy2 = cy2;
  *cpx  = x2;
  *cpy  = y2;
}

void nsvg__pathCubicBezShortTo(struct NSVGParser *p, float *cpx, float *cpy,
                               float *cpx2, float *cpy2, float *args, int rel) {
  float x1, y1, x2, y2, cx1, cy1, cx2, cy2;

  x1 = *cpx;
  y1 = *cpy;
  if (rel) {
    cx2 = *cpx + args[0];
    cy2 = *cpy + args[1];
    x2  = *cpx + args[2];
    y2  = *cpy + args[3];
  } else {
    cx2 = args[0];
    cy2 = args[1];
    x2  = args[2];
    y2  = args[3];
  }

  cx1 = 2 * x1 - *cpx2;
  cy1 = 2 * y1 - *cpy2;

  nsvg__cubicBezTo(p, cx1, cy1, cx2, cy2, x2, y2);

  *cpx2 = cx2;
  *cpy2 = cy2;
  *cpx  = x2;
  *cpy  = y2;
}

void nsvg__pathQuadBezTo(struct NSVGParser *p, float *cpx, float *cpy,
                         float *cpx2, float *cpy2, float *args, int rel) {
  float x1, y1, x2, y2, cx, cy;
  float cx1, cy1, cx2, cy2;

  x1 = *cpx;
  y1 = *cpy;
  if (rel) {
    cx = *cpx + args[0];
    cy = *cpy + args[1];
    x2 = *cpx + args[2];
    y2 = *cpy + args[3];
  } else {
    cx = args[0];
    cy = args[1];
    x2 = args[2];
    y2 = args[3];
  }

  // Convert to cubix bezier
  cx1 = x1 + 2.0f / 3.0f * (cx - x1);
  cy1 = y1 + 2.0f / 3.0f * (cy - y1);
  cx2 = x2 + 2.0f / 3.0f * (cx - x2);
  cy2 = y2 + 2.0f / 3.0f * (cy - y2);
  nsvg__cubicBezTo(p, cx1, cy1, cx2, cy2, x2, y2);

  *cpx2 = cx;
  *cpy2 = cy;
  *cpx  = x2;
  *cpy  = y2;
}

void nsvg__pathQuadBezShortTo(struct NSVGParser *p, float *cpx, float *cpy,
                              float *cpx2, float *cpy2, float *args, int rel) {
  float x1, y1, x2, y2, cx, cy;
  float cx1, cy1, cx2, cy2;

  x1 = *cpx;
  y1 = *cpy;
  if (rel) {
    x2 = *cpx + args[0];
    y2 = *cpy + args[1];
  } else {
    x2 = args[0];
    y2 = args[1];
  }

  cx = 2 * x1 - *cpx2;
  cy = 2 * y1 - *cpy2;

  // Convert to cubix bezier
  cx1 = x1 + 2.0f / 3.0f * (cx - x1);
  cy1 = y1 + 2.0f / 3.0f * (cy - y1);
  cx2 = x2 + 2.0f / 3.0f * (cx - x2);
  cy2 = y2 + 2.0f / 3.0f * (cy - y2);
  nsvg__cubicBezTo(p, cx1, cy1, cx2, cy2, x2, y2);

  *cpx2 = cx;
  *cpy2 = cy;
  *cpx  = x2;
  *cpy  = y2;
}

float nsvg__sqr(float x) { return x * x; }
float nsvg__vmag(float x, float y) { return sqrtf(x * x + y * y); }

float nsvg__vecrat(float ux, float uy, float vx, float vy) {
  return (ux * vx + uy * vy) / (nsvg__vmag(ux, uy) * nsvg__vmag(vx, vy));
}

float nsvg__vecang(float ux, float uy, float vx, float vy) {
  float r          = nsvg__vecrat(ux, uy, vx, vy);
  if (r < -1.0f) r = -1.0f;
  if (r > 1.0f) r  = 1.0f;
  return ((ux * vy < uy * vx) ? -1.0f : 1.0f) * acosf(r);
}

void nsvg__pathArcTo(struct NSVGParser *p, float *cpx, float *cpy, float *args,
                     int rel) {
  // Ported from canvg (https://code.google.com/p/canvg/)
  float rx, ry, rotx;
  float x1, y1, x2, y2, cx, cy, dx, dy, d;
  float x1p, y1p, cxp, cyp, s, sa, sb;
  float ux, uy, vx, vy, a1, da;
  float x, y, tanx, tany, a, px, py, ptanx, ptany, t[6];
  float sinrx, cosrx;
  int fa, fs;
  int i, ndivs;
  float hda, kappa;

  rx   = fabsf(args[0]);                 // y radius
  ry   = fabsf(args[1]);                 // x radius
  rotx = args[2] / 180.0f * NSVG_PI;     // x rotation engle
  fa   = fabsf(args[3]) > 1e-6 ? 1 : 0;  // Large arc
  fs   = fabsf(args[4]) > 1e-6 ? 1 : 0;  // Sweep direction
  x1   = *cpx;                           // start point
  y1   = *cpy;
  if (rel) {  // end point
    x2 = *cpx + args[5];
    y2 = *cpy + args[6];
  } else {
    x2 = args[5];
    y2 = args[6];
  }

  dx = x1 - x2;
  dy = y1 - y2;
  d  = sqrtf(dx * dx + dy * dy);
  if (d < 1e-6f || rx < 1e-6f || ry < 1e-6f) {
    // The arc degenerates to a line
    nsvg__lineTo(p, x2, y2);
    *cpx = x2;
    *cpy = y2;
    return;
  }

  sinrx = sinf(rotx);
  cosrx = cosf(rotx);

  // Convert to center point parameterization.
  // http://www.w3.org/TR/SVG11/implnote.html#ArcImplementationNotes
  // 1) Compute x1', y1'
  x1p = cosrx * dx / 2.0f + sinrx * dy / 2.0f;
  y1p = -sinrx * dx / 2.0f + cosrx * dy / 2.0f;
  d   = nsvg__sqr(x1p) / nsvg__sqr(rx) + nsvg__sqr(y1p) / nsvg__sqr(ry);
  if (d > 1) {
    d = sqrtf(d);
    rx *= d;
    ry *= d;
  }
  // 2) Compute cx', cy'
  s  = 0.0f;
  sa = nsvg__sqr(rx) * nsvg__sqr(ry) - nsvg__sqr(rx) * nsvg__sqr(y1p) -
       nsvg__sqr(ry) * nsvg__sqr(x1p);
  sb = nsvg__sqr(rx) * nsvg__sqr(y1p) + nsvg__sqr(ry) * nsvg__sqr(x1p);
  if (sa < 0.0f) sa = 0.0f;
  if (sb > 0.0f) s  = sqrtf(sa / sb);
  if (fa == fs) s   = -s;
  cxp               = s * rx * y1p / ry;
  cyp               = s * -ry * x1p / rx;

  // 3) Compute cx,cy from cx',cy'
  cx = (x1 + x2) / 2.0f + cosrx * cxp - sinrx * cyp;
  cy = (y1 + y2) / 2.0f + sinrx * cxp + cosrx * cyp;

  // 4) Calculate theta1, and delta theta.
  ux = (x1p - cxp) / rx;
  uy = (y1p - cyp) / ry;
  vx = (-x1p - cxp) / rx;
  vy = (-y1p - cyp) / ry;
  a1 = nsvg__vecang(1.0f, 0.0f, ux, uy);  // Initial angle
  da = nsvg__vecang(ux, uy, vx, vy);      // Delta angle

  //        if (vecrat(ux,uy,vx,vy) <= -1.0f) da = NSVG_PI;
  //        if (vecrat(ux,uy,vx,vy) >= 1.0f) da = 0;

  if (fa) {
    // Choose large arc
    if (da > 0.0f)
      da = da - 2 * NSVG_PI;
    else
      da = 2 * NSVG_PI + da;
  }

  // Approximate the arc using cubic spline segments.
  t[0] = cosrx;
  t[1] = sinrx;
  t[2] = -sinrx;
  t[3] = cosrx;
  t[4] = cx;
  t[5] = cy;

  // Split arc into max 90 degree segments.
  ndivs                = (int)(fabsf(da) / (NSVG_PI * 0.5f) + 0.5f);
  hda                  = (da / (float)ndivs) / 2.0f;
  kappa                = fabsf(4.0f / 3.0f * (1.0f - cosf(hda)) / sinf(hda));
  if (da < 0.0f) kappa = -kappa;

  for (i = 0; i <= ndivs; i++) {
    a  = a1 + da * (i / (float)ndivs);
    dx = cosf(a);
    dy = sinf(a);
    nsvg__xformPoint(&x, &y, dx * rx, dy * ry, t);  // position
    nsvg__xformVec(&tanx, &tany, -dy * rx * kappa, dx * ry * kappa,
                   t);  // tangent
    if (i > 0)
      nsvg__cubicBezTo(p, px + ptanx, py + ptany, x - tanx, y - tany, x, y);
    px    = x;
    py    = y;
    ptanx = tanx;
    ptany = tany;
  }

  *cpx = x2;
  *cpy = y2;
}

void nsvg__parsePath(struct NSVGParser *p, const char **attr) {
  const char *s;
  char cmd;
  float args[10];
  int nargs;
  int rargs;
  float cpx, cpy, cpx2, cpy2;
  const char *tmp[4];
  char closedFlag;
  int i;
  char item[64];

  for (i = 0; attr[i]; i += 2) {
    if (strcmp(attr[i], "d") == 0) {
      s = attr[i + 1];

      nsvg__resetPath(p);
      cpx        = 0;
      cpy        = 0;
      closedFlag = 0;
      nargs      = 0;

      while (*s) {
        s = nsvg__getNextPathItem(s, item);
        if (!*item) break;
        if (nsvg__isnum(item[0])) {
          if (nargs < 10) args[nargs++] = (float)atof(item);
          if (nargs >= rargs) {
            switch (cmd) {
            case 'm':
            case 'M':
              nsvg__pathMoveTo(p, &cpx, &cpy, args, cmd == 'm' ? 1 : 0);
              // Moveto can be followed by multiple coordinate pairs,
              // which should be treated as linetos.
              cmd   = (cmd == 'm') ? 'l' : 'L';
              rargs = nsvg__getArgsPerElement(cmd);
              break;
            case 'l':
            case 'L':
              nsvg__pathLineTo(p, &cpx, &cpy, args, cmd == 'l' ? 1 : 0);
              break;
            case 'H':
            case 'h':
              nsvg__pathHLineTo(p, &cpx, &cpy, args, cmd == 'h' ? 1 : 0);
              break;
            case 'V':
            case 'v':
              nsvg__pathVLineTo(p, &cpx, &cpy, args, cmd == 'v' ? 1 : 0);
              break;
            case 'C':
            case 'c':
              nsvg__pathCubicBezTo(p, &cpx, &cpy, &cpx2, &cpy2, args,
                                   cmd == 'c' ? 1 : 0);
              break;
            case 'S':
            case 's':
              nsvg__pathCubicBezShortTo(p, &cpx, &cpy, &cpx2, &cpy2, args,
                                        cmd == 's' ? 1 : 0);
              break;
            case 'Q':
            case 'q':
              nsvg__pathQuadBezTo(p, &cpx, &cpy, &cpx2, &cpy2, args,
                                  cmd == 'q' ? 1 : 0);
              break;
            case 'T':
            case 't':
              nsvg__pathQuadBezShortTo(p, &cpx, &cpy, &cpx2, &cpy2, args,
                                       cmd == 's' ? 1 : 0);
              break;
            case 'A':
            case 'a':
              nsvg__pathArcTo(p, &cpx, &cpy, args, cmd == 'a' ? 1 : 0);
              break;
            default:
              if (nargs >= 2) {
                cpx = args[nargs - 2];
                cpy = args[nargs - 1];
              }
              break;
            }
            nargs = 0;
          }
        } else {
          cmd   = item[0];
          rargs = nsvg__getArgsPerElement(cmd);
          if (cmd == 'M' || cmd == 'm') {
            // Commit path.
            if (p->npts > 0) nsvg__addPath(p, closedFlag);
            // Start new subpath.
            nsvg__resetPath(p);
            closedFlag = 0;
            nargs      = 0;
          } else if (cmd == 'Z' || cmd == 'z') {
            closedFlag = 1;
            // Commit path.
            if (p->npts > 0) nsvg__addPath(p, closedFlag);
            // Start new subpath.
            nsvg__resetPath(p);
            closedFlag = 0;
            nargs      = 0;
          }
        }
      }
      // Commit path.
      if (p->npts) nsvg__addPath(p, closedFlag);
    } else {
      tmp[0] = attr[i];
      tmp[1] = attr[i + 1];
      tmp[2] = 0;
      tmp[3] = 0;
      nsvg__parseAttribs(p, tmp);
    }
  }

  nsvg__addShape(p);
}

void nsvg__parseRect(struct NSVGParser *p, const char **attr) {
  float x  = 0.0f;
  float y  = 0.0f;
  float w  = 0.0f;
  float h  = 0.0f;
  float rx = -1.0f;  // marks not set
  float ry = -1.0f;
  int i;

  for (i = 0; attr[i]; i += 2) {
    if (!nsvg__parseAttr(p, attr[i], attr[i + 1])) {
      if (strcmp(attr[i], "x") == 0) x      = nsvg__parseFloat(attr[i + 1]);
      if (strcmp(attr[i], "y") == 0) y      = nsvg__parseFloat(attr[i + 1]);
      if (strcmp(attr[i], "width") == 0) w  = nsvg__parseFloat(attr[i + 1]);
      if (strcmp(attr[i], "height") == 0) h = nsvg__parseFloat(attr[i + 1]);
      if (strcmp(attr[i], "rx") == 0) rx = fabsf(nsvg__parseFloat(attr[i + 1]));
      if (strcmp(attr[i], "ry") == 0) ry = fabsf(nsvg__parseFloat(attr[i + 1]));
    }
  }

  if (rx < 0.0f && ry > 0.0f) rx = ry;
  if (ry < 0.0f && rx > 0.0f) ry = rx;
  if (rx < 0.0f) rx              = 0.0f;
  if (ry < 0.0f) ry              = 0.0f;
  if (rx > w / 2.0f) rx          = w / 2.0f;
  if (ry > h / 2.0f) ry          = h / 2.0f;

  if (w != 0.0f && h != 0.0f) {
    nsvg__resetPath(p);

    if (rx < 0.00001f || ry < 0.0001f) {
      nsvg__moveTo(p, x, y);
      nsvg__lineTo(p, x + w, y);
      nsvg__lineTo(p, x + w, y + h);
      nsvg__lineTo(p, x, y + h);
    } else {
      // Rounded rectangle
      nsvg__moveTo(p, x + rx, y);
      nsvg__lineTo(p, x + w - rx, y);
      nsvg__cubicBezTo(p, x + w - rx * (1 - NSVG_KAPPA90), y, x + w,
                       y + ry * (1 - NSVG_KAPPA90), x + w, y + ry);
      nsvg__lineTo(p, x + w, y + h - ry);
      nsvg__cubicBezTo(p, x + w, y + h - ry * (1 - NSVG_KAPPA90),
                       x + w - rx * (1 - NSVG_KAPPA90), y + h, x + w - rx,
                       y + h);
      nsvg__lineTo(p, x + rx, y + h);
      nsvg__cubicBezTo(p, x + rx * (1 - NSVG_KAPPA90), y + h, x,
                       y + h - ry * (1 - NSVG_KAPPA90), x, y + h - ry);
      nsvg__lineTo(p, x, y + ry);
      nsvg__cubicBezTo(p, x, y + ry * (1 - NSVG_KAPPA90),
                       x + rx * (1 - NSVG_KAPPA90), y, x + rx, y);
    }

    nsvg__addPath(p, 1);

    nsvg__addShape(p);
  }
}

void nsvg__parseCircle(struct NSVGParser *p, const char **attr) {
  float cx = 0.0f;
  float cy = 0.0f;
  float r  = 0.0f;
  int i;

  for (i = 0; attr[i]; i += 2) {
    if (!nsvg__parseAttr(p, attr[i], attr[i + 1])) {
      if (strcmp(attr[i], "cx") == 0) cx = nsvg__parseFloat(attr[i + 1]);
      if (strcmp(attr[i], "cy") == 0) cy = nsvg__parseFloat(attr[i + 1]);
      if (strcmp(attr[i], "r") == 0) r   = fabsf(nsvg__parseFloat(attr[i + 1]));
    }
  }

  if (r > 0.0f) {
    nsvg__resetPath(p);

    nsvg__moveTo(p, cx + r, cy);
    nsvg__cubicBezTo(p, cx + r, cy + r * NSVG_KAPPA90, cx + r * NSVG_KAPPA90,
                     cy + r, cx, cy + r);
    nsvg__cubicBezTo(p, cx - r * NSVG_KAPPA90, cy + r, cx - r,
                     cy + r * NSVG_KAPPA90, cx - r, cy);
    nsvg__cubicBezTo(p, cx - r, cy - r * NSVG_KAPPA90, cx - r * NSVG_KAPPA90,
                     cy - r, cx, cy - r);
    nsvg__cubicBezTo(p, cx + r * NSVG_KAPPA90, cy - r, cx + r,
                     cy - r * NSVG_KAPPA90, cx + r, cy);

    nsvg__addPath(p, 1);

    nsvg__addShape(p);
  }
}

void nsvg__parseEllipse(struct NSVGParser *p, const char **attr) {
  float cx = 0.0f;
  float cy = 0.0f;
  float rx = 0.0f;
  float ry = 0.0f;
  int i;

  for (i = 0; attr[i]; i += 2) {
    if (!nsvg__parseAttr(p, attr[i], attr[i + 1])) {
      if (strcmp(attr[i], "cx") == 0) cx = nsvg__parseFloat(attr[i + 1]);
      if (strcmp(attr[i], "cy") == 0) cy = nsvg__parseFloat(attr[i + 1]);
      if (strcmp(attr[i], "rx") == 0) rx = fabsf(nsvg__parseFloat(attr[i + 1]));
      if (strcmp(attr[i], "ry") == 0) ry = fabsf(nsvg__parseFloat(attr[i + 1]));
    }
  }

  if (rx > 0.0f && ry > 0.0f) {
    nsvg__resetPath(p);

    nsvg__moveTo(p, cx + rx, cy);
    nsvg__cubicBezTo(p, cx + rx, cy + ry * NSVG_KAPPA90, cx + rx * NSVG_KAPPA90,
                     cy + ry, cx, cy + ry);
    nsvg__cubicBezTo(p, cx - rx * NSVG_KAPPA90, cy + ry, cx - rx,
                     cy + ry * NSVG_KAPPA90, cx - rx, cy);
    nsvg__cubicBezTo(p, cx - rx, cy - ry * NSVG_KAPPA90, cx - rx * NSVG_KAPPA90,
                     cy - ry, cx, cy - ry);
    nsvg__cubicBezTo(p, cx + rx * NSVG_KAPPA90, cy - ry, cx + rx,
                     cy - ry * NSVG_KAPPA90, cx + rx, cy);

    nsvg__addPath(p, 1);

    nsvg__addShape(p);
  }
}

void nsvg__parseLine(struct NSVGParser *p, const char **attr) {
  float x1 = 0.0;
  float y1 = 0.0;
  float x2 = 0.0;
  float y2 = 0.0;
  int i;

  for (i = 0; attr[i]; i += 2) {
    if (!nsvg__parseAttr(p, attr[i], attr[i + 1])) {
      if (strcmp(attr[i], "x1") == 0) x1 = nsvg__parseFloat(attr[i + 1]);
      if (strcmp(attr[i], "y1") == 0) y1 = nsvg__parseFloat(attr[i + 1]);
      if (strcmp(attr[i], "x2") == 0) x2 = nsvg__parseFloat(attr[i + 1]);
      if (strcmp(attr[i], "y2") == 0) y2 = nsvg__parseFloat(attr[i + 1]);
    }
  }

  nsvg__resetPath(p);

  nsvg__moveTo(p, x1, y1);
  nsvg__lineTo(p, x2, y2);

  nsvg__addPath(p, 0);

  nsvg__addShape(p);
}

void nsvg__parsePoly(struct NSVGParser *p, const char **attr, int closeFlag) {
  int i;
  const char *s;
  float args[2];
  int nargs, npts = 0;
  char item[64];

  nsvg__resetPath(p);

  for (i = 0; attr[i]; i += 2) {
    if (!nsvg__parseAttr(p, attr[i], attr[i + 1])) {
      if (strcmp(attr[i], "points") == 0) {
        s     = attr[i + 1];
        nargs = 0;
        while (*s) {
          s             = nsvg__getNextPathItem(s, item);
          args[nargs++] = (float)atof(item);
          if (nargs >= 2) {
            if (npts == 0)
              nsvg__moveTo(p, args[0], args[1]);
            else
              nsvg__lineTo(p, args[0], args[1]);
            nargs = 0;
            npts++;
          }
        }
      }
    }
  }

  nsvg__addPath(p, (char)closeFlag);

  nsvg__addShape(p);
}

void nsvg__parseSVG(struct NSVGParser *p, const char **attr) {
  int i;
  for (i = 0; attr[i]; i += 2) {
    if (!nsvg__parseAttr(p, attr[i], attr[i + 1])) {
      if (strcmp(attr[i], "width") == 0) {
        p->image->wunits[0] = '\0';
        sscanf(attr[i + 1], "%f%s", &p->image->width, p->image->wunits);
      } else if (strcmp(attr[i], "height") == 0) {
        p->image->hunits[0] = '\0';
        sscanf(attr[i + 1], "%f%s", &p->image->height, p->image->hunits);
      }
    }
  }
}

void nsvg__startElement(void *ud, const char *el, const char **attr) {
  struct NSVGParser *p = (struct NSVGParser *)ud;

  // Skip everything in defs
  if (p->defsFlag) return;

  if (strcmp(el, "g") == 0) {
    nsvg__pushAttr(p);
    nsvg__parseAttribs(p, attr);
  } else if (strcmp(el, "path") == 0) {
    if (p->pathFlag)  // Do not allow nested paths.
      return;
    nsvg__pushAttr(p);
    nsvg__parsePath(p, attr);
    nsvg__popAttr(p);
  } else if (strcmp(el, "rect") == 0) {
    nsvg__pushAttr(p);
    nsvg__parseRect(p, attr);
    nsvg__popAttr(p);
  } else if (strcmp(el, "circle") == 0) {
    nsvg__pushAttr(p);
    nsvg__parseCircle(p, attr);
    nsvg__popAttr(p);
  } else if (strcmp(el, "ellipse") == 0) {
    nsvg__pushAttr(p);
    nsvg__parseEllipse(p, attr);
    nsvg__popAttr(p);
  } else if (strcmp(el, "line") == 0) {
    nsvg__pushAttr(p);
    nsvg__parseLine(p, attr);
    nsvg__popAttr(p);
  } else if (strcmp(el, "polyline") == 0) {
    nsvg__pushAttr(p);
    nsvg__parsePoly(p, attr, 0);
    nsvg__popAttr(p);
  } else if (strcmp(el, "polygon") == 0) {
    nsvg__pushAttr(p);
    nsvg__parsePoly(p, attr, 1);
    nsvg__popAttr(p);
  } else if (strcmp(el, "defs") == 0) {
    p->defsFlag = 1;
  } else if (strcmp(el, "svg") == 0) {
    nsvg__parseSVG(p, attr);
  }
}

void nsvg__endElement(void *ud, const char *el) {
  struct NSVGParser *p = (struct NSVGParser *)ud;

  if (strcmp(el, "g") == 0) {
    nsvg__popAttr(p);
  } else if (strcmp(el, "path") == 0) {
    p->pathFlag = 0;
  } else if (strcmp(el, "defs") == 0) {
    p->defsFlag = 0;
  }
}

void nsvg__content(void *ud, const char *s) {
  // empty
}

void dump(struct NSVGimage *image) {
  struct NSVGshape *shape;
  if (image == NULL) return;
  shape = image->shapes;
  while (shape != NULL) {
    struct NSVGpath *path;
    path              = shape->paths;
    while (path) path = path->next;
    shape             = shape->next;
  }
}

struct NSVGimage *nsvgParse(char *input) {
  struct NSVGParser *p;
  struct NSVGimage *ret = 0;

  p = nsvg__createParser();
  if (p == NULL) {
    return NULL;
  }

  nsvg__parseXML(input, nsvg__startElement, nsvg__endElement, nsvg__content, p);

  ret      = p->image;
  p->image = NULL;

  dump(ret);

  nsvg__deleteParser(p);

  return ret;
}

struct NSVGimage *nsvgParseFromFile(const char *filename) {
  FILE *fp = NULL;
  int size;
  char *data              = NULL;
  struct NSVGimage *image = NULL;

  fp = fopen(filename, "rb");
  if (!fp) goto error;
  fseek(fp, 0, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  data = (char *)malloc(size + 1);
  if (data == NULL) goto error;
  fread(data, size, 1, fp);
  data[size] = '\0';  // Must be null terminated.
  fclose(fp);
  image = nsvgParse(data);
  free(data);

  return image;

error:
  if (fp) fclose(fp);
  if (data) free(data);
  if (image) nsvgDelete(image);
  return NULL;
}

}  // namespace svg_parser

//=------------------------------------------------------------------------------------------------------------------------------
//=------------------------------------------------------------------------------------------------------------------------------
//=------------------------------------------------------------------------------------------------------------------------------
//=------------------------------------------------------------------------------------------------------------------------------

class TImageWriterSvg final : public TImageWriter {
public:
  TImageWriterSvg(const TFilePath &, TPropertyGroup *);
  ~TImageWriterSvg() {}

private:
  // double m_maxThickness;
  // not implemented
  TImageWriterSvg(const TImageWriterSvg &);
  TImageWriterSvg &operator=(const TImageWriterSvg &src);

public:
  void save(const TImageP &) override;
};

//-----------------------------------------------------------------------------
class TImageReaderSvg final : public TImageReader {
  TLevelP m_level;

public:
  TImageReaderSvg(const TFilePath &path, TLevelP &level)
      : TImageReader(path), m_level(level) {}

  TImageP load() override;
};

TImageWriterP TLevelWriterSvg::getFrameWriter(TFrameId fid) {
  TImageWriterSvg *iwm =
      new TImageWriterSvg(m_path.withFrame(fid), getProperties());
  return TImageWriterP(iwm);
}

//-----------------------------------------------------------------------------
TImageWriterSvg::TImageWriterSvg(const TFilePath &f, TPropertyGroup *prop)
    : TImageWriter(f)
//, m_maxThickness(0)
{
  setProperties(prop);
}

//-----------------------------------------------------------------------------

TLevelWriterSvg::TLevelWriterSvg(const TFilePath &path, TPropertyGroup *winfo)
    : TLevelWriter(path, winfo)
//, m_pli         (0)
//, m_frameNumber (0)
{}

//-----------------------------------------------------------------------------

static void writeRegion(TRegion *r, TPalette *plt, QTextStream &out,
                        double ly) {
  if (r->getEdgeCount() == 0) return;
  std::vector<const TQuadratic *> quadsOutline;

  for (int i = 0; i < (int)r->getEdgeCount(); i++) {
    TEdge *e   = r->getEdge(i);
    TStroke *s = e->m_s;
    int index0, index1;
    double t0, t1;
    double w0 = e->m_w0, w1 = e->m_w1;

    if (w0 > w1) {
      TStroke *s1 = new TStroke(*s);
      s1->changeDirection();
      double totalLength = s->getLength();
      w0 = s1->getParameterAtLength(totalLength - s->getLength(w0));
      w1 = s1->getParameterAtLength(totalLength - s->getLength(w1));
      s  = s1;

      assert(w0 <= w1);
    }

    s->getChunkAndT(w0, index0, t0);
    s->getChunkAndT(w1, index1, t1);

    for (int j = index0; j <= index1; j++) {
      const TQuadratic *q = s->getChunk(j);
      if (j == index0 && t0 != 0) {
        TQuadratic q1, *q2 = new TQuadratic();
        q->split(t0, q1, *q2);
        q = q2;
      }
      if (j == index1 && t1 != 1) {
        TQuadratic *q1 = new TQuadratic(), q2;
        q->split(t1, *q1, q2);
        q = q1;
      }
      quadsOutline.push_back(q);
    }
  }

  if (quadsOutline.empty()) return;

  out << "<path  \n";
  TPixel32 col = plt->getStyle(r->getStyle())->getMainColor();
  if (col == TPixel::Transparent) col == TPixel::White;

  out << "style=\"fill:rgb(" << col.r << "," << col.g << "," << col.b
      << ")\" \n";
  out << "d=\"M " << quadsOutline[0]->getP0().x << " "
      << ly - quadsOutline[0]->getP0().y << "\n";

  for (int i = 0; i < quadsOutline.size(); i++)
    out << "Q " << quadsOutline[i]->getP1().x << ","
        << ly - quadsOutline[i]->getP1().y << "," << quadsOutline[i]->getP2().x
        << "," << ly - quadsOutline[i]->getP2().y << "\n";
  out << " \" /> \n";
  for (int i = 0; i < (int)r->getSubregionCount(); i++)
    writeRegion(r->getSubregion(i), plt, out, ly);
}

//--------------------------------------------------------------------------------------

static void writeOutlineStroke(TStroke *s, TPalette *plt, QTextStream &out,
                               double ly, double quality) {
  if (s->getChunkCount() == 0) return;
  if (s->getMaxThickness() == 0) return;

  std::vector<TQuadratic *> quadsOutline;
  computeOutlines(s, 0, s->getChunkCount() - 1, quadsOutline, quality);
  if (quadsOutline.empty()) return;

  out << "<path  \n";
  TPixel32 col = plt->getStyle(s->getStyle())->getMainColor();

  out << "style=\"fill:rgb(" << col.r << "," << col.g << "," << col.b
      << ")\" \n";
  out << "d=\"M " << quadsOutline[0]->getP0().x << " "
      << ly - quadsOutline[0]->getP0().y << "\n";

  for (int i = 0; i < quadsOutline.size(); i++)
    out << "Q " << quadsOutline[i]->getP1().x << ","
        << ly - quadsOutline[i]->getP1().y << "," << quadsOutline[i]->getP2().x
        << "," << ly - quadsOutline[i]->getP2().y << "\n";
  out << " \" /> \n";
}

//----------------------------------------------------------

static double computeAverageThickness(const TStroke *s) {
  int count = s->getControlPointCount();

  double resThick = 0;

  for (int i = 0; i < s->getControlPointCount(); i++) {
    double thick = s->getControlPoint(i).thick;
    if (i >= 2 && i < s->getControlPointCount() - 2) resThick += thick;
  }

  if (count < 6) return s->getControlPoint(count / 2 + 1).thick;
  return resThick / (s->getControlPointCount() - 4);
}

//----------------------------------------------------------------

static void writeCenterlineStroke(TStroke *s, TPalette *plt, QTextStream &out,
                                  double ly) {
  if (s->getChunkCount() == 0) return;
  if (s->getMaxThickness() == 0) return;

  double thick = 2 * computeAverageThickness(s);

  out << "<path  \n";
  TPixel32 col = plt->getStyle(s->getStyle())->getMainColor();

  out << "style=\"stroke:rgb(" << col.r << "," << col.g << "," << col.b
      << ")\" stroke-width=\"" << thick << " \"  \n";
  out << "d=\"M " << s->getChunk(0)->getP0().x << " "
      << ly - s->getChunk(0)->getP0().y << "\n";

  for (int i = 0; i < s->getChunkCount(); i++)
    out << "Q " << s->getChunk(i)->getP1().x << ","
        << ly - s->getChunk(i)->getP1().y << "," << s->getChunk(i)->getP2().x
        << "," << ly - s->getChunk(i)->getP2().y << "\n";
  out << " \" /> \n";
}

//----------------------------------------------------------

Tiio::SvgWriterProperties::SvgWriterProperties()
    : m_strokeMode("Stroke Mode"), m_outlineQuality("Outline Quality") {
  m_strokeMode.addValue(L"Centerline");
  m_strokeMode.addValue(L"Outline");
  m_outlineQuality.addValue(L"High");
  m_outlineQuality.addValue(L"Medium");
  m_outlineQuality.addValue(L"Low");
  bind(m_strokeMode);
  bind(m_outlineQuality);
}

void Tiio::SvgWriterProperties::updateTranslation() {
  m_strokeMode.setQStringName(tr("Stroke Mode"));
  m_outlineQuality.setQStringName(tr("Outline Quality"));
  m_strokeMode.setItemUIName(L"Centerline", tr("Centerline"));
  m_strokeMode.setItemUIName(L"Outline", tr("Outline"));
  m_outlineQuality.setItemUIName(L"High", tr("High"));
  m_outlineQuality.setItemUIName(L"Medium", tr("Medium"));
  m_outlineQuality.setItemUIName(L"Low", tr("Low"));
}
//----------------------------------------------------------------------------

// void writeSvg(QString path, TVectorImageP v)
void TImageWriterSvg::save(const TImageP &img) {
  const TVectorImageP v = (const TVectorImageP)img;

  if (v->getStrokeCount() == 0) return;

  TPalette *plt = v->getPalette();

  TRectD r  = v->getBBox();
  double ly = r.getP00().y + r.getP11().y;

  QFile file(this->getFilePath().getQString());
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

  QTextStream out(&file);
  out.setRealNumberPrecision(1);
  out.setRealNumberNotation(QTextStream::FixedNotation);

  out << "<?xml version=\"1.0\"?>\n";
  out << "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" "
         "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n";

  out << "<g transform=\"translate(" << -r.getP00().x << "," << -r.getP00().y
      << ")\" stroke-width=\"0\" fill=\"none\" >\n";

  bool isCenterline =
      ((TEnumProperty *)(m_properties->getProperty("Stroke Mode")))
          ->getValue() == L"Centerline";
  double quality = 1;

  if (!isCenterline) {
    if (((TEnumProperty *)(m_properties->getProperty("Outline Quality")))
            ->getValue() == L"Low")
      quality = 200;
    else if (((TEnumProperty *)(m_properties->getProperty("Outline Quality")))
                 ->getValue() == L"Medium")
      quality = 10;
  }

  for (int j = 0; j < (int)v->getRegionCount(); j++)
    writeRegion(v->getRegion(j), plt, out, ly);
  for (int j = 0; j < (int)v->getStrokeCount(); j++)
    if (isCenterline)
      writeCenterlineStroke(v->getStroke(j), plt, out, ly);
    else
      writeOutlineStroke(v->getStroke(j), plt, out, ly, quality);
  out << "</g> \n";

  out << "</svg> \n";
}

//-----------------------------------------------------------------------------

namespace {
int addColorToPalette(TPalette *plt, unsigned int _color) {
  TPixel color(_color & 0xFF, (_color >> 8) & 0xFF, _color >> 16);
  for (int i = 0; i < plt->getStyleCount(); i++)
    if (plt->getStyle(i)->getMainColor() == color) return i;
  TPalette::Page *page = plt->getPage(0);
  int index            = page->addStyle(color);
  return index;  // plt->addStyle(color);
}

int findColor(TPalette *plt, unsigned int _color) {
  TPixel color(_color & 0xFF, (_color >> 8) & 0xFF, _color >> 16);
  for (int i = 0; i < plt->getStyleCount(); i++)
    if (plt->getStyle(i)->getMainColor() == color) return i;
  assert(false);
  return -1;
}

//-----------------------------------------------------------------------------

TStroke *buildStroke(NSVGpath *path, float width) {
  assert((path->npts - 1) % 3 == 0);

  TThickPoint p0 = TThickPoint(path->pts[0], -path->pts[1], width);
  std::vector<TThickPoint> points;

  points.push_back(p0);

  for (int i = 1; i < path->npts; i += 3) {
    std::vector<TThickQuadratic *> chunkArray;

    computeQuadraticsFromCubic(
        p0, TThickPoint(path->pts[2 * i], -path->pts[2 * i + 1], width),
        TThickPoint(path->pts[2 * i + 2], -path->pts[2 * i + 3], width),
        TThickPoint(path->pts[2 * i + 4], -path->pts[2 * i + 5], width), 0.01,
        chunkArray);

    for (int j = 0; j < chunkArray.size(); j++) {
      points.push_back(chunkArray[j]->getP1());
      points.push_back(chunkArray[j]->getP2());
    }
    p0 = chunkArray.back()->getP2();
  }

  if (points.empty()) return 0;

  if (path->closed) {
    if (points.back() != points.front()) {
      points.push_back(0.5 * (points.back() + points.front()));
      points.push_back(points.front());
    } else {
      int gasp = 0;
    }
  }
  TStroke *s = new TStroke(points);

  s->setSelfLoop(path->closed);

  return s;
}

}  // namespace

//-----------------------------------------------------------------------------

TImageP TImageReaderSvg::load() {
  NSVGimage *svgImg =
      nsvgParseFromFile(m_path.getQString().toStdString().c_str());
  if (!svgImg) return TImageP();

  TPalette *plt = m_level->getPalette();
  assert(plt);

  TVectorImage *vimage = new TVectorImage();
  vimage->setPalette(plt);

  for (NSVGshape *shape = svgImg->shapes; shape; shape = shape->next) {
    int inkIndex, paintIndex;
    NSVGpath *path = shape->paths;
    if (!path) continue;

    // TVectorImageP vapp = new TVectorImage();
    // TPalette* appPlt = new TPalette();
    // vapp->setPalette(appPlt);

    TPixel color(shape->fillColor & 0xFF, (shape->fillColor >> 8) & 0xFF,
                 shape->fillColor >> 16);
    if (!shape->hasFill) {
      assert(color == TPixel::Black);
      shape->hasFill = true;
    }
    if (shape->hasStroke) inkIndex = findColor(plt, shape->strokeColor);

    if (shape->hasFill) paintIndex = findColor(plt, shape->fillColor);

    // vapp->setPalette(plt.getPointer());
    int startStrokeIndex = vimage->getStrokeCount();
    for (; path; path = path->next) {
      TStroke *s = buildStroke(path, shape->hasStroke ? shape->strokeWidth : 0);
      if (!s) continue;
      s->setStyle(shape->hasStroke ? inkIndex : 0);
      vimage->addStroke(s);
    }
    if (startStrokeIndex == vimage->getStrokeCount()) continue;

    vimage->group(startStrokeIndex,
                  vimage->getStrokeCount() - startStrokeIndex);
    if (shape->hasFill) {
      vimage->enterGroup(startStrokeIndex);
      vimage->selectFill(TRectD(-9999999, -9999999, 9999999, 9999999), 0,
                         paintIndex, true, true, false);
      vimage->exitGroup();
    }

    /* vapp->findRegions();
if (paintIndex!=-1)
for (int i=0; i<(int)vapp->getRegionCount(); i++)
vapp->getRegion(i)->setStyle(paintIndex);
std::vector<int> indexes(vapp->getStrokeCount());
for (int i=0; i<(int)vapp->getStrokeCount() ;i++)
indexes[i] = vimage->getStrokeCount()+i;
vimage->insertImage(vapp, indexes);*/
    // delete appPlt;
  }

  nsvgDelete(svgImg);
  // if (m_level)
  // m_level->setPalette(plt);
  return TImageP(vimage);
}

//-----------------------------------------------------

TLevelReaderSvg::TLevelReaderSvg(const TFilePath &path) : TLevelReader(path) {}

//-----------------------------------------------------

TImageReaderP TLevelReaderSvg::getFrameReader(TFrameId fid) {
  return new TImageReaderSvg(getFilePath().withFrame(fid), m_level);
}

//-----------------------------------------------------

TLevelP TLevelReaderSvg::loadInfo() {
  m_level             = TLevelReader::loadInfo();
  TPalette *plt       = new TPalette();
  TLevel::Iterator it = m_level->begin();

  for (; it != m_level->end(); ++it) {
    NSVGimage *svgImg = nsvgParseFromFile(
        m_path.withFrame(it->first).getQString().toStdString().c_str());
    if (!svgImg) continue;

    for (NSVGshape *shape = svgImg->shapes; shape; shape = shape->next) {
      if (shape->hasStroke) addColorToPalette(plt, shape->strokeColor);

      if (shape->hasFill) addColorToPalette(plt, shape->fillColor);
    }

    nsvgDelete(svgImg);
  }

  m_level->setPalette(plt);
  return m_level;
}
