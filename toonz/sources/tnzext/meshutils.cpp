

// TnzCore includes
#include "tgl.h"

// TnzExt includes
#include "ext/ttexturesstorage.h"
#include "ext/plasticdeformerstorage.h"

// tcg includes
#include "tcg/tcg_iterator_ops.h"

#include "ext/meshutils.h"

//********************************************************************************************
//    Templated drawing functions
//********************************************************************************************

namespace {

struct NoColorFunction {
  void faceColor(int f, int m) {}
  void edgeColor(int e, int m) {}
  void vertexColor(int v, int m) {}
};

//-------------------------------------------------------------------------------

template <typename VerticesContainer, typename PointType,
          typename ColorFunction>
inline void tglDrawEdges(const TTextureMesh &mesh,
                         const VerticesContainer &vertices,
                         ColorFunction colorFunction) {
  // Draw the mesh wireframe
  glBegin(GL_LINES);

  TTextureMesh::edges_container::const_iterator et, eEnd = mesh.edges().end();

  for (et = mesh.edges().begin(); et != eEnd; ++et) {
    const TTextureMesh::edge_type &ed = *et;

    int v0 = ed.vertex(0), v1 = ed.vertex(1);

    const PointType &p0 = vertices[v0];
    const PointType &p1 = vertices[v1];

    colorFunction.edgeColor(et.index(), -1);

    colorFunction.vertexColor(v0, -1);
    glVertex2d(tcg::point_traits<PointType>::x(p0),
               tcg::point_traits<PointType>::y(p0));

    colorFunction.vertexColor(v1, -1);
    glVertex2d(tcg::point_traits<PointType>::x(p1),
               tcg::point_traits<PointType>::y(p1));
  }

  glEnd();
}

//-------------------------------------------------------------------------------

template <typename ColorFunction>
inline void tglDrawFaces(const TMeshImage &meshImage,
                         ColorFunction colorFunction) {
  glBegin(GL_TRIANGLES);

  int m, mCount = meshImage.meshes().size();
  for (m = 0; m != mCount; ++m) {
    const TTextureMesh &mesh                  = *meshImage.meshes()[m];
    const tcg::list<TTextureVertex> &vertices = mesh.vertices();

    // Draw the mesh wireframe
    TTextureMesh::faces_container::const_iterator ft, fEnd = mesh.faces().end();

    for (ft = mesh.faces().begin(); ft != fEnd; ++ft) {
      int v0, v1, v2;
      mesh.faceVertices(ft.index(), v0, v1, v2);

      const TTextureVertex &p0 = vertices[v0];
      const TTextureVertex &p1 = vertices[v1];
      const TTextureVertex &p2 = vertices[v2];

      colorFunction.faceColor(ft.index(), m);

      colorFunction.vertexColor(v0, m), glVertex2d(p0.P().x, p0.P().y);
      colorFunction.vertexColor(v1, m), glVertex2d(p1.P().x, p1.P().y);
      colorFunction.vertexColor(v2, m), glVertex2d(p2.P().x, p2.P().y);
    }
  }

  glEnd();
}

//-------------------------------------------------------------------------------

template <typename ColorFunction>
inline void tglDrawFaces(const TMeshImage &meshImage,
                         const PlasticDeformerDataGroup *group,
                         ColorFunction colorFunction) {
  glBegin(GL_TRIANGLES);

  // Draw faces according to the group's sorted faces list
  typedef std::vector<std::pair<int, int>> SortedFacesVector;

  const SortedFacesVector &sortedFaces     = group->m_sortedFaces;
  const std::vector<TTextureMeshP> &meshes = meshImage.meshes();

  int m = -1;
  const TTextureMesh *mesh;
  const double *dstCoords;

  int v0, v1, v2;

  // Draw each face individually. Change tile and mesh data only if they change
  SortedFacesVector::const_iterator sft, sfEnd(sortedFaces.end());
  for (sft = sortedFaces.begin(); sft != sfEnd; ++sft) {
    int f = sft->first, m_ = sft->second;

    if (m != m_) {
      m = m_;

      mesh      = meshes[m].getPointer();
      dstCoords = group->m_datas[m].m_output.get();
    }

    mesh->faceVertices(f, v0, v1, v2);

    const double *d0 = dstCoords + (v0 << 1), *d1 = dstCoords + (v1 << 1),
                 *d2 = dstCoords + (v2 << 1);

    colorFunction.faceColor(f, m);

    colorFunction.vertexColor(v0, m), glVertex2d(*d0, *(d0 + 1));
    colorFunction.vertexColor(v1, m), glVertex2d(*d1, *(d1 + 1));
    colorFunction.vertexColor(v2, m), glVertex2d(*d2, *(d2 + 1));
  }

  glEnd();
}

}  // namespace

