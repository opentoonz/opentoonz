// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FDTShapes.h

	This header-file contains the declarations of all low-level shape-related classes. 
	Their parent classes are in the parentheses:

		class FShape;
		class FACXForm;
		class FALineStyle;
		class FAGradRecord;
		class FCXForm;  (public FACXForm)
		class FCXFormWAlpha; (public FACXForm)
		class FDTDefineMorphShape; (public FDT)
		class FDTDefineShape; (public FDT)
		class FDTDefineShape2; (public FDT)
		class FDTDefineShape3; (public FDT)
		class FAFillStyle;
		class FFillStyle; (public FAFillStyle)
		class FFillStyleArray;
		class FFillStyleBitmap; (public FFillStyle)
		class FFillStyleGradient; (public FFillStyle)
		class FFillStyleSolid; (public FFillStyle)
		class FGradient;
		class FGradRecord; (public FAGradRecord)
		class FLineStyle; (public FALineStyle)
		class FLineStyleArray;
		class FMorphFillStyle; (public FAFillStyle)
		class FMorphFillStyleBitmap; (public FMorphFillStyle)
		class FMorphFillStyleGradient; (public FMorphFillStyle)
		class FMorphFillStyleSolid; (public FMorphFillStyle)
		class FMorphGradRecord; (public FAGradRecord)
		class FMorphLineStyle; (public FALineStyle)
		class FShapeRec;
		class FShapeRecChange; (public FShapeRec)
		class FShapeRecEdgeStraight; (public FShapeRec)
		class FShapeRecEdgeCurved; (public FShapeRec)
		class FShapeRecEnd; (public FShapeRec)
		class FShapeWStyle;

****************************************************************************************/

#ifndef _FDT_SHAPES_H_
#define _FDT_SHAPES_H_

#include "Macromedia.h"
#include "FDT.h"
#include "FSWFStream.h"
#include "FPrimitive.h"
#include "tpixel.h"

class FMorphFillStyle;
class FMorphLineStyle;
class FShapeRec;
class FFillStyle;
class FLineStyle;
class FFillStyleArray;
class FLineStyleArray;
class FShapeWStyle;
class FGradient;

#ifdef WIN32 // added from DV
#pragma warning(push)
#pragma warning(disable : 4786)
#pragma warning(disable : 4251)

#endif

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TFLASH_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//! Holds an array of ShapeRec's
/*!	The ShapeRecs are  written to a SWFStream in 
	the order they are added to the array. Normally used to store Font 
	Glyphs in a Definefont tag. An array of ShapeRec'sThis should probably 
	be called FShapeArray or such. FShapeWStyle is an expanded version.
	\sa FShapeRec, FShapeRecChange,
*/
class DVAPI FShape
{
public:
	FShape();
	virtual ~FShape();

	//!	Sets the number of bits necessary to index a Fill Style array into the SWF field nFillBits.
	/*! The SWF SHAPE tag needs the number of bits necessary to index a 
	fillstyle array However, what Fill Style Array is it referring to ??? 
	Put 0 for now.
	\param _nFillBits
	*/
	void SetFillBits(U32 _nFillBits);
	//!	Sets the number of bits necessary to index a Line Style array into theSWF field nFillBits.
	/*! The SWF SHAPE tag also needs the number of bits necessary to index a 
	linestyle array. Again, what Line Style Array is it referring to ??? 
	Put 0 for now.
	\param _nLineBits
	*/
	void SetLineBits(U32 _nLineBits);

	//!	Add a ShapeRec (of type curve, line, or ChangeRec) to this Shape
	/*!	Add a ShapeRec which could be a ChangeRec, or an EdgeRec (curve or line)
		to this Shape.  The first is always a ChangeRec to set drawing position.
	
	  \param	shapeRec
	 */
	void AddShapeRec(FShapeRec *shapeRec);

	//!	Stream the ShapeRecs out to the temporary buffer
	/*!	For each ShapeRec, tell it the # of fill bits, line bits, 
		and tell it to write itself to the temp stream.
	
	  \param	_SWFStream: the temporary output stream
	 */
	virtual void WriteToSWFStream(FSWFStream *_SWFStream); //not Complete, see comments

private:
	U32 nFillBits;
	U32 nLineBits;
	std::vector<FShapeRec *> shapeRecs;
};

//!	Abstract Base class of FCXForm and FCXFormWAlpha
/*!	Specifies a color transform for certain objects

  \sa FCXForm, 	FCXFormWAlpha
 */
class DVAPI FACXForm
{
public:
	//!	Virtual method. Write the color transform out to stream.
	/*!	Implemented by FCXForm, FCXFormWAlpha
	
	  \param	_SWFStream
	 */
	virtual ~FACXForm() {}
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) = 0;
};

//!	Abstract Base class of FLineStyle, FMorphLineStyle
/*!	All line styles fall into this category (morph or regular)

  \sa	FLineStyle, FMorphLineStyle
 */
class DVAPI FALineStyle
{
public:
	virtual ~FALineStyle() {}
	//!	Virtual Method for writing LineStyles out to a stream
	/*!	Virtual Method for writing LineStyles out to a stream
	
	  \param	_SWFStream	
	 */
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) = 0;
};

