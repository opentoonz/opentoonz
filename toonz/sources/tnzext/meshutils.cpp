

// TnzCore includes
#include "tgl.h"

// TnzExt includes
#include "ext/ttexturesstorage.h"
#include "ext/plasticdeformerstorage.h"

// tcg includes
#include "tcg/tcg_iterator_ops.h"

#include "ext/meshutils.h"

#include <array>

//********************************************************************************************
//    Templated drawing functions
//********************************************************************************************

namespace
{

struct NoColorFunction {
	void faceColor(int f, int m) {}
	void edgeColor(int e, int m) {}
	void vertexColor(int v, int m) {}
};

//-------------------------------------------------------------------------------

template <typename VerticesContainer, typename PointType, typename ColorFunction>
inline void tglDrawEdges(const TTextureMesh &mesh, const VerticesContainer &vertices,
						 ColorFunction colorFunction)
{
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
		glVertex2d(tcg::point_traits<PointType>::x(p0), tcg::point_traits<PointType>::y(p0));

		colorFunction.vertexColor(v1, -1);
		glVertex2d(tcg::point_traits<PointType>::x(p1), tcg::point_traits<PointType>::y(p1));
	}

	glEnd();
}

//-------------------------------------------------------------------------------

template <typename ColorFunction>
inline void tglDrawFaces(const TMeshImage &meshImage, ColorFunction colorFunction)
{
	glBegin(GL_TRIANGLES);

	int i = 0;
	for (auto const &mesh : meshImage.meshes()) {
		tcg::list<TTextureVertex> const &vertices = mesh->vertices();
		int const m = i++;

		// Draw the mesh wireframe
		int j = 0;
		for (auto const &ft : mesh->faces()) {
			int const index = j++;

			int v0, v1, v2;
			mesh->faceVertices(index, v0, v1, v2);

			TTextureVertex const &p0 = vertices[v0];
			TTextureVertex const &p1 = vertices[v1];
			TTextureVertex const &p2 = vertices[v2];

			colorFunction.faceColor(index, m);

			colorFunction.vertexColor(v0, m), glVertex2d(p0.P().x, p0.P().y);
			colorFunction.vertexColor(v1, m), glVertex2d(p1.P().x, p1.P().y);
			colorFunction.vertexColor(v2, m), glVertex2d(p2.P().x, p2.P().y);
		}
	}

	glEnd();
}

//-------------------------------------------------------------------------------

template <typename ColorFunction>
inline void tglDrawFaces(const TMeshImage &meshImage, const PlasticDeformerDataGroup *group,
						 ColorFunction colorFunction)
{
	glBegin(GL_TRIANGLES);

	const std::vector<TTextureMeshP> &meshes = meshImage.meshes();

	// Draw faces according to the group's sorted faces list
	// Draw each face individually. Change tile and mesh data only if they change
	int m = -1;
	for (auto const &sft : group->m_sortedFaces) {
		TTextureMesh const *mesh = nullptr;
		double const *dstCoords = nullptr;

		if (m != sft.second) {
			m = sft.second;
			mesh = meshes[m].getPointer();
			dstCoords = group->m_datas[m].m_output.get();
		}

		int v0, v1, v2;
		mesh->faceVertices(sft.first, v0, v1, v2);

		double const *const d0 = dstCoords + (v0 << 1);
		double const *const d1 = dstCoords + (v1 << 1);
		double const *const d2 = dstCoords + (v2 << 1);

		colorFunction.faceColor(sft.first, m);

		colorFunction.vertexColor(v0, m), glVertex2d(d0[0], d0[1]);
		colorFunction.vertexColor(v1, m), glVertex2d(d1[0], d1[1]);
		colorFunction.vertexColor(v2, m), glVertex2d(d2[0], d2[1]);
	}

	glEnd();
}

} // namespace

//********************************************************************************************
//    Mesh Image Utility  functions implementation
//********************************************************************************************

void transform(const TMeshImageP &meshImage, const TAffine &aff)
{
	const std::vector<TTextureMeshP> &meshes = meshImage->meshes();

	int m, mCount = meshes.size();
	for (m = 0; m != mCount; ++m) {
		TTextureMesh &mesh = *meshes[m];

		tcg::list<TTextureMesh::vertex_type> &vertices = mesh.vertices();

		tcg::list<TTextureMesh::vertex_type>::iterator vt, vEnd(vertices.end());
		for (vt = vertices.begin(); vt != vEnd; ++vt)
			vt->P() = aff * vt->P();
	}
}

//-------------------------------------------------------------------------------

void tglDrawEdges(const TMeshImage &mi, const PlasticDeformerDataGroup *group)
{
	const std::vector<TTextureMeshP> &meshes = mi.meshes();

	int m, mCount = meshes.size();
	if (group) {
		for (m = 0; m != mCount; ++m)
			tglDrawEdges<const TPointD *, TPointD, NoColorFunction>(
				*meshes[m], (const TPointD *)group->m_datas[m].m_output.get(), NoColorFunction());
	} else {
		for (m = 0; m != mCount; ++m) {
			const TTextureMesh &mesh = *meshes[m];

			tglDrawEdges<tcg::list<TTextureMesh::vertex_type>, TTextureVertex, NoColorFunction>(
				mesh, mesh.vertices(), NoColorFunction());
		}
	}
}

