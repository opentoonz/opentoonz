#pragma once

#ifndef T_STAGE_OBJECT_INCLUDED
#define T_STAGE_OBJECT_INCLUDED

// TnzCore includes
#include "tgeometry.h"

// TnzBase includes
#include "tdoubleparam.h"

// TnzExt includes
#include "ext/plasticskeletondeformation.h"

// TnzLib includes
#include "tstageobjectid.h"

// tcg includes
#include "tcg/tcg_controlled_access.h"

// Qt includes
#include <QStack>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//====================================================

//    Forward declarations

class TPinnedRangeSet;
class TStroke;
class TStageObjectParams;
class TStageObjectSpline;
class TCamera;
class TStageObjectTree;
class PlasticSkeletonDeformation;

//====================================================

//********************************************************************************************
//    TStageObject  declaration
//********************************************************************************************

/*!
  TStageObject is the Toonz class representing animatable scene objects like
columns, pegbars,
  tables and cameras (pretty much every object in Toonz's stage schematic).

\par Description

  A TStageObject contains a common set of editable parameters (coordinates,
angles and more)
  that define the object's state along an xsheet timeline. These parameters are
in the form of
  animatable TDoubleParam instances and can be accessed directly through the
getParam() method.
  \n\n
  A notable responsibility of the TStageObject class is that of building the
keyframes data that
  will be shown in xsheet viewes (the key icons). The object's keyframes are
built and cached
  each time one of its associated parameters is changed.
  \n\n
  Furthermore, a TStageObject instance is typically a node in a tree-like
hierarchycal structure
  of class TStageObjectTree which is used to define relative object placement
between objects.

\par Creation

  Client code should access TStageObject instances through its TXsheet owner,
like:

  \code

  TStageObjectId objectId = TApp::instance()->getCurrentObject()->getObjectId();
  TStageObject* stageObject = TApp::instance()->getCurrentXsheet()
    ->getXsheet()->getStageObject(objectId);

  \endcode

  The actual creation of new TStageObject instances is currently a
responsibility of the
  TStageObjectTree class.
*/

class DVAPI TStageObject final : public TSmartObject, public TParamObserver {
  DECLARE_CLASS_CODE

public:
  /*!
Used to describe the object status - ie how the object can move.
The default value is XY.
*/
  enum Status {
    XY,            //!< The object can move freely on a plane
    PATH     = 1,  //!< The movement take place on a spline
    PATH_AIM = 2,  //!< The movement take place on a spline with the orientation
                   //! of the object
    //!< defined by the user through \b TStageObject::T_Angle and the total
    //! angle is
    //!< the sum of the user angle plus the angle formed by
    //!< the tangent at the curve in the actual point
    IK = 3,  //!< The object is translated according to inverse kinematics
    PATH_UPPK = 5,  //!< PATH + UPPK (Update Pos Path Keyframe, when the spline
                    //! is changed; see TStageObjectSpline)
    PATH_AIM_UPPK = 6,  //!< PATH_AIM + UPPK
    UPPK_MASK     = 4,  //!< (status&UPPK_MASK)!=0 <==> UPPK enabled
    STATUS_MASK   = 3   //!< (status&STATUS_MASK) => old status
  };

  //! Used to describe object parameters.
  enum Channel {
    T_Angle,   //!< Object's angle (rotation)
    T_X,       //!< Local x coordinate in inches
    T_Y,       //!< Local y coordinate in inches
    T_Z,       //!< Layer depth
    T_SO,      //!< Stacking Order
    T_ScaleX,  //!< X-axis scale factor
    T_ScaleY,  //!< Y-axis scale factor
    T_Scale,
    T_Path,  //!< point position along the spline, as a percentage of the length
             //! of the spline
    T_ShearX,  //!< Shear along x-axis
    T_ShearY,  //!< Shear along y-axis
    T_ChannelCount
  };

  /*!
The Keyframe struct define a keyframe.
A keyframe allows to define object values in a specific frame.
   Keyframes are the starting and ending points of a transition in a scene.
   Its parameters are taken as references to make changes on the paramters of
other frames,
   as:

\li interpolation of parameters between two keyframes in the timeline,
\li extrapolation of parameters after or before a single keyframe.
*/
  struct Keyframe;

  /*!
   Define a map of int and keyframe necessary to know object keyframe;
   Its purpose is the indexing of the object's keyframes.
*/
  typedef std::map<int, Keyframe> KeyframeMap;

public:
  /*! Initializes a stage object in his \e tree with an unique id.
\n The constructor is called by \b TStageObjectTree  to create a new column in
the tree or
  in general a new node in the tree.
*/
  TStageObject(TStageObjectTree *tree, TStageObjectId id);
  /*!
  This is the destructor.
*/
  ~TStageObject();

