#ifndef ANIMATIONAUTOCOMPLETE_H
#define ANIMATIONAUTOCOMPLETE_H

//#define DEBUGGING

//#define ODAY_SYNTHESIS
#define ADAM_SYSTHESIS

#ifdef DEBUGGING
		//#define SHOW_NORMALS
		//#define SHOW_MATCHING_STROKE
		//#define SHOW_PAIR_LINES
		//#define SHOW_SPACE_VICINITY
		//#define SHOW_PAIR_STROKES
#endif

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
    PointWithStroke(SamplePoint point, TStroke* stroke, int index) : point(point), stroke(stroke), index(index) {}
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

	TPointD getCentralSample();
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

#ifdef DEBUGGING
  //TODO: remove at production
#ifdef SHOW_SPACE_VICINITY
  std::vector<TStroke*> drawSpaceVicinity(TStroke* stroke);
#endif
#ifdef SHOW_NORMALS
  std::vector<TStroke *> drawNormalStrokes(TStroke* stroke);
#endif
#if defined(SHOW_PAIR_LINES) || defined(SHOW_MATCHING_STROKE)
  StrokeWithNeighbours* matchedStroke;
#endif
#ifdef SHOW_PAIR_LINES
  std::vector<TStroke*> m_similarPairLines;
  std::vector<TStroke*> m_oldSimilarPairLines;
#endif
#ifdef SHOW_PAIR_STROKES
  std::vector<TStroke*> pairStrokes;
#endif
#endif

  std::vector<StrokeWithNeighbours*> m_strokesWithNeighbours;
private:
  const int m_spaceVicinityRadius = 100;

  std::vector<StrokeWithNeighbours*> m_synthesizedStrokes;

  TPointD getTangentUnitVector(PointWithStroke* pointer);
  double gaussianConstant(PointWithStroke* chuck1, PointWithStroke* chuck2);
  double operationsSimilarity (StrokeWithNeighbours* stroke1, StrokeWithNeighbours* stroke2);

  StrokeWithNeighbours *generateSynthesizedStroke(StrokeWithNeighbours* lastStroke, StrokeWithNeighbours* similarStroke);

#if defined(SHOW_PAIR_LINES) || defined(SHOW_PAIR_STROKES)
  TStroke* generateLineStroke(TPointD beginning, TPointD end);
#endif
#ifdef SHOW_PAIR_LINES
  bool pairsAreMatchingStrokeAndLastStroke(StrokeWithNeighbours* stroke1, StrokeWithNeighbours* stroke2);
#endif
  double pointsSimilarity (PointWithStroke* point1, PointWithStroke* point2);
  double pointsSimilarityWithoutWeights(PointWithStroke* point1, PointWithStroke* point2);
  double getAppearanceSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getTemporalSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getSpatialSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getNeighborhoodSimilarity(StrokeWithNeighbours *stroke1, StrokeWithNeighbours *stroke2);

  std::vector<SimilarPairPoint> getSimilarPairPoints(StrokeWithNeighbours *stroke1, StrokeWithNeighbours *stroke2);
  std::vector<SimilarPairStroke> getSimilarPairStrokes(StrokeWithNeighbours* stroke1 ,StrokeWithNeighbours* stroke2);

  bool strokeSelfLooping(TStroke* stroke);
  double getCentralSimilarities(std::vector<SimilarPairPoint> similarPairPoints);
  double getSimilarPairPointBySampleId(TStroke *stroke1, TStroke *stroke2);

  double magnitude(std::vector<double> points);

  std::vector<StrokeWithNeighbours*> getNeighbours(PointWithStroke point);
  void getNeighbours(StrokeWithNeighbours* stroke);

  bool withinSpaceVicinity(const SamplePoint samplePoint, const SamplePoint point);
  void initializeSynthesis();
  double getSampleId(const int &index, const int &n);
  double getReversedSampleId(const int &index, const int &n);
  std::vector<StrokeWithNeighbours*> search(StrokeWithNeighbours *operation1);
  StrokeWithNeighbours* assign(std::vector<StrokeWithNeighbours*>);
  std::vector<StrokeWithNeighbours*> getContextStrokes(StrokeWithNeighbours* stroke);

  TPointD meanGlobal(std::vector<SamplePoint> globalSamples);
  SimilarPairPoint* meanLocal(std::vector<SimilarPairPoint *> localPairs);
  TPointD deviationGlobal(std::vector<SamplePoint> globalSamples);
  SimilarPairPoint* deviationLocal(std::vector<SimilarPairPoint *> localPairs);

  double differnceOfTwoNeighborhood(StrokeWithNeighbours* stroke1, StrokeWithNeighbours* stroke2,  std::vector<SimilarPairStroke> similarPairStrokes);

  // oday's alternative to generate synthesized stroke
  StrokeWithNeighbours *predictionPositionUpdate(StrokeWithNeighbours* currentStroke, StrokeWithNeighbours* nextStroke);
  void getPredictedNeighours(StrokeWithNeighbours* predictedStroke, StrokeWithNeighbours* similarStroke);
};

#endif // ANIMATIONAUTOCOMPLETE_H
