#ifndef ANIMATIONAUTOCOMPLETE_H
#define ANIMATIONAUTOCOMPLETE_H

/*!
It is a macro to be used to enable debugging parts of the code. It has no effect in itself.
You must enable one or more of the debugging macros below. ie. SHOW_TANGENT_LINES
*/
//#define DEBUGGING

#define ODAY_SYNTHESIS
//#define ADAM_SYSTHESIS

#ifdef DEBUGGING
		//#define SHOW_TANGENT_LINES // Shows tangent lines for each sample point on a stroke
		//#define SHOW_MATCHING_STROKE // highlights the stroke that had the highest similarity score with the last stroke.
		//#define SHOW_PAIR_LINES // draws lines connecting sample points that are matching in two strokes
		#define SHOW_SPACE_VICINITY // draws the space vicinity around each sample point.
		//#define SHOW_PAIR_STROKES // draws lines connecting different strokes that are matching in two neighbourhoods
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

class StrokeWithNeighbours;

/*!
Contains the point, the stroke it's part of, and its index within that stroke.
*/
class PointWithStroke
{
public:
	PointWithStroke() {}
	PointWithStroke(SamplePoint point, StrokeWithNeighbours* stroke, int index) : point(point), stroke(stroke), index(index) {}
	~PointWithStroke() {}
	SamplePoint point;
	StrokeWithNeighbours* stroke;
	int index;
};

typedef std::unordered_set< PointWithStroke *> SetOfPoints;
typedef std::unordered_set< StrokeWithNeighbours *> SetOfStrokes;
typedef std::vector< std::vector<double> > HungerianMatrix;

class StrokeWithNeighbours
{
public:
	StrokeWithNeighbours()
	{
		stroke		  = nullptr;
		nextStroke	  = nullptr;
		contextStroke = nullptr;
	}

    TStroke* stroke;
	SetOfStrokes neighbours;
	StrokeWithNeighbours* nextStroke;
	StrokeWithNeighbours* contextStroke;

	TPointD getCentralSample();
};

struct SimilarPairPoint
{
	double dissimilarityFactor;
	PointWithStroke* point1;
	PointWithStroke* point2;
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

/*!
This is the main class that has all the functions for similarity analysis and synthesis
*/
class AnimationAutoComplete {
public:
  AnimationAutoComplete() {}
  ~AnimationAutoComplete() {}

  /*!
  To get prediction this function should be called as you add each new stroke to the canvas.
  This is the starting point where all the other functions gets called.
  */
  void addStroke(TStroke* stroke);

  /*!
  After adding strokes you should expect the predicted strokes to be returned using this function.
  */
  std::vector<StrokeWithNeighbours*> getSynthesizedStrokes();

#ifdef DEBUGGING
  //TODO: remove at production
#ifdef SHOW_SPACE_VICINITY
  std::vector<TStroke*> drawSpaceVicinity(TStroke* stroke);
#endif
#ifdef SHOW_TANGENT_LINES
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


private:
  const int m_spaceVicinityRadius = 100;

  std::vector<StrokeWithNeighbours*> m_strokesWithNeighbours; // Stores all the drawn strokes.
  std::vector<StrokeWithNeighbours*> m_synthesizedStrokes;	  // Stores output ie predicted strokes

  TPointD getTangentUnitVector(PointWithStroke* pointer); // gets the tangent to a certain point. useful for spatial similarity analysis
  double gaussianConstant(PointWithStroke* chuck1, PointWithStroke* chuck2);

  StrokeWithNeighbours *generateSynthesizedStroke(StrokeWithNeighbours* lastStroke, StrokeWithNeighbours* similarStroke);

#if defined(SHOW_PAIR_LINES) || defined(SHOW_PAIR_STROKES)
  TStroke* generateLineStroke(TPointD beginning, TPointD end);
#endif
#ifdef SHOW_PAIR_LINES
  bool pairsAreMatchingStrokeAndLastStroke(StrokeWithNeighbours* stroke1, StrokeWithNeighbours* stroke2);
#endif

  //  Similarity Analysis functions
  double operationsSimilarity (StrokeWithNeighbours* stroke1, StrokeWithNeighbours* stroke2);
  double pointsSimilarity (PointWithStroke* point1, PointWithStroke* point2);
  double pointsSimilarityWithoutWeights(PointWithStroke* point1, PointWithStroke* point2);
  double getAppearanceSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getTemporalSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getSpatialSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getNeighborhoodSimilarity(StrokeWithNeighbours *stroke1, StrokeWithNeighbours *stroke2);
  double getCentralSimilarities(std::vector<SimilarPairPoint> similarPairPoints);

  // takes two strokes and returns pairs of matching points
  std::vector<SimilarPairPoint> getSimilarPairPoints(StrokeWithNeighbours *stroke1, StrokeWithNeighbours *stroke2);
  // takes two strokes and returns pairs of matching strokes within thier respective neighbourhoods
  std::vector<SimilarPairStroke> getSimilarPairStrokes(StrokeWithNeighbours* stroke1 ,StrokeWithNeighbours* stroke2);

  // returns whether a stroke is self looping ie, circle, rectangle or any other closed shape
  bool strokeSelfLooping(TStroke* stroke);

  // sqrt( p1^2 + p2^2 + ... )
  double magnitude(std::vector<double> points);

  // returns the neighbours within the space vicinity as well as its two preceding strokes
  std::vector<StrokeWithNeighbours*> getNeighbours(PointWithStroke point);
  // the neighbourhood of a stroke is a union of the neighbours of all of its sample points
  void getNeighbours(StrokeWithNeighbours* stroke);

  bool withinSpaceVicinity(const SamplePoint samplePoint, const SamplePoint point);

  void beginSynthesis();
  std::vector<StrokeWithNeighbours*> search(StrokeWithNeighbours *operation1);
  std::vector<StrokeWithNeighbours*> assign(std::vector<StrokeWithNeighbours*>);

  // Sample ID is the index of a point divided by the total number (n) of sample points in a stroke
  double getSampleId(const int &index, const int &n);
  double getReversedSampleId(const int &index, const int &n);

  // a context stroke is a long stroke that serves as a context to the neighbourhood
  // a context stroke is longer than 2 * the length of the predicted stroke
  std::vector<StrokeWithNeighbours*> getContextStrokes(StrokeWithNeighbours* stroke);

  TPointD meanGlobal(std::vector<SamplePoint> globalSamples);
  SimilarPairPoint* meanLocal(std::vector<SimilarPairPoint *> localPairs);
  TPointD deviationGlobal(std::vector<SamplePoint> globalSamples);
  SimilarPairPoint* deviationLocal(std::vector<SimilarPairPoint *> localPairs);

  double differnceOfTwoNeighborhood(StrokeWithNeighbours* stroke1, StrokeWithNeighbours* stroke2,  std::vector<SimilarPairStroke> similarPairStrokes);

  // Oday's alternative to generate synthesized stroke
  StrokeWithNeighbours *predictionPositionUpdate(StrokeWithNeighbours* currentStroke, StrokeWithNeighbours* nextStroke);
  std::vector<StrokeWithNeighbours*> getPredictedNeighours(StrokeWithNeighbours* predictedStroke, StrokeWithNeighbours* similarStroke);
};

#endif // ANIMATIONAUTOCOMPLETE_H
