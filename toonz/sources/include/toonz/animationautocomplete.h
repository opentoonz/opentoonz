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

struct SimilarPair
{
	double dissimilarityFactor;
	PointWithStroke* point1;
	PointWithStroke* point2;
};


class GlobalSimilarityGraph
{
	std::map<SimilarPair*, std::vector<SimilarPair*>> connections;

public:
	int numberOfNodes = 0;

	GlobalSimilarityGraph() {}
	~GlobalSimilarityGraph() {}
	void insertNode(SimilarPair* pair, std::vector<SimilarPair*> connections);
	std::vector<SimilarPair *> getConnections(SimilarPair* pair);
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

struct StrokeWithScore
{
	double score;
	StrokeWithNeighbours* stroke;
};

class AnimationAutoComplete {
public:
  AnimationAutoComplete() {}
  ~AnimationAutoComplete() {}


  void addStroke(TStroke* stroke);
  std::vector<StrokeWithNeighbours*> getSynthesizedStrokes();

  //TODO: remove at production
  std::vector<TStroke*> drawSpaceVicinity(TStroke* stroke);

  HungerianMatrix setHungerianMatrix(StrokeWithNeighbours* stroke1 ,StrokeWithNeighbours* stroke2);
  TStroke* drawstrokeLine(TStroke* stroke);

  TPointD  getNormal(PointWithStroke* pointer);

private:

  int m_spaceVicinityRadius = 100;
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

  SimilarPair getMostSimilarPoint(PointWithStroke* point, TStroke* stroke);

  double pointsSimilarity (PointWithStroke* point1, PointWithStroke* point2);
  double pointsSimilarityWithoutWeights(PointWithStroke* point1, PointWithStroke* point2);
  double getAppearanceSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getTemporalSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getSpatialSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getNeighborhoodSimilarity(StrokeWithNeighbours *stroke1, StrokeWithNeighbours *stroke2);

  std::vector<double> getCentralSimilarities(TStroke* stroke);

  std::vector <SimilarPair*> getNeighborhoodMatchingPairs (StrokeWithNeighbours* stroke1,
														   StrokeWithNeighbours* stroke2);

  SamplePoint minimizeDissimilarity (SamplePoint* central, SamplePoint* point);

  double magnitude(std::vector<double> points);

  std::vector<SimilarPair> getSimilarPairs(StrokeWithNeighbours*, StrokeWithNeighbours*);
  std::vector<double> differnceOfTwoNeighborhood(StrokeWithNeighbours* stroke1, StrokeWithNeighbours* stroke2);
  std::vector<StrokeWithNeighbours*> getNeighbours(SamplePoint point);

  bool withinSpaceVicinity(const SamplePoint samplePoint, const SamplePoint point);
  void initializeSynthesis();

  std::vector<StrokeWithNeighbours*> search(StrokeWithNeighbours *operation1);
  StrokeWithNeighbours* assign(std::vector<StrokeWithNeighbours*>);

  TPointD meanGlobal(std::vector<SamplePoint> globalSamples);
  SimilarPair* meanLocal(std::vector<SimilarPair *> localPairs);
  TPointD deviationGlobal(std::vector<SamplePoint> globalSamples);
  SimilarPair* deviationLocal(std::vector<SimilarPair *> localPairs);

  // hungarian function for matching pairs
   // double hungarian_method();



};

#endif // ANIMATIONAUTOCOMPLETE_H
