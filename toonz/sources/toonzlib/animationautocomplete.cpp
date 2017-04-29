#include "toonz/animationautocomplete.h"

#include "tcurves.h"
#include "drawutil.h"

#include "toonz/hungarian.h"

void AnimationAutoComplete::addStroke(TStroke* stroke)
{
	// clears previous predictions
	m_synthesizedStrokes.clear();
	StrokeWithNeighbours* strokeWithNeighbours = new StrokeWithNeighbours();

	if(m_strokesWithNeighbours.size()!=0)
		m_strokesWithNeighbours.back()->nextStroke = strokeWithNeighbours;

	m_strokesWithNeighbours.push_back(strokeWithNeighbours);

	strokeWithNeighbours->stroke = stroke;

	int chuckCount = stroke->getChunkCount();
	for (int i = 0; i < chuckCount; i++)
	{
        PointWithStroke point(stroke->getChunk(i), stroke, i);
        std::vector<StrokeWithNeighbours*> neighbours = getNeighbours(point);
		strokeWithNeighbours->neighbours.insert(neighbours.begin(), neighbours.end());
	}
    bool zblo7a=false;
    zblo7a=inRange(stroke);


	if (m_strokesWithNeighbours.size() >= 2)
		initializeSynthesis();
}

//----------
bool AnimationAutoComplete::inRange(TStroke* stroke)
{
    int count = stroke->getChunkCount();
    double range =3;
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

std::vector<StrokeWithNeighbours*> AnimationAutoComplete::getNeighbours(PointWithStroke point)
{
	std::vector<StrokeWithNeighbours*> neighbours;

	for(int i = 0; i < m_strokesWithNeighbours.size(); i++)
	{
		TStroke* stroke = m_strokesWithNeighbours[i]->stroke;

		for(int j = 0; j < stroke->getChunkCount(); j++)
            if(withinSpaceVicinity(point.point, stroke->getChunk(j)) && stroke!=point.stroke)
			{
				neighbours.push_back(m_strokesWithNeighbours[i]);
				break;
			}
	}
	int size = m_strokesWithNeighbours.size();
    if(size>2)
	{

	neighbours.push_back(m_strokesWithNeighbours[size-2]);
	neighbours.push_back(m_strokesWithNeighbours[size-3]);
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
	dissimilarityFactor += pow(getAppearanceSimilarity(point1, point2),2);
	dissimilarityFactor += pow(getTemporalSimilarity(point1, point2),2);
	dissimilarityFactor += pow(getSpatialSimilarity(point1, point2),2);

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
	if(point1->stroke->getStyle() != point2->stroke->getStyle())
		dissimilarityFactor++;

    dissimilarityFactor += fabs(point1->point->getThickP0().thick - point2->point->getThickP0().thick);

	return dissimilarityFactor;
}

double AnimationAutoComplete::getTemporalSimilarity(PointWithStroke *point1, PointWithStroke *point2)
{
    double sampleId1=getSampleId(point1->index, point1->stroke->getChunkCount());
    double sampleId2=getSampleId(point2->index, point2->stroke->getChunkCount());
    return (fabs(sampleId1-sampleId2));
}

double AnimationAutoComplete::getSpatialSimilarity(PointWithStroke *point1, PointWithStroke *point2)
{
    TPointD direction1 = getNormal(point1);
    TPointD direction2 = getNormal(point2);
    return sqrt(pow(fabs(direction1.x-direction2.x),2)+pow(fabs(direction1.y-direction2.y),2));

}

std::vector<StrokeWithNeighbours*> AnimationAutoComplete::search(StrokeWithNeighbours* lastStroke)
{

    if(!lastStroke)
		return std::vector<StrokeWithNeighbours*>();

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
		score_stroke.score = min;
        score_stroke.stroke = m_strokesWithNeighbours[i];

		if(score < min)
			min = score;

        scores.push_back(score_stroke);
	}

    std::vector<StrokeWithNeighbours*> similarstrokes ;
    for(StrokeWithScore Score : scores)
	{
        double score = Score.score;

		if(score < 2 * min)
            similarstrokes.push_back(Score.stroke);
	}

    return similarstrokes;
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

		std::vector<SimilarPairPoint> similarStrokeMatchingPairs = getSimilarPairPoints(similarStroke, nextToSimilarStroke);
		std::vector<SimilarPairPoint> lastDrawnMatchingPairs = getSimilarPairPoints(nextStroke, lastStroke);

        for (int i = 0; i < similarStrokeMatchingPairs.size() && i < lastDrawnMatchingPairs.size(); i++)
		{
			double guassian = gaussianConstant(similarStrokeMatchingPairs[i].point1->point , similarStrokeMatchingPairs[i].point2->point);
			double similarityScore = pointsSimilarity(similarStrokeMatchingPairs[i].point1, similarStrokeMatchingPairs[i].point2);
			double similarityScoreWithoutWeights = pointsSimilarityWithoutWeights(lastDrawnMatchingPairs[i].point1, lastDrawnMatchingPairs[i].point2);
			double score = guassian * pow((similarityScore - similarityScoreWithoutWeights), 2);

			if (score > min)
				continue;

			min = score;
			outputStroke.score = min;
            outputStroke.stroke = nextStroke;
		}
	}

    return outputStroke.stroke;
}

