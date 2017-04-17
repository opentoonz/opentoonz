#include "toonz/animationautocomplete.h"

#include "tcurves.h"
#include "drawutil.h"

void AnimationAutoComplete::addStroke(TStroke* stroke)
{
	StrokeWithNeighbours* strokeWithNeighbours = new StrokeWithNeighbours();

	if(m_strokesWithNeighbours.size()!=0)
		m_strokesWithNeighbours.back()->nextStroke = strokeWithNeighbours;

	m_strokesWithNeighbours.push_back(strokeWithNeighbours);

	strokeWithNeighbours->stroke = stroke;

	int chuckCount = stroke->getChunkCount();
	for (int i = 0; i < chuckCount; i++)
	{
		SetOfPoints neighbours = getNeighbours(stroke->getChunk(i));
		strokeWithNeighbours->neighbours.insert(neighbours.begin(), neighbours.end());
	}

	if (m_strokesWithNeighbours.size() >= 2)
		initializeSynthesis();
}

//--------------------------------------------------------------------------------------------------
double AnimationAutoComplete::gaussianConstant(SamplePoint chuck1, SamplePoint chuck2)
{
	TPointD sample1= chuck1->getP0();
	TPointD sample2= chuck2->getP0();
	TThickPoint s1; TThickPoint s2;
	s1.x=sample1.x;  s1.y=sample1.y;
	s2.x=sample2.x;  s2.y=sample2.y;
	double distance=tdistance2(s1,s2);
	distance=sqrt(distance);
	#define OMEGA 10
	return exp(-distance/OMEGA);
}

SetOfPoints AnimationAutoComplete::getNeighbours(const SamplePoint point)
{
	SetOfPoints neighbours;

	for(int i = 0; i < m_strokesWithNeighbours.size(); i++)
	{
		TStroke* stroke = m_strokesWithNeighbours[i]->stroke;

		for(int j = 0; j < stroke->getChunkCount(); j++)
			if(withinSpaceVicinity(point, stroke->getChunk(j)))
			{
				PointWithStroke *samplePoint = new PointWithStroke();
				samplePoint->stroke = stroke;
				samplePoint->point = stroke->getChunk(j);

				neighbours.insert(samplePoint);
			}
	}

	return neighbours;
}

bool AnimationAutoComplete::withinSpaceVicinity(const SamplePoint samplePoint, const SamplePoint point)
{
	double distance = norm(samplePoint->getP2() - point->getP2());
	if(distance <= m_spaceVicinityRadius)
		return true;
	else
		return false;
}

void AnimationAutoComplete::initializeSynthesis()
{
	StrokeWithNeighbours* lastStroke = m_strokesWithNeighbours.back();
	std::vector<StrokeWithNeighbours*> similarStrokes = search(lastStroke);

	StrokeWithNeighbours* outputsStroke = assign(similarStrokes);

	if(outputsStroke)
		m_synthesizedStrokes.push_back(outputsStroke);
}

std::vector<StrokeWithNeighbours*> AnimationAutoComplete::getSynthesizedStrokes()
{
	return m_synthesizedStrokes;
}

double AnimationAutoComplete::pointsSimilarity(PointWithStroke* point1, PointWithStroke* point2)
{
	double dissimilarityFactor = 0;
	// the smaller the factor the more similar they are
	dissimilarityFactor += getAppearanceSimilarity(point1, point2);
	dissimilarityFactor += getTemporalSimilarity(point1, point2);
	dissimilarityFactor += getSpatialSimilarity(point1, point2);

	return dissimilarityFactor;
}

double AnimationAutoComplete::pointsSimilarityWithoutWeights(PointWithStroke *point1, PointWithStroke *point2)
{

}

