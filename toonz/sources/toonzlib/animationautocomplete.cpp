#include "toonz/animationautocomplete.h"

#include "tcurves.h"
#include "drawutil.h"

#include "toonz/hungarian.h"

#include <toonz/strokegenerator.h>

void AnimationAutoComplete::addStroke(TStroke* stroke)
{
#ifdef DEBUGGING
	//TODO: remove at production
#ifdef SHOW_PAIR_LINES
	m_oldSimilarPairLines = m_similarPairLines;
	m_similarPairLines.clear();
#endif
#if defined(SHOW_PAIR_LINES) || defined(SHOW_MATCHING_STROKE)
	matchedStroke =  nullptr;
#endif
#endif
	// clears previous predictions
	m_synthesizedStrokes.clear();
	StrokeWithNeighbours* strokeWithNeighbours = new StrokeWithNeighbours();

	if(m_strokesWithNeighbours.size()!=0)
		m_strokesWithNeighbours.back()->nextStroke = strokeWithNeighbours;

	m_strokesWithNeighbours.push_back(strokeWithNeighbours);

	strokeWithNeighbours->stroke = stroke;

	getNeighbours(strokeWithNeighbours);
	getContextStrokes(strokeWithNeighbours);

	if (m_strokesWithNeighbours.size() >= 3)
		beginSynthesis();
}

void AnimationAutoComplete::setStrokesWithNeighbours(std::vector<StrokeWithNeighbours*> p_strokes)
{
	m_strokesWithNeighbours = p_strokes;
}

//----------
bool AnimationAutoComplete::strokeSelfLooping(TStroke* stroke)
{
	int count = stroke->getChunkCount();
	double range = 8;
	TThickQuadratic* lastPointHolder=stroke->getChunk(count-1);

	TPointD lastpoint=lastPointHolder->getP2();
	for (int i =0;i<count-1;i++)
	{
		TThickQuadratic *currentPointHolder=stroke->getChunk(i);
		TPointD currentPoint=currentPointHolder->getP0();
		//calculate the distance
		double x=lastpoint.x-currentPoint.x;
		x=x*x;
		double y=lastpoint.y-currentPoint.y;
		y=y*y;
		double d=sqrt(x+y);
		if(d<range)
			return true;
	}
	return false;
}

//--------------------------------------------------------------------------------------------------
double AnimationAutoComplete::gaussianConstant(PointWithStroke *chuck1, PointWithStroke *chuck2)
{
	TPointD sample1;
	TPointD sample2;
	if(chuck1->index==chuck1->stroke->stroke->getChunkCount()-1)
		sample1=chuck1->point->getP2();
	if (chuck2->index==chuck2->stroke->stroke->getChunkCount()-1)
		sample2= chuck2->point->getP2();
	else
	{

		sample1= chuck1->point->getP0();
		sample2= chuck2->point->getP0();
	}
	TThickPoint s1; TThickPoint s2;
	s1.x=sample1.x;  s1.y=sample1.y;
	s2.x=sample2.x;  s2.y=sample2.y;
	double distance=tdistance2(s1,s2);
	distance=sqrt(distance);
#define OMEGA 10
	return exp(-distance/OMEGA);
}

void AnimationAutoComplete::getNeighbours(StrokeWithNeighbours* stroke)
{
	int chuckCount = stroke->stroke->getChunkCount();
	for (int i = 0; i < chuckCount; i++)
	{
		PointWithStroke point(stroke->stroke->getChunk(i), stroke, i);
		if (i == chuckCount - 1)
			point.setLastPoint(true);
		else
			point.setLastPoint(false);

		std::vector<StrokeWithNeighbours*> neighbours = getNeighbours(point);
		stroke->neighbours.insert(neighbours.begin(), neighbours.end());
	}
}



std::vector<StrokeWithNeighbours*> AnimationAutoComplete::getNeighbours(PointWithStroke point)
{
	std::vector<StrokeWithNeighbours*> neighbours;

	for(int i = 0; i < m_strokesWithNeighbours.size(); i++)
	{
		TStroke* stroke = m_strokesWithNeighbours[i]->stroke;

		// we used to exclude the stroke that is being compared from the neighbours
		// now we don't. we don't see any impact on accuracy
		for(int j = 0; j < stroke->getChunkCount(); j++)
			if(withinSpaceVicinity(point.point, stroke->getChunk(j)))
			{
				neighbours.push_back(m_strokesWithNeighbours[i]);
				m_strokesWithNeighbours[i]->neighbours.insert(point.stroke); // if you're my neighbour, then I'm your neighbour
				break;
			}
	}

	int size = m_strokesWithNeighbours.size();
	if(size > 2)
	{
		neighbours.push_back(m_strokesWithNeighbours[size - 2]);
		neighbours.push_back(m_strokesWithNeighbours[size - 3]);
	}

	return neighbours;
}

std::unordered_set<StrokeWithNeighbours*> AnimationAutoComplete:: insertVectorIntoSet(std::vector<StrokeWithNeighbours*> vector, std::unordered_set<StrokeWithNeighbours*> set)
{
	std::copy( vector.begin(), vector.end(), std::inserter(set , set.end() ) );
	return set;
}

bool AnimationAutoComplete::withinSpaceVicinity(const SamplePoint samplePoint, const SamplePoint point)
{
	double distance = norm(samplePoint->getP2() - point->getP2());
	if(distance <= m_spaceVicinityRadius)
		return true;
	else
		return false;
}

