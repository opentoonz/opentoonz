

//#include "traster.h"
#include "tcolorutils.h"
#include "tmathutil.h"

#include <set>
#include <list>
#include <cmath>

typedef float KEYER_FLOAT;

//------------------------------------------------------------------------------
#ifdef _MSC_VER
#define ISNAN _isnan
#else
#define ISNAN std::isnan
#endif

//------------------------------------------------------------------------------

//#define CLUSTER_ELEM_CONTAINER_IS_A_SET
//#define WITH_ALPHA_IN_STATISTICS

//------------------------------------------------------------------------------

class ClusterStatistic {
public:
  KEYER_FLOAT sumComponents[3];  // vector 3x1
  unsigned int elemsCount;
  KEYER_FLOAT matrixR[9];  // matrix 3x3 = sum(x * transposed(x))
                           // where x are the pixels in the cluster

  KEYER_FLOAT covariance[9];  // covariance matrix
  TPoint sumCoords;

#ifdef WITH_ALPHA_IN_STATISTICS

  KEYER_FLOAT sumAlpha;

#endif
};

//------------------------------------------------------------------------------

class ClusterElem {
public:
  ClusterElem(unsigned char _r, unsigned char _g, unsigned char _b,
              KEYER_FLOAT _a, unsigned int _x = 0, unsigned int _y = 0)
      : r(toDouble(_r))
      , g(toDouble(_g))
      , b(toDouble(_b))
      , a(_a)
      , x(_x)
      , y(_y)
      , pix32(TPixel32(_r, _g, _b)) {}

  ~ClusterElem() {}

  static KEYER_FLOAT toDouble(unsigned char chan) {
    return ((KEYER_FLOAT)chan) * (KEYER_FLOAT)(1.0 / 255.0);
  }

  unsigned int x;
  unsigned int y;
  KEYER_FLOAT r;
  KEYER_FLOAT g;
  KEYER_FLOAT b;
  KEYER_FLOAT a;
  TPixel32 pix32;
};

//------------------------------------------------------------------------------

#ifdef CLUSTER_ELEM_CONTAINER_IS_A_SET

typedef std::set<ClusterElem *> ClusterElemContainer;

#else

typedef std::vector<ClusterElem *> ClusterElemContainer;

#endif

//------------------------------------------------------------------------------

class Cluster {
public:
  Cluster();
  Cluster(const Cluster &rhs);

  ~Cluster();

  void computeCovariance();
  void insert(ClusterElem *elem);
  void computeStatistics();
  void getMeanAxis(KEYER_FLOAT axis[3]);

  ClusterStatistic statistic;
  ClusterElemContainer data;

  KEYER_FLOAT eigenVector[3];
  KEYER_FLOAT eigenValue;

private:
  void operator=(const Cluster &);
};

//------------------------------------------------------------------------------

typedef std::vector<Cluster *> ClusterContainer;

//----------------------------------------------------------------------------

void chooseLeafToClusterize(ClusterContainer::iterator &itRet,
                            KEYER_FLOAT &eigenValue, KEYER_FLOAT eigenVector[3],
                            ClusterContainer &clusters);

void split(Cluster *subcluster1, Cluster *subcluster2,
           KEYER_FLOAT eigenVector[3], Cluster *cluster);

void SolveCubic(KEYER_FLOAT a,   /* coefficient of x^3 */
                KEYER_FLOAT b,   /* coefficient of x^2 */
                KEYER_FLOAT c,   /* coefficient of x   */
                KEYER_FLOAT d,   /* constant term      */
                int *solutions,  /* # of distinct solutions */
                KEYER_FLOAT *x); /* array of solutions      */

unsigned short int calcCovarianceEigenValues(const KEYER_FLOAT covariance[9],
                                             KEYER_FLOAT eigenValues[3]);

//----------------------------------------------------------------------------

void split(Cluster *subcluster1, Cluster *subcluster2,
           KEYER_FLOAT eigenVector[3], Cluster *cluster) {
  KEYER_FLOAT n = (KEYER_FLOAT)cluster->statistic.elemsCount;

  KEYER_FLOAT mean[3];
  mean[0] = cluster->statistic.sumComponents[0] / n;
  mean[1] = cluster->statistic.sumComponents[1] / n;
  mean[2] = cluster->statistic.sumComponents[2] / n;

  ClusterElemContainer::const_iterator it = cluster->data.begin();
  for (; it != cluster->data.end(); ++it) {
    ClusterElem *elem = *it;

    KEYER_FLOAT r = (KEYER_FLOAT)elem->r;
    KEYER_FLOAT g = (KEYER_FLOAT)elem->g;
    KEYER_FLOAT b = (KEYER_FLOAT)elem->b;

    // cluster->data.erase(it);

    if (eigenVector[0] * r + eigenVector[1] * g + eigenVector[2] * b <=
        eigenVector[0] * mean[0] + eigenVector[1] * mean[1] +
            eigenVector[2] * mean[2])
      subcluster1->insert(elem);
    else
      subcluster2->insert(elem);
  }
}

//----------------------------------------------------------------------------