double AnimationAutoComplete::getAppearanceSimilarity(PointWithStroke* point1, PointWithStroke* point2)
{
	double dissimilarityFactor = 0;
	if(point1->stroke->getStyle() != point2->stroke->getStyle())
		dissimilarityFactor++;

	dissimilarityFactor += fabs(point1->point->getThickP0().thick - point2->point->getThickP0().thick);

	return dissimilarityFactor;
}

double AnimationAutoComplete::getTemporalSimilarity(PointWithStroke *point1, PointWithStroke *point2)
{


}

double AnimationAutoComplete::getSpatialSimilarity(PointWithStroke *point1, PointWithStroke *point2)
{

}

double AnimationAutoComplete::getNeighborhoodSimilarity(StrokeWithNeighbours* operation1,StrokeWithNeighbours* operation2)
{


}

std::vector<StrokeWithNeighbours*> AnimationAutoComplete::search(StrokeWithNeighbours* operation1)
{

	if(!operation1)
		return std::vector<StrokeWithNeighbours*>();

	double min = 10000000;
	StrokeWithScore score_stroke;

	for(int i = m_strokesWithNeighbours.size()-1; i > m_strokesWithNeighbours.size()-30; i--)
	{
		if (!m_strokesWithNeighbours[i]->nextStroke)
			continue;

		double score = getNeighborhoodSimilarity(operation1, m_strokesWithNeighbours[i]->nextStroke);
		if(score < min)
		{
			min = score;
			score_stroke.score = min;
			score_stroke.stroke = m_strokesWithNeighbours[i];
		}
	}

	std::vector<StrokeWithNeighbours*> similarstroke;
	for(int i = m_strokesWithNeighbours.size()-1; i > m_strokesWithNeighbours.size() - 30; i--)
	{
		if (!m_strokesWithNeighbours[i]->nextStroke)
			continue;

		double score = operationsSimilarity(operation1, m_strokesWithNeighbours[i]->nextStroke);

		if(score < 2 * min)
			similarstroke.push_back(m_strokesWithNeighbours[i]->nextStroke);
	}

	return similarstroke;
}

StrokeWithNeighbours *AnimationAutoComplete::assign(std::vector<StrokeWithNeighbours *> similarStrokes)
{
	if(similarStrokes.empty())
		return nullptr;

	double min = 9999999999;
	StrokeWithScore outputStroke;
	StrokeWithNeighbours* lastStroke = m_strokesWithNeighbours.back();

	for (StrokeWithNeighbours* similarStroke : similarStrokes) {
		StrokeWithNeighbours* nextToSimilarStroke = similarStroke->nextStroke;
		StrokeWithNeighbours* nextStroke = generateSynthesizedStroke(lastStroke, similarStroke, nextToSimilarStroke);

		std::vector<SimilarPair> similarStrokeMatchingPairs = getSimilarPairs(similarStroke, nextToSimilarStroke);
		std::vector<SimilarPair> lastDrawnMatchingPairs = getSimilarPairs(nextStroke, lastStroke);

		for (int i = 0; i < similarStrokeMatchingPairs.size() || i < lastDrawnMatchingPairs.size(); i++)
		{
			double guassian = gaussianConstant(similarStrokeMatchingPairs[i].point1->point , similarStrokeMatchingPairs[i].point2->point);
			double similarityScore = pointsSimilarity(similarStrokeMatchingPairs[i].point1, similarStrokeMatchingPairs[i].point2);
			double similarityScoreWithoutWeights = pointsSimilarityWithoutWeights(lastDrawnMatchingPairs[i].point1, lastDrawnMatchingPairs[i].point2);
			double score = guassian * pow((similarityScore - similarityScoreWithoutWeights), 2);

			if (score > min)
				continue;

			min = score;
			outputStroke.score = min;
			outputStroke.stroke = similarStroke;
		}
	}

	return outputStroke.stroke;
}