double AnimationAutoComplete::getSimilarPairPointBySampleId(TStroke *stroke1, TStroke *stroke2)
{
    int minimumIndex =0;
    int maximumIndex =0;
    if(stroke1->getChunkCount()<stroke2->getChunkCount())
    {
        minimumIndex = stroke1->getChunkCount();
        maximumIndex=stroke2->getChunkCount();
    }
    else
    {
        minimumIndex = stroke2->getChunkCount();
        maximumIndex = stroke1->getChunkCount();
    }

    std::vector<SimilarPairPoint> similarPairs;

    for(int i=0;i<minimumIndex;i++)
    {
        double nearestSampleId = 99999999;
        int minimum =0;
        for(int j=0;j<maximumIndex;j++)
        {
            double sampleId = fabs(getSampleId(i,minimumIndex)-getSampleId(j,maximumIndex));
            if(sampleId<nearestSampleId)
            {
                nearestSampleId=sampleId;
                minimum = j;
            }
        }
        SimilarPairPoint similarPair;
        PointWithStroke* point1 = new PointWithStroke();
        point1->index=i;
        point1->stroke=stroke1;
        similarPair.point1=point1;
        PointWithStroke* point2= new PointWithStroke();
        point2->index=minimum;
        point2->stroke=stroke2;
        similarPair.point2=point2;
        similarPairs.push_back((similarPair));

    }
    double disimilarity =0;
    for(int i=0;i<similarPairs.size();i++)
    {
        disimilarity += pointsSimilarity(similarPairs[i].point1, similarPairs[i].point2);
    }

return disimilarity/similarPairs.size();

}

double AnimationAutoComplete::differnceOfTwoNeighborhood(StrokeWithNeighbours* stroke1, StrokeWithNeighbours* stroke2,  std::vector<SimilarPairStroke> similarPairStrokes)
{
    double differenceInScores = 9999999999;

    for(SimilarPairStroke similarPair : similarPairStrokes)
    {
        double stroke1Similarity = operationsSimilarity(stroke1, similarPair.stroke1);
        double stroke2Similarity = operationsSimilarity(stroke2, similarPair.stroke2);
        differenceInScores += pow(stroke1Similarity - stroke2Similarity, 2);
    }

    return differenceInScores;
}


double AnimationAutoComplete::operationsSimilarity(StrokeWithNeighbours* stroke1, StrokeWithNeighbours* stroke2)
{

	double disSimilarityScore = 0;

	std::vector<SimilarPairPoint> similarPairs = getSimilarPairPoints(stroke1,stroke2);


  for(int i=0;i<similarPairs.size();i++)
  {
      disSimilarityScore += pointsSimilarity(similarPairs[i].point1,similarPairs[i].point2);
  }

  return disSimilarityScore/similarPairs.size();
}

