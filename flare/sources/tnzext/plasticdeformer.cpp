

// tcg includes
#include "tcg/tcg_misc.h"

// tlin includes
#include "tlin/tlin.h"

// STD includes
#include <assert.h>
#include <memory>

#include "ext/plasticdeformer.h"

#include <cstring>

/*! \file plasticdeformer.cpp

  This file contains the implementation of a Mesh Deformer as specified in the
  Igarashi-Moscovich-Hughes paper "As-Rigid-As-Possible Shape Manipulation",
  with some slight modifications.
  \n\n
  Notably, the choice of handle points is not limited to mesh vertices, but free
  along the mesh surface. This can be achieved by constraining the classical
  least-squares minimization problems on a linear hyperplane, in the form:

  <P> \f$

  \left{ \begin{array}{c}
    v^t H v + v^t f + c   \rightarrow   \mbox{min} \\
    A v + d = 0
  \end{array} \right.

  \f$ </P>

  which can be solved using the Lagrange Multipliers theorem:

  <P> \f$

  \Rightarrow \left{ \begin{array}{c}
    H v + f = A^t \lambda
    A v + d = 0
  \end{array} \right.

  \Rightarrow \left( \begin{array}{c} H \\ A \end{array} \begin{array}{c} -A^t
  \\ 0 \end{array} \right)
              \left( \begin{array}{c} v \\ \lambda \end{array} \right) =
              \left( \begin{array}{c} -f \\ -d \end{array} \right)

  \f$ </P>
*/

//#define GL_DEBUG                                    // Debug using OpenGL.
// Must deform while drawing.
#ifdef GL_DEBUG   // NOTE: You should force deformations,
#include "tgl.h"  // see plasticdeformersstorage.cpp
#endif

//******************************************************************************************
//    Local structures
//******************************************************************************************

namespace {

//! A linear constraint of the kind   k[0] * v0 + k[1] * v1 + k[2] * v2 = b;
//! (b is not included though)
struct LinearConstraint {
  int m_h;        //!< Original handle index corresponding to this constraint
  int m_v[3];     //!< The mesh vertex indices v0, v1, v2
  double m_k[3];  //!< Constraint coefficients
};

//-------------------------------------------------------------------------------------------

struct SuperFactors_free {
  void operator()(tlin::SuperFactors *f) { tlin::freeF(f); }
};

//-------------------------------------------------------------------------------------------

using SuperFactorsPtr = std::unique_ptr<tlin::SuperFactors, SuperFactors_free>;
using DoublePtr       = std::unique_ptr<double[]>;
using TPointDPtr      = std::unique_ptr<TPointD[]>;

}  // namespace

//******************************************************************************************
//    Local functions
//******************************************************************************************

