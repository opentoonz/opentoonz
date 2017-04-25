#ifndef ANIMATIONAUTOCOMPLETE_H
#define ANIMATIONAUTOCOMPLETE_H

#include <unordered_set>
#include <vector>

#include "tstroke.h"

//************************************************************************
//    Animation Auto-complete declaration
//    Detects repetitions in strokes within/across frames and predicts
//    the next stroke(s) accordingly.
//************************************************************************
typedef TThickQuadratic* SamplePoint;

class PointWithStroke
{
public:
    PointWithStroke() {}
    PointWithStroke(SamplePoint point, TStroke* stroke) : point(point), stroke(stroke) {}
    ~PointWithStroke() {}
    SamplePoint point;
    TStroke* stroke;
    int index;
};

struct Neighbor
{
    TStroke* stroke;
    int localTimeStamp;// we calculate the local timestamp by dividing the index over the total number of points
};

typedef std::vector<Neighbor*> Neighbors;

struct SimilarPairPoint
{
    double dissimilarityFactor;
    PointWithStroke* point1;
    PointWithStroke* point2;
};

struct MatchingOperations
{
    TStroke* stroke1;
    TStroke* stroke2;
};

class GlobalSimilarityGraph
{

    std::map<SimilarPairPoint*, std::vector<SimilarPairPoint*>> connections;



public:
    int numberOfNodes = 0;


	GlobalSimilarityGraph() {}
	~GlobalSimilarityGraph() {}
    void insertNode(SimilarPairPoint* pair, std::vector<SimilarPairPoint*> connections);
    std::vector<SimilarPairPoint *> getConnections(SimilarPairPoint* pair);

};

typedef std::unordered_set< PointWithStroke *> SetOfPoints;
typedef std::vector<std::vector<double> > HungerianMatrix;

class StrokeWithNeighbours
{
public:
    TStroke* stroke;
    std::unordered_set<StrokeWithNeighbours*> neighbours;
    StrokeWithNeighbours *nextStroke;
};

struct SimilarPairStroke
{
    double dissimilarityFactor;
    StrokeWithNeighbours* stroke1;
    StrokeWithNeighbours* stroke2;
};

struct StrokeWithScore
{
    double score;
    StrokeWithNeighbours* stroke;
};

struct Hungarian
{
    TStroke* stroke1;
    TStroke* stroke2;
    double score;
};

class AnimationAutoComplete {
public:
  AnimationAutoComplete() {}
  ~AnimationAutoComplete() {}


  void addStroke(TStroke* stroke);
  std::vector<StrokeWithNeighbours*> getSynthesizedStrokes();

  //TODO: remove at production
  std::vector<TStroke*> drawSpaceVicinity(TStroke* stroke);
	 //TODO: remove at production
  TStroke* drawNormalStroke(TStroke* stroke);

  TPointD  getNormal(PointWithStroke* pointer);

private:

  const int m_spaceVicinityRadius = 100;
  std::vector<StrokeWithNeighbours*> m_strokesWithNeighbours;
  std::vector<StrokeWithNeighbours*> m_synthesizedStrokes;
  std::vector<double> points;
  double gaussianConstant( SamplePoint chuck1, SamplePoint chuck2);
  double operationsSimilarity (StrokeWithNeighbours* stroke1, StrokeWithNeighbours* stroke2);
  int withinTemporalVicinity(PointWithStroke* point1, PointWithStroke* point2);

  // stroke with corresponding similarity score
  std::vector<StrokeWithScore> getSimilarStrokes (StrokeWithNeighbours* stroke);

  StrokeWithNeighbours* mostSimilarStroke (StrokeWithNeighbours* stroke);
  StrokeWithNeighbours *generateSynthesizedStroke(StrokeWithNeighbours* lastStroke,StrokeWithNeighbours* similarStroke,
                                                  StrokeWithNeighbours* nextToSimilarStroke);

  SimilarPairPoint getMostSimilarPoint(PointWithStroke* point, TStroke* stroke);

  double pointsSimilarity (PointWithStroke* point1, PointWithStroke* point2);
  double pointsSimilarityWithoutWeights(PointWithStroke* point1, PointWithStroke* point2);
  double getAppearanceSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getTemporalSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getSpatialSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getNeighborhoodSimilarity(StrokeWithNeighbours *stroke1, StrokeWithNeighbours *stroke2);
  std::vector<SimilarPairPoint> getSimilarPairPoints(StrokeWithNeighbours *stroke1, StrokeWithNeighbours *stroke2);
  std::vector<SimilarPairStroke> getSimilarPairStrokes(StrokeWithNeighbours* stroke1 ,StrokeWithNeighbours* stroke2);

  double AnimationAutoComplete::getCentralSimilarities(std::vector<SimilarPairPoint> similarPairPoints);

  std::vector<SimilarPairStroke *> AnimationAutoComplete::getNeighborhoodMatchingPairs(StrokeWithNeighbours *stroke1, StrokeWithNeighbours *stroke2);


  SamplePoint minimizeDissimilarity (SamplePoint* central, SamplePoint* point);

  double magnitude(std::vector<double> points);

  std::vector<StrokeWithNeighbours*> getNeighbours(SamplePoint point);

  bool withinSpaceVicinity(const SamplePoint samplePoint, const SamplePoint point);
  void initializeSynthesis();
  double AnimationAutoComplete::getSampleId(int index, int n);

  std::vector<StrokeWithNeighbours*> search(StrokeWithNeighbours *operation1);
  StrokeWithNeighbours* assign(std::vector<StrokeWithNeighbours*>);

  TPointD meanGlobal(std::vector<SamplePoint> globalSamples);
  SimilarPairPoint* meanLocal(std::vector<SimilarPairPoint *> localPairs);
  TPointD deviationGlobal(std::vector<SamplePoint> globalSamples);
  SimilarPairPoint* deviationLocal(std::vector<SimilarPairPoint *> localPairs);
  std::vector<double> AnimationAutoComplete::differnceOfTwoNeighborhood(std::vector<SimilarPairPoint> similarPairPoints);

};

#endif // ANIMATIONAUTOCOMPLETE_H