//********************************************************************************************
//    Mesh Image Utility  functions implementation
//********************************************************************************************

void transform(const TMeshImageP &meshImage, const TAffine &aff) {
  const std::vector<TTextureMeshP> &meshes = meshImage->meshes();

  int m, mCount = meshes.size();
  for (m = 0; m != mCount; ++m) {
    TTextureMesh &mesh = *meshes[m];

    tcg::list<TTextureMesh::vertex_type> &vertices = mesh.vertices();

    tcg::list<TTextureMesh::vertex_type>::iterator vt, vEnd(vertices.end());
    for (vt = vertices.begin(); vt != vEnd; ++vt) vt->P() = aff * vt->P();
  }
}

//-------------------------------------------------------------------------------

void tglDrawEdges(const TMeshImage &mi, const PlasticDeformerDataGroup *group) {
  const std::vector<TTextureMeshP> &meshes = mi.meshes();

  int m, mCount = meshes.size();
  if (group) {
    for (m = 0; m != mCount; ++m)
      tglDrawEdges<const TPointD *, TPointD, NoColorFunction>(
          *meshes[m], (const TPointD *)group->m_datas[m].m_output.get(),
          NoColorFunction());
  } else {
    for (m = 0; m != mCount; ++m) {
      const TTextureMesh &mesh = *meshes[m];

      tglDrawEdges<tcg::list<TTextureMesh::vertex_type>, TTextureVertex,
                   NoColorFunction>(mesh, mesh.vertices(), NoColorFunction());
    }
  }
}

//-------------------------------------------------------------------------------

void tglDrawFaces(const TMeshImage &image,
                  const PlasticDeformerDataGroup *group) {
  if (group)
    tglDrawFaces(image, group, NoColorFunction());
  else
    tglDrawFaces(image, NoColorFunction());
}

//********************************************************************************************
//    Colored drawing functions
//********************************************************************************************

namespace {

struct LinearColorFunction {
  typedef double (*ValueFunc)(const LinearColorFunction *cf, int m,
                              int primitive);

public:
  const TMeshImage &m_meshImg;
  const PlasticDeformerDataGroup *m_group;

  double m_min, m_max;
  double *m_cMin, *m_cMax;

  double m_dt;
  bool m_degenerate;

  ValueFunc m_func;

public:
  LinearColorFunction(const TMeshImage &meshImg,
                      const PlasticDeformerDataGroup *group, double min,
                      double max, double *cMin, double *cMax, ValueFunc func)
      : m_meshImg(meshImg)
      , m_group(group)
      , m_min(min)
      , m_max(max)
      , m_cMin(cMin)
      , m_cMax(cMax)
      , m_dt(max - min)
      , m_degenerate(m_dt < 1e-4)
      , m_func(func) {}

  void operator()(int primitive, int m) {
    if (m_degenerate) {
      glColor4d(0.5 * (m_cMin[0] + m_cMax[0]), 0.5 * (m_cMin[1] + m_cMax[1]),
                0.5 * (m_cMin[2] + m_cMax[2]), 0.5 * (m_cMin[3] + m_cMax[3]));
      return;
    }

    double val = m_func(this, m, primitive);
    double t = (val - m_min) / m_dt, one_t = (m_max - val) / m_dt;

    glColor4d(
        one_t * m_cMin[0] + t * m_cMax[0], one_t * m_cMin[1] + t * m_cMax[1],
        one_t * m_cMin[2] + t * m_cMax[2], one_t * m_cMin[3] + t * m_cMax[3]);
  }
};

//-------------------------------------------------------------------------------

struct LinearVertexColorFunction final : public LinearColorFunction,
                                         public NoColorFunction {
  LinearVertexColorFunction(const TMeshImage &meshImg,
                            const PlasticDeformerDataGroup *group, double min,
                            double max, double *cMin, double *cMax,
                            ValueFunc func)
      : LinearColorFunction(meshImg, group, min, max, cMin, cMax, func) {}

  void vertexColor(int v, int m) { operator()(v, m); }
};

//-------------------------------------------------------------------------------

struct LinearFaceColorFunction final : public LinearColorFunction,
                                       public NoColorFunction {
  LinearFaceColorFunction(const TMeshImage &meshImg,
                          const PlasticDeformerDataGroup *group, double min,
                          double max, double *cMin, double *cMax,
                          ValueFunc func)
      : LinearColorFunction(meshImg, group, min, max, cMin, cMax, func) {}

  void faceColor(int v, int m) { operator()(v, m); }
};

}  // namespace

//===============================================================================