//-------------------------------------------------------------------------------

void tglDrawFaces(const TMeshImage &image, const PlasticDeformerDataGroup *group)
{
	if (group)
		tglDrawFaces(image, group, NoColorFunction());
	else
		tglDrawFaces(image, NoColorFunction());
}

//********************************************************************************************
//    Colored drawing functions
//********************************************************************************************

namespace
{

struct LinearColorFunction {
	typedef double (*ValueFunc)(const LinearColorFunction *cf, int m, int primitive);

  public:
	const TMeshImage &m_meshImg;
	const PlasticDeformerDataGroup *m_group;

	double m_min, m_max;
	double *m_cMin, *m_cMax;

	double m_dt;
	bool m_degenerate;

	ValueFunc m_func;

  public:
	LinearColorFunction(const TMeshImage &meshImg, const PlasticDeformerDataGroup *group,
						double min, double max, double *cMin, double *cMax, ValueFunc func)
		: m_meshImg(meshImg), m_group(group), m_min(min), m_max(max), m_cMin(cMin), m_cMax(cMax),
		  m_dt(max - min), m_degenerate(m_dt < 1e-4), m_func(func)
	{
	}

	void operator()(int primitive, int m)
	{
		if (m_degenerate) {
			glColor4d(0.5 * (m_cMin[0] + m_cMax[0]), 0.5 * (m_cMin[1] + m_cMax[1]),
					  0.5 * (m_cMin[2] + m_cMax[2]), 0.5 * (m_cMin[3] + m_cMax[3]));
			return;
		}

		double val = m_func(this, m, primitive);
		double t = (val - m_min) / m_dt, one_t = (m_max - val) / m_dt;

		glColor4d(one_t * m_cMin[0] + t * m_cMax[0], one_t * m_cMin[1] + t * m_cMax[1],
				  one_t * m_cMin[2] + t * m_cMax[2], one_t * m_cMin[3] + t * m_cMax[3]);
	}
};

//-------------------------------------------------------------------------------

struct LinearVertexColorFunction : public LinearColorFunction, public NoColorFunction {
	LinearVertexColorFunction(const TMeshImage &meshImg, const PlasticDeformerDataGroup *group,
							  double min, double max, double *cMin, double *cMax, ValueFunc func)
		: LinearColorFunction(meshImg, group, min, max, cMin, cMax, func)
	{
	}

	void vertexColor(int v, int m) { operator()(v, m); }
};

//-------------------------------------------------------------------------------

struct LinearFaceColorFunction : public LinearColorFunction, public NoColorFunction {
	LinearFaceColorFunction(const TMeshImage &meshImg, const PlasticDeformerDataGroup *group,
							double min, double max, double *cMin, double *cMax, ValueFunc func)
		: LinearColorFunction(meshImg, group, min, max, cMin, cMax, func)
	{
	}

	void faceColor(int v, int m) { operator()(v, m); }
};

} // namespace

//===============================================================================

void tglDrawSO(const TMeshImage &image, double minColor[4], double maxColor[4],
			   const PlasticDeformerDataGroup *group, bool deformedDomain)
{
	struct locals {
		static double returnSO(const LinearColorFunction *cf, int m, int f)
		{
			return cf->m_group->m_datas[m].m_so[f];
		}
	};

	double min = 0.0, max = 0.0;
	if (group)
		min = group->m_soMin, max = group->m_soMax;

	LinearFaceColorFunction colorFunction(image, group, min, max, minColor, maxColor,
										  locals::returnSO);

	if (group && deformedDomain)
		tglDrawFaces(image, group, colorFunction);
	else
		tglDrawFaces(image, colorFunction);
}

//-------------------------------------------------------------------------------

void tglDrawRigidity(const TMeshImage &image, double minColor[4], double maxColor[4],
					 const PlasticDeformerDataGroup *group, bool deformedDomain)
{
	struct locals {
		static double returnRigidity(const LinearColorFunction *cf, int m, int v)
		{
			return cf->m_meshImg.meshes()[m]->vertex(v).P().rigidity;
		}
	};

	LinearVertexColorFunction colorFunction(image, group, 1.0, 1e4, minColor, maxColor,
											locals::returnRigidity);

	if (group && deformedDomain)
		tglDrawFaces(image, group, colorFunction);
	else
		tglDrawFaces(image, colorFunction);
}

//***********************************************************************************************
//    Texturized drawing  implementation
//***********************************************************************************************