  //! Retrieves the stage object's id.
  TStageObjectId getId() const;

  //! Sets the object's id.
  void setId(const TStageObjectId &id) {
    if (id != TStageObjectId::NoneId) m_id = id;
  }

  void setName(const std::string &name);
  std::string getName() const;

  //! Returns the id's full name, ie the name of the object with its id
  //! appended.
  std::string getFullName() const;

  //! Returns whether this object is already in use.
  bool isOpened() const { return m_isOpened; }

  //! Sets the usage status of the object.
  void setIsOpened(bool value) { m_isOpened = value; }

  //! Returns whether the object has been locked (can't move).
  bool isLocked() const { return m_locked; }

  //! Locks the object.
  void lock(bool on) { m_locked = on; }

  // used by Inverse Kinematics
  TPinnedRangeSet *getPinnedRangeSet() const { return m_pinnedRangeSet; }

  //! Sets the object's parent.
  void setParent(const TStageObjectId &parentId);

  //! Returns the \a id of the object's parent. Use the owner xsheet to access
  //! the parent object.
  TStageObjectId getParent() const;

  //! Returns the motion path associated to the object
  TStageObjectSpline *getSpline() const;

  /*!
   Sets the motion path of the object.
   A \e null pointer argument make \e this an object without a path and set the
object status to  TStageObject::XY,
that is it can move on a palne.
   \e this gets ownership of \e spline.
\note does NOT take spline ownership
*/
  void setSpline(TStageObjectSpline *spline);

  /*!
As setSpline(), but doesn't notify;
actually setSpline() calls doSetSpline() and then notifies
\note does NOT take spline ownership
*/
  void doSetSpline(TStageObjectSpline *spline);

  //! Returns whether stageObject is ancestor of this.
  bool isAncestor(TStageObject *stageObject) const;

  //! Sets the object's status.
  void setStatus(Status status);

  //! Returns the object's status.
  Status getStatus() const { return m_status; }

  //! helper functions to manipulate status
  inline bool isAimEnabled() const {
    return (m_status & STATUS_MASK) == PATH_AIM;
  }
  inline bool isPathEnabled() const {
    return (m_status & STATUS_MASK) == PATH_AIM ||
           (m_status & STATUS_MASK) == PATH;
  }
  inline bool isUppkEnabled() const { return (m_status & UPPK_MASK) != 0; }
  void enableAim(bool enabled);
  void enablePath(bool enabled);
  void enableUppk(bool enabled);

  //! Returns object's position in the schematic view.
  TPointD getDagNodePos() const { return m_dagNodePos; }

  /*!
   Sets the position of the object node object in the widget work area.
This function is provided for status saving purpose of the interface that
manages the schematic's view.
*/
  void setDagNodePos(const TPointD &p) { m_dagNodePos = p; }

  /*!
Returns string containing pegbar handle name like  A,B...Z, or H1,H2...H8.
\n It refers to the handle of the actual pegbar, ie the offset center of the
pegbar.
The default value is B that centers the pegbar and corresponds to an hook with
index equal to 0.
Hooks are column references (0 is the center) in the sheet and are data
structures defined in the file hook.h.
\n H1 has an index equal to 1, H2 => 2 and so on.
\sa setHandle() and getParentHandle()
*/
  const std::string &getHandle() const { return m_handle; }

  /*!
Return string containing pegbar parent handle name. Es.: A,B...Z, or H1,H2...H8.
\sa setParentHandle() and getHandle()
*/
  const std::string &getParentHandle() const { return m_parentHandle; }

  /*!
Sets the name of the pegbar handle name.
An empty or an H1,...,H8  string sets the offset to the actual center of the
object.
\sa getHandle()
*/
  void setHandle(const std::string &s);

  //! Sets the name of the pegbar parent handle name.
  void setParentHandle(const std::string &s);

  //! Sets the channel \e type to the value \e val
  void setParam(Channel type, double frame, double val);

  //! Returns the \b TStageObject::Channel value associated with the \e frame.
  double getParam(Channel type, double frame) const;

  //! Sets the center of the \e frame to \e center.
  void setCenter(double frame, const TPointD &center);

  //! Returns the center of the \e frame.
  TPointD getCenter(double frame) const;

  //! Returns the offset of the object. \sa  setOffset().
  TPointD getOffset() const;

  //! Sets the offset of the object. \sa  getOffset().
  void setOffset(const TPointD &off);

  // n.b. accesso diretto alla variabili interne center e offset. serve per
  // undo/redo
  void getCenterAndOffset(TPointD &center, TPointD &offset) const;
  void setCenterAndOffset(const TPointD &center, const TPointD &offset);