double AnimationAutoComplete::getNeighborhoodSimilarity(StrokeWithNeighbours *stroke1, StrokeWithNeighbours *stroke2)
{
    // if output was not accurate we should consider implementing a different operationsSimilarity that
    // doesn't simply return a double representing an average of points similarity.
    // we should instead return a vector.

    std::vector <SimilarPairPoint> similarPoints=getSimilarPairPoints(stroke1,stroke2);
    double centralSimilarities = getCentralSimilarities(similarPoints);

    std::vector<SimilarPairStroke> similarStrokes = getSimilarPairStrokes(stroke1,stroke2);
    double differenceInNeighborhoods = differnceOfTwoNeighborhood(stroke1,stroke2, similarStrokes);

    return sqrt(centralSimilarities + differenceInNeighborhoods);
}

double AnimationAutoComplete::getSampleId(int index, int n)
{
    //in case we use global time stamp, we will multiply by 0.9
    // to avoid having samples in different strokes with the same global TS
    return (index/n);
}

double AnimationAutoComplete::getCentralSimilarities(std::vector<SimilarPairPoint> similarPairPoints)
{
    PointWithStroke* central1 = new PointWithStroke();
    PointWithStroke* central2 = new PointWithStroke();
    central1->stroke=similarPairPoints[0].point1->stroke;
    central2->stroke=similarPairPoints[0].point2->stroke;
    int n = similarPairPoints.size();
    double dissimilarityfactor =0;
    if(n%2==0)
	{
        central1->index=(n/2);
        central2->index=(n/2);
	}
	else
	{
        central1->index=((n/2)+1);
        central2->index=((n/2)+1);
	}
    central1->point = central1->stroke->getChunk(central1->index);
    central2->point = central2->stroke->getChunk(central2->index);
    for(int i=0;i<n;i++)
	{
        if (i!=central1->index||central2->index)
		{
            dissimilarityfactor += pow(pointsSimilarity(similarPairPoints[i].point1, central1)-pointsSimilarity(similarPairPoints[i].point2,central2),2);
		}
	}
    return dissimilarityfactor/n;

}

double AnimationAutoComplete::magnitude(std::vector<double> points)
{
	double sum=0;

	for(int i=0;i<points.size();i++)
		sum+= points[i]*points[i];

	sum = sqrt(sum);
	return sum;
}


StrokeWithNeighbours *AnimationAutoComplete::generateSynthesizedStroke(StrokeWithNeighbours *lastStroke, StrokeWithNeighbours *similarStroke, StrokeWithNeighbours *nextToSimilarStroke)
{
	StrokeWithNeighbours* outputStroke = new StrokeWithNeighbours();
	std::vector<TThickPoint> points;
    double count1 = similarStroke->stroke->getChunkCount();
    double count2 = nextToSimilarStroke->stroke->getChunkCount();

    int loopCount = (int)fmin(count1, count2);
    int count3 = lastStroke->stroke->getChunkCount();
    loopCount = (int) fmin (loopCount,count3);

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

TPointD AnimationAutoComplete::meanGlobal(std::vector<SamplePoint> globalSamples)
{
	double mean1 =0;
	double mean2 =0;
   for(SamplePoint sample : globalSamples)
   {
	   mean1+=sample->getP0().x;
	   mean2+=sample->getP0().y;

   }
   mean1 /=globalSamples.size();
   mean2/=globalSamples.size();
   TPointD mean;
   mean.x=mean1;
   mean.y=mean2;
   return mean;
}

SimilarPairPoint* AnimationAutoComplete::meanLocal(std::vector<SimilarPairPoint *> localPairs)
{
	double mean1x = 0;
	double mean1y = 0;
	double mean2x = 0;
	double mean2y = 0;

    for(SimilarPairPoint* similarPair : localPairs)
	{
		int index1 = similarPair->point1->index;
		mean1x +=  similarPair->point1->stroke->getChunk(index1)->getP0().x;
		mean1y+= similarPair->point1->stroke->getChunk(index1)->getP0().y;
		int index2 = similarPair->point2->index;
		mean2x = similarPair->point2->stroke->getChunk(index2)->getP0().x;
		mean2y = similarPair->point2->stroke->getChunk(index2)->getP0().y;

	}
    SimilarPairPoint* mean = new SimilarPairPoint();
   int size = localPairs.size();
	mean->point1->point->setP0(TPointD(mean1x/size,mean1y/size));
	mean->point2->point->setP0(TPointD(mean2x/size,mean2y/size));

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
		stdX+=pow(samplePoint->getP0().x-mean.x,2);
		stdY+=pow(samplePoint->getP0().y-mean.y,2);
	}

	std.x=stdX/size;
	std.y=stdY/size;
	return std;
}