void chooseLeafToClusterize(ClusterContainer::iterator &itRet,
                            KEYER_FLOAT &eigenValue, KEYER_FLOAT eigenVector[3],
                            ClusterContainer &clusters) {
  itRet = clusters.end();

  ClusterContainer::iterator itFound = clusters.end();
  bool found                         = false;
  KEYER_FLOAT maxEigenValue          = 0.0;

  unsigned short int multeplicity = 0;

  ClusterContainer::iterator it = clusters.begin();
  for (; it != clusters.end(); ++it) {
    unsigned short int tmpMulteplicity = 0;
    KEYER_FLOAT tmpEigenValue;

    Cluster *cluster = *it;
    // Calculates the covariance matrix.
    const KEYER_FLOAT *clusterCovariance = cluster->statistic.covariance;
    assert(!ISNAN(clusterCovariance[0]));

    // Calculate the eigenvalues ​​of the covariance matrix of the cluster
    // statistics
    // (because the array is symmetrical the eigenvalues are all real)
    KEYER_FLOAT eigenValues[3];
    tmpMulteplicity = calcCovarianceEigenValues(clusterCovariance, eigenValues);
    assert(tmpMulteplicity > 0);

    tmpEigenValue = std::max({eigenValues[0], eigenValues[1], eigenValues[2]});
    cluster->eigenValue = tmpEigenValue;

    // Check if there are any cluster updates to search for.
    if (itFound == clusters.end()) {
      itFound       = it;
      maxEigenValue = tmpEigenValue;
      multeplicity  = tmpMulteplicity;
      found         = true;
    } else {
      if (tmpEigenValue > maxEigenValue) {
        itFound       = it;
        maxEigenValue = tmpEigenValue;
        multeplicity  = tmpMulteplicity;
      }
    }
  }

  if (found) {
    assert(multeplicity > 0);
    itRet      = itFound;
    eigenValue = maxEigenValue;

    // Calculates the eigenvector related to 'maxEigenValue'
    Cluster *clusterFound = *itFound;

    assert(multeplicity > 0);

    KEYER_FLOAT tmpMatrixM[9];

    const KEYER_FLOAT *clusterCovariance = clusterFound->statistic.covariance;
    int i                                = 0;
    for (; i < 9; ++i) tmpMatrixM[i]     = clusterCovariance[i];

    tmpMatrixM[0] -= maxEigenValue;
    tmpMatrixM[4] -= maxEigenValue;
    tmpMatrixM[8] -= maxEigenValue;

    for (i = 0; i < 3; ++i) eigenVector[i] = 0.0;

    if (multeplicity == 1) {
      KEYER_FLOAT u11 =
          tmpMatrixM[4] * tmpMatrixM[8] - tmpMatrixM[5] * tmpMatrixM[5];
      KEYER_FLOAT u12 =
          tmpMatrixM[2] * tmpMatrixM[5] - tmpMatrixM[1] * tmpMatrixM[8];
      KEYER_FLOAT u13 =
          tmpMatrixM[1] * tmpMatrixM[5] - tmpMatrixM[2] * tmpMatrixM[5];
      KEYER_FLOAT u22 =
          tmpMatrixM[0] * tmpMatrixM[8] - tmpMatrixM[2] * tmpMatrixM[2];
      KEYER_FLOAT u23 =
          tmpMatrixM[1] * tmpMatrixM[2] - tmpMatrixM[5] * tmpMatrixM[0];
      KEYER_FLOAT u33 =
          tmpMatrixM[0] * tmpMatrixM[4] - tmpMatrixM[1] * tmpMatrixM[1];

      KEYER_FLOAT uMax = std::max({u11, u12, u13, u22, u23, u33});

      if (uMax == u11) {
        eigenVector[0] = u11;
        eigenVector[1] = u12;
        eigenVector[2] = u13;
      } else if (uMax == u12) {
        eigenVector[0] = u12;
        eigenVector[1] = u22;
        eigenVector[2] = u23;
      } else if (uMax == u13) {
        eigenVector[0] = u13;
        eigenVector[1] = u23;
        eigenVector[2] = u33;
      } else if (uMax == u22) {
        eigenVector[0] = u12;
        eigenVector[1] = u22;
        eigenVector[2] = u23;
      } else if (uMax == u23) {
        eigenVector[0] = u13;
        eigenVector[1] = u23;
        eigenVector[2] = u33;
      } else if (uMax == u33) {
        eigenVector[0] = u13;
        eigenVector[1] = u23;
        eigenVector[2] = u33;
      } else {
        assert(false && "impossibile!!");
      }

    } else if (multeplicity == 2) {
      short int row = -1;
      short int col = -1;

      KEYER_FLOAT mMax =
          std::max({tmpMatrixM[0], tmpMatrixM[1], tmpMatrixM[2], tmpMatrixM[4],
                    tmpMatrixM[5], tmpMatrixM[8]});

      if (mMax == tmpMatrixM[0]) {
        row = 1;
        col = 1;
      } else if (mMax == tmpMatrixM[1]) {
        row = 1;
        col = 2;
      } else if (mMax == tmpMatrixM[2]) {
        row = 1;
        col = 3;
      } else if (mMax == tmpMatrixM[4]) {
        row = 2;
        col = 2;
      } else if (mMax == tmpMatrixM[5]) {
        row = 2;
        col = 3;
      } else if (mMax == tmpMatrixM[8]) {
        row = 3;
        col = 3;
      }

      if (row == 1) {
        if (col == 1 || col == 2) {
          eigenVector[0] = -tmpMatrixM[1];
          eigenVector[1] = tmpMatrixM[0];
          eigenVector[2] = 0.0;
        } else {
          eigenVector[0] = tmpMatrixM[2];
          eigenVector[1] = 0.0;
          eigenVector[2] = -tmpMatrixM[0];
        }
      } else if (row == 2) {
        eigenVector[0] = 0.0;
        eigenVector[1] = -tmpMatrixM[5];
        eigenVector[2] = tmpMatrixM[4];
      } else if (row == 3) {
        eigenVector[0] = 0.0;
        eigenVector[1] = -tmpMatrixM[8];
        eigenVector[2] = tmpMatrixM[5];
      }
    } else if (multeplicity == 3) {
      eigenVector[0] = 1.0;
      eigenVector[1] = 0.0;
      eigenVector[2] = 0.0;
    } else {
      assert(false && "impossibile!!");
    }

    // Normalization of calculated eigenvector.
    /*
KEYER_FLOAT eigenVectorMagnitude = sqrt(eigenVector[0]*eigenVector[0] +
                             eigenVector[1]*eigenVector[1] +
                             eigenVector[2]*eigenVector[2]);
assert(eigenVectorMagnitude > 0);

eigenVector[0] /= eigenVectorMagnitude;
eigenVector[1] /= eigenVectorMagnitude;
eigenVector[2] /= eigenVectorMagnitude;
*/

    clusterFound->eigenVector[0] = eigenVector[0];
    clusterFound->eigenVector[1] = eigenVector[1];
    clusterFound->eigenVector[2] = eigenVector[2];

    assert(!ISNAN(eigenVector[0]));
    assert(!ISNAN(eigenVector[1]));
    assert(!ISNAN(eigenVector[2]));
  }
}