namespace {

inline void vertices(const TTextureMesh &mesh, int faceIdx, const TPointD *&p0,
                     const TPointD *&p1, const TPointD *&p2) {
  int v0, v1, v2;
  mesh.faceVertices(faceIdx, v0, v1, v2);

  p0 = &mesh.vertex(v0).P(), p1 = &mesh.vertex(v1).P(),
  p2 = &mesh.vertex(v2).P();
}

//-------------------------------------------------------------------------------------------

inline void buildTriangularCoordinates(const TTextureMesh &mesh, int &faceIdx,
                                       const TPointD &p, double &c0, double &c1,
                                       double &c2) {
  TPointD coords;
  const TPointD *p0, *p1, *p2;

  // Follow the hint first
  if (faceIdx >= 0 && faceIdx < mesh.facesCount() &&
      mesh.faceContains(faceIdx, p)) {
    vertices(mesh, faceIdx, p0, p1, p2);
    coords = tcg::point_ops::triCoords(p, *p0, *p1, *p2);
    c1 = coords.x, c2 = coords.y, c0 = 1.0 - c1 - c2;
    return;
  }

  // Find out the face searching on the mesh
  faceIdx = mesh.faceContaining(p);
  if (faceIdx >= 0) {
    vertices(mesh, faceIdx, p0, p1, p2);
    coords = tcg::point_ops::triCoords(p, *p0, *p1, *p2);
    c1 = coords.x, c2 = coords.y, c0 = 1.0 - c1 - c2;
    return;
  }
}

//-------------------------------------------------------------------------------------------

inline void addConstraint1d(int j, const LinearConstraint &cnstr,
                            tlin::spmat &mat) {
  int i, var, nVars = 3;
  for (i = 0; i < nVars; ++i) {
    var = cnstr.m_v[i];
    mat.at(j, var) += cnstr.m_k[i];
    mat.at(var, j) = mat(j, var);
  }
}

//-------------------------------------------------------------------------------------------

inline void addConstraint2d(int j, const LinearConstraint &cnstr,
                            tlin::spmat &mat) {
  int i, varX, varY, k = j + 1, nVars = 3;
  for (i = 0; i < nVars; ++i) {
    varX = 2 * cnstr.m_v[i], varY = varX + 1;

    mat.at(j, varX) += cnstr.m_k[i];
    mat.at(k, varY) += cnstr.m_k[i];
    mat.at(varX, j) = mat(j, varX);
    mat.at(varY, k) = mat(k, varY);
  }
}

//-------------------------------------------------------------------------------------------

inline void addGValues(int v0x, int v0y, int v1x, int v1y, int v2x, int v2y,
                       double px, double py, double w, tlin::spmat &G) {
  double sqPy = py * py, one_px = 1.0 - px;
  double a = w * (one_px * one_px + sqPy), b = w * (px * one_px - sqPy),
         c = w * (py * one_px + px * py), d = w * (px * px + sqPy);

  sqPy *= w;
  one_px *= w;
  px *= w;
  py *= w;

  G.at(v0x, v0x) += a;
  G.at(v0x, v1x) += b;
  G.at(v0x, v1y) += c;
  G.at(v0x, v2x) -= one_px;
  G.at(v0x, v2y) -= py;

  G.at(v0y, v0y) += a;
  G.at(v0y, v1x) -= py;
  G.at(v0y, v1y) += b;
  G.at(v0y, v2x) += py;
  G.at(v0y, v2y) -= one_px;

  G.at(v1x, v0x) += b;
  G.at(v1x, v0y) += -py;
  G.at(v1x, v1x) += d;
  G.at(v1x, v2x) += -px;
  G.at(v1x, v2y) += py;

  G.at(v1y, v0x) += c;
  G.at(v1y, v0y) += b;
  G.at(v1y, v1y) += d;
  G.at(v1y, v2x) -= py;
  G.at(v1y, v2y) -= px;

  G.at(v2x, v0x) -= one_px;
  G.at(v2x, v0y) += py;
  G.at(v2x, v1x) -= px;
  G.at(v2x, v1y) -= py;
  G.at(v2x, v2x) += w;

  G.at(v2y, v0x) -= py;
  G.at(v2y, v0y) -= one_px;
  G.at(v2y, v1x) += py;
  G.at(v2y, v1y) -= px;
  G.at(v2y, v2y) += w;
}

//-------------------------------------------------------------------------------------------

void buildF(double px, double py, tlin::spmat &F) {
  double one_px = 1.0 - px, sqPy = py * py;

  F.at(0, 0) = 1.0 + one_px * one_px + sqPy;
  F.at(1, 1) = F(0, 0);

  F.at(0, 2) = px * one_px - sqPy;
  F.at(0, 3) = py * one_px + px * py;
  F.at(1, 2) = -F(0, 3);
  F.at(1, 3) = F(0, 2);

  F.at(2, 0) = F(0, 2);
  F.at(2, 1) = F(1, 2);
  F.at(3, 0) = F(0, 3);
  F.at(3, 1) = F(1, 3);

  F.at(2, 2) = 1.0 + px * px + sqPy;
  F.at(3, 3) = F(2, 2);
}

//-------------------------------------------------------------------------------------------

void build_c(double v0x, double v0y, double v1x, double v1y, double v2x,
             double v2y, double px, double py, double *c) {
  c[0] = (v0x + v2x * (1.0 - px) + v2y * py);
  c[1] = (v0y - v2x * py + v2y * (1.0 - px));
  c[2] = (v1x + v2x * px - v2y * py);
  c[3] = (v1y + v2y * px + v2x * py);
}

//-------------------------------------------------------------------------------------------

inline void addHValues(int v0, int v1, double w, tlin::spmat &H) {
  H.at(v0, v0) += w;
  H.at(v1, v0) -= w;
  H.at(v0, v1) -= w;
  H.at(v1, v1) += w;
}

//-------------------------------------------------------------------------------------------

inline void add_f_values(int v0, int v1, double fit0, double fit1, double w,
                         double *f) {
  double f0_f1 = w * (fit0 - fit1);
  f[v0] += f0_f1;
  f[v1] -= f0_f1;
}

}  // namespace