SimilarPairPoint* AnimationAutoComplete::deviationLocal(std::vector<SimilarPairPoint *> localPairs)
{
    SimilarPairPoint* mean = meanLocal(localPairs);
	double std1X =  0;
	double std1Y =  0;
	double std2X =  0;
	double std2Y =  0;
    SimilarPairPoint* std = new SimilarPairPoint();

	int size = localPairs.size()-1;


    for(SimilarPairPoint* similarPair: localPairs)
	{
		int index1 = similarPair->point1->index;
		std1X+=pow(similarPair->point1->stroke->getChunk(index1)->getP0().x-mean->point1->point->getP0().x,2);
		std1Y+=pow(similarPair->point1->stroke->getChunk(index1)->getP0().y-mean->point1->point->getP0().y,2);
		int index2 = similarPair->point2->index;
		std2X+=pow(similarPair->point2->stroke->getChunk(index2)->getP0().x-mean->point2->point->getP0().x,2);
		std2Y+=pow(similarPair->point2->stroke->getChunk(index2)->getP0().y-mean->point2->point->getP0().y,2);
	}
	std->point1->point->setP0(TPointD(std1X/size,std1Y/size));
	std->point2->point->setP0(TPointD(std2X/size,std2Y/size));
	return std;
}

//TODO: remove at production
std::vector<TStroke*> AnimationAutoComplete::drawSpaceVicinity(TStroke *stroke)
{
	std::vector<TStroke*> strokes;

	for(int i = 0; i < stroke->getChunkCount(); i++)
		strokes.push_back(makeEllipticStroke(3, stroke->getChunk(i)->getP0(), m_spaceVicinityRadius, m_spaceVicinityRadius));
    return strokes;
}

std::vector<SimilarPairStroke> AnimationAutoComplete::getSimilarPairStrokes(StrokeWithNeighbours *stroke1, StrokeWithNeighbours *stroke2)
{
    HungarianAlgorithm obj_hungarianAlgorithm;
    std::vector<StrokeWithNeighbours*>ReorderStroke1;
    std::vector<StrokeWithNeighbours*>ReorderStroke2;
    std::vector<int>SolveFunctionVector;
    std::vector<SimilarPairStroke> SimilarPairStrokeVector;
    SimilarPairStroke similarPairStroke;

    // un ordered set to vectors
    HungerianMatrix hungerianMatrix;

    for(StrokeWithNeighbours*stroke:stroke1->neighbours)
    {
        ReorderStroke1.push_back(stroke);
    }
    for(StrokeWithNeighbours*stroke:stroke2->neighbours)
    {
        ReorderStroke2.push_back(stroke);
    }
    // prepare Hungerian matrix
    for(int i = 0; i<ReorderStroke1.size(); i++)
    {
        std::vector<double>PushedVector;
        for(int j=0;j<ReorderStroke2.size();j++)
        {
            double Score=operationsSimilarity(ReorderStroke1[i],ReorderStroke2[j]);
            PushedVector.push_back(Score);
        }

        hungerianMatrix.push_back(PushedVector);
    }
        // function solve hungarian matrix and return vector of most similar pairs
    if (hungerianMatrix.size())
        obj_hungarianAlgorithm.Solve(hungerianMatrix ,SolveFunctionVector);
      //prepare vector of similar pair stroke
      for( int i=0;i<SolveFunctionVector.size();i++)
        {
          // strokepair1 is the i of vector solveFunctionVector;
          // strokepair2 is the index of vector solveFunctionVector[i];
          if(SolveFunctionVector[i] == -1)
              continue;

          StrokeWithNeighbours* strokepair1=ReorderStroke1[i];
          StrokeWithNeighbours* strokepair2=ReorderStroke2[SolveFunctionVector[i]];
          similarPairStroke.stroke1=strokepair1;
          similarPairStroke.stroke2=strokepair2;
          similarPairStroke.dissimilarityFactor=hungerianMatrix[i][SolveFunctionVector[i]];
          SimilarPairStrokeVector.push_back(similarPairStroke);

      }
      return SimilarPairStrokeVector;
}