void tglDraw(const TMeshImage &meshImage, const DrawableTextureData &texData,
			 const TAffine &meshToTexAff, const PlasticDeformerDataGroup &group)
{
#ifdef _WIN32
	typedef void (*PFNGLBLENDFUNCSEPARATEPROC)(GLenum, GLenum, GLenum, GLenum);
	static PFNGLBLENDFUNCSEPARATEPROC const glBlendFuncSeparate =
		reinterpret_cast<PFNGLBLENDFUNCSEPARATEPROC>(::wglGetProcAddress("glBlendFuncSeparate"));
#endif

	// Prepare OpenGL
	glPushAttrib(GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_HINT_BIT); // Preserve original status bits

	glEnable(GL_BLEND);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	glEnable(GL_LINE_SMOOTH);
	glLineWidth(1.0f);

	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	auto const &tiles = texData.m_textureData->m_tileDatas;

	// Prepare each tile's affine
	std::unique_ptr<TAffine[]> tileAff(new TAffine[tiles.size()]);
	{
		std::size_t i = 0;
		for (auto const &tile : tiles) {
			TRectD const &rect = tile.m_tileGeometry;
			TScale const scale(1.0 / (rect.x1 - rect.x0), 1.0 / (rect.y1 - rect.y0));
			TTranslation const translate(-rect.x0, -rect.y0);
			tileAff[i] = scale * translate * meshToTexAff;
		}
	}

	// Draw each face individually, according to the group's sorted faces list.
	// Change tile and mesh data only if they change - improves performance
	int m = -1;
	TTextureMesh const *mesh = nullptr;
	double const *dstCoords = nullptr;
	GLuint texId = -1;
	for (auto const &sft : group.m_sortedFaces) {
		if (m != sft.second) {
			// Change mesh if different from current
			m = sft.second;
			mesh = meshImage.meshes()[m].getPointer();
			dstCoords = group.m_datas[m].m_output.get();
		}

		// Draw each face
		TTextureMesh::face_type const &fc = mesh->face(sft.first);
		TTextureMesh::edge_type const &ed0 = mesh->edge(fc.edge(0));
		TTextureMesh::edge_type const &ed1 = mesh->edge(fc.edge(1));
		TTextureMesh::edge_type const &ed2 = mesh->edge(fc.edge(2));

		int const v0 = ed0.vertex(0);
		int const v1 = ed0.vertex(1);
		int const v2 = ed1.vertex((ed1.vertex(0) == v0) | (ed1.vertex(0) == v1));

		// Edge X's Other Vertex Index (see below)
		int const f =
			(ed1.vertex(0) == v1) | (ed1.vertex(1) == v1); // ed1 and ed2 will refer to vertexes
		int const g = 1 - f;							   // with index 2 and these.

		TPointD const &p0 = mesh->vertex(v0).P();
		TPointD const &p1 = mesh->vertex(v1).P();
		TPointD const &p2 = mesh->vertex(v2).P();

		// Draw face against tile
		std::size_t i = 0;
		for (auto const &tileData : tiles) {
			// Map each face vertex to tile coordinates
			std::array<TPointD, 3> const s = {
				tileAff[i] * p0, tileAff[i] * p1, tileAff[i] * p2,
			};
			++i;

			// Test the face bbox - tile intersection
			if ((tmin(s[0].x, s[1].x, s[2].x) > 1.0) || (tmin(s[0].y, s[1].y, s[2].y) > 1.0) ||
				(tmax(s[0].x, s[1].x, s[2].x) < 0.0) || (tmax(s[0].y, s[1].y, s[2].y) < 0.0)) {
				continue;
			}

			// If the tile has changed, interrupt the glBegin/glEnd block and bind the
			// OpenGL texture corresponding to the new tile
			if (texId != tileData.m_textureId) {
				texId = tileData.m_textureId;

				// This must be OUTSIDE a glBegin/glEnd block
				glBindTexture(GL_TEXTURE_2D, texId);
			}

			std::array<double const *, 3> const d = {
				dstCoords + (v0 << 1), dstCoords + (v1 << 1), dstCoords + (v2 << 1),
			};

			// First, draw antialiased face edges on the mesh border.
			glBegin(GL_LINES);
			{
				if (ed0.facesCount() < 2) {
					glTexCoord2d(s[0].x, s[0].y), glVertex2d(d[0][0], d[0][1]);
					glTexCoord2d(s[1].x, s[1].y), glVertex2d(d[1][0], d[1][1]);
				}

				if (ed1.facesCount() < 2) {
					glTexCoord2d(s[f].x, s[f].y), glVertex2d(d[f][0], d[f][1]);
					glTexCoord2d(s[2].x, s[2].y), glVertex2d(d[2][0], d[2][1]);
				}

				if (ed2.facesCount() < 2) {
					glTexCoord2d(s[g].x, s[g].y), glVertex2d(d[g][0], d[g][1]);
					glTexCoord2d(s[2].x, s[2].y), glVertex2d(d[2][0], d[2][1]);
				}
			}
			glEnd();

			// Finally, draw the face
			glBegin(GL_TRIANGLES);
			{
				glTexCoord2d(s[0].x, s[0].y), glVertex2d(d[0][0], d[0][1]);
				glTexCoord2d(s[1].x, s[1].y), glVertex2d(d[1][0], d[1][1]);
				glTexCoord2d(s[2].x, s[2].y), glVertex2d(d[2][0], d[2][1]);
			}
			glEnd();
		}
	}

	glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture

	glPopAttrib();
}