//

//!	Virtual Base Class of FGradRecord,FMorphGradRecord
/*!	All grad records fall into this category

  \sa	FGradRecord,FMorphGradRecord
 */
class DVAPI FAGradRecord
{
public:
	virtual ~FAGradRecord() {}

	//!	Virtual Method for writing a Gradient out to a stream

	/*!
	  \param	_SWFStream	
	 */
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) = 0;
};

//

//!	Color Transform
/*!	Specifies a color transformation for some object without transparency information

  \sa	
 */
class DVAPI FCXForm : public FACXForm
{
public:
	//!	Set the values of the transform - no alpha channel
	/*!	Do we need the _has parameters?
		Is 0 a valid value for each of these mult or add params?
	
	  \param	U32 _hasAdd:  Has color addition values if equal to 1
	  \param	U32 _hasMult:  Has color multipy values if equal to 1
	  \param	S32 _redMultTerm: Red multiply value
	  \param	S32 _greenMultTerm: Green multiply value
	  \param	S32 _blueMultiTerm: Blue multiply value
	  \param	S32 _redAddTerm: Red addition value
	  \param	S32 _greenAddTerm: Green addition value
	  \param	S32 _blueAddTerm: Blue addition value

	 */
	FCXForm(
		U32 _hasAdd, U32 _hasMult, S32 _redMultTerm, S32 _greenMultTerm,
		S32 _blueMultiTerm, S32 _redAddTerm, S32 _greenAddTerm, S32 _blueAddTerm);

	//!	Write a SWF Color Transform without Alpha to Stream
	/*!	Writes the _has bits, and depending on their values, the multiply and add
		color values. Computes the SWF nBits field from color values passed.
	
	  \param	_SWFStream	
	 */
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

protected:
	//flags
	U32 hasAdd;
	U32 hasMult;

	// bits in each term field
	U32 nBits;

	S32 redMultTerm;
	S32 greenMultTerm;
	S32 blueMultTerm;
	S32 redAddTerm;
	S32 greenAddTerm;
	S32 blueAddTerm;

	// finds minimum number of bits needed to represent the largest term
	U32 MinBits(void);
};

// Specifies a color transformation for some object with transparency information

//!	Set the values of the transform - same as FCXForm with alpha channel
/*!	Do we need the _has parameters?
		Is 0 a valid value for each of these mult or add params?
	
	  \param	U32 _hasAdd:  Has color addition values if equal to 1
	  \param	U32 _hasMult:  Has color multipy values if equal to 1
	  \param	S32 _redMultTerm: Red multiply value
	  \param	S32 _greenMultTerm: Green multiply value
	  \param	S32 _blueMultiTerm: Blue multiply value
	  \param	S32 _alphaMultTerm: Alpha transparency multiply value
	  \param	S32 _redAddTerm: Red addition value
	  \param	S32 _greenAddTerm: Green addition value
	  \param	S32 _blueAddTerm: Blue addition value
	  \param	S32 _alphaAddTerm: Alpha transparency addition value

	 */
class DVAPI FCXFormWAlpha : public FCXForm
{

public:
	FCXFormWAlpha(U32 _hasAdd, U32 _hasMult, S32 _redMultTerm, S32 _greenMultTerm, S32 _blueMultTerm, S32 _alphaMultTerm,
				  S32 _redAddTerm, S32 _greenAddTerm, S32 _blueAddTerm, S32 _alphaAddTerm);
	//!	Write a SWF Color Transform with Alpha to Stream
	/*!	Writes the _has bits, and depending on their values, the multiply and add
			color values. Computes the SWF nBits field from color values passed.
	
			  \param	_SWFStream	
		*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	//flags
	//	U32 hasAdd;
	//	U32 hasMult;

	//bits in each term field
	U32 nBits;

	//	S32 redMultTerm;
	//	S32 greenMultTerm;
	//	S32 blueMultTerm;
	S32 alphaMultTerm;

	//	S32 redAddTerm;
	//	S32 greenAddTerm;
	//	S32 blueAddTerm;
	S32 alphaAddTerm;

	//returns minimum number of bits needed to represent largest term value
	U32 MinBits(void);
};

//

//!	a flash object which defines a morphing shape.  It contains appearance information about an original shape and a shape being morphed into
/*!	Note: this is the SWF info and should be rewritten. I would have expected that a
	morph would take two character IDs instead of defining everything here. ???
	Defines the metamorphosis of one shape (Shape1) into another (Shape2). 
	The ShapeBounds1 specifies the boundaries of the original shape, 
	while ShapeBounds2 specifies the boundaries of the shape into which 
	Shape1 changes. The data specified in MorphFillStyles and MorphLineStyles 
	are for both shapes. For example, the fill style for Shape1 is specified, 
	then the corresponding fill style for Shape2 is specified. Edges1 
	specifies the edges for Shape1 as well as the morph style data for 
	both shapes. Edges2 specifies the edges for Shape2, and contains no 
	style data. The number of edges specified in Edges1 must equal the 
	number of edges in Edges2. 

	Should point to example code here. 	
 */

class DVAPI FDTDefineMorphShape : public FDT
{

public:
	//!	Constructor just takes the before and after bounds RECTs
	/*!	
		 \param	_rect1, _rect2 rects of before and after shapes
		*/
	FDTDefineMorphShape(FRect *_rect1, FRect *_rect2);
	virtual ~FDTDefineMorphShape();
	//!	Add Fill style to array and record its index
	/*!	 You should have used FMorphFillStyles (solid, gradient, bitmap)
			to create a morph fill style 
				\param	fillStyle
			*/
	U32 AddFillStyle(FMorphFillStyle *fillStyle);