double AnimationAutoComplete::operationsSimilarity(StrokeWithNeighbours* stroke1,
												   StrokeWithNeighbours* stroke2)
{
	double dissimilarityScore = 0;
	std::vector<SimilarPair> similarPairs;

	for(int i = 0; i < stroke1->stroke->getChunkCount(); i++)
	{
		PointWithStroke* point = new PointWithStroke(stroke1->stroke->getChunk(i), stroke1->stroke);
		SimilarPair pair = getMostSimilarPoint(point, stroke2->stroke);
		similarPairs.push_back(pair);
		dissimilarityScore += pair.dissimilarityFactor;
	}

	return dissimilarityScore;
}

double AnimationAutoComplete::magnitude(std::vector<double> points)
{
	double sum=0;
	for(int i=0;i<points.size();i++)
	{
		sum+= points[i]*points[i];
	}
	sum = sqrt(sum);
	return sum;
}

std::vector<SimilarPair> AnimationAutoComplete::getSimilarPairs(StrokeWithNeighbours *, StrokeWithNeighbours *)
{

}


StrokeWithNeighbours *AnimationAutoComplete::generateSynthesizedStroke(StrokeWithNeighbours *lastStroke, StrokeWithNeighbours *similarStroke, StrokeWithNeighbours *nextToSimilarStroke)
{
	StrokeWithNeighbours* outputStroke = new StrokeWithNeighbours();
	std::vector<TThickPoint> points;

	int loopCount = (int)fmin((double)similarStroke->stroke->getChunkCount(), (double)nextToSimilarStroke->stroke->getChunkCount());

	for(int i = 0; i < loopCount; i++)
	{
		TPointD p1 = similarStroke->stroke->getChunk(i)->getP0();
		TPointD p2 = nextToSimilarStroke->stroke->getChunk(i)->getP0();
		double x1 = p1.x , x2 = p2.x;
		double y1 = p1.y , y2 = p2.y;
		double x_diffrence = x2-x1;
		double y_diffrence = y2-y1;
		double x_output = lastStroke->stroke->getChunk(i)->getP0().x + x_diffrence;
		double y_output = lastStroke->stroke->getChunk(i)->getP0().y + y_diffrence;

		TThickPoint p = TThickPoint(x_output,y_output,similarStroke->stroke->getChunk(i)->getThickP0().thick);

		if(points.size() > 0)
		{
			TThickPoint old = points.back();
			if (norm2(p - old) < 4) continue;
			TThickPoint mid((old + p) * 0.5, (p.thick + old.thick) * 0.5);
			points.push_back(mid);
		}
		points.push_back(p);
	}

	outputStroke->stroke = new TStroke(points);
	return outputStroke;
}

std::vector<StrokeWithScore> AnimationAutoComplete::getSimilarStrokes(StrokeWithNeighbours* stroke)
{
	std::vector<StrokeWithScore> similarStrokes;

	for(int i = 0; i < m_strokesWithNeighbours.size(); i++)
	{
		StrokeWithScore strokeWithScore;
		strokeWithScore.stroke = m_strokesWithNeighbours[i];
		strokeWithScore.score = operationsSimilarity(stroke, m_strokesWithNeighbours[i]);
		similarStrokes.push_back(strokeWithScore);
	}

	return similarStrokes;
}

SimilarPair AnimationAutoComplete::getMostSimilarPoint(PointWithStroke *point, TStroke *stroke)
{
	SimilarPair mostSimilarPair;

	mostSimilarPair.point1 = point;
	PointWithStroke* point2 = new PointWithStroke(stroke->getChunk(0), stroke);
	mostSimilarPair.dissimilarityFactor = pointsSimilarity(point, point2);
	mostSimilarPair.point2 = point2;

	for (int i = 1; i < stroke->getChunkCount(); i++)
	{
		point2 = new PointWithStroke(stroke->getChunk(i), stroke);
		double similarityScore = pointsSimilarity(point, point2);
		if (mostSimilarPair.dissimilarityFactor > similarityScore)
		{
			delete mostSimilarPair.point2;
			mostSimilarPair.point2 = point2;
			mostSimilarPair.dissimilarityFactor = similarityScore;
		}
		else delete point2;
	}

	return mostSimilarPair;
}