void AnimationAutoComplete::beginSynthesis()
{
	StrokeWithNeighbours* lastStroke = m_strokesWithNeighbours.back();
	std::vector<StrokeWithNeighbours*> similarStrokes = search(lastStroke);

	m_synthesizedStrokes = assign(similarStrokes);

	// call the whole system recursively
	if (m_numberOfLevelDeepIntoRecursion < m_recursionLimit && m_synthesizedStrokes.size())
	{
		AnimationAutoComplete aac(m_numberOfLevelDeepIntoRecursion+1);
		aac.setStrokesWithNeighbours(m_strokesWithNeighbours);
		aac.addStroke(m_synthesizedStrokes.back()->stroke);
		m_synthesizedStrokes = concatinateVectors(m_synthesizedStrokes, aac.getSynthesizedStrokes());
	}
}

std::vector<StrokeWithNeighbours*> AnimationAutoComplete::getSynthesizedStrokes()
{
	return m_synthesizedStrokes;
}

template <class T>
std::vector<T> AnimationAutoComplete::concatinateVectors(std::vector<T> vector1, std::vector<T> vector2)
{
	std::move(vector2.begin(), vector2.end(), std::back_inserter(vector1));
	return vector1;
}

double AnimationAutoComplete::pointsSimilarity(PointWithStroke* point1, PointWithStroke* point2)
{
	double dissimilarityFactor = 0;
	dissimilarityFactor += 0.0 * pow(getAppearanceSimilarity(point1, point2),2);
	dissimilarityFactor += 0.4 * pow(getTemporalSimilarity(point1, point2),2);
	dissimilarityFactor += 0.6 * pow(getSpatialSimilarity(point1, point2),2);

	return sqrt(dissimilarityFactor);
}

double AnimationAutoComplete::pointsSimilarityWithoutWeights(PointWithStroke *point1, PointWithStroke *point2)
{
	double dissimilarityFactor = 0;
	// the smaller the factor the more similar they are
	dissimilarityFactor += pow(getAppearanceSimilarity(point1, point2),2);
	dissimilarityFactor += pow(getTemporalSimilarity(point1, point2),2);
	dissimilarityFactor += pow(getSpatialSimilarity(point1, point2),2);

	return sqrt(dissimilarityFactor);
}

double AnimationAutoComplete::getAppearanceSimilarity(PointWithStroke* point1, PointWithStroke* point2)
{
	double dissimilarityFactor = 0;
	if(point1->stroke->stroke->getStyle() != point2->stroke->stroke->getStyle())
		dissimilarityFactor++;

	dissimilarityFactor += fabs(point1->point->getThickP0().thick - point2->point->getThickP0().thick);

	return dissimilarityFactor;
}

double AnimationAutoComplete::getTemporalSimilarity(PointWithStroke *point1, PointWithStroke *point2)
{
	double sampleId1 = getSampleId(point1->index, point1->stroke->stroke->getChunkCount());
	double sampleId2 = getSampleId(point2->index, point2->stroke->stroke->getChunkCount());

	double reverseSampleId1 = getReversedSampleId(point1->index, point1->stroke->stroke->getChunkCount());
	double reverseSampleId2 = getReversedSampleId(point2->index, point2->stroke->stroke->getChunkCount());

	return fmax(fabs(sampleId1 - sampleId2), fabs(reverseSampleId1 - reverseSampleId2));
}

double AnimationAutoComplete::getSpatialSimilarity(PointWithStroke *point1, PointWithStroke *point2)
{
	TPointD direction1 = getTangentUnitVector(point1);
	TPointD direction2 = getTangentUnitVector(point2);
	return tdistance(direction1, direction2);
}

std::vector<StrokeWithNeighbours*> AnimationAutoComplete::search(StrokeWithNeighbours* lastStroke)
{
	bool lastStrokeIsSelfLooping = strokeSelfLooping(lastStroke->stroke);

	assert(lastStroke);

	double min = 10000000;
	StrokeWithScore score_stroke;
	std::vector<StrokeWithScore> scores;

	// TODO: clean up at production
	int i = 0;
	int neighborhoodSize = m_strokesWithNeighbours.size();
	i = neighborhoodSize-2;
	int hamada = (m_strokesWithNeighbours.size()-30);

	for(; i > hamada && i > -1; i--)
	{
		double score = getNeighborhoodSimilarity(lastStroke, m_strokesWithNeighbours[i]);
		score_stroke.score = score;
		score_stroke.stroke = m_strokesWithNeighbours[i];

		if(score < min)
		{
			min = score;
		}
		scores.push_back(score_stroke);
	}

	std::vector<StrokeWithNeighbours*> similarstrokes ;
	for(StrokeWithScore Score : scores)
	{
		double score = Score.score;

		if((score <2*min) && (lastStrokeIsSelfLooping == strokeSelfLooping(Score.stroke->stroke)))
			similarstrokes.push_back(Score.stroke);
	}

	return similarstrokes;
}