	// here changed.
	//!	Add line style to array and record its index.
	/*!	 You need a pair of line styles (lineWidth + lineColor),
		     which contains both "morph from" and "morph to" line style information.
			\param	startLineWidth	Line thickness of starting shape in twips.
			\param	startLineColor	Line color of starting shape. (should be RGBA type).
			\param	finalLineWidth	Line thickness of ending shape in twips.
			\param	finalLineColor	Line color of ending shape. (should be RGBA type).
			\return	the position in the array.
			*/
	U32 AddLineStyle(U32 startLineWidth, FColor *startLineColor,
					 U32 finalLineWidth, FColor *finalLineColor);

	//!	Cleans up the internal representation of the fill and line style arrays.
	void FinishStyleArrays(void);

	//!	Add a Shape Record to the first morph object definition
	/*!	The AddShapeRec_1 method takes style information for both shapes and edge 
	information for the "morph from" shape
	  \param	_shapeRec the change, straight, curve record, or end record
	 */
	void AddShapeRec_1(FShapeRec *_shapeRec);
	//!	Add a Shape Record to the second morph object
	/*!	The AddEdgeRec_2 method takes edge information for just the "morph to" 
	  \param	_shapeRec the change, straight, curve record, or end record
	 */
	void AddEdgeRec_2(FShapeRec *_shapeRec);

	//!	Record morphing objects's ID for later reference
	U16 ID(void);

	//!	Called by the FObj to write to the output stream
	/*!		  \param	_SWFStream
	 */
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U16 characterID;
	FRect *shapeBounds_1; // 1 subscript denotes original shape
	FRect *shapeBounds_2; // and 2 subscript denotes shape being morphed to
	FShape shape1;
	FShape shape2;
	U32 offset;
	FFillStyleArray *morphFillStyleArray;
	FLineStyleArray *morphLineStyleArray;
	U32 nFillBits;
	U32 nLineBits;
	U8 styleArraysFinished;
};

//!	Create a DefineShape tag
/*!	Creates a Flash 1.0 Define Shape tag. You should use DefineShape3.
	All the DefineShape tags only differ in the fill/line style arrays
	and fill/line style records 

  \sa	FFillStyle, FLineStyle, FShapeRec, FShapeWStyle, 
	see also FExampleRectangle.cpp	
 */
class DVAPI FDTDefineShape : public FDT
{

public:
	//!	Create the basic empty shell of a DefineShape tag
	/*!	Only the bounds rect is passed. Fills, Lines, and 
				Shapes must be added.
	
				\param	_rect the bounds rectangle
			*/
	FDTDefineShape(FRect *_rect);
	virtual ~FDTDefineShape();

	//!	Add a Shape record to the internal ShapeRec array
	/*!	Use FShapeRecChange, FShapeRecEdgeStraight, FShapeRecEdgeStraight
				to create edges to add to this shape
			\param	_shapeRec
			*/
	void AddShapeRec(FShapeRec *_shapeRec);

	//!	Cleans up the internal representation of the fill and line style arrays.
	void FinishStyleArrays(void);
	//!	Add fill style to shape,
	/*!	Remember to store the position of the fill style. Use this when 
				creating the shape change record later, to indicate which style
				to use for the fill.
				\param	fillStyle
				\return the position in the array.  when creating the shape change record later.
			*/
	U32 AddFillStyle(FFillStyle *fillStyle);

	// changed here.
	//!	Add solid fill style to shape,
	/*!	Remember to store the position of the fill style. Use this when 
				creating the shape change record later, to indicate which style
				to use for the fill.
				\param	fillColor
				\return the position in the array.  when creating the shape change record later.
			*/
	U32 AddSolidFillStyle(FColor *fillColor);

	// here changed.
	//!	Add line style to array and record its index.
	/*!	Remember to store the position of the fill style. Use this 
			when creating the shape change record later, to indicate which style
			to use for the next edges.
			\param	lineWidth	Line thickness in twips.
			\param	lineColor	Line color. (should be RGB type).
			\return the position in the array. 
		*/
	U32 AddLineStyle(U32 lineWidth, FColor *lineColor);

	//!	Record objects's ID for when you do a PlaceObject
	U16 ID(void);
	//!	Called by the FObj to write to the output stream
	/*!		  \param	_SWFStream
	 */
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);
	virtual void SetId(U16 id) { characterID = id; }
	bool IsDefineShape();

private:
	U16 characterID;
	FRect *shapeBounds;
	FShapeWStyle *shapeWithStyle;
	FFillStyleArray *fillStyleArray;
	FLineStyleArray *lineStyleArray;
	//U32 nFillBits;
	//U32 nLineBits;
	U8 styleArraysFinished;
};