//******************************************************************************************
//    PlasticDeformer::Imp  definition
//******************************************************************************************

class PlasticDeformer::Imp {
public:
  TTextureMeshP m_mesh;                  //!< Deformed mesh (cannot be changed)
  std::vector<PlasticHandle> m_handles;  //!< Compiled handles
  std::vector<LinearConstraint> m_constraints1,
      m_constraints3;  //!< Compiled constraints (depends on the above)
  bool m_compiled;     //!< Whether the deformer is ready to deform()

public:
  Imp();

  void initialize(const TTextureMeshP &mesh);
  void compile(const std::vector<PlasticHandle> &handles, int *faceHints);
  void deform(const TPointD *dstHandles, double *dstVerticesCoords);

  void copyOriginals(double *dstVerticesCoords);

public:
  tlin::spmat m_G;         //!< Pre-initialized entries for the 1st
                           //!< linear system
  SuperFactorsPtr m_invC;  //!< C's factors (C is G plus linear constraints)
  DoublePtr m_q;           //!< Step 1's known term
  DoublePtr m_out;         //!< Step 1's result

  // Step 1 members:
  //   The first step of a MeshDeformer instance is about building the desired
  //   vertices configuration.
  void initializeStep1();
  void compileStep1(const std::vector<PlasticHandle> &handles);
  void deformStep1(const TPointD *dstHandles, double *dstVerticesCoords);

  void releaseInitializedData();

public:
  std::vector<SuperFactorsPtr>
      m_invF;  //!< Each of step 2's systems factorizations

  TPointDPtr m_relativeCoords;  //!< Faces' p2 coordinates in (p0, p1)'s
                                //! orthogonal reference
  double m_v[4], m_c[4];        //!< Known term and output coordinates

  TPointDPtr m_fitTriangles;  //!< Output face coordinates

  // Step 2 members:
  //   The second step of MeshDeformer rigidly maps neighbourhoods of the
  //   original mesh
  //   to fit as much as possible the neighbourhoods in the step 1 result.
  void initializeStep2();
  void compileStep2(const std::vector<PlasticHandle> &handles);
  void deformStep2(const TPointD *dstHandles, double *dstVerticesCoords);

public:
  // NOTE: This step accepts separation in the X and Y components

  tlin::spmat m_H;         //!< Step 3's system entries
  SuperFactorsPtr m_invK;  //!< System inverse

  DoublePtr m_fx, m_fy;  //!< Known terms
  DoublePtr m_x, m_y;    //!< Output values

  // Step 3 members:
  //   The third step of MeshDeformer glues together the mapped neighbourhoods
  //   from step2.
  void initializeStep3();
  void compileStep3(const std::vector<PlasticHandle> &handles);
  void deformStep3(const TPointD *dstHandles, double *dstVerticesCoords);
};

//=================================================================================

PlasticDeformer::Imp::Imp() {}

//-------------------------------------------------------------------------------------------