std::vector<StrokeWithNeighbours*> AnimationAutoComplete::assign(std::vector<StrokeWithNeighbours *> similarStrokes)
{
	if(similarStrokes.empty())
		return std::vector<StrokeWithNeighbours*>();

	double min = 9999999999;
	StrokeWithScore outputStroke;
	StrokeWithNeighbours* finalSimilarStroke;
	StrokeWithNeighbours* lastStroke = m_strokesWithNeighbours.back();

	for (StrokeWithNeighbours* similarStroke : similarStrokes) {
		StrokeWithNeighbours* nextToSimilarStroke = similarStroke->nextStroke;
#ifdef ADAM_SYSTHESIS
		StrokeWithNeighbours* nextStroke = generateSynthesizedStroke(lastStroke, similarStroke);
#endif
#ifdef ODAY_SYNTHESIS
		StrokeWithNeighbours* nextStroke = predictionPositionUpdate(similarStroke, lastStroke);
#endif

		std::vector<SimilarPairPoint> similarStrokeMatchingPairs = getSimilarPairPoints(similarStroke, nextToSimilarStroke);
		std::vector<SimilarPairPoint> lastDrawnMatchingPairs = getSimilarPairPoints(nextStroke, lastStroke);

		for (int i = 0; i < similarStrokeMatchingPairs.size() && i < lastDrawnMatchingPairs.size(); i++)
		{
			double guassian = gaussianConstant(similarStrokeMatchingPairs[i].point1 , similarStrokeMatchingPairs[i].point2);
			double similarityScore = pointsSimilarity(similarStrokeMatchingPairs[i].point1, similarStrokeMatchingPairs[i].point2);
			double similarityScoreWithoutWeights = pointsSimilarityWithoutWeights(lastDrawnMatchingPairs[i].point1, lastDrawnMatchingPairs[i].point2);
			double score = guassian * pow((similarityScore - similarityScoreWithoutWeights), 2);

			if (score > min)
				continue;

			min = score;
			outputStroke.score = min;
			outputStroke.stroke = nextStroke;
			finalSimilarStroke = similarStroke;
#ifdef DEBUGGING
#if defined(SHOW_PAIR_LINES) || defined(SHOW_MATCHING_STROKE)
	matchedStroke = finalSimilarStroke;
#endif
#endif
		}
	}

	//StrokeWithNeighbours* output = adaptToContext(outputStroke.stroke, finalSimilarStroke->nextStroke);
	StrokeWithNeighbours* output = outputStroke.stroke;

	std::vector<StrokeWithNeighbours*> outputStrokes;

	if (output)
		outputStrokes.push_back(output);
//	#ifdef ADAM_SYSTHESIS
//		outputStrokes = getPredictedNeighours(output, finalSimilarStroke->nextStroke);
//	#endif
//	#ifdef ODAY_SYNTHESIS
//		outputStrokes.push_back(outputStroke.stroke);
//	#endif

	return outputStrokes;
}

StrokeWithNeighbours* AnimationAutoComplete::adaptToContext(StrokeWithNeighbours* stroke, StrokeWithNeighbours* similarStroke)
{
	// currently only handling a single shared context
	StrokeWithNeighbours* context;

	if (!stroke->contextStrokes.size())
		return stroke;

	context = *(stroke->contextStrokes.begin());

	if (!hasContext(similarStroke, context))
		return stroke;

	//std::vector<TPointD> newStrokePoints;
	double averageDistance = 0;

	for (int i = 0; i < stroke->stroke->getChunkCount(); i++)
	{
		double distanceToTravel = getClosestDistanceToContextStroke(similarStroke->getPointWithStroke(i), context);
		averageDistance += distanceToTravel;

//		TPointD projectionOnContext = getProjectionOnContext(stroke->getPointWithStroke(i), context);

//		if (projectionOnContext == TPointD(-1, -1))
//			continue; // TODO: handle case where one point is parallel to the context

//		TPointD point = stroke->getTPointD(i);

//		TPointD newPoint = movePointTowardsAnotherByDistance(point, projectionOnContext, distanceToTravel);

//		stroke->getChunk(i)->setP0(newPoint);

//		stroke->getPointWithStroke(i).normalizeP1();

//		if (newStrokePoints.size())
//		{
//			TPointD lastPoint = newStrokePoints.back();
//			TPointD mid = lastPoint + newPoint;
//			mid.x /= 2;
//			mid.y /= 2;
//			newStrokePoints.push_back(mid);
//		}

		//newStrokePoints.push_back(newPoint);
	}

	averageDistance /= stroke->stroke->getChunkCount();
	TPointD projectionOnContext = getProjectionOnContext(stroke->getCentralPointWithStroke(), context);
	TPointD newPoint = movePointTowardsAnotherByDistance(stroke->getCentralSample(), projectionOnContext, averageDistance);

	TAffine aff = TTranslation(-newPoint);
	stroke->stroke->transform(aff);

//	TStroke* newTStroke = new TStroke(newStrokePoints);
//	newTStroke->setAverageThickness(stroke->stroke->getAverageThickness());
//	StrokeWithNeighbours* newStroke = new StrokeWithNeighbours();
//	newStroke->stroke = newTStroke;
//	getNeighbours(newStroke);
//	getContextStrokes(newStroke);

//	return newStroke;
	return stroke;
}

bool AnimationAutoComplete::hasContext(StrokeWithNeighbours* stroke, StrokeWithNeighbours* context)
{
	bool foundCommonContext = false;

	for (StrokeWithNeighbours* similarContext : stroke->contextStrokes)
		if (similarContext == context)
		{
			foundCommonContext = true;
			break;
		}

	return foundCommonContext;
}

void AnimationAutoComplete::getContextStrokes(StrokeWithNeighbours *stroke)
{
	for(StrokeWithNeighbours* neighbour : stroke->neighbours)
		if (neighbour->stroke->getLength() >= (2 * stroke->stroke->getLength()))
			stroke->contextStrokes.insert(neighbour);
}

double AnimationAutoComplete::differnceOfTwoNeighborhood(StrokeWithNeighbours* stroke1, StrokeWithNeighbours* stroke2,  std::vector<SimilarPairStroke> similarPairStrokes)
{
	double differenceInScores = 0;

	for(SimilarPairStroke similarPair : similarPairStrokes)
	{
		double stroke1Similarity = operationsSimilarity(stroke1, similarPair.stroke1);
		double stroke2Similarity = operationsSimilarity(stroke2, similarPair.stroke2);
		differenceInScores += abs(stroke1Similarity - stroke2Similarity);

#ifdef SHOW_PAIR_STROKES
		//TODO: Remove at Production
		TPointD beginning = similarPair.stroke1->getCentralSample()->point->getP0();
		TPointD end		= similarPair.stroke2->getCentralSample()->point->getP0();
		pairStrokes.push_back(generateLineStroke(beginning, end));
#endif //show maching strokes
	}

	return differenceInScores;
}