//!	Create a DefineShape2 tag
/*!	Creates a Flash 2.0 Define Shape tag.  You should use DefineShape3.
	All the DefineShape tags only differ in the fill/line style arrays
	and fill/line style records 

  \sa	FFillStyle, FLineStyle, FShapeRec, FShapeWStyle, 
	see also FExampleRectangle.cpp	
 */
class DVAPI FDTDefineShape2 : public FDT
{

public:
	//!	Create the basic empty shell of a DefineShape tag
	/*!	Only the bounds rect is passed. Fills, Lines, and 
				Shapes must be added.
	
				\param	_rect the bounds rectangle
			*/
	FDTDefineShape2(FRect *_rect);
	virtual ~FDTDefineShape2();
	//!	Add a Shape record to the internal ShapeRec array
	/*!	Use FShapeRecChange, FShapeRecEdgeStraight, FShapeRecEdgeStraight
				to create edges to add to this shape
			\param	_shapeRec
			*/
	void AddShapeRec(FShapeRec *_shapeRec);

	//!	Cleans up the internal representation of the fill and line style arrays.
	void FinishStyleArrays(void);

	//!	Add fill style to shape,
	/*!	Remember to store the position of the fill style. Use this when 
			creating the shape change record later, to indicate which style
			to use for the fill. Remember, DefineShape2s don't support Alpha.
			\param	fillStyle
			\return the position in the array.  When creating the shape change record later.
		*/
	U32 AddFillStyle(FFillStyle *fillStyle);

	// here changed.
	//!	Add solid fill style to shape,
	/*!	Remember to store the position of the fill style. Use this when 
			creating the shape change record later, to indicate which style
			to use for the fill. Remember, DefineShape2s don't support Alpha.
			\param	fillColor
			\return the position in the array.  When creating the shape change record later.
		*/
	U32 AddSolidFillStyle(FColor *fillColor);

	// here changed.
	//!	Add line style to rectangle,
	/*!	Remember to store the position of the fill style. Use this 
			when creating the shape change record later, to indicate which style
			to use for the next edges.
			\param	lineWidth	Line thickness in twips.
			\param	lineColor	Line color. (should be RGB type).
			\return the position in the array. 
		*/
	U32 AddLineStyle(U32 lineWidth, FColor *lineColor);

	//!	Record objects's ID for when you do a PlaceObject
	U16 ID(void);
	//!	Called by the FObj to write to the output stream
	/*!		  \param	_SWFStream
	 */
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);
	virtual void SetId(U16 id) { characterID = id; }
	bool IsDefineShape();

private:
	U16 characterID;
	FRect *shapeBounds;
	FShapeWStyle *shapeWithStyle;
	FFillStyleArray *fillStyleArray;
	FLineStyleArray *lineStyleArray;
	U32 nFillBits;
	U32 nLineBits;
	U8 styleArraysFinished;
};

//!	Create a DefineShape3 tag
/*!	Creates a Flash 3.0 Define Shape tag. 
	All the DefineShape tags only differ in the fill/line style arrays
	and fill/line style records (accepts alpha color values).

  \sa	FFillStyle, FLineStyle, FShapeRec, FShapeWStyle, 
	see also FExampleRectangle.cpp	
 */

class DVAPI FDTDefineShape3 : public FDT
{

public:
	//!	Create the basic empty shell of a DefineShape tag
	/*!	Only the bounds rect is passed. Fills, Lines, and 
				Shapes must be added.
	
				\param	_rect the bounds rectangle
			*/
	FDTDefineShape3();
	FDTDefineShape3(FRect *_rect);
	void setBounds(FRect *_rect);
	virtual ~FDTDefineShape3();
	//!	Add a Shape record to the internal ShapeRec array
	/*!	Use FShapeRecChange, FShapeRecEdgeStraight, FShapeRecEdgeStraight
				to create edges to add to this shape
			\param	_shapeRec
			*/
	void AddShapeRec(FShapeRec *_shapeRec);
	//!	Cleans up the internal representation of the fill and line style arrays.
	void FinishStyleArrays(void);

	//!	Add fill style to shape,
	/*!	Remember to store the position of the fill style. Use this when 
			creating the shape change record later, to indicate which style
			to use for the fill. 
			\param	fillStyle
			\return the position in the array.  When creating the shape change record later.
		*/
	U32 AddFillStyle(FFillStyle *fillStyle);

	//!	Add solid fill style to shape,
	/*!	Remember to store the position of the fill style. Use this when 
			creating the shape change record later, to indicate which style
			to use for the fill. 
			\param	fillColor
			\return the position in the array.  When creating the shape change record later.
		*/
	U32 AddSolidFillStyle(FColor *fillColor);

	//!	Add line style to array and record its index.
	/*!	Remember to store the position of the fill style. Use this 
			when creating the shape change record later, to indicate which style
			to use for the next edges.
			\param	lineWidth	Line thickness in twips.
			\param	lineColor	Line color. (should be RGBA type).
			\return the position in the array. 
		*/
	U32 AddLineStyle(U32 lineWidth, FColor *lineColor);