//----------------------------------------------------------------------------

unsigned short int calcCovarianceEigenValues(
    const KEYER_FLOAT clusterCovariance[9], KEYER_FLOAT eigenValues[3]) {
  unsigned short int multeplicity = 0;

  KEYER_FLOAT a11 = clusterCovariance[0];
  KEYER_FLOAT a12 = clusterCovariance[1];
  KEYER_FLOAT a13 = clusterCovariance[2];
  KEYER_FLOAT a22 = clusterCovariance[4];
  KEYER_FLOAT a23 = clusterCovariance[5];
  KEYER_FLOAT a33 = clusterCovariance[8];

  KEYER_FLOAT c0 =
      (KEYER_FLOAT)(a11 * a22 * a33 + 2.0 * a12 * a13 * a23 - a11 * a23 * a23 -
                    a22 * a13 * a13 - a33 * a12 * a12);

  KEYER_FLOAT c1 = (KEYER_FLOAT)(a11 * a22 - a12 * a12 + a11 * a33 - a13 * a13 +
                                 a22 * a33 - a23 * a23);

  KEYER_FLOAT c2 = (KEYER_FLOAT)(a11 + a22 + a33);

  int solutionsCount = 0;
  SolveCubic((KEYER_FLOAT)-1.0, c2, -c1, c0, &solutionsCount, eigenValues);
  assert(solutionsCount > 0);
  multeplicity = 4 - solutionsCount;

  assert(!ISNAN(eigenValues[0]));
  assert(!ISNAN(eigenValues[1]));
  assert(!ISNAN(eigenValues[2]));

  assert(multeplicity > 0);
  return multeplicity;
}

//----------------------------------------------------------------------------