double AnimationAutoComplete::operationsSimilarity(StrokeWithNeighbours* stroke1, StrokeWithNeighbours* stroke2)
{
	double disSimilarityScore = 0;
	std::vector<SimilarPairPoint> similarPairs = getSimilarPairPoints(stroke1,stroke2);

	for(int i=0;i<similarPairs.size();i++)
		disSimilarityScore += pow(pointsSimilarity(similarPairs[i].point1,similarPairs[i].point2),2);

	return sqrt(disSimilarityScore);
}


double AnimationAutoComplete::getNeighborhoodSimilarity(StrokeWithNeighbours *stroke1, StrokeWithNeighbours *stroke2)
{
	// if output was not accurate we should consider implementing a different operationsSimilarity that
	// doesn't simply return a double repre.xsenting an average of points similarity.
	// we should instead return a vector.

	std::vector <SimilarPairPoint> similarPoints=getSimilarPairPoints(stroke1,stroke2);
	double centralSimilarities = getCentralSimilarities(similarPoints);

	std::vector<SimilarPairStroke> similarStrokes = getSimilarPairStrokes(stroke1,stroke2);
	double differenceInNeighborhoods = differnceOfTwoNeighborhood(stroke1,stroke2, similarStrokes);

	return sqrt (centralSimilarities + differenceInNeighborhoods);
}

inline double AnimationAutoComplete::getSampleId(const int &index, const int &n)
{
	//in case we use global time stamp, we will multiply by 0.9
	// to avoid having samples in different strokes with the same global TS
	return (index/n);
}

// to handle the case when a user draws a stroke in a direction and then the a similar stroke in the opposite direction
// for example drawing a line starting top to bottom and then another similar line but bottom up.
inline double AnimationAutoComplete::getReversedSampleId(const int &index, const int &n)
{
	return (n - index) / n;
}

double AnimationAutoComplete::getClosestDistanceToContextStroke(PointWithStroke point, StrokeWithNeighbours* context)
{
	TPointD projection = getProjectionOnContext(point, context);
	return tdistance(point.getTPointD(), projection);
}

PointWithStroke AnimationAutoComplete::getClosestPointInContextStroke(PointWithStroke point, StrokeWithNeighbours* context)
{
	PointWithStroke closestPoint;
	double minimumDistance = 99999999999;

	for (int i = 0; i < context->stroke->getChunkCount() - 1; i++)
	{
		double distance = tdistance(point.getTPointD(), context->getTPointD(i));
		if (distance < minimumDistance)
		{
			closestPoint = context->getPointWithStroke(i);
			minimumDistance = distance;
		}
	}

	return closestPoint;
}

TPointD AnimationAutoComplete::getProjectionOnContext(PointWithStroke point, StrokeWithNeighbours* context)
{
	PointWithStroke closestPointOnContext = getClosestPointInContextStroke(point, context);
	PointWithStroke next = closestPointOnContext.getNext();
	PointWithStroke previous = closestPointOnContext.getPrevious();

	TPointD projectionOnNext;
	TPointD projectionOnPrevious;
	if (next.stroke)
		projectionOnNext = getProjectionOnLine(point.getTPointD(), closestPointOnContext.getTPointD(), next.getTPointD());
	if (previous.stroke)
		projectionOnPrevious = getProjectionOnLine(point.getTPointD(), closestPointOnContext.getTPointD(), previous.getTPointD());

	double distance = tdistance(point.getTPointD(), projectionOnNext);
	double distance2 = tdistance(point.getTPointD(), projectionOnPrevious);

	return (distance > distance2) ? projectionOnNext : projectionOnPrevious;
}

TPointD AnimationAutoComplete::getProjectionOnLine(TPointD point, TPointD L1, TPointD L2)
{
	// the Line has two points A and B
	// the projection is the Point D
	// the outside point is C
	// https://stackoverflow.com/questions/10301001/perpendicular-on-a-line-segment-from-a-given-point
	//t=[(Cx-Ax)(Bx-Ax)+(Cy-Ay)(By-Ay)]/[(Bx-Ax)^2+(By-Ay)^2]
	double a = (point.x - L1.x) * (L2.x - L1.x);
	double b = (point.y - L1.y) * (L2.y - L1.y);
	double c = pow(L2.x - L1.x, 2);
	double d = pow(L2.y - L1.y, 2);

	if (c + d == 0)
		return TPointD(-1, -1);

	double t = (a + b) / (c + d);

	//Dx=Ax+t(Bx-Ax)
	//Dy=Ay+t(By-Ay)
	TPointD projection;
	projection.x = L1.x + t*(L2.x - L1.x);
	projection.y = L1.y + t*(L2.y - L1.y);

	return projection;
}

TPointD AnimationAutoComplete::movePointTowardsAnotherByDistance(TPointD start, TPointD end, double distanceToTravel)
{
	return distanceToTravel * normalize((end - start)) + start;
}

double AnimationAutoComplete::getCentralSimilarities(std::vector<SimilarPairPoint> similarPairPoints)
{
	PointWithStroke* central1 = new PointWithStroke();
	PointWithStroke* central2 = new PointWithStroke();
	central1->stroke = similarPairPoints[0].point1->stroke;
	central2->stroke = similarPairPoints[0].point2->stroke;
	int n = similarPairPoints.size();
	double dissimilarityfactor = 0;

	if(n%2 == 0)
	{
		central1->index = (n/2);
		central2->index = (n/2);
	}
	else
	{
		central1->index = ((n/2)+1);
		central2->index = ((n/2)+1);
	}

	if (n == 1)
	{
		central1->index = 0;
		central2->index = 0;
	}

	central1->point = similarPairPoints[central1->index].point1->point;
	central2->point = similarPairPoints[central2->index].point2->point;
	std::vector<double> similarity1;
	std::vector<double> similarity2;

	for(int i = 0; i < n; i++)
		if (i != central1->index)
			similarity1.push_back(pointsSimilarity(similarPairPoints[i].point1, central1));

	for(int i = 0; i < n; i++)
		if (i != central2->index)
			similarity2.push_back(pointsSimilarity(similarPairPoints[i].point2,central2));

	if (!similarity1.size() || !similarity2.size())
		return 999999;

	for(int i = 0; i < n; i++)
		dissimilarityfactor += pow(similarity1[i] - similarity2[i], 2);

	return sqrt(dissimilarityfactor)/n;
}