void tglDrawSO(const TMeshImage &image, double minColor[4], double maxColor[4],
               const PlasticDeformerDataGroup *group, bool deformedDomain) {
  struct locals {
    static double returnSO(const LinearColorFunction *cf, int m, int f) {
      return cf->m_group->m_datas[m].m_so[f];
    }
  };

  double min = 0.0, max = 0.0;
  if (group) min = group->m_soMin, max = group->m_soMax;

  LinearFaceColorFunction colorFunction(image, group, min, max, minColor,
                                        maxColor, locals::returnSO);

  if (group && deformedDomain)
    tglDrawFaces(image, group, colorFunction);
  else
    tglDrawFaces(image, colorFunction);
}

//-------------------------------------------------------------------------------

void tglDrawRigidity(const TMeshImage &image, double minColor[4],
                     double maxColor[4], const PlasticDeformerDataGroup *group,
                     bool deformedDomain) {
  struct locals {
    static double returnRigidity(const LinearColorFunction *cf, int m, int v) {
      return cf->m_meshImg.meshes()[m]->vertex(v).P().rigidity;
    }
  };

  LinearVertexColorFunction colorFunction(image, group, 1.0, 1e4, minColor,
                                          maxColor, locals::returnRigidity);

  if (group && deformedDomain)
    tglDrawFaces(image, group, colorFunction);
  else
    tglDrawFaces(image, colorFunction);
}

//***********************************************************************************************
//    Texturized drawing  implementation
//***********************************************************************************************