  //! Returns true if \e frame is a keyframe. \sa TStageObject::Keyframe
  bool isKeyframe(int frame) const;

  //! Returns true if \e frame is a keyframe for all channels.
  bool isFullKeyframe(int frame) const;

  //! Returns true if \e frame is a keyframe for all channels with exeption of
  //! global scale channel.
  bool is52FullKeyframe(int frame) const;

  /*!
Retrieves from the list of the keyframes a keyframe object
(TStageObject::Keyframe) associated
with the frame.
*/
  Keyframe getKeyframe(int frame) const;

  void setKeyframeWithoutUndo(int frame, const Keyframe &);
  void setKeyframeWithoutUndo(int frame);
  void removeKeyframeWithoutUndo(int frame);

  /*!
  Moves Keyframe \e src to \e dst. All the Channels associated are copied and
  the reference to \e src is deleted.
  Returns \e true on success.
*/
  bool moveKeyframe(int dst, int src);

  /*
  This function checks if a list of keyframes can be moved without deleting an
  existing keyframe.
*/
  bool canMoveKeyframes(std::set<int> &frames, int delta);

  /*
  Translates the list \e frames of \e delta frames. Returns \e true on success.
*/
  bool moveKeyframes(std::set<int> &frames, int delta);

  /** Gets a range of keyframes */
  void getKeyframes(KeyframeMap &keyframes) const;
  bool getKeyframeRange(int &r0, int &r1) const;

  bool getKeyframeSpan(int row, int &r0, double &ease0, int &r1,
                       double &ease1) const;

  TAffine getPlacement(double t);
  TAffine getParentPlacement(double t) const;

  /*!
Returns the object's depth at specified frame.
\sa Methods getGlobalNoScaleZ() and getNoScaleZ().
*/
  double getZ(double frame);

  //!	Returns the object's stacking order at specified frame.
  double getSO(double frame);

  //! Returns the absolute depth with no scale factor.
  double getGlobalNoScaleZ() const;

  //! This method returns the original depth of the pegbar.
  double getNoScaleZ() const;

  /*!
Returns the depth with no scale factor.
Movig along Z axis results in a scale factor on the plane XY.
This method sets the original depth of the pegbar.
\n \sa getGlobalNoScaleZ().
*/
  void setNoScaleZ(double noScaleZ);

  //! Returns the stored skeleton deformation, if any
  PlasticSkeletonDeformationP getPlasticSkeletonDeformation() const;

  //! Stores the specified skeleton deformation
  void setPlasticSkeletonDeformation(const PlasticSkeletonDeformationP &sd);

  //! Returns whether the object has children.
  bool hasChildren() const { return !m_children.empty(); }
  const std::list<TStageObject *> &getChildren() const { return m_children; }

  /*!
Returns \e true if the objects cycle is enabled,
i.e. if a finite sequence is a cycle.
*/
  bool isCycleEnabled() const;

  /*!
Enables cycle of the object, this method is provided to take care
of the position of the viewer in the sequence.
\sa isCycleEnabled()
*/
  void enableCycle(bool on);

  /*!
Gets local placement of the object taking care of translation, rotation, shear
and scale
of the \e frame
*/
  TAffine getLocalPlacement(double frame) {
    return computeLocalPlacement(frame);
  }

  //! Returns the \e channel's value of the object.
  TDoubleParam *getParam(Channel channel) const;

  //! Copies the data of the object in a new object with a new id and adds it to
  //! the tree.
  TStageObject *clone();

  //! Returns a new data-structure filled with the actual data of the object.
  TStageObjectParams *getParams() const;

  //! Sets the object's values to \e src values and update the object.
  void assignParams(const TStageObjectParams *src,
                    bool doParametersClone = true);

  //! Returns a pointer to the actual camera.
  TCamera *getCamera() { return m_camera; }
  const TCamera *getCamera() const { return m_camera; }

  /*!
Removes this object from the list of children of his actual parent.
Doesn't delete \e this.
*/
  void detachFromParent();

  //! Makes all children of \e this childern of \e parentId.
  void attachChildrenToParent(const TStageObjectId &parentId);

  //! Resets the area position setting internal time of the object and of all
  //! his children to -1.
  void invalidate();

  /*!
Return true if current object is visible, if object position in z-axis is
less than 1000; othrwise return false.
  \n Set \b TAffine \b aff to current object trasformation matrix computed
  using camera and object trasformation matrix,
\b cameraAff, \b objectAff, and using camera and object position in z-axis,
\b cameraZ, \b objectZ.
\n \b objectZ default value is zero, big value describe the nearest camera
object;
original distance between table and camera is 1000.
\n \b cameraZ default value is zero, negative value suggest that camera is near
to
the table, camera is in the table for cameraZ = -1000.
*/
  static bool perspective(TAffine &aff, const TAffine &cameraAff,
                          double cameraZ, const TAffine &objectAff,
                          double objectZ, double objectNoScaleZ);