void PlasticDeformer::Imp::initialize(const TTextureMeshP &mesh) {
  assert("Input mesh must be squeezed!" &&
         mesh->verticesCount() == mesh->vertices().nodesCount() &&
         mesh->edgesCount() == mesh->edges().nodesCount() &&
         mesh->facesCount() == mesh->faces().nodesCount());

  m_mesh = mesh;

  initializeStep1();
  initializeStep2();
  initializeStep3();

  m_compiled = false;  // Compilation is expected after a new initialization
}

//-------------------------------------------------------------------------------------------

void PlasticDeformer::Imp::compile(const std::vector<PlasticHandle> &handles,
                                   int *faceHints) {
  assert(m_mesh);

  m_handles.clear(), m_handles.reserve(handles.size());
  m_constraints1.clear(), m_constraints3.clear();

  LinearConstraint constr;

  // Build the linear constraints raising from the mesh-handles pairing
  int h, hCount = handles.size();
  for (h = 0; h < hCount; ++h) {
    int localFaceIdx = -1, &faceIdx = faceHints ? faceHints[h] : localFaceIdx;

    ::buildTriangularCoordinates(*m_mesh, faceIdx, handles[h].m_pos,
                                 constr.m_k[0], constr.m_k[1], constr.m_k[2]);

    if (faceIdx >= 0) {
      constr.m_h = h;

      m_mesh->faceVertices(faceIdx, constr.m_v[0], constr.m_v[1],
                           constr.m_v[2]);
      m_constraints1.push_back(constr);
      if (handles[h].m_interpolate) m_constraints3.push_back(constr);

      // Store the handles to retain precompiled data
      m_handles.push_back(handles[h]);
    }
  }

  // m_handles = handles;                              // NOT directly copied
  // like this: some handles could be
  // (geometrically) OUTSIDE the mesh!

  // Now, invoke the actual compilation procedures
  m_compiled = true;

  if (m_handles.size() < 2) return;

  compileStep1(handles);  // These may set m_compiled = false. Must still be
  compileStep2(handles);  // called even when that happens - as they are
  compileStep3(handles);  // responsible for resources reclamation.
}

//-------------------------------------------------------------------------------------------

void PlasticDeformer::Imp::deform(const TPointD *dstHandles,
                                  double *dstVerticesCoords) {
  assert(m_mesh);
  assert(dstVerticesCoords);

  // Deal with trivial cases
  if (!m_compiled || m_handles.size() == 0) {
    // Cannot deform anything - just copy the source mesh vertices into dst ones
    copyOriginals(dstVerticesCoords);
    return;
  }

  assert(dstHandles);

  if (m_handles.size() == 1) {
    // 1 handle inside the mesh - pure translational case

    const PlasticHandle &srcHandle = m_handles.front();
    const TPointD &dstHandlePos    = dstHandles[m_constraints1.front().m_h];

    TPointD shift(dstHandlePos - srcHandle.m_pos);

    int v, vCount = m_mesh->verticesCount();
    for (v = 0; v != vCount; ++v, dstVerticesCoords += 2) {
      dstVerticesCoords[0] = m_mesh->vertex(v).P().x + shift.x;
      dstVerticesCoords[1] = m_mesh->vertex(v).P().y + shift.y;
    }

    return;
  }

  deformStep1(dstHandles, dstVerticesCoords);
  deformStep2(dstHandles, dstVerticesCoords);
  deformStep3(dstHandles, dstVerticesCoords);
}

//-------------------------------------------------------------------------------------------

void PlasticDeformer::Imp::copyOriginals(double *dstVerticesCoords) {
  int v, vCount = m_mesh->verticesCount();
  for (v = 0; v != vCount; ++v, dstVerticesCoords += 2) {
    dstVerticesCoords[0] = m_mesh->vertex(v).P().x;
    dstVerticesCoords[1] = m_mesh->vertex(v).P().y;
  }
}

//-------------------------------------------------------------------------------------------