	//!	Record objects's ID for when you do a PlaceObject
	U16 ID(void);
	//!	Called by the FObj to write to the output stream
	/*!		  \param	_SWFStream
	 */
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);
	virtual void SetId(U16 id) { characterID = id; }
	bool IsDefineShape();
	void changeColor(const FColor &oldColor, const FColor &newColor);
	void changeColor(const std::map<TPixel, TPixel> &table);

	FRect GetBBox() const { return *shapeBounds; }
private:
	U16 characterID;
	FRect *shapeBounds;
	FShapeWStyle *shapeWithStyle;
	FFillStyleArray *fillStyleArray;
	FLineStyleArray *lineStyleArray;
	U32 nFillBits;
	U32 nLineBits;
	U8 styleArraysFinished;
};

//!	Abstract Base Class. All edge records fall into this category
/*!	The fill classes from the solid, gradient, bitmap are children.

  \sa	FFillStyleGradient, FFillStyleBitmap, FFillStyleSolid
 */
class DVAPI FAFillStyle
{
public:
	virtual bool IsSolidStyle() { return false; };

	virtual ~FAFillStyle() {}
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) = 0;
};

//The fill style used by regular non-morph shapes

//!	Why does this exist?
/*!	It is used as a parameter to other functions but I don't know why.

  \sa	FFillStyleGradient, FFillStyleBitmap, FFillStyleSolid
 */
class DVAPI FFillStyle : public FAFillStyle
{
public:
	virtual ~FFillStyle() {}
};

//!An array of fill styles.
/*!	

  \sa	FFillStyleGradient, FFillStyleBitmap, FFillStyleSolid, FAFillStyle
*/
class DVAPI FFillStyleArray
{

public:
	FFillStyleArray(void) {}
	virtual ~FFillStyleArray(void);
	//!	The given fill style is added to the end of the fill style array.
	/*!	The position of the added fill style is returned so that the fill style can later be referenced.

			\param	fillStyle the style to add
			*/

	U32 Add(FAFillStyle *fillStyle);
	//!	Returns the size of the fill style list.
	U32 Size();
	FAFillStyle *get(unsigned int index);

	//!	Writes the array to the stream
	/*!	Travels through all of the nodes in the array and writing
		their fill styles.  First has to write the count of fill style arrays.  See's if count
		is small enough to fit in to an 8 bit field, and either writes the count into an 8 bit
		field, or writes all 1's into an 8 bit field and writes the real count into a 16 bit 
		field.  		
	
			\param	_SWFStream
		*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	//the list which contains all of the fill styles
	std::vector<FAFillStyle *> fillStyleArray;
};

//!	The Bitmap fill style used by regular non-morph shapes
/*!	Can be tiled or clipped, depending on the flag.

  \sa	FFillStyleArray, FFillStyleGradient, FFillStyleBitmap, FFillStyleSolid, FAFillStyle
 */
//
//

class DVAPI FFillStyleBitmap : public FFillStyle
{

public:
	/*!
	
			\param	tiled indicates if the Bitmap fill style is tiled (tiledFlag==1) or 
				clipped (tiledFlag==0).
			\param	matrix translation matrix for offsetting, rotating, scaleing etc. the bitmap
		*/
	FFillStyleBitmap(int tiled, U16 ID, FMatrix *matrix);
	virtual ~FFillStyleBitmap(void);
	//!	Writes the bitmap fill style to the given FSWFStream.
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	int tiledFlag;
	U16 bitmapID;
	FMatrix *bitmapMatrix;
};

//!	The Gradient fill style used by regular non-morph shapes
/*!	Can be linear or radial, depending on the flag.

  \sa	FFillStyleArray, FGradient
 */
class DVAPI FFillStyleGradient : public FFillStyle
{

public:
	//!	Create a Gradient fill style
	/*!	
			\param linear indicates if the gradient fill style is linear (linearFlag==1) or 
	radial (linearFlag==0).	
			\param	matrix matrix translation matrix for placing the gradient in the fill area
				Not sure what each of the matrix fields do???
			\param gradient a pre-build gradient object. See FGradient
		*/
	FFillStyleGradient(int linear, FMatrix *matrix, FGradient *gradient);
	virtual ~FFillStyleGradient(void);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	int linearFlag;
	FMatrix *gradFillMatrix;
	FGradient *gradFill;
};

//The solid fill style used by regular non-morph shapes

//!	Creates a Solid Fill object
/*!	Basically just contains a color

  \sa FFillStyleArray, FFillStyleGradient, FFillStyleBitmap	
 */
class DVAPI FFillStyleSolid : public FFillStyle
{
public:
	//!	Create a style object
	/*!	Whether or not FColor contains alpha information is not 
				important until the color is written out.
	
			\param	Fcolor which can contain alpha or not
			*/
	FFillStyleSolid(FColor *_color);

	virtual ~FFillStyleSolid();