void SolveCubic(KEYER_FLOAT a,  /* coefficient of x^3 */
                KEYER_FLOAT b,  /* coefficient of x^2 */
                KEYER_FLOAT c,  /* coefficient of x   */
                KEYER_FLOAT d,  /* constant term      */
                int *solutions, /* # of distinct solutions */
                KEYER_FLOAT *x) /* array of solutions      */
{
  static const KEYER_FLOAT epsilon = (KEYER_FLOAT)0.0001;
  if (a != 0 && fabs(b - 0.0) <= epsilon && fabs(c - 0.0) <= epsilon &&
      fabs(d - 0.0) <= epsilon)
  // if(a != 0 && b == 0 && c == 0 && d == 0)
  {
    *solutions = 1;
    x[0] = x[1] = x[2] = 0.0;
    return;
  }
  KEYER_FLOAT a1 = (KEYER_FLOAT)(b / a);
  KEYER_FLOAT a2 = (KEYER_FLOAT)(c / a);
  KEYER_FLOAT a3 = (KEYER_FLOAT)(d / a);
  KEYER_FLOAT Q  = (KEYER_FLOAT)((a1 * a1 - 3.0 * a2) / 9.0);
  KEYER_FLOAT R =
      (KEYER_FLOAT)((2.0 * a1 * a1 * a1 - 9.0 * a1 * a2 + 27.0 * a3) / 54.0);
  KEYER_FLOAT R2_Q3 = (KEYER_FLOAT)(R * R - Q * Q * Q);
  KEYER_FLOAT theta;
  KEYER_FLOAT PI = (KEYER_FLOAT)3.1415926535897932384626433832795;

  if (R2_Q3 <= 0) {
    *solutions = 3;
    theta      = (KEYER_FLOAT)acos(R / sqrt(Q * Q * Q));
    x[0]       = (KEYER_FLOAT)(-2.0 * sqrt(Q) * cos(theta / 3.0) - a1 / 3.0);
    x[1]       = (KEYER_FLOAT)(-2.0 * sqrt(Q) * cos((theta + 2.0 * PI) / 3.0) -
                         a1 / 3.0);
    x[2] = (KEYER_FLOAT)(-2.0 * sqrt(Q) * cos((theta + 4.0 * PI) / 3.0) -
                         a1 / 3.0);

    assert(!ISNAN(x[0]));
    assert(!ISNAN(x[1]));
    assert(!ISNAN(x[2]));

    /*
long KEYER_FLOAT v;
v = x[0];
assert(areAlmostEqual(a*v*v*v+b*v*v+c*v+d, 0.0));
v = x[1];
assert(areAlmostEqual(a*v*v*v+b*v*v+c*v+d, 0.0));
v = x[2];
assert(areAlmostEqual(a*v*v*v+b*v*v+c*v+d, 0.0));
*/
  } else {
    *solutions = 1;
    x[0] = (KEYER_FLOAT)pow((float)(sqrt(R2_Q3) + fabs(R)), (float)(1 / 3.0));
    x[0] += (KEYER_FLOAT)(Q / x[0]);
    x[0] *= (KEYER_FLOAT)((R < 0.0) ? 1 : -1);
    x[0] -= (KEYER_FLOAT)(a1 / 3.0);

    assert(!ISNAN(x[0]));

    /*
long KEYER_FLOAT v;
v = x[0];
assert(areAlmostEqual(a*v*v*v+b*v*v+c*v+d, 0.0));
*/
  }
}

//----------------------------------------------------------------------------

//------------------------------------------------------------------------------

static void clusterize(ClusterContainer &clusters, int clustersCount) {
  unsigned int clustersSize = clusters.size();
  assert(clustersSize >= 1);

  // Ensure the clusters are always leaves.
  // Tree calculated using the algorithm described by Orchard TSE - Bouman.
  // (c.f.r. "Color Quantization of Images" - M.Orchard, C. Bouman)

  // number of iterations , the number of clusters = number of iterations + 1
  int m = clustersCount - 1;
  int i = 0;
  for (; i < m; ++i) {
    // Choose the cluster leaf of the tree (the cluster in
    // ClusterContainer "clusters") that has the highest eigenvalue, ie
    // The cluster that has higher variance axis
    // (which is the eigenvector corresponding to the largest eigenvalue).

    KEYER_FLOAT eigenValue     = 0.0;
    KEYER_FLOAT eigenVector[3] = {0.0, 0.0, 0.0};
    ClusterContainer::iterator itChoosedCluster;

    chooseLeafToClusterize(itChoosedCluster, eigenValue, eigenVector, clusters);

    assert(itChoosedCluster != clusters.end());
    Cluster *choosedCluster = *itChoosedCluster;

#if 0

    // If the cluster chosen for the subdivision contains single
    // element means that there's nothing left to divide up and exit
    // the loop.
    // This happens when checking how many more clusters of elements
    // there are in the initial cluste.
    if(choosedCluster->statistic.elemsCount == 1)
      break;

#else

    // A cluster that has only one element doesn't make much sense to exist,
    // It also creates problems in the computation of the covariance matrix.
    // Stop when the cluster contains less than 4 elements.
    if (choosedCluster->statistic.elemsCount == 3) break;

#endif

    // Subdivides the cluster chosen in two other clusters.
    Cluster *subcluster1 = new Cluster();
    Cluster *subcluster2 = new Cluster();
    split(subcluster1, subcluster2, eigenVector, choosedCluster);
    assert(subcluster1);
    assert(subcluster2);
    if ((subcluster1->data.size() == 0) || (subcluster2->data.size() == 0))
      break;

    // Calculates the new report for 'subcluster1'.
    subcluster1->computeStatistics();

    // Calculates the new statistic for 'subcluster2'.
    int j = 0;
    for (; j < 3; ++j) {
      subcluster2->statistic.sumComponents[j] =
          choosedCluster->statistic.sumComponents[j] -
          subcluster1->statistic.sumComponents[j];
    }

    subcluster2->statistic.sumCoords.x = choosedCluster->statistic.sumCoords.x -
                                         subcluster1->statistic.sumCoords.x;

    subcluster2->statistic.sumCoords.y = choosedCluster->statistic.sumCoords.y -
                                         subcluster1->statistic.sumCoords.y;

    subcluster2->statistic.elemsCount = choosedCluster->statistic.elemsCount -
                                        subcluster1->statistic.elemsCount;

#ifdef WITH_ALPHA_IN_STATISTICS

    subcluster2->statistic.sumAlpha =
        choosedCluster->statistic.sumAlpha - subcluster1->statistic.sumAlpha;

#endif

    for (j                              = 0; j < 9; ++j)
      subcluster2->statistic.matrixR[j] = choosedCluster->statistic.matrixR[j] -
                                          subcluster1->statistic.matrixR[j];

    subcluster2->computeCovariance();

    // Update the appropriate ClusterContainer "clusters", by deleting
    // the cluster chosen and inserting the two newly created.
    // So ClusterContainer "cluster" only ever has
    // the leaves created by the algorithm TSE.

    Cluster *cluster = *itChoosedCluster;
    assert(cluster);
    cluster->data.clear();
    // clearPointerContainer(cluster->data);
    assert(cluster->data.size() == 0);
    delete cluster;
    clusters.erase(itChoosedCluster);

    clusters.push_back(subcluster1);
    clusters.push_back(subcluster2);
  }
}