//TODO: remove at production
std::vector<TStroke*> AnimationAutoComplete::drawSpaceVicinity(TStroke *stroke)
{
	std::vector<TStroke*> strokes;

	for(int i = 0; i < stroke->getChunkCount(); i++)
		strokes.push_back(makeEllipticStroke(3, stroke->getChunk(i)->getP0(), m_spaceVicinityRadius, m_spaceVicinityRadius));
	return strokes;
}

TStroke* AnimationAutoComplete::drawstrokeLine(TStroke *stroke)
{
	//todo: strokeline
    std::vector<TPointD> vec;
    if( 1>=stroke->getChunkCount())
      {  return nullptr;}
    else {
    vec.push_back(stroke->getChunk(1)->getP0());

    double y2 = stroke->getChunk(1)->getP1().y;
    double y1 = stroke->getChunk(1)->getP0().y;
    double x2 = stroke->getChunk(1)->getP1().x;
    double x1 = stroke->getChunk(1)->getP0().x;

    double slope_tangent= (y2 - y1) / (x2 - x1);
   if (slope_tangent==0)
   {
       double new_x=x1+100;
       vec.push_back(TPointD(new_x,y1));
       TStroke* strokeLine = new TStroke(vec);
       return strokeLine;
   }

   else {
    double c_tangent = y2 - (slope_tangent * x2);

    double slope_prependicular=1/slope_tangent;
    double c_prependicular=y2+(slope_prependicular*x2);

    double new_x = x2 + 100;
    double new_y =(-slope_prependicular*new_x )+c_prependicular;

    vec.push_back(TPointD(new_x,new_y));
	TStroke* strokeLine = new TStroke(vec);
    return strokeLine;}
    }
}



//TODO : getNormal
TPointD AnimationAutoComplete::getNormal(PointWithStroke* pointer)
{
    if( pointer->index>=pointer->stroke->getChunkCount())
        return ;
    else {
    //vec.push_back(stroke->getChunk(pointer->index)->getP0());

    double y2 = pointer->stroke->getChunk(pointer->index)->getP1().y;
    double y1 = pointer->stroke->getChunk(pointer->index)->getP0().y;
    double x2 = pointer->stroke->getChunk(pointer->index)->getP1().x;
    double x1 = pointer->stroke->getChunk(pointer->index)->getP0().x;

    double slope_tangent= (y2 - y1) / (x2 - x1);
    if(slope_tangent==0)
    {
        return TPointD(0,1);

    }
    else{
        double c_tangent = y2 - (slope_tangent * x2);

        double slope_prependicular=1/slope_tangent;
        double c_prependicular=y2+(slope_prependicular*x2);

    double new_x = x2 + 10;
    double new_y =(-slope_prependicular*new_x )+c_prependicular;
    // Todo:Magnitude sqrt() lel x pow 2 + y pow 2 ;
    double x_def=new_x-x2;
    double y_def=new_y-y2;
    double x_pow2=x_def*x_def;
    double y_pow2= y_def*y_def;
    double point_magnitude=sqrt(x_pow2+y_pow2);
    double x_unit=x_def/point_magnitude;
    double y_unit =y_def/point_magnitude;
    return TPointD(x_unit,y_unit);}}

}

void GlobalSimilarityGraph::insertNode(SimilarPair *pair, std::vector<SimilarPair *> connections)
{
    this->connections.insert(std::pair<SimilarPair*, std::vector<SimilarPair*>>(pair, connections));
	numberOfNodes++;
}

std::vector<SimilarPair *> GlobalSimilarityGraph::getConnections(SimilarPair *pair)
{
	if (pair)
		return this->connections[pair];
	return std::vector<SimilarPair *>();
}