	//!	Write the color to the file stream
	/*!	When this object is serialized, it automatically handles the case of
				including the alpha information if its there.
			\param	*_SWFStream the output stream
			*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);
	bool IsSolidStyle() { return true; };
	void setColor(FColor *_color)
	{
		delete color;
		color = _color;
	}
	FColor *getColor() { return color; }

private:
	FColor *color;
};

//!	Gradient information is stored in an array of Gradient Records
/*!	This is that array. This style of gradient records is used with 
	non-morphing objects.

  \sa	FGradRecord
 */
class DVAPI FGradient
{

public:
	FGradient(void);
	~FGradient(void);
	//!	Add a Gradient Record to the internal list
	/*!	Up to 8 gradient records may be added to this list
				The ratio field within each GradientRec determines what percentage
				of a gradient is a particular color.
			\param	FAGradRecord*  pointer to a gradient record
			*/
	void Add(FAGradRecord *g);
	void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	//the list which contains all of the grad records
	std::list<FAGradRecord *> gradRecords;
};

//	used by non-morph shapes

//!	A non morph gradient record.
/*!	Note that ostensibly the ratio parameter is the percentage complete aTLong 
	the gradient that this color should be but it's really not known what this
	field is.
  \sa	FGradient, FFillStyleGradient
 */
class DVAPI FGradRecord : public FAGradRecord
{
public:
	//!Create the Gradient record with the mysterious ration parm.
	/*!	
		
		  \param U_ratio know one knows for sure
		  \param FColor one of the colors to shift through
		 */
	FGradRecord(U32 _ratio, FColor *color);

	virtual ~FGradRecord();

	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U32 ratio;
	FColor *color;
};

//

//!	The line style used by regular non-morph shapes
/*!	LineStyles are held in arrays of LineStyles which are held
	in DefineShape Tags.   

  \sa	FExampleRectangle.cpp, FLineStyleArray
 */
class DVAPI FLineStyle : public FALineStyle
{

public:
	//!Line Style is just a thickness and color
	/*!	
		
		  \param	width U32
		  \param	_color FColor	
		 */
	FLineStyle(U32 _width, FColor *_color);
	virtual ~FLineStyle();
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U32 width;
	FColor *color;
};

//
//!	An array of line styles.
/*!	LineStyles are held in arrays of LineStyles which are held
	in DefineShape Tags.   

  \sa	FLineStyle, FExampleRectangle.cpp
*/
class DVAPI FLineStyleArray
{

public:
	//!Create an empty initialized array
	FLineStyleArray(void);
	~FLineStyleArray(void);
	//!Add the line style to the array.
	/*!	Automatically handles the case of packing the corresponding SWF object
			properly when you go over 255 styles.  Not that we can see why you'd do
			this.
		
		  \param		lineStyle the style object to add to the internal array
		 */
	U32 Add(FALineStyle *lineStyle);
	//! The number of items in the list
	/*! This is needed when a containing DefineShape needs to know how how
	many styles there are and therefore how many bits are needed for an index.
		The given line style is added to the end of the line style array.  The position of 
	the added line style is returned so that the line style can later be referenced.
	*/
	U32 Size(void);
	//! Write the array to the output stream.
	/*!		Writes to the stream by travelling through all of the nodes in 
		the array and writing their line styles.  First has to write the count 
		of line style arrays.  Sees if count is small enough to fit in to an 
		8 bit field, and either writes the count into an 8 bit field, or writes 
		all 1's into an 8 bit field and writes the real count into a 16 bit field.
		
		  \param	*_SWFStream	
		 */
	void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	//the list which contains all of the line styles
	std::list<FALineStyle *> lineStyleArray;
};

//!	The Base class of fill style objects used by morph shapes
/*!	

  \sa	FMorphFillStyleGradient, FMorphFillStyleBitmap, FMorphFillStyleSolid
 */
class DVAPI FMorphFillStyle : public FAFillStyle
{
public:
	virtual ~FMorphFillStyle() {}
	virtual void WriteToSWFStream(FSWFStream * /* _SWFStream */) {}
};

//
//

//!The Bitmap fill style used by morph shapes
/*!	Note that all Morph fill styles are documented in SWF as a single overloaded
		data structure.  It's simplier to have three separate routiens.
		
		This can be tiled or clipped, depending on the flag.
	
	\sa	FMorphFillStyleGradient, FMorphFillStyleBitmap, FMorphFillStyleSolid, FMatrix
	 */
class DVAPI FMorphFillStyleBitmap : public FMorphFillStyle
{

public:
	//	The tiledFlag indicates if the Bitmap fill style is tiled (tiledFlag==1) or
	//	clipped (tiledFlag==0).

