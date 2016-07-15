

//#include "traster.h"
#include "tcolorutils.h"
#include "tmathutil.h"

#include <set>
#include <list>
#include <cmath>

typedef float KEYER_FLOAT;

//------------------------------------------------------------------------------
#ifdef _WIN32
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
  KEYER_FLOAT sumComponents[3];  // vettore 3x1
  unsigned int elemsCount;
  KEYER_FLOAT matrixR[9];  // matrice 3x3 = somma(x * trasposta(x))
                           // dove x sono i pixel del cluster

  KEYER_FLOAT covariance[9];  // matrice di covarianza
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
    // calcola la matrice di covarianza
    const KEYER_FLOAT *clusterCovariance = cluster->statistic.covariance;
    assert(!ISNAN(clusterCovariance[0]));

    // calcola gli autovalori della matrice di covarianza della statistica
    // del cluster (siccome la matrice e' simmetrica gli autovalori
    // sono tutti reali)
    KEYER_FLOAT eigenValues[3];
    tmpMulteplicity = calcCovarianceEigenValues(clusterCovariance, eigenValues);
    assert(tmpMulteplicity > 0);

    tmpEigenValue = std::max({eigenValues[0], eigenValues[1], eigenValues[2]});
    cluster->eigenValue = tmpEigenValue;

    // eventuale aggiornamento del cluster da cercare
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

    // calcola l'autovettore relativo a maxEigenValue
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

    // normalizzazione dell'autovettore calcolato
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

  // faccio in modo che in clusters ci siano solo e sempre le foglie
  // dell'albero calcolato secondo l'algoritmo TSE descritto da Orchard-Bouman
  // (c.f.r. "Color Quantization of Images" - M.Orchard, C. Bouman)

  // numero di iterazioni, numero di cluster = numero di iterazioni + 1
  int m = clustersCount - 1;
  int i = 0;
  for (; i < m; ++i) {
    // sceglie la foglia dell'albero dei cluster (ovvero il cluster nel
    // ClusterContainer "clusters") che ha il maggiore autovalore, ovvero
    // il cluster che ha maggiore varainza rispetto all'asse opportuno
    // (che poi e' l'autovettore corrispondente all'autovalore piu' grande)
    KEYER_FLOAT eigenValue     = 0.0;
    KEYER_FLOAT eigenVector[3] = {0.0, 0.0, 0.0};
    ClusterContainer::iterator itChoosedCluster;

    chooseLeafToClusterize(itChoosedCluster, eigenValue, eigenVector, clusters);

    assert(itChoosedCluster != clusters.end());
    Cluster *choosedCluster = *itChoosedCluster;

#if 0

    // se il cluster che si e' scelto per la suddivisione contiene un solo
    // elemento vuol dire che non c'e' piu' niente da suddividere e si esce
    // dal ciclo.
    // Questo succede quando si sono chiesti piu' clusters di quanti elementi
    // ci sono nel cluster iniziale.
    if(choosedCluster->statistic.elemsCount == 1)
      break;

#else

    // un cluster che ha un solo elemento non ha molto senso di esistere,
    // credo crei problemi anche nel calcolo della matrice di covarianza,
    // quindi mi fermo quando il cluster contiene meno di 4 elementi
    if (choosedCluster->statistic.elemsCount == 3) break;

#endif

    // suddivide il cluster scelto in altri due cluster
    Cluster *subcluster1 = new Cluster();
    Cluster *subcluster2 = new Cluster();
    split(subcluster1, subcluster2, eigenVector, choosedCluster);
    assert(subcluster1);
    assert(subcluster2);
    if ((subcluster1->data.size() == 0) || (subcluster2->data.size() == 0))
      break;

    // calcola la nuova statistica per subcluster1
    subcluster1->computeStatistics();

    // calcola la nuova statistica per subcluster2
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

    // aggiorna in modo opportuno il ClusterContainer "clusters", cancellando
    // il cluster scelto e inserendo i due appena creati.
    // Facendo cosi' il ClusterContainer "cluster" contiene solo e sempre
    // le foglie dell'albero creato dall'algoritmo TSE.

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
    // instabilita' numerica ???
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
  // inizializza a zero la statistica del cluster
  statistic.elemsCount = 0;

  statistic.sumCoords = TPoint(0, 0);

  int i                                         = 0;
  for (; i < 3; ++i) statistic.sumComponents[i] = 0.0;

  for (i = 0; i < 9; ++i) statistic.matrixR[i] = 0.0;

  // calcola la statistica del cluster
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

    // prima riga della matrice R
    statistic.matrixR[0] += r * r;
    statistic.matrixR[1] += r * g;
    statistic.matrixR[2] += r * b;

    // seconda riga della matrice R
    statistic.matrixR[3] += r * g;
    statistic.matrixR[4] += g * g;
    statistic.matrixR[5] += g * b;

    // terza riga della matrice R
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
                                         const TRaster32P &raster, int maxColorCount) {
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