//------------------------------------------------------------------------------

Cluster::Cluster() {}

//------------------------------------------------------------------------------

Cluster::Cluster(const Cluster &rhs) : statistic(rhs.statistic) {
  ClusterElemContainer::const_iterator it = rhs.data.begin();
  for (; it != rhs.data.end(); ++it) data.push_back(new ClusterElem(**it));
}

//------------------------------------------------------------------------------

Cluster::~Cluster() { clearPointerContainer(data); }

//------------------------------------------------------------------------------

void Cluster::computeCovariance() {
  KEYER_FLOAT sumComponentsMatrix[9];

  KEYER_FLOAT sumR = statistic.sumComponents[0];
  KEYER_FLOAT sumG = statistic.sumComponents[1];
  KEYER_FLOAT sumB = statistic.sumComponents[2];

  sumComponentsMatrix[0] = sumR * sumR;
  sumComponentsMatrix[1] = sumR * sumG;
  sumComponentsMatrix[2] = sumR * sumB;

  sumComponentsMatrix[3] = sumComponentsMatrix[1];
  sumComponentsMatrix[4] = sumG * sumG;
  sumComponentsMatrix[5] = sumG * sumB;

  sumComponentsMatrix[6] = sumComponentsMatrix[2];
  sumComponentsMatrix[7] = sumComponentsMatrix[5];
  sumComponentsMatrix[8] = sumB * sumB;

  KEYER_FLOAT n = (KEYER_FLOAT)statistic.elemsCount;
  assert(n > 0);
  int i = 0;
  for (; i < 9; ++i) {
    statistic.covariance[i] = statistic.matrixR[i] - sumComponentsMatrix[i] / n;
    assert(!ISNAN(statistic.matrixR[i]));
    // assert(statistic.covariance[i] >= 0.0);
    // numerical instability???
    if (statistic.covariance[i] < 0.0) statistic.covariance[i] = 0.0;
  }
}

//------------------------------------------------------------------------------

void Cluster::insert(ClusterElem *elem) {
#ifdef CLUSTER_ELEM_CONTAINER_IS_A_SET

  data.insert(elem);

#else

  data.push_back(elem);

#endif
}

//------------------------------------------------------------------------------

void Cluster::computeStatistics() {
  // Initializes the cluster statistics.
  statistic.elemsCount = 0;

  statistic.sumCoords = TPoint(0, 0);

  int i                                         = 0;
  for (; i < 3; ++i) statistic.sumComponents[i] = 0.0;

  for (i = 0; i < 9; ++i) statistic.matrixR[i] = 0.0;

  // Compute cluster statistics.
  ClusterElemContainer::const_iterator it = data.begin();
  for (; it != data.end(); ++it) {
    const ClusterElem *elem = *it;

#ifdef WITH_ALPHA_IN_STATISTICS
    KEYER_FLOAT alpha = elem->a;
#endif
    KEYER_FLOAT r = (KEYER_FLOAT)elem->r;
    KEYER_FLOAT g = (KEYER_FLOAT)elem->g;
    KEYER_FLOAT b = (KEYER_FLOAT)elem->b;

    statistic.sumComponents[0] += r;
    statistic.sumComponents[1] += g;
    statistic.sumComponents[2] += b;

#ifdef WITH_ALPHA_IN_STATISTICS

    statistic.sumAlpha += alpha;

#endif

    // The first row of the matrix R
    statistic.matrixR[0] += r * r;
    statistic.matrixR[1] += r * g;
    statistic.matrixR[2] += r * b;

    // Second row of the matrix R
    statistic.matrixR[3] += r * g;
    statistic.matrixR[4] += g * g;
    statistic.matrixR[5] += g * b;

    // The third row of the matrix R
    statistic.matrixR[6] += r * b;
    statistic.matrixR[7] += b * g;
    statistic.matrixR[8] += b * b;

    statistic.sumCoords.x += elem->x;
    statistic.sumCoords.y += elem->y;

    ++statistic.elemsCount;
  }

  assert(statistic.elemsCount > 0);

  computeCovariance();
}

//------------------------------------------------------------------------------