void tglDraw(const TMeshImage &meshImage, const DrawableTextureData &texData,
             const TAffine &meshToTexAff,
             const PlasticDeformerDataGroup &group) {
  typedef MeshTexturizer::TextureData::TileData TileData;

  // Prepare OpenGL
  glPushAttrib(GL_COLOR_BUFFER_BIT | GL_LINE_BIT |
               GL_HINT_BIT);  // Preserve original status bits

  glEnable(GL_BLEND);

  glEnable(GL_LINE_SMOOTH);
  glLineWidth(1.0f);

  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

  // Prepare variables
  const std::vector<TTextureMeshP> &meshes = meshImage.meshes();
  const TTextureMesh *mesh;

  typedef std::vector<std::pair<int, int>> SortedFacesVector;
  const SortedFacesVector &sortedFaces = group.m_sortedFaces;

  const MeshTexturizer::TextureData *td = texData.m_textureData;
  int t, tCount = td->m_tileDatas.size();

  GLuint texId = -1;
  int m        = -1;
  const double *dstCoords;

  int v0, v1, v2;
  int e1ovi, e2ovi;  // Edge X's Other Vertex Index (see below)

  // Prepare each tile's affine
  std::vector<TAffine> tileAff(tCount);
  for (t = 0; t != tCount; ++t) {
    const TileData &tileData = td->m_tileDatas[t];
    const TRectD &tileRect   = tileData.m_tileGeometry;

    tileAff[t] = TScale(1.0 / (tileRect.x1 - tileRect.x0),
                        1.0 / (tileRect.y1 - tileRect.y0)) *
                 TTranslation(-tileRect.x0, -tileRect.y0) * meshToTexAff;
  }

  // Draw each face individually, according to the group's sorted faces list.
  // Change tile and mesh data only if they change - improves performance

  SortedFacesVector::const_iterator sft, sfEnd(sortedFaces.end());
  for (sft = sortedFaces.begin(); sft != sfEnd; ++sft) {
    int f = sft->first, m_ = sft->second;

    if (m != m_) {
      // Change mesh if different from current
      m = m_;

      mesh      = meshes[m].getPointer();
      dstCoords = group.m_datas[m].m_output.get();
    }

    // Draw each face
    const TTextureMesh::face_type &fc = mesh->face(f);

    const TTextureMesh::edge_type &ed0 = mesh->edge(fc.edge(0)),
                                  &ed1 = mesh->edge(fc.edge(1)),
                                  &ed2 = mesh->edge(fc.edge(2));

    {
      v0 = ed0.vertex(0);
      v1 = ed0.vertex(1);
      v2 = ed1.vertex((ed1.vertex(0) == v0) | (ed1.vertex(0) == v1));

      e1ovi = (ed1.vertex(0) == v1) |
              (ed1.vertex(1) == v1);  // ed1 and ed2 will refer to vertexes
      e2ovi = 1 - e1ovi;              // with index 2 and these.
    }

    const TPointD &p0 = mesh->vertex(v0).P(), &p1 = mesh->vertex(v1).P(),
                  &p2 = mesh->vertex(v2).P();

    for (t = 0; t != tCount; ++t) {
      // Draw face against tile
      const TileData &tileData = td->m_tileDatas[t];

      // Map each face vertex to tile coordinates
      TPointD s[3] = {tileAff[t] * p0, tileAff[t] * p1, tileAff[t] * p2};

      // Test the face bbox - tile intersection
      if (std::min({s[0].x, s[1].x, s[2].x}) > 1.0 ||
          std::min({s[0].y, s[1].y, s[2].y}) > 1.0 ||
          std::max({s[0].x, s[1].x, s[2].x}) < 0.0 ||
          std::max({s[0].y, s[1].y, s[2].y}) < 0.0)
        continue;

      // If the tile has changed, interrupt the glBegin/glEnd block and bind the
      // OpenGL texture corresponding to the new tile
      if (tileData.m_textureId != texId) {
        texId = tileData.m_textureId;

        glBindTexture(
            GL_TEXTURE_2D,
            tileData
                .m_textureId);  // This must be OUTSIDE a glBegin/glEnd block
      }

      const double *d[3] = {dstCoords + (v0 << 1), dstCoords + (v1 << 1),
                            dstCoords + (v2 << 1)};

      /*
Now, draw primitives. A note about pixel arithmetic, here.

Since line antialiasing in OpenGL just manipulates output fragments' alpha
components,
we must require that the input texture is NONPREMULTIPLIED.

Furthermore, this function does not rely on the assumption that the output alpha
component
is discarded (as it happens when drawing on screen). This means that just using
a simple
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) is not an option, since this
way THE INPUT
SRC ALPHA GETS MULTIPLIED BY ITSELF - see glBlendFunc's docs - and that shows.

The solution is to separate the rendering of RGB and M components - the formers
use
GL_SRC_ALPHA, while the latter uses GL_ONE. The result is a PREMULTIPLIED image.
*/

      // First, draw antialiased face edges on the mesh border.
      bool drawEd0 = (ed0.facesCount() < 2), drawEd1 = (ed1.facesCount() < 2),
           drawEd2 = (ed2.facesCount() < 2);

      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

      glBegin(GL_LINES);
      {
        if (drawEd0) {
          glTexCoord2d(s[0].x, s[0].y), glVertex2d(*d[0], *(d[0] + 1));
          glTexCoord2d(s[1].x, s[1].y), glVertex2d(*d[1], *(d[1] + 1));
        }

        if (drawEd1) {
          glTexCoord2d(s[e1ovi].x, s[e1ovi].y),
              glVertex2d(*d[e1ovi], *(d[e1ovi] + 1));
          glTexCoord2d(s[2].x, s[2].y), glVertex2d(*d[2], *(d[2] + 1));
        }

        if (drawEd2) {
          glTexCoord2d(s[e2ovi].x, s[e2ovi].y),
              glVertex2d(*d[e2ovi], *(d[e2ovi] + 1));
          glTexCoord2d(s[2].x, s[2].y), glVertex2d(*d[2], *(d[2] + 1));
        }
      }
      glEnd();

      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);

      glBegin(GL_LINES);
      {
        if (drawEd0) {
          glTexCoord2d(s[0].x, s[0].y), glVertex2d(*d[0], *(d[0] + 1));
          glTexCoord2d(s[1].x, s[1].y), glVertex2d(*d[1], *(d[1] + 1));
        }

        if (drawEd1) {
          glTexCoord2d(s[e1ovi].x, s[e1ovi].y),
              glVertex2d(*d[e1ovi], *(d[e1ovi] + 1));
          glTexCoord2d(s[2].x, s[2].y), glVertex2d(*d[2], *(d[2] + 1));
        }

        if (drawEd2) {
          glTexCoord2d(s[e2ovi].x, s[e2ovi].y),
              glVertex2d(*d[e2ovi], *(d[e2ovi] + 1));
          glTexCoord2d(s[2].x, s[2].y), glVertex2d(*d[2], *(d[2] + 1));
        }
      }
      glEnd();

      // Finally, draw the face
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

      glBegin(GL_TRIANGLES);
      {
        glTexCoord2d(s[0].x, s[0].y), glVertex2d(*d[0], *(d[0] + 1));
        glTexCoord2d(s[1].x, s[1].y), glVertex2d(*d[1], *(d[1] + 1));
        glTexCoord2d(s[2].x, s[2].y), glVertex2d(*d[2], *(d[2] + 1));
      }
      glEnd();

      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);

      glBegin(GL_TRIANGLES);
      {
        glTexCoord2d(s[0].x, s[0].y), glVertex2d(*d[0], *(d[0] + 1));
        glTexCoord2d(s[1].x, s[1].y), glVertex2d(*d[1], *(d[1] + 1));
        glTexCoord2d(s[2].x, s[2].y), glVertex2d(*d[2], *(d[2] + 1));
      }
      glEnd();
    }
  }

  glBindTexture(GL_TEXTURE_2D, 0);  // Unbind texture

  glPopAttrib();
}