double AnimationAutoComplete::magnitude(std::vector<double> points)
{
	double sum = 0;

	for(int i = 0; i < points.size(); i++)
		sum+= points[i]*points[i];

	sum = sqrt(sum);
	return sum;
}


StrokeWithNeighbours *AnimationAutoComplete::generateSynthesizedStroke(StrokeWithNeighbours *lastStroke, StrokeWithNeighbours *similarStroke)
{
	StrokeWithNeighbours* outputStroke = new StrokeWithNeighbours();
	StrokeWithNeighbours* nextToSimilarStroke = similarStroke->nextStroke;

	TPointD centralSimilarStroke = similarStroke->getCentralSample();
	TPointD centralNextToSimilarStroke = nextToSimilarStroke->getCentralSample();
	TPointD centralLastStroke = lastStroke->getCentralSample();

	outputStroke->stroke = new TStroke(*nextToSimilarStroke->stroke);

	// last - output = similiar -next
	// output = last -(simiilar - next)
	TPointD centralOutputStroke = centralLastStroke - (centralSimilarStroke - centralNextToSimilarStroke) ;
	TPointD offset = centralOutputStroke - centralNextToSimilarStroke;

	TAffine aff = TTranslation(offset);

	outputStroke->stroke->transform(aff);

	getNeighbours(outputStroke);
	getContextStrokes(outputStroke);
	return outputStroke;
}

#if defined(SHOW_PAIR_LINES) || defined(SHOW_PAIR_STROKES)
TStroke *AnimationAutoComplete::generateLineStroke(TPointD beginning, TPointD end)
{
	if (beginning == end)
		return nullptr;
	TPointD middle = (end + beginning);
	middle.x = middle.x / 2;
	middle.y = middle.y / 2;
	std::vector<TPointD> v;
	v.push_back(beginning);
	v.push_back(middle);
	v.push_back(end);
	return new TStroke(v);
}

bool AnimationAutoComplete::pairsAreMatchingStrokeAndLastStroke(StrokeWithNeighbours *stroke1, StrokeWithNeighbours *stroke2)
{
	if (!matchedStroke)
		return false;

	if ((stroke1->getCentralSample() == matchedStroke->getCentralSample() && stroke2->getCentralSample() == m_strokesWithNeighbours.back()->getCentralSample()) ||
		(stroke1->getCentralSample() == m_strokesWithNeighbours.back()->getCentralSample() && stroke2->getCentralSample() == matchedStroke->getCentralSample())  )
		return true;

	return false;
}
#endif

TPointD AnimationAutoComplete::meanGlobal(std::vector<SamplePoint> globalSamples)
{
	double mean1 = 0;
	double mean2 = 0;
	for(SamplePoint sample : globalSamples)
	{
		mean1 += sample->getP0().x;
		mean2 += sample->getP0().y;

	}
	mean1 /= globalSamples.size();
	mean2 /= globalSamples.size();
	TPointD mean;
	mean.x = mean1;
	mean.y = mean2;
	return mean;
}

SimilarPairPoint AnimationAutoComplete::meanLocal(std::vector<SimilarPairPoint> localPairs)
{
	double mean1x = 0;
	double mean1y = 0;
	double mean2x = 0;
	double mean2y = 0;

	for(SimilarPairPoint similarPair : localPairs)
	{
		int index1 = similarPair.point1->index;
		int index2 = similarPair.point2->index;

		mean1x	  += similarPair.point1->stroke->stroke->getChunk(index1)->getP0().x;
		mean1y	  += similarPair.point1->stroke->stroke->getChunk(index1)->getP0().y;
		mean2x	   = similarPair.point2->stroke->stroke->getChunk(index2)->getP0().x;
		mean2y	   = similarPair.point2->stroke->stroke->getChunk(index2)->getP0().y;
	}

	SimilarPairPoint mean;
	int size = localPairs.size();
	mean.point1->point->setP0(TPointD(mean1x / size, mean1y / size));
	mean.point2->point->setP0(TPointD(mean2x / size, mean2y / size));

	return mean;
}

TPointD AnimationAutoComplete::deviationGlobal(std::vector<SamplePoint> globalSamples)
{
	TPointD mean = meanGlobal(globalSamples);
	double stdX = 0;
	double stdY = 0;
	TPointD std;
	int size = globalSamples.size()-1;

	for(SamplePoint samplePoint: globalSamples)
	{
		stdX += pow(samplePoint->getP0().x - mean.x,2);
		stdY += pow(samplePoint->getP0().y - mean.y,2);
	}

	std.x = stdX/size;
	std.y = stdY/size;
	return std;
}