TStroke* AnimationAutoComplete::drawNormalStroke(TStroke *stroke)
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


std::vector<TPointD> AnimationAutoComplete::predictionPositionUpdate(StrokeWithNeighbours* currentStroke, StrokeWithNeighbours* nextStroke)
{
    int count=0;
	if(currentStroke->stroke->getChunkCount()>nextStroke->stroke->getChunkCount())
		count=nextStroke->stroke->getChunkCount();
    else
		count=currentStroke->stroke->getChunkCount();
    SamplePoint sampleCurrentStroke;//sample point
    SamplePoint sampleNextStroke;
    std::vector<TPointD>predectedStrock;
    for(int i=0;i<count;i++)
    {
		sampleCurrentStroke=currentStroke->stroke->getChunk(i);
		sampleNextStroke=nextStroke->stroke->getChunk(i);
        //std::vector<TPointD> matrixA;
		TPointD subtractionMatrix = sampleNextStroke->getP0() - sampleCurrentStroke->getP0();
        TPointD Segma;
        Segma.x=1;
        Segma.y=1;
        //SamplePoint result=subtractionMatrix+sampleCurrentStroke+Segma;
		TPointD result = subtractionMatrix + (sampleCurrentStroke->getP0());
		result = result +(Segma);
		predectedStrock.push_back(result);
    }
    return predectedStrock;
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

void GlobalSimilarityGraph::insertNode(SimilarPairPoint *pair, std::vector<SimilarPairPoint *> connections)
{
    this->connections.insert(std::pair<SimilarPairPoint*, std::vector<SimilarPairPoint*>>(pair, connections));
	numberOfNodes++;
}

std::vector<SimilarPairPoint *> GlobalSimilarityGraph::getConnections(SimilarPairPoint *pair)
{
	if (pair)
		return this->connections[pair];
    return std::vector<SimilarPairPoint *>();
}

std::vector<SimilarPairPoint> AnimationAutoComplete::getSimilarPairPoints(StrokeWithNeighbours *stroke1, StrokeWithNeighbours *stroke2)
{
	HungerianMatrix input;
    std::vector<int>assignment;
    int sizeSTroke1 = stroke1->stroke->getChunkCount();
    int sizeSTroke2 = stroke2->stroke->getChunkCount();
	std::vector<SimilarPairPoint> similarPointsIndex;

    for(int i=0;i<sizeSTroke1;i++)
    {
       std::vector<double> tmp;
       for(int j=0;j<sizeSTroke2;j++)
       {
           PointWithStroke* point1 = new PointWithStroke();
           point1->index=i;
           point1->point = stroke1->stroke->getChunk(i);
           point1->stroke=stroke1->stroke;
           PointWithStroke* point2 = new PointWithStroke();
           point2->index=j;
           point2->point = stroke2->stroke->getChunk(j);
           point2->stroke=stroke2->stroke;
           double pointsimilar = pointsSimilarity(point1,point2);
           tmp.push_back(pointsimilar);
       }
       input.push_back(tmp);
    }

    HungarianAlgorithm hungarian;

    hungarian.Solve(input,assignment);

    for(int i=0;i<assignment.size();i++)
    {
       if(assignment[i]!=-1)
       {
		  SimilarPairPoint p;
          PointWithStroke* point1 = new PointWithStroke();
          point1->index=i;
          point1->stroke = stroke1->stroke;
          point1->point = stroke1->stroke->getChunk(point1->index);

          PointWithStroke* point2 = new PointWithStroke();
          point2->index=assignment[i];
          point2->stroke = stroke2->stroke;
          point2->point = stroke2->stroke->getChunk(point2->index);

		  p.point1=point1;
		  p.point2=point2;

		  similarPointsIndex.push_back(p);
       }
    }
    return similarPointsIndex;
}