void Cluster::getMeanAxis(KEYER_FLOAT axis[3]) {
  KEYER_FLOAT n = (KEYER_FLOAT)statistic.elemsCount;

#if 1

  axis[0] = (KEYER_FLOAT)(sqrt(statistic.covariance[0]) / n);
  axis[1] = (KEYER_FLOAT)(sqrt(statistic.covariance[4]) / n);
  axis[2] = (KEYER_FLOAT)(sqrt(statistic.covariance[8]) / n);

#else

  KEYER_FLOAT I[3];
  KEYER_FLOAT J[3];
  KEYER_FLOAT K[3];

  I[0] = statistic.covariance[0];
  I[1] = statistic.covariance[1];
  I[2] = statistic.covariance[2];

  J[0] = statistic.covariance[3];
  J[1] = statistic.covariance[4];
  J[2] = statistic.covariance[5];

  K[0] = statistic.covariance[6];
  K[1] = statistic.covariance[7];
  K[2] = statistic.covariance[8];

  KEYER_FLOAT magnitudeI = I[0] * I[0] + I[1] * I[1] + I[2] * I[2];
  KEYER_FLOAT magnitudeJ = J[0] * J[0] + J[1] * J[1] + J[2] * I[2];
  KEYER_FLOAT magnitudeK = K[0] * K[0] + K[1] * K[1] + K[2] * I[2];

  if (magnitudeI >= magnitudeJ && magnitudeI >= magnitudeK) {
    axis[0] = sqrt(I[0] / n);
    axis[1] = sqrt(I[1] / n);
    axis[2] = sqrt(I[2] / n);
  } else if (magnitudeJ >= magnitudeI && magnitudeJ >= magnitudeK) {
    axis[0] = sqrt(J[0] / n);
    axis[1] = sqrt(J[1] / n);
    axis[2] = sqrt(J[2] / n);
  } else if (magnitudeK >= magnitudeI && magnitudeK >= magnitudeJ) {
    axis[0] = sqrt(K[0] / n);
    axis[1] = sqrt(K[1] / n);
    axis[2] = sqrt(K[2] / n);
  }

#endif
}

//------------------------------------------------------------------------------

//#define METODO_USATO_SU_TOONZ46

static void buildPaletteForBlendedImages(std::set<TPixel32> &palette,
                                         const TRaster32P &raster,
                                         int maxColorCount) {
  int lx = raster->getLx();
  int ly = raster->getLy();

  ClusterContainer clusters;
  Cluster *cluster = new Cluster;
  raster->lock();
  for (int y = 0; y < ly; ++y) {
    TPixel32 *pix = raster->pixels(y);
    for (int x = 0; x < lx; ++x) {
      TPixel32 color = *(pix + x);
      ClusterElem *ce =
          new ClusterElem(color.r, color.g, color.b, (float)(color.m / 255.0));
      cluster->insert(ce);
    }
  }
  raster->unlock();
  cluster->computeStatistics();

  clusters.push_back(cluster);
  clusterize(clusters, maxColorCount);

  palette.clear();
  // palette.reserve( clusters.size());

  for (UINT i = 0; i < clusters.size(); ++i) {
    ClusterStatistic &stat = clusters[i]->statistic;
    TPixel32 col((int)(stat.sumComponents[0] / stat.elemsCount * 255),
                 (int)(stat.sumComponents[1] / stat.elemsCount * 255),
                 (int)(stat.sumComponents[2] / stat.elemsCount * 255), 255);
    palette.insert(col);

    clearPointerContainer(clusters[i]->data);
  }

  clearPointerContainer(clusters);
}

//------------------------------------------------------------------------------

#include <QPoint>
#include <QRect>
#include <QList>
#include <QMap>

namespace {
#define DISTANCE 3

bool inline areNear(const TPixel &c1, const TPixel &c2) {
  if (abs(c1.r - c2.r) > DISTANCE) return false;
  if (abs(c1.g - c2.g) > DISTANCE) return false;
  if (abs(c1.b - c2.b) > DISTANCE) return false;
  if (abs(c1.m - c2.m) > DISTANCE) return false;

  return true;
}

bool find(const std::set<TPixel32> &palette, const TPixel &color) {
  std::set<TPixel32>::const_iterator it = palette.begin();

  while (it != palette.end()) {
    if (areNear(*it, color)) return true;
    ++it;
  }

  return false;
}

static TPixel32 getPixel(int x, int y, const TRaster32P &raster) {
  return raster->pixels(y)[x];
}

struct EdgePoint {
  QPoint pos;
  enum QUADRANT {
    RightUpper = 0x01,
    LeftUpper  = 0x02,
    LeftLower  = 0x04,
    RightLower = 0x08
  };
  enum EDGE {
    UpperEdge = 0x10,
    LeftEdge  = 0x20,
    LowerEdge = 0x40,
    RightEdge = 0x80
  };

  unsigned char info = 0;

  EdgePoint(int x, int y) {
    pos.setX(x);
    pos.setY(y);
  }
  // identify the edge pixel by checking if the four neighbor pixels
  // (distanced by step * 3 pixels) has the same color as the center pixel
  void initInfo(const TRaster32P &raster, const int step) {
    int lx            = raster->getLx();
    int ly            = raster->getLy();
    TPixel32 keyColor = getPixel(pos.x(), pos.y(), raster);
    info              = 0;
    int dist          = step * 3;
    if (pos.y() < ly - dist &&
        keyColor == getPixel(pos.x(), pos.y() + dist, raster))
      info = info | UpperEdge;
    if (pos.x() >= dist &&
        keyColor == getPixel(pos.x() - dist, pos.y(), raster))
      info = info | LeftEdge;
    if (pos.y() >= dist &&
        keyColor == getPixel(pos.x(), pos.y() - dist, raster))
      info = info | LowerEdge;
    if (pos.x() < lx - dist &&
        keyColor == getPixel(pos.x() + dist, pos.y(), raster))
      info = info | RightEdge;

    // identify available corners
    if (info & UpperEdge) {
      if (info & RightEdge) info = info | RightUpper;
      if (info & LeftEdge) info  = info | LeftUpper;
    }
    if (info & LowerEdge) {
      if (info & RightEdge) info = info | RightLower;
      if (info & LeftEdge) info  = info | LeftLower;
    }
  }