SimilarPairPoint AnimationAutoComplete::deviationLocal(std::vector<SimilarPairPoint> localPairs, SimilarPairPoint mean)
{
	double std1X =  0;
	double std1Y =  0;
	double std2X =  0;
	double std2Y =  0;
	SimilarPairPoint std;

	int size = localPairs.size() - 1;

	for(SimilarPairPoint similarPair: localPairs)
	{
		int index1 = similarPair.point1->index;
		int index2 = similarPair.point2->index;

		std1X += pow(similarPair.point1->stroke->stroke->getChunk(index1)->getP0().x - mean.point1->point->getP0().x,2);
		std1Y += pow(similarPair.point1->stroke->stroke->getChunk(index1)->getP0().y - mean.point1->point->getP0().y,2);
		std2X += pow(similarPair.point2->stroke->stroke->getChunk(index2)->getP0().x - mean.point2->point->getP0().x,2);
		std2Y += pow(similarPair.point2->stroke->stroke->getChunk(index2)->getP0().y - mean.point2->point->getP0().y,2);
	}

	std.point1->point->setP0(TPointD(std1X / size, std1Y / size));
	std.point2->point->setP0(TPointD(std2X / size, std2Y / size));
	return std;
}

#ifdef DEBUGGING
#ifdef SHOW_SPACE_VICINITY
//TODO: remove at production
std::vector<TStroke*> AnimationAutoComplete::drawSpaceVicinity(TStroke *stroke)
{
	std::vector<TStroke*> strokes;

	for(int i = 0; i < stroke->getChunkCount(); i++)
		strokes.push_back(makeEllipticStroke(3, stroke->getChunk(i)->getP0(), m_spaceVicinityRadius, m_spaceVicinityRadius));
	return strokes;
}
#endif // draw space vicinity
#endif // debugging

std::vector<SimilarPairStroke> AnimationAutoComplete::getSimilarPairStrokes(StrokeWithNeighbours *stroke1, StrokeWithNeighbours *stroke2)
{
	HungarianAlgorithm obj_hungarianAlgorithm;
	std::vector<StrokeWithNeighbours*>ReorderStroke1;
	std::vector<StrokeWithNeighbours*>ReorderStroke2;
	std::vector<int>SolveFunctionVector;
	std::vector<SimilarPairStroke> SimilarPairStrokeVector;
	SimilarPairStroke similarPairStroke;

	// unordered set to vectors
	HungerianMatrix hungerianMatrix;

	for(StrokeWithNeighbours*stroke:stroke1->neighbours)
		ReorderStroke1.push_back(stroke);

	for(StrokeWithNeighbours*stroke:stroke2->neighbours)
		ReorderStroke2.push_back(stroke);

	// prepare Hungerian matrix
	for(int i = 0; i < ReorderStroke1.size(); i++)
	{
		std::vector<double> PushedVector;
		for(int j = 0; j < ReorderStroke2.size(); j++)
		{
			double Score = operationsSimilarity(ReorderStroke1[i], ReorderStroke2[j]);
			PushedVector.push_back(Score);
		}

		hungerianMatrix.push_back(PushedVector);
	}
	// function solve hungarian matrix and return vector of most similar pairs
	if (hungerianMatrix.size())
		obj_hungarianAlgorithm.Solve(hungerianMatrix, SolveFunctionVector);

	//prepare vector of similar pair stroke
	for(int i = 0; i < SolveFunctionVector.size(); i++)
	{
		// strokepair1 is the i of vector solveFunctionVector;
		// strokepair2 is the index of vector solveFunctionVector[i];
		if(SolveFunctionVector[i] == -1)
			continue;

		StrokeWithNeighbours* strokepair1 = ReorderStroke1[i];
		StrokeWithNeighbours* strokepair2 = ReorderStroke2[SolveFunctionVector[i]];
		similarPairStroke.stroke1 = strokepair1;
		similarPairStroke.stroke2 = strokepair2;
		similarPairStroke.dissimilarityFactor = hungerianMatrix[i][SolveFunctionVector[i]];
		SimilarPairStrokeVector.push_back(similarPairStroke);
	}

	return SimilarPairStrokeVector;
}
StrokeWithNeighbours* AnimationAutoComplete::getLocalSimilarPairStrokes(std::unordered_set<StrokeWithNeighbours*> contextStrokes, StrokeWithNeighbours *stroke)
{
	double min = 99999999999;
	StrokeWithNeighbours* minOperation = new StrokeWithNeighbours();
	for(StrokeWithNeighbours* contextStroke: contextStrokes)
	{
		double score = operationsSimilarity(contextStroke,stroke);
		if(score<min)
		{
			min =score;
			minOperation=contextStroke;
		}
	}
	return minOperation;

}

std::vector<std::vector<SimilarPairStroke*>> AnimationAutoComplete::localAnalysis(std::vector<StrokeWithNeighbours *> similarStrokes, StrokeWithNeighbours *lastStroke)
{
	std::vector<std::vector<SimilarPairStroke*>> allSimilarContextStrokes;

	for(StrokeWithNeighbours* contextStroke: lastStroke->contextStrokes)
	{
		std::vector<SimilarPairStroke*> similarContextStrokes;
		SimilarPairStroke* similarPairs0 = new SimilarPairStroke();
		similarPairs0->stroke1 = lastStroke;
		similarPairs0->stroke2 = contextStroke;
		similarContextStrokes.push_back(similarPairs0);

		for(int i = 0; i < similarStrokes.size(); i++)
		{
			SimilarPairStroke* similarPairs = new SimilarPairStroke();
			similarPairs->stroke1 = similarStrokes[i];
			StrokeWithNeighbours* stroke2 = getLocalSimilarPairStrokes(similarStrokes[i]->contextStrokes, contextStroke);
			similarPairs->stroke2 = stroke2;
			similarContextStrokes.push_back(similarPairs);
		}
		allSimilarContextStrokes.push_back(similarContextStrokes);
	}

	return allSimilarContextStrokes;
}