  void loadData(TIStream &is);
  void saveData(TOStream &os);

  // Group management
  int setGroupId(int value);
  void setGroupId(int value, int position);
  int getGroupId();
  QStack<int> getGroupIdStack();
  void removeGroupId(int position);
  int removeGroupId();

  bool isGrouped();
  bool isContainedInGroup(int groupId);

  void setGroupName(const std::wstring &name, int position = -1);
  std::wstring getGroupName(bool fromEditor);
  QStack<std::wstring> getGroupNameStack();
  int removeGroupName(bool fromEditor);
  void removeGroupName(int position = -1);

  void removeFromAllGroup();

  void editGroup();
  bool isGroupEditing();
  void closeEditingGroup(int groupId);
  int getEditingGroupId();
  std::wstring getEditingGroupName();

  void updateKeyframes();  //!< Rebuilds the internal keyframes table.

  double paramsTime(
      double t) const;  //!< Traduces the xsheet-time into the one suitable
  //!< for param values extraction (dealing with repeat/cycling)

private:
  // Lazily updated data

  struct LazyData {
    KeyframeMap m_keyframes;
    double m_time;

    LazyData();
  };

private:
  tcg::invalidable<LazyData> m_lazyData;

  TStageObjectId m_id;
  TStageObjectTree *m_tree;
  TStageObject *m_parent;
  std::list<TStageObject *> m_children;

  bool m_cycleEnabled;

  TAffine m_localPlacement;
  TAffine m_absPlacement;

  TStageObjectSpline *m_spline;
  Status m_status;

  TDoubleParamP m_x, m_y, m_z, m_so, m_rot, m_scalex, m_scaley, m_scale,
      m_posPath, m_shearx, m_sheary;

  PlasticSkeletonDeformationP
      m_skeletonDeformation;  //!< Deformation curves for a plastic skeleton

  TPointD m_center;
  TPointD m_offset;

  // ----For Inverse Kinematics interpolation
  TPinnedRangeSet *m_pinnedRangeSet;

  TAffine computeIkRootOffset(int t);
  int m_ikflag;
  // ----

  double m_noScaleZ;

  std::string m_name;
  TPointD m_dagNodePos;
  bool m_isOpened;
  std::string m_handle, m_parentHandle;
  TCamera *m_camera;
  QStack<int> m_groupId;
  QStack<std::wstring> m_groupName;
  int m_groupSelector;

  bool m_locked;

private:
  // Not copyable
  TStageObject(const TStageObject &);
  TStageObject &operator=(const TStageObject &);

  TPointD getHandlePos(std::string handle, int row) const;
  TAffine computeLocalPlacement(double frame);
  TStageObject *findRoot(double frame) const;
  TStageObject *getPinnedDescendant(int frame);

private:
  // Lazy data-related functions

  LazyData &lazyData();
  const LazyData &lazyData() const;

  void update(LazyData &ld) const;

  void invalidate(LazyData &ld) const;
  void updateKeyframes(LazyData &ld) const;

  void onChange(const class TParamChange &c) override;
};

//-----------------------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TStageObject>;
#endif

typedef TSmartPointerT<TStageObject> TStageObjectP;

//=============================================================================

// TODO: togliere questo include da qui.
// Bisogna: includere qui il contenuto del file e eliminare
// tstageobjectkeyframe.h
// oppure creare un altro file con la dichiarazione di TStageObjectParams
#include "tstageobjectkeyframe.h"

//=============================================================================
//
// TStageObjectParams
//
//=============================================================================
class DVAPI TStageObjectParams {
public:
  TStageObjectId m_id, m_parentId;
  std::vector<TStageObjectId> m_children;
  std::map<int, TStageObject::Keyframe> m_keyframes;
  bool m_cycleEnabled, m_isOpened;
  TStageObjectSpline *m_spline;
  TStageObject::Status m_status;
  std::string m_handle, m_parentHandle;
  TPinnedRangeSet *m_pinnedRangeSet;
  TDoubleParamP m_x, m_y, m_z, m_so, m_rot, m_scalex, m_scaley, m_scale,
      m_posPath, m_shearx, m_sheary;
  PlasticSkeletonDeformationP
      m_skeletonDeformation;  //!< Deformation curves for a plastic skeleton
  double m_noScaleZ;
  TPointD m_center, m_offset;
  std::string m_name;

  TStageObjectParams();
  TStageObjectParams(TStageObjectParams *data);
  ~TStageObjectParams();
  TStageObjectParams *clone();
};

#endif