	//! Bitmap fill style for a morph
	/*!	
		
		  \param tiled	The tiledFlag indicates if the Bitmap fill style is tiled (tiledFlag==1) or 
	clipped (tiledFlag==0)	
		\param ID Character ID of bitmap
		\param matrix1 rotation/translation Matrix for first state
		\param matrix2 for the second state
		 */
	FMorphFillStyleBitmap(int tiled, U16 ID, FMatrix *matrix1, FMatrix *matrix2);
	virtual ~FMorphFillStyleBitmap(void);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	int tiledFlag;
	U16 bitmapID;
	FMatrix *bitmapMatrix1;
	FMatrix *bitmapMatrix2;
};

//!The Gradient fill style used by morph shapes
/*!	Note that all Morph fill styles are documented in SWF as a single overloaded
		data structure.  It's simplier to have three separate routines.
		
		Can be linear or radial, depending on the flag, depending on the flag.
	
	\sa	FMorphFillStyleGradient, FMorphFillStyleBitmap, FMorphFillStyleSolid, FGradient, FMatrix, FFillStyleArray
	 */
class DVAPI FMorphFillStyleGradient : public FMorphFillStyle
{

public:
	//!Construct a Gradient Fill Style
	/*!	Ultimatly to go in a MorphFillStyles Array and then get 
			added to a DefineMorphShape
			
			  \param		linear The linearFlag indicates if the gradient fill 
			  style is linear (linearFlag==1) or radial (linearFlag==0)
			  \param matrix1
			  \param matrix2 translation matrix for the gradient before and after.
			  \param gradient an FGradient fill
			*/
	FMorphFillStyleGradient(int linear, FMatrix *matrix1, FMatrix *matrix2, FGradient *gradient);
	virtual ~FMorphFillStyleGradient(void);
	//!Writes the Gradient fill style to the given FSWFStream.
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	int linearFlag;
	FMatrix *gradFillMatrix1;
	FMatrix *gradFillMatrix2;
	FGradient *gradFill;
};

//!The solid fill style used by morph shapes
/*!	Each Fill style has two colors for the two objects being morphed.  The FillStyles are
in a FFillStyle array.  In this case the fill styles object 1 and 2 are interleaved. The
FillStyleArray is then stored in a DefineMorphShape.

  \sa FMorphFillStyleGradient, FMorphFillStyleBitmap, FMorphFillStyleSolid, FGradient, FMatrix,FFillStyleArray 	
 */
class DVAPI FMorphFillStyleSolid : public FMorphFillStyle
{

public:
	/*!	
		  \param _color1 Solid fill color with transparency information for first shape	
		  \param _color2 Same for second
		 */
	FMorphFillStyleSolid(FColor *_color1, FColor *_color2);
	virtual ~FMorphFillStyleSolid();
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	FColor *color1;
	FColor *color2;
};

//!The grad record used by morph shapes
/*!	The Morph Gradient store an array of these records.  Each is like a double
of the regular Gradient fill record.  Each Morph Gradient Record has two colors 
and two ratio numbers for the two objects being morphed.  Theses Gradient Records 
possibly 8 of them, are stored in an FAGradient array, the base of all gradient records. 
Normally the Colors in a gradient array store the colors aTLong the gradient.  These
store pairs of colors, one for each of the two objects aTLong with the mysterious ratio number.
 The FMorphFillStyleGradient is then stored in a DefineMorphShape.

  \sa FMorphFillStyleGradient, FMorphFillStyleBitmap, FMorphFillStyleSolid, FAGradient, FMatrix,FFillStyleArray 	
 */
class DVAPI FMorphGradRecord : public FAGradRecord
{

public:
	//!Create a Morph Gradient record
	/*!	
		  \param	_ratio1 for the first object
		  \param	_color1
		  \param	_ratio2 for the second object
		  \param	_color2
		 */
	FMorphGradRecord(U32 _ratio1, FColor *_color1, U32 _ratio2, FColor *_color2);
	virtual ~FMorphGradRecord();

	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U32 ratio1;
	U32 ratio2;

	FColor *color1;
	FColor *color2;
};

//!	Morph line styles to go in the DefineMorph Object
/*!	MorphLineStyle is an FALineStyle.  These get put into 
	FLineStyleArrays which in turn are put in the Define Morph Object.
	These are just line regular line styles, except that they 
	carry a pair of line styles, one for the morph from object
	and the corresponding one in the Morphe to object.

  \sa	FLineStyleArray
 */
class FMorphLineStyle : public FALineStyle
{

public:
	//!create the pair of styles, width/color
	/*!	
		
		  \param width1
		  \param width2
		  \param _color1
		  \param _color2

		 */
	FMorphLineStyle(U32 _width1, U32 _width2, FColor *_color1, FColor *_color2);
	virtual ~FMorphLineStyle();
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	FColor *color1;
	FColor *color2;
	U32 width1;
	U32 width2;
};

//!	Virtual Base class for the change, straight, and curved Shape records
/*!	

  \sa 	FShapeRecChange, FShapeRecEdgeStraight, FShapeRecEdgeCurved
 */
class DVAPI FShapeRec
{

public:
	virtual ~FShapeRec() {}
	virtual bool isFShapeRecChange() { return false; }
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) = 0;
	virtual void IncludeNFillBitInfo(U32 _nFillBits) = 0;
	virtual void IncludeNLineBitInfo(U32 _nLineBits) = 0;
	virtual bool IsRecChange() { return false; }
};

//	shape record that defines changes in line style, fill style, position, or a new set of styles

class DVAPI FShapeRecChange : public FShapeRec
{

public:
	FShapeRecChange(U32 _stateNewStyles,
					U32 _stateLineStyle,
					U32 _stateFillStyle1,
					U32 _stateFillStyle0,
					U32 _stateMoveTo,
					S32 _moveDeltaX,
					S32 _moveDeltaY,
					U32 _fill0Style,
					U32 _fill1Style,
					U32 _lineStyle,
					FFillStyleArray *_fillStyles,
					FLineStyleArray *_lineStyles);

	virtual ~FShapeRecChange(void);