void AnimationAutoComplete::localPrediction(std::vector<std::vector<SimilarPairStroke> > allSimilarContextStrokes, std::vector<SimilarPairPoint> &means, std::vector<SimilarPairPoint> &deviations)
{
	for(std::vector<SimilarPairStroke> similarContextStrokes : allSimilarContextStrokes)
	{
		std::vector<SimilarPairPoint> allSimilarPairPoints;

		for(SimilarPairStroke similarPair : similarContextStrokes)
		{
			std::vector<SimilarPairPoint> similarSamplePairs = getSimilarPairPoints(similarPair.stroke1, similarPair.stroke2);
			allSimilarPairPoints = concatinateVectors(allSimilarPairPoints, similarSamplePairs);
		}

		SimilarPairPoint mean = meanLocal(allSimilarPairPoints);
		SimilarPairPoint deviation = deviationLocal(allSimilarPairPoints, mean);
		means.push_back(mean);
		deviations.push_back(deviation);
	}
}

#ifdef DEBUGGING
#ifdef SHOW_TANGENT_LINES
std::vector<TStroke*> AnimationAutoComplete::drawNormalStrokes(TStroke *stroke)
{
	std::vector<TStroke*> output;
	if(stroke->getChunkCount() < 1)
		return output; //empty vector

	for (int i = 0; i < stroke->getChunkCount(); i++)
	{
		PointWithStroke* point = new PointWithStroke(stroke->getChunk(i), stroke, i);
		TPointD normal = getTangentUnitVector(point);

		StrokeGenerator strokeGenerator;
		strokeGenerator.add(point->point->getThickP0(), 1);

		TThickPoint point2 = point->point->getThickP0() + normal*30;
		strokeGenerator.add(point2, 1);

		output.push_back(strokeGenerator.makeStroke(0));
	}

	return output;
}
#endif //show normals
#endif //debugging

StrokeWithNeighbours* AnimationAutoComplete::predictionPositionUpdate(StrokeWithNeighbours* currentStroke, StrokeWithNeighbours* nextStroke)
{
	int count=0;
	if(currentStroke->stroke->getChunkCount() > nextStroke->stroke->getChunkCount())
		count = nextStroke->stroke->getChunkCount();
	else
		count = currentStroke->stroke->getChunkCount();
	SamplePoint sampleCurrentStroke;//sample point
	SamplePoint sampleNextStroke;
	std::vector<TPointD>predectedStrock;
	for(int i = 0; i < count; i++)
	{
		sampleCurrentStroke = currentStroke->stroke->getChunk(i);
		sampleNextStroke = nextStroke->stroke->getChunk(i);
		//std::vector<TPointD> matrixA;
		TPointD subtractionMatrix = sampleNextStroke->getP0() - sampleCurrentStroke->getP0();
		TPointD Segma;
		Segma=sampleNextStroke->getP0();
		Segma.x=50;
		Segma.y=50;
		//SamplePoint result=subtractionMatrix+sampleCurrentStroke+Segma;
		TPointD result = subtractionMatrix +(sampleCurrentStroke->getP0());
		result = result + Segma;

		if(predectedStrock.size()>0)
		{
			TPointD midPoint;
			TPointD tmp;
			tmp = predectedStrock.back();
			midPoint.x = ((result.x + tmp.x)/2);
			midPoint.y = ((result.y + tmp.y)/2);
			predectedStrock.push_back(midPoint);
		}

		predectedStrock.push_back(result);
	}

	StrokeWithNeighbours* output = new StrokeWithNeighbours();
	output->stroke = new TStroke(predectedStrock);
	//getNeighbours(output);
	return output;
}

std::vector<StrokeWithNeighbours*> AnimationAutoComplete::getPredictedNeighours(StrokeWithNeighbours *predictedStroke, StrokeWithNeighbours *similarStroke)
{
#ifdef DEBUGGING
#if defined(SHOW_PAIR_LINES) || defined(SHOW_MATCHING_STROKE)
	matchedStroke = similarStroke;
#endif
#endif

	TPointD predictedCentral = predictedStroke->getCentralSample();
	TPointD similarCentral = similarStroke->getCentralSample();
	TPointD offset = predictedCentral - similarCentral;
	TAffine aff = TTranslation(offset);

	TStroke* dummyStroke = new TStroke(*predictedStroke->stroke);
	StrokeWithNeighbours* clonePredictedStroke = new StrokeWithNeighbours();
	clonePredictedStroke->stroke = dummyStroke;
	clonePredictedStroke->neighbours = predictedStroke->neighbours;

	//the clonePredictedStroke gets his neighbours filled in this function
	std::vector<SimilarPairStroke> neighbourhoodPairs = getSimilarPairStrokes(clonePredictedStroke, similarStroke);
	std::vector<StrokeWithNeighbours*> newNeighbours;

	for (SimilarPairStroke pair : neighbourhoodPairs)
	{
		SetOfStrokes::const_iterator it = clonePredictedStroke->neighbours.find(pair.stroke2);
		if (it == clonePredictedStroke->neighbours.end())
			continue;
		StrokeWithNeighbours* newNeighbour = new StrokeWithNeighbours();
		newNeighbour->stroke = new TStroke(*pair.stroke2->stroke);
		newNeighbour->stroke->transform(aff);
		predictedStroke->neighbours.insert(newNeighbour);
		newNeighbours.push_back(newNeighbour);
	}

	return newNeighbours;
}

