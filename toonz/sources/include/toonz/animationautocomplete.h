#ifndef ANIMATIONAUTOCOMPLETE_H
#define ANIMATIONAUTOCOMPLETE_H

#include <unordered_set>

#include "tstroke.h"

//************************************************************************
//    Animation Auto-complete declaration
//    Detects repetitions in strokes within/across frames and predicts
//    the next stroke(s) accordingly.
//************************************************************************
typedef TThickQuadratic* SamplePoint;

class  PointWithStroke
{
public:
	PointWithStroke() {}
	PointWithStroke(SamplePoint point, TStroke* stroke) : point(point), stroke(stroke) {}
	~PointWithStroke() {}
	SamplePoint point;
	TStroke* stroke;
    int index;

};

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

class StrokeWithNeighbours
{
public:
	TStroke* stroke;
	SetOfPoints neighbours;
	StrokeWithNeighbours *nextStroke;
};

struct StrokeWithScore
{
public:
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


  TStroke* drawstrokeLine(TStroke* stroke);

  TPointD  getNormal(PointWithStroke* pointer);

private:
  int m_spaceVicinityRadius = 100;
  std::vector<StrokeWithNeighbours*> m_strokesWithNeighbours;
  std::vector<StrokeWithNeighbours*> m_synthesizedStrokes;
  std::vector<double> points;

  double gaussianConstant( SamplePoint chuck1, SamplePoint chuck2);
  double operationsSimilarity (StrokeWithNeighbours* stroke1, StrokeWithNeighbours* stroke2);

  // stroke with corresponding similarity score
  std::vector<StrokeWithScore> getSimilarStrokes (StrokeWithNeighbours* stroke);

  StrokeWithNeighbours *generateSynthesizedStroke(StrokeWithNeighbours* lastStroke,StrokeWithNeighbours* similarStroke,
												  StrokeWithNeighbours* nextToSimilarStroke);

  SimilarPair getMostSimilarPoint(PointWithStroke* point, TStroke* stroke);

  double pointsSimilarity (PointWithStroke* point1, PointWithStroke* point2);
  double pointsSimilarityWithoutWeights(PointWithStroke* point1, PointWithStroke* point2);
  double getAppearanceSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getTemporalSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getSpatialSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getNeighborhoodSimilarity(StrokeWithNeighbours* operation1, StrokeWithNeighbours* operation2);
  double magnitude(std::vector<double> points);
  std::vector<SimilarPair> getSimilarPairs(StrokeWithNeighbours*, StrokeWithNeighbours*);

  SetOfPoints getNeighbours(const SamplePoint point);
  bool withinSpaceVicinity(const SamplePoint samplePoint, const SamplePoint point);
  void initializeSynthesis();
  std::vector<StrokeWithNeighbours*> search(StrokeWithNeighbours *operation1);
  StrokeWithNeighbours* assign(std::vector<StrokeWithNeighbours*>);


};

#endif // ANIMATIONAUTOCOMPLETE_H