	virtual bool isFShapeRecChange() { return true; }
	void getFillLineBits(U32 &_nFillBits, U32 &_nLineBits)
	{
		_nFillBits = nFillBits;
		_nLineBits = nLineBits;
	}
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);
	virtual void IncludeNFillBitInfo(U32 _nFillBits);
	virtual void IncludeNLineBitInfo(U32 _nLineBits);
	bool IsRecChange() { return true; }
	FFillStyleArray *GetFillStyles();

private:
	U32 stateNewStyles;
	U32 stateLineStyle;
	U32 stateFillStyle0;
	U32 stateFillStyle1;
	U32 stateMoveTo;
	U32 nMoveBits;
	U32 nFillBits; // these two values are stored in the Shape field
	U32 nLineBits; // they are passed to each ShapeRec
	S32 moveDeltaX;
	S32 moveDeltaY;
	U32 fill0Style;
	U32 fill1Style;
	U32 lineStyle;
	FFillStyleArray *fillStyles;
	FLineStyleArray *lineStyles;
	U32 MinBits(void);
};

//!	Create a straight edge
/*!	The XY values are passed as delta values from the previous XY points.
	Creating and adding several straight edges in a row will create a set
	of connected edges.

	This may change to absolution positions in the future. ???

	See also FExampleRectangle.cpp	
	
 */
class DVAPI FShapeRecEdgeStraight : public FShapeRec
{
public:
	//!	Create a straight edge from the previous point
	/*!	This is the equivalent of a "LineTo" command
	
			\param	generalDX
			\param generalDY
			*/
	FShapeRecEdgeStraight(S32 dx, S32 dy);
	virtual ~FShapeRecEdgeStraight(){};

	//!Write out as Vertical, Horizontal or general line SWF Record automatically
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);
	//!dummy in this object.  This call only makes sense for a Change Rec, here for historical
	virtual void IncludeNFillBitInfo(U32 _nFillBits);
	//!dummy in this object.  This call only makes sense for a Change Rec, here for historical
	virtual void IncludeNLineBitInfo(U32 _nLineBits);

private:
	U32 edgeFlag;
	U32 nBits;
	U32 generalLineFlag;
	S32 generalDeltaX;
	S32 generalDeltaY;
	U32 verticalLineFlag;
	S32 horizontalDeltaX;
	S32 verticalDeltaY;
	U32 MinBits(void);
};

//!	Create a bezier edge
/*!	The control points are passed as delta values from the previous control points.
	This may change to absolution positions in the future. ???

	See also [example file], [bezier info file]
	
 */
class DVAPI FShapeRecEdgeCurved : public FShapeRec
{
public:
	//!	Create a bezier
	/*!	All values expressed in twips ???
			\param	controlDX delta from the last X control value
			\param  controlDY delta from the last Y control value 
			\param  anchorDX delta from the last X anchor value
			\param  anchorDY delta from the last Y anchor value
			*/
	FShapeRecEdgeCurved(S32 controlDX, S32 controlDY, S32 anchorDX, S32 anchorDY);
	virtual ~FShapeRecEdgeCurved(void) {}
	//!	Called by containing Shape when file is serialized
	/*!	Writes the anchor, control, pts, as well as the minBits and edgeFlag	  
	\param	_SWFStream
	 */
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);
	//!dummy in this object.  This call only makes sense for a Change Rec, here for historical
	virtual void IncludeNFillBitInfo(U32 _nFillBits);
	//!dummy in this object.  This call only makes sense for a Change Rec, here for historical
	virtual void IncludeNLineBitInfo(U32 _nLineBits);

private:
	U32 edgeFlag;
	U32 nBits;
	S32 controlDeltaX;
	S32 controlDeltaY;
	S32 anchorDeltaX;
	S32 anchorDeltaY;
	U32 MinBits(void);
};

//The shape record which signifies the end of a shape record array.

class DVAPI FShapeRecEnd : public FShapeRec
{

public:
	FShapeRecEnd(void);
	//!Write an EndRecord.  Signifies the last ShapeRecord in the ShapeRecArray
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);
	//!dummy in this object.  This call only makes sense for a Change Rec, here for historical
	virtual void IncludeNFillBitInfo(U32 _nFillBits);
	//!dummy in this object.  This call only makes sense for a Change Rec, here for historical
	virtual void IncludeNLineBitInfo(U32 _nLineBits);
};

class DVAPI FShapeWStyle
{

public:
	FShapeWStyle(FFillStyleArray *_fillStyles, FLineStyleArray *_lineStyles);
	~FShapeWStyle(void);
	void AddShapeRec(FShapeRec *shapeRec);
	void WriteToSWFStream(FSWFStream *_SWFStream);
	U32 NumFillBits();
	U32 NumLineBits();
	FFillStyleArray *GetFillStyles() { return fillStyles; }
	void changeColor(const FColor &oldColor, const FColor &newColor);
	void changeColor(const std::map<TPixel, TPixel> &table);

private:
	FFillStyleArray *fillStyles;
	FLineStyleArray *lineStyles;
	U32 nFillBits;
	U32 nLineBits;
	std::vector<FShapeRec *> shapeRecs; //see comments, will need to be changed
};

#ifdef WIN32 // added from DV
#pragma warning(pop)
#endif
#endif