TPointD AnimationAutoComplete::getTangentUnitVector(PointWithStroke* pointWithStroke)
{
	assert(pointWithStroke->index < pointWithStroke->stroke->stroke->getChunkCount());
	double y2 = 0;
	double y1 = 0;
	double x2 = 0;
	double x1 = 0;
	if(pointWithStroke->index!=pointWithStroke->stroke->stroke->getChunkCount()-1)
	{
		y2 = pointWithStroke->point->getP1().y;
		y1 = pointWithStroke->point->getP0().y;
		x2 = pointWithStroke->point->getP1().x;
		x1 = pointWithStroke->point->getP0().x;
	}
	else
	{
		y2 = pointWithStroke->point->getP1().y;
		y1 = pointWithStroke->point->getP2().y;
		x2 = pointWithStroke->point->getP1().x;
		x1 = pointWithStroke->point->getP2().x;
	}

	TPointD point1 = TPointD(x1, y1);
	TPointD point2 = TPointD(x2, y2);

	if (point2 - point1 == TPointD(0, 0))
		return TPointD(0, 0);

	return normalize(point2 - point1);
}

std::vector<SimilarPairPoint> AnimationAutoComplete::getSimilarPairPoints(StrokeWithNeighbours *stroke1, StrokeWithNeighbours *stroke2)
{
	HungerianMatrix input;
	std::vector<int>assignment;
	int sizeSTroke1 = stroke1->stroke->getChunkCount();
	int sizeSTroke2 = stroke2->stroke->getChunkCount();
	std::vector<SimilarPairPoint> similarPoints;

	for(int i=0;i<sizeSTroke1;i++)
	{
		std::vector<double> tmp;

		for(int j=0;j<sizeSTroke2;j++)
		{
			PointWithStroke* point1 = new PointWithStroke();
			point1->index = i;
			point1->point = stroke1->stroke->getChunk(i);
			point1->stroke = stroke1;
			PointWithStroke* point2 = new PointWithStroke();
			point2->index = j;
			point2->point = stroke2->stroke->getChunk(j);
			point2->stroke = stroke2;
			double pointsimilar = pointsSimilarity(point1, point2);
			tmp.push_back(pointsimilar);
		}

		input.push_back(tmp);
	}

	HungarianAlgorithm hungarian;

	hungarian.Solve(input,assignment);

	for(int i = 0; i < assignment.size(); i++)
	{
		if(assignment[i] != -1)
		{
			SimilarPairPoint p;
			PointWithStroke* point1 = new PointWithStroke();
			point1->index = i;
			point1->stroke = stroke1;
			point1->point = stroke1->stroke->getChunk(point1->index);

			PointWithStroke* point2 = new PointWithStroke();
			point2->index = assignment[i];
			point2->stroke = stroke2;
			point2->point = stroke2->stroke->getChunk(point2->index);

			p.point1 = point1;
			p.point2 = point2;

			similarPoints.push_back(p);
#ifdef DEBUGGING
#ifdef SHOW_PAIR_LINES
			//TODO: Remove at Production
			//only draw matching pairs between the last stroke and matching stroke
			if (pairsAreMatchingStrokeAndLastStroke(stroke1, stroke2))
			{
				TPointD beginning = p.point1->point->getP0();
				TPointD end = p.point2->point->getP0();
				m_similarPairLines.push_back(generateLineStroke(beginning, end));
			}
#endif //show maching strokes
#endif //debugging
		}
	}

	return similarPoints;
}

TPointD StrokeWithNeighbours::getCentralSample()
{
	return getCentralPointWithStroke().getTPointD();
}

PointWithStroke StrokeWithNeighbours::getCentralPointWithStroke()
{
	PointWithStroke central1 ;
	central1.stroke = this;
	int n = stroke->getChunkCount();

	if(n%2 == 0)
		central1.index = n/2;
	else
		central1.index = (n/2) + 1;

	if (n == 1)
		central1.index = 0;

	central1.point = stroke->getChunk(central1.index);
	return central1;
}

double StrokeWithNeighbours::getLength()
{
	return stroke->getLength();
}

TPointD StrokeWithNeighbours::getTPointD(int i)
{
	if (isLastPoint(i))
		return stroke->getChunk(i)->getP2();

	return stroke->getChunk(i)->getP0();
}

SamplePoint StrokeWithNeighbours::getChunk(int i)
{
	return stroke->getChunk(i);
}

PointWithStroke StrokeWithNeighbours::getPointWithStroke(int i)
{
	PointWithStroke point(stroke->getChunk(i), this, i);
	point.setLastPoint(isLastPoint(i));
	return point;
}

bool StrokeWithNeighbours::isLastPoint(int i)
{
	if (i == stroke->getChunkCount() - 1)
		return true;

	return false;
}

void StrokeWithNeighbours::setPoint(int i, TPointD newPoint)
{
	if (isLastPoint(i))
		stroke->getChunk(i)->setP2(newPoint);
	else
		stroke->getChunk(i)->setP0(newPoint);
}

void PointWithStroke::setLastPoint(bool value)
{
	lastPoint = value;
}

PointWithStroke PointWithStroke::getNext()
{
	if (stroke->isLastPoint(index))
		return PointWithStroke();

	return PointWithStroke(stroke->getChunk(index+1), stroke, index+1);
}

PointWithStroke PointWithStroke::getPrevious()
{
	if (index == 0)
		return PointWithStroke();

	return PointWithStroke(stroke->getChunk(index-1), stroke, index-1);
}

void PointWithStroke::normalizeP1()
{
	if (stroke->isLastPoint(index))
		return;

	TPointD P1 = getTPointD() + getNext().getTPointD();
	P1.x /= 2.0;
	P1.y /= 2.0;

	point->setP1(P1);
}

PointWithStroke::PointWithStroke()
{
	index = -1;
	stroke = nullptr;
	point = nullptr;
}

PointWithStroke::PointWithStroke(SamplePoint point, StrokeWithNeighbours* stroke, int index) : point(point), stroke(stroke), index(index)
{
	if (stroke->isLastPoint(index))
		lastPoint = true;
	else
		lastPoint = false;
}

TPointD PointWithStroke::getTPointD()
{
	if (lastPoint)
		return point->getP2();

	return point->getP0();
}