  bool isCorner() {
    return info & RightUpper || info & LeftUpper || info & RightLower ||
           info & LeftLower;
  }
};

struct ColorChip {
  QRect rect;
  TPixel32 color;
  QPoint center;

  ColorChip(const QPoint &topLeft, const QPoint &bottomRight)
      : rect(topLeft, bottomRight) {}

  bool validate(const TRaster32P &raster, const int step) {
    int lx = raster->getLx();
    int ly = raster->getLy();

    // just in case - boundary conditions
    if (!QRect(0, 0, lx - 1, ly - 1).contains(rect)) return false;

    // rectangular must be equal or bigger than 3 * lineWidth
    if (rect.width() < step * 3 || rect.height() < step * 3) return false;

    // obtain center color
    center = rect.center();
    color  = getPixel(center.x(), center.y(), raster);

    // it should not be transparent
    if (color == TPixel::Transparent) return false;

    // rect should be filled with single color
    raster->lock();
    for (int y = rect.top() + step; y <= rect.bottom() - 1; y += step) {
      TPixel *pix = raster->pixels(y) + rect.left() + step;
      for (int x = rect.left() + step; x <= rect.right() - 1;
           x += step, pix += step) {
        if (*pix != color) {
          raster->unlock();
          return false;
        }
      }
    }
    raster->unlock();

    return true;
  }
};

bool lowerLeftThan(const EdgePoint &ep1, const EdgePoint &ep2) {
  if (ep1.pos.y() != ep2.pos.y()) return ep1.pos.y() < ep2.pos.y();
  return ep1.pos.x() < ep2.pos.x();
}

bool colorChipUpperLeftThan(const ColorChip &chip1, const ColorChip &chip2) {
  if (chip1.center.y() != chip2.center.y())
    return chip1.center.y() > chip2.center.y();
  return chip1.center.x() < chip2.center.x();
}

bool colorChipLowerLeftThan(const ColorChip &chip1, const ColorChip &chip2) {
  if (chip1.center.y() != chip2.center.y())
    return chip1.center.y() < chip2.center.y();
  return chip1.center.x() < chip2.center.x();
}

bool colorChipLeftUpperThan(const ColorChip &chip1, const ColorChip &chip2) {
  if (chip1.center.x() != chip2.center.x())
    return chip1.center.x() < chip2.center.x();
  return chip1.center.y() > chip2.center.y();
}

}  // namespace

/*-- 似ている色をまとめて1つのStyleにする --*/
void TColorUtils::buildPalette(std::set<TPixel32> &palette,
                               const TRaster32P &raster, int maxColorCount) {
  int lx   = raster->getLx();
  int ly   = raster->getLy();
  int wrap = raster->getWrap();

  int x, y;
  TPixel old      = TPixel::Black;
  int solidColors = 0;
  int count       = maxColorCount;
  raster->lock();
  for (y = 1; y < ly - 1 && count > 0; y++) {
    TPixel *pix = raster->pixels(y);
    for (x = 1; x < lx - 1 && count > 0; x++, pix++) {
      TPixel color = *pix;
      if (areNear(color, *(pix - 1)) && areNear(color, *(pix + 1)) &&
          areNear(color, *(pix - wrap)) && areNear(color, *(pix + wrap)) &&
          areNear(color, *(pix - wrap - 1)) &&
          areNear(color, *(pix - wrap + 1)) &&
          areNear(color, *(pix + wrap - 1)) &&
          areNear(color, *(pix + wrap + 1))) {
        solidColors++;
        if (!areNear(*pix, old) && !find(palette, *pix)) {
          old = color;
          count--;
          palette.insert(color);
        }
      }
    }
  }

  raster->unlock();

  if (solidColors < lx * ly / 2) {
    palette.clear();
    buildPaletteForBlendedImages(palette, raster, maxColorCount);
  }
}
//------------------------------------------------------------------------------

/*-- 全ての異なるピクセルの色を別のStyleにする --*/
void TColorUtils::buildPrecisePalette(std::set<TPixel32> &palette,
                                      const TRaster32P &raster,
                                      int maxColorCount) {
  int lx   = raster->getLx();
  int ly   = raster->getLy();
  int wrap = raster->getWrap();

  int x, y;
  int count = maxColorCount;
  raster->lock();
  for (y = 1; y < ly - 1 && count > 0; y++) {
    TPixel *pix = raster->pixels(y);
    for (x = 1; x < lx - 1 && count > 0; x++, pix++) {
      if (!find(palette, *pix)) {
        TPixel color = *pix;
        count--;
        palette.insert(color);
      }
    }
  }

  raster->unlock();

  /*-- 色数が最大値を超えたら、似ている色をまとめて1つのStyleにする手法を行う
   * --*/
  if (count == 0) {
    palette.clear();
    buildPalette(palette, raster, maxColorCount);
  }
}