void PlasticDeformer::Imp::releaseInitializedData() {
  // Release m_G and m_H
  {
    tlin::spmat temp;
    swap(m_G, temp);
  }  // Ensure resources release by swapping them
  {
    tlin::spmat temp;
    swap(m_H, temp);
  }  // with newly initialized instances
}

//******************************************************************************************
//    Plastic Deformation Step 1
//******************************************************************************************

void PlasticDeformer::Imp::initializeStep1() {
  const TTextureMesh &mesh = *m_mesh;
  int vCount = mesh.verticesCount(), vCount_2 = 2 * vCount;

  m_G = tlin::spmat(vCount_2, vCount_2);

  // Initialize the linear system indices for the stored mesh
  int f, fCount = mesh.facesCount();
  for (f = 0; f < fCount; ++f) {
    int v0, v1, v2;
    mesh.faceVertices(f, v0, v1, v2);

    int v0x = 2 * v0;   // We'll use 2 params per vertex - with
    int v0y = v0x + 1;  // adjacent x and y parameters
    int v1x = 2 * v1;
    int v1y = v1x + 1;
    int v2x = 2 * v2;
    int v2y = v2x + 1;

    const TTextureMesh::vertex_type &vx0 = mesh.vertex(v0);
    const TTextureMesh::vertex_type &vx1 = mesh.vertex(v1);
    const TTextureMesh::vertex_type &vx2 = mesh.vertex(v2);

    TPointD c0, c1, c2;
    c2 = tcg::point_ops::ortCoords(vx2.P(), vx0.P(), vx1.P());
    c0 = tcg::point_ops::ortCoords(vx0.P(), vx1.P(), vx2.P());
    c1 = tcg::point_ops::ortCoords(vx1.P(), vx2.P(), vx0.P());

    addGValues(v0x, v0y, v1x, v1y, v2x, v2y, c2.x, c2.y, vx2.P().rigidity, m_G);
    addGValues(v1x, v1y, v2x, v2y, v0x, v0y, c0.x, c0.y, vx0.P().rigidity, m_G);
    addGValues(v2x, v2y, v0x, v0y, v1x, v1y, c1.x, c1.y, vx1.P().rigidity, m_G);
  }
}

//-------------------------------------------------------------------------------------------

void PlasticDeformer::Imp::compileStep1(
    const std::vector<PlasticHandle> &handles) {
  // First, release resources
  m_invC.reset();
  m_q.reset();
  m_out.reset();

  // Now, start compiling
  const TTextureMesh &mesh = *m_mesh;
  int vCount = mesh.verticesCount(), hCount = m_handles.size();

  int cSize = 2 * (vCount + hCount);  // Coefficients count

  // Initialize C with G
  tlin::SuperMatrix *trC = 0;
  {
    tlin::spmat C(cSize, cSize);
    C.entries()                      = m_G.entries();
    C.entries().hashFunctor().m_cols = C.cols();
    C.entries().rehash(C.entries()
                           .buckets()
                           .size());  // Rehash to entries according to C's size

    // Add the entries constraining handles
    int i;
    std::vector<LinearConstraint>::iterator ht, hEnd(m_constraints1.end());
    for (i = 0, ht = m_constraints1.begin(); ht != hEnd; ++i, ++ht)
      addConstraint2d(2 * (vCount + i), *ht, C);

    tlin::traduceS(C, trC);
  }

  // Build m_invC
  tlin::SuperFactors *invC = 0;
  tlin::factorize(trC, invC);

  tlin::freeS(trC);

  if (invC) {
    m_invC.reset(invC);

    // Reallocate arrays
    m_q.reset(new double[cSize]);
    m_out.reset(new double[cSize]);

    memset(m_q.get(), 0,
           2 * vCount *
               sizeof(double));  // Initialize the system's known term with 0
  } else
    m_compiled = false;
}

//-------------------------------------------------------------------------------------------