//------------------------------------------------------------------------------

void TColorUtils::buildColorChipPalette(QList<QPair<TPixel32, TPoint>> &palette,
                                        const TRaster32P &raster,
                                        int maxColorCount,
                                        const TPixel32 &gridColor,
                                        const int gridLineWidth,
                                        const int colorChipOrder) {
  int lx   = raster->getLx();
  int ly   = raster->getLy();
  int wrap = raster->getWrap();

  QList<EdgePoint> edgePoints;

  // search for gridColor in the image
  int step = gridLineWidth;
  int x, y;
  for (y = 0; y < ly; y += step) {
    TPixel *pix = raster->pixels(y);
    for (x = 0; x < lx; x += step, pix += step) {
      if (*pix == gridColor) {
        EdgePoint edgePoint(x, y);
        edgePoint.initInfo(raster, step);
        // store the edgePoint if it can be a corner
        if (edgePoint.isCorner()) edgePoints.append(edgePoint);
      }
    }
  }

  // std::cout << "edgePoints.count = " << edgePoints.count() << std::endl;
  // This may be unnecessary
  qSort(edgePoints.begin(), edgePoints.end(), lowerLeftThan);

  QList<ColorChip> colorChips;

  // make rectangles by serching in the corner points
  for (int ep0 = 0; ep0 < edgePoints.size(); ep0++) {
    QMap<EdgePoint::QUADRANT, int> corners;

    // if a point cannot be the first corner, continue
    if ((edgePoints.at(ep0).info & EdgePoint::RightUpper) == 0) continue;

    // find vertices of rectangle in counter clockwise direction

    // if a point is found which can be the first corner
    // search for the second corner point at the right side of the first one
    for (int ep1 = ep0 + 1; ep1 < edgePoints.size(); ep1++) {
      // end searching at the end of scan line
      if (edgePoints.at(ep0).pos.y() != edgePoints.at(ep1).pos.y()) break;
      // if a point cannot be the second corner, continue
      if ((edgePoints.at(ep1).info & EdgePoint::LeftUpper) == 0) continue;

      // if a point is found which can be the second corner
      // search for the third corner point at the upper side of the second one
      for (int ep2 = ep1 + 1; ep2 < edgePoints.size(); ep2++) {
        // the third point must be at the same x position as the second one
        if (edgePoints.at(ep1).pos.x() != edgePoints.at(ep2).pos.x()) continue;

        // if a point cannot be the third corner, continue
        if ((edgePoints.at(ep2).info & EdgePoint::LeftLower) == 0) continue;

        // if a point is found which can be the third corner
        // search for the forth corner point at the left side of the third one
        for (int ep3 = ep1 + 1; ep3 < ep2; ep3++) {
          // if the forth point is found
          if ((edgePoints.at(ep3).info & EdgePoint::RightLower) &&
              edgePoints.at(ep0).pos.x() == edgePoints.at(ep3).pos.x() &&
              edgePoints.at(ep2).pos.y() == edgePoints.at(ep3).pos.y()) {
            corners[EdgePoint::RightLower] = ep3;
            break;
          }
        }  // search for ep3 loop

        if (corners.contains(EdgePoint::RightLower)) {
          corners[EdgePoint::LeftLower] = ep2;
          break;
        }

      }  // search for ep2 loop

      if (corners.contains(EdgePoint::LeftLower)) {
        corners[EdgePoint::LeftUpper] = ep1;
        break;
      }

    }  // search for ep1 loop

    // check if all the 4 corner points are found
    if (corners.contains(EdgePoint::LeftUpper)) {
      corners[EdgePoint::RightUpper] = ep0;

      assert(corners.size() == 4);

      // register color chip
      ColorChip chip(edgePoints.at(corners[EdgePoint::RightUpper]).pos,
                     edgePoints.at(corners[EdgePoint::LeftLower]).pos);
      if (chip.validate(raster, step)) colorChips.append(chip);

      // remove the coner information from the corner point
      QMap<EdgePoint::QUADRANT, int>::const_iterator i = corners.constBegin();
      while (i != corners.constEnd()) {
        edgePoints[i.value()].info & ~i.key();
        ++i;
      }
      if (colorChips.count() >= maxColorCount) break;
    }
  }

  // std::cout << "colorChips.count = " << colorChips.count() << std::endl;
  if (!colorChips.empty()) {
    // 0:UpperLeft 1:LowerLeft 2:LeftUpper
    // sort the color chips
    switch (colorChipOrder) {
    case 0:
      qSort(colorChips.begin(), colorChips.end(), colorChipUpperLeftThan);
      break;
    case 1:
      qSort(colorChips.begin(), colorChips.end(), colorChipLowerLeftThan);
      break;
    case 2:
      qSort(colorChips.begin(), colorChips.end(), colorChipLeftUpperThan);
      break;
    }

    for (int c = 0; c < colorChips.size(); c++)
      palette.append(qMakePair(
          colorChips.at(c).color,
          TPoint(colorChips.at(c).center.x(), colorChips.at(c).center.y())));
  }
}

//------------------------------------------------------------------------------