void PlasticDeformer::Imp::deformStep1(const TPointD *dstHandles,
                                       double *dstVerticesCoords) {
  int vCount2 = 2 * m_mesh->verticesCount();
  int cSize   = vCount2 + 2 * m_handles.size();

  // Copy destination handles into the system's known term
  int i, h;
  for (i = vCount2, h = 0; i < cSize; i += 2, ++h) {
    const TPointD &dstHandlePos = dstHandles[m_constraints1[h].m_h];

    m_q[i] = dstHandlePos.x;  // NOTE: We could use calloc on-the-fly, sparing
    m_q[i + 1] =
        dstHandlePos.y;  // some memory. Have to benchmark the cost, though...
  }

  // Solve the linear system
  double *out = m_out.get();
  tlin::solve(m_invC.get(), m_q.get(), out);

#ifdef GL_DEBUG

  glColor3d(1.0, 0.0, 0.0);  // Red
  glBegin(GL_LINES);

  // Draw deformed mesh edges
  int e, eCount = m_mesh->edgesCount();
  for (e = 0; e < eCount; ++e) {
    const TTextureMesh::edge_type &ed = m_mesh->edge(e);

    double *x0 = out + 2 * ed.vertex(0), *x1 = out + 2 * ed.vertex(1);

    glVertex2d(*x0, *(x0 + 1));
    glVertex2d(*x1, *(x1 + 1));
  }

  glEnd();

#endif
}

//******************************************************************************************
//    Plastic Deformation Step 2
//******************************************************************************************

void PlasticDeformer::Imp::initializeStep2() {
  const TTextureMesh &mesh = *m_mesh;
  int f, fCount = mesh.facesCount();

  // Clear and re-initialize vars
  tlin::spmat F(4, 4);
  tlin::SuperMatrix *trF;

  std::vector<SuperFactorsPtr>(fCount).swap(m_invF);

  m_relativeCoords.reset(new TPointD[fCount]);
  m_fitTriangles.reset(new TPointD[3 * fCount]);

  // Build step 2's system factorizations (yep, can be done at this point)
  const TPointD *p0, *p1, *p2;

  for (f = 0; f < fCount; ++f) {
    ::vertices(mesh, f, p0, p1, p2);

    TPointD c(tcg::point_ops::ortCoords(*p2, *p0, *p1));
    m_relativeCoords[f] = c;

    F.clear();
    buildF(c.x, c.y, F);

    trF = 0;
    tlin::traduceS(F, trF);

    tlin::SuperFactors *invF = 0;

    tlin::factorize(trF, invF);
    m_invF[f].reset(invF);

    tlin::freeS(trF);
  }
}

//-------------------------------------------------------------------------------------------

void PlasticDeformer::Imp::compileStep2(
    const std::vector<PlasticHandle> &handles) {
  // Nothing to do :)
}

//-------------------------------------------------------------------------------------------

void PlasticDeformer::Imp::deformStep2(const TPointD *dstHandles,
                                       double *dstVerticesCoords) {
  const TTextureMesh &mesh = *m_mesh;
  int vCount               = mesh.verticesCount();

  memset(m_fx.get(), 0,
         vCount * sizeof(double));  // These should be part of step 3...
  memset(m_fy.get(), 0,
         vCount * sizeof(double));  // They are filled here just for convenience

  // Build fit triangles
  TPointD *fitTri = m_fitTriangles.get(), *relCoord = m_relativeCoords.get();
  double *out1 = m_out.get();

  int f, fCount = mesh.facesCount();
  for (f = 0; f < fCount; ++f, fitTri += 3, ++relCoord) {
    int v0, v1, v2;
    m_mesh->faceVertices(f, v0, v1, v2);

    const RigidPoint &p0 = mesh.vertex(v0).P(), &p1 = mesh.vertex(v1).P(),
                     &p2 = mesh.vertex(v2).P();

    double *v0x = out1 + (v0 << 1), *v0y = v0x + 1, *v1x = out1 + (v1 << 1),
           *v1y = v1x + 1, *v2x = out1 + (v2 << 1), *v2y = v2x + 1;

    build_c(*v0x, *v0y, *v1x, *v1y, *v2x, *v2y, relCoord->x, relCoord->y, m_c);

    double *vPtr = (double *)m_v;
    tlin::solve(m_invF[f].get(), (double *)m_c, vPtr);

    fitTri[0].x = m_v[0], fitTri[0].y = m_v[1];
    fitTri[1].x = m_v[2], fitTri[1].y = m_v[3];

    fitTri[2].x = fitTri[0].x + relCoord->x * (fitTri[1].x - fitTri[0].x) +
                  relCoord->y * (fitTri[1].y - fitTri[0].y);
    fitTri[2].y = fitTri[0].y + relCoord->x * (fitTri[1].y - fitTri[0].y) +
                  relCoord->y * (fitTri[0].x - fitTri[1].x);

    // Scale with respect to baricenter. The baricenter is used since it makes
    // distance from vertices equally
    // weighting - which is the same expected when minimizing positions (by
    // collateral effect) in the next step.
    TPointD baricenter((fitTri[0].x + fitTri[1].x + fitTri[2].x) / 3.0,
                       (fitTri[0].y + fitTri[1].y + fitTri[2].y) / 3.0);

    double scale = sqrt(
        norm2(TPointD(p1.x - p0.x, p1.y - p0.y)) /
        norm2(TPointD(fitTri[1].x - fitTri[0].x, fitTri[1].y - fitTri[0].y)));

    fitTri[0] = scale * (fitTri[0] - baricenter) + baricenter;
    fitTri[1] = scale * (fitTri[1] - baricenter) + baricenter;
    fitTri[2] = scale * (fitTri[2] - baricenter) + baricenter;

    // Build f -- note: this should be part of step 3, we're just avoiding the
    // same cycle twice :)
    add_f_values(v0, v1, fitTri[0].x, fitTri[1].x,
                 std::min(p0.rigidity, p1.rigidity), m_fx.get());
    add_f_values(v0, v1, fitTri[0].y, fitTri[1].y,
                 std::min(p0.rigidity, p1.rigidity), m_fy.get());

    add_f_values(v1, v2, fitTri[1].x, fitTri[2].x,
                 std::min(p1.rigidity, p2.rigidity), m_fx.get());
    add_f_values(v1, v2, fitTri[1].y, fitTri[2].y,
                 std::min(p1.rigidity, p2.rigidity), m_fy.get());

    add_f_values(v2, v0, fitTri[2].x, fitTri[0].x,
                 std::min(p2.rigidity, p0.rigidity), m_fx.get());
    add_f_values(v2, v0, fitTri[2].y, fitTri[0].y,
                 std::min(p2.rigidity, p0.rigidity), m_fy.get());
  }

#ifdef GL_DEBUG

  glColor3d(0.0, 0.0, 1.0);  // Blue

  // Draw fit triangles
  fitTri = m_fitTriangles.get();

  for (f = 0; f < fCount; ++f, fitTri += 3) {
    glBegin(GL_LINE_LOOP);

    glVertex2d(fitTri[0].x, fitTri[0].y);
    glVertex2d(fitTri[1].x, fitTri[1].y);
    glVertex2d(fitTri[2].x, fitTri[2].y);

    glEnd();
  }

#endif
}

//******************************************************************************************
//    Plastic Deformation Step 3
//******************************************************************************************

void PlasticDeformer::Imp::initializeStep3() {
  const TTextureMesh &mesh = *m_mesh;
  int vCount               = mesh.verticesCount();

  m_H = tlin::spmat(vCount, vCount);

  int f, fCount = mesh.facesCount();
  for (f = 0; f < fCount; ++f) {
    int v0, v1, v2;
    mesh.faceVertices(f, v0, v1, v2);

    const RigidPoint &p0 = mesh.vertex(v0).P(), &p1 = mesh.vertex(v1).P(),
                     &p2 = mesh.vertex(v2).P();

    addHValues(v0, v1, std::min(p0.rigidity, p1.rigidity), m_H);
    addHValues(v1, v2, std::min(p1.rigidity, p2.rigidity), m_H);
    addHValues(v2, v0, std::min(p2.rigidity, p0.rigidity), m_H);
  }
}

//-------------------------------------------------------------------------------------------

void PlasticDeformer::Imp::compileStep3(
    const std::vector<PlasticHandle> &handles) {
  // First, release resources
  m_invK.reset();
  m_x.reset();
  m_y.reset();
  m_fx.reset();
  m_fy.reset();

  // If compilation already failed, skip
  if (!m_compiled) return;

  const TTextureMesh &mesh = *m_mesh;

  int vCount = mesh.verticesCount();
  int kSize  = vCount + m_constraints3.size();

  // Initialize K with H
  tlin::SuperMatrix *trK = 0;
  {
    tlin::spmat K(kSize, kSize);
    K.entries()                      = m_H.entries();
    K.entries().hashFunctor().m_cols = K.cols();
    K.entries().rehash(K.entries()
                           .buckets()
                           .size());  // Rehash to entries according to K's size

    // Add the entries constraining handles
    int c, cnstrCount = m_constraints3.size();
    for (c = 0; c < cnstrCount; ++c) {
      // Add the linear constraint to the system
      addConstraint1d(vCount + c, m_constraints3[c], K);
    }

    tlin::traduceS(K, trK);
  }

  // Build m_invK
  tlin::SuperFactors *invK = 0;
  tlin::factorize(trK, invK);

  tlin::freeS(trK);

  if (invK) {
    m_invK.reset(invK);

    m_x.reset(new double[kSize]);
    m_y.reset(new double[kSize]);
    m_fx.reset(new double[kSize]);
    m_fy.reset(new double[kSize]);
  } else
    m_compiled = false;
}

//-------------------------------------------------------------------------------------------

void PlasticDeformer::Imp::deformStep3(const TPointD *dstHandles,
                                       double *dstVerticesCoords) {
  int v, vCount = m_mesh->verticesCount();
  int c;
  int h, hCount = m_handles.size();

  for (c = 0, h = 0; h < hCount; ++h) {
    if (!m_handles[h].m_interpolate) continue;

    const TPointD &dstHandlePos = dstHandles[m_constraints1[h].m_h];

    m_fx[vCount + c] = dstHandlePos.x;
    m_fy[vCount + c] = dstHandlePos.y;

    ++c;
  }

  double *x = m_x.get(), *y = m_y.get();
  tlin::solve(m_invK.get(), m_fx.get(), x);
  tlin::solve(m_invK.get(), m_fy.get(), y);

  int i;
  for (i = v = 0; v < vCount; ++v, i += 2) {
    dstVerticesCoords[i]     = m_x[v];
    dstVerticesCoords[i + 1] = m_y[v];
  }
}

//**********************************************************************************************
//    Plastic Deformer  implementation
//**********************************************************************************************

PlasticDeformer::PlasticDeformer() : m_imp(new Imp) {}

//---------------------------------------------------------------------------------

PlasticDeformer::~PlasticDeformer() {}

//---------------------------------------------------------------------------------

bool PlasticDeformer::compiled() const { return m_imp->m_compiled; }

//---------------------------------------------------------------------------------

void PlasticDeformer::initialize(const TTextureMeshP &mesh) {
  m_imp->initialize(mesh);
}

//---------------------------------------------------------------------------------

bool PlasticDeformer::compile(const std::vector<PlasticHandle> &handles,
                              int *faceHints) {
  m_imp->compile(handles, faceHints);
  return compiled();
}

//---------------------------------------------------------------------------------

void PlasticDeformer::deform(const TPointD *dstHandles,
                             double *dstVerticesCoords) const {
  m_imp->deform(dstHandles, dstVerticesCoords);
}

//---------------------------------------------------------------------------------

void PlasticDeformer::releaseInitializedData() {
  m_imp->releaseInitializedData();
}
