
	Toonz Shader Fxs Manual

==========================================================

 1. Introduction


Toonz 7.1 allows users to write new Fxs using GLSL (the
OpenGL Shading Language).

Shader Fx interfaces are read once at Toonz's startup,
but the underlying fx algorithm can be modified in
real time to ease the fx creation process.


Users reading these notes for the first time may want to
refer to the official GLSL guide at:

  http://www.opengl.org/documentation/glsl/

Up-and-running examples of GLSL (fragment) shader programs
can be found at the GLSL sanbox gallery, from which some of
the provided examples are adapted from (requires a
WebGL-compatible web browser, such as Firefox or Google
Chrome):

  http://glsl.heroku.com/

Further examples can be found at the beautiful gallery at:

  https://www.shadertoy.com/

==========================================================

 2. Requirements


The most recent version of your graphics drivers, as well
as a fairly recent graphics card.

Specifically, graphics drivers must support OpenGL 2.1,
Transform Feedback and Pixel Buffers (either as a built-in
feature or through extensions).

==========================================================

 3. Limitations


Shader fxs are rendered on the GPU, meaning that they are
typically executed in a massively parallel fashion - ie fast.

However, since most systems only adopt one GPU, only one
Shader fx is allowed to be rendered at the same time.
This means that Shader Fxs do not take advantage of multiple
rendering threads in a Toonz rendering process like common
CPU-based fxs do.


Shader Fx are intended to apply a fragment shader on the
output surface for the fx. In other words, each output pixel
is processed separately using the supplied fragment shader
program.

This prevents the implementation of more complex output
patterns that span multiple pixels at the same time.

Furthermore, there is no way to specify intermediate buffer
objects to read or write data to - which is often a common
need when writing fxs.

==========================================================

 3. Implementing a Shader Fx


In order to implement a shader fx it's currently necessary
to either create or edit the following files:

  a. <Toonz Stuff Path>/config/current.txt

	This file hosts the associations between fxs and
	their parameters and the names displayed in the GUI
	(which are not locale-dependent).

  b. <Toonz Stuff Path>/profiles/layouts/fxs/fxs.lst

	The list of fxs as displayed in the right-click
	contextual menus like "Add Fx" or "Insert Fx"

  c. <Toonz Stuff Path>/profiles/layouts/fxs/<Your Shader Fx Name>.xml

	Parameters tabbing in the Fx Parameters Editor

  d. <Toonz Library Path>/shaders/<Your Shader Fx Name>.xml

	The Shader Fx interface.

  e. The actual shader program files


Please, observe that the paths and names outside brackets
are mandatory.

Apart from point (d) and (e) discussed separately, it is best
to locate existing entries and emulate their behavior.
You can typically find related entries by searching "Shader"
in each file.

==========================================================

 4. The Shader Interface File


The Shader Fx Interface file at (3.d) is an xml document that
defines the main properties of the fx.

Specifically:

  a. Shader program files to be compiled at run-time

  b. Input ports for the fx

  c. Parameters

  d. Restrictions to the class of world/output coordinates
     transforms handled by the fx

The file is read once when Toonz starts, so any modification
will not be recognized until Toonz is restarted.


The complete recognized file structure is as follows:


<MainProgram>                       // (4.a) The applied fragment shader
  <Name>
    SHADER_myShaderName             // Internal name of the fx (mandatory, a simple app-unique literal id)
  </Name>
  <ProgramFile>
    "programs/myShader.frag"        // The shader program file (3.e), relative to the
  </ProgramFile>                    // path of the interface file.
</MainProgram>

<InputPorts>                        // (4.b) - Only a *fixed* number of ports allowed
  <InputPort>                       // A first port
    "Source"                        // The displayed port name
  </InputPort>

  <InputPort>                       // Second port
    "Control"
  </InputPort>
  
  <PortsProgram>                    // (4.a) Vertex shader used to acquire the geometry of
    <Name>                          // input images. See (5.b).
      SHADER_myShader_ports         // The unique id for the vertex shader program (mandatory)
    </Name>
    <ProgramFile>
      "programs/myShader_ports.vert"
    </ProgramFile>
  </PortsProgram>
</InputPorts>

<BBoxProgram>                       // (4.a) Vertex shader used to calculate the fx's bbox.
  <Name>                            //  See (5.c).
    SHADER_myShader_bbox
  </Name>
  <ProgramFile>
    "programs/myShader_bbox.vert"
  </ProgramFile>
</BBoxProgram>

<HandledWorldTransforms>            // (4.d) Optional, see (5.a)
  isotropic                         // May be either 'any' (default) or 'isotropic'.
</HandledWorldTransforms>           // Isotropic transforms exclude shears and non-uniform scales.

<Parameters>                        // (4.c)
  <Parameter>

    float radius                    // Parameter declaration

    <Default>                       // Additional Paramater attributes (can be omitted)
      10                            // The parameter default
    </Default>
    <Range>
      0 20                          // The parameter range
    </Range>
    <Concept>
      length                        // The parameter concept type - or, how it is represented
    </Concept>                      // by the Toonz GUI
  </Parameter>

  <Parameter>

    float angle

    <Concept>
      angle_ui                      // Concepts of type <concept type>_ui are editable in
      <Name>                        // camera stand
        "My Angle"
      </Name>
    </Concept>
  </Parameter>
</Parameters>

<Concept>                           // Composite parameter concepts can be formed by 2 or
                                    // more parameters
  polar_ui

  <Name>
    "My Polar Coordinates"
  </Name>
  <Parameter>                       // List of involved parameters
    radius
  </Parameter>
  <Parameter>
    angle
  </Parameter>
</Concept>

----------------------------------------------------------

4.1. Parameter Declarations


Parameters are introduced by a declaration typically matching
the corresponding GLSL variable declaration.

The complete recognized list of supported parameter types is:

  bool, float, vec2, int, ivec2, rgb, rgba


The 'rgb' and 'rgba' types map to GLSL 'vec3' and 'vec4'
variables respectively, but are displayed with the appropriate
color editors by Toonz - plus, the range of their components
automatically maps from [0, 255] in Toonz and the Shader
Interface file to [0.0, 1.0] in the corresponding shader program
files.

----------------------------------------------------------

4.2. Parameter Concepts


Parameter 'concepts' are additional parameter properties that
regard the way Toonz represents a certain parameter type.

For example, a 'float' variable type may either indicate
an angle, the length of a segment, a percentage value,
and more.

Fx writers may want to explicitly specify a parameter concept
for the following reasons:

  a. Impose a measure to the parameter (e.g. degress, inches, %)

  b. Make the parameter editable in camera-stand


The complete list of supported parameter concepts is the following:

  percent            - Displayed with the percentage '%' unit

  length             - Displayed in length units (inches, mm, cm, etc..)

  angle              - Displayed in angular units 'º'

  point              - A vec2 displayed in length units

  radius_ui          - Like length, displaying a radius in camstand. May compose with a point (the center)

  width_ui           - Like length, displaying a vertical line width. May compose with the line's angle.

  angle_ui           - Like angle, displaying it in camstand

  point_ui           - Like point, in camstand

  xy_ui              - Composes two float types in a point

  vector_ui          - Composes two float types in an 'arrow'-like vector

  polar_ui           - Like vector_ui, from a length and an angle

  size_ui            - Displays a square indicating a size. May compose width and height in a rect.

  quad_ui            - Composes 4 points in a quadrilateral

  rect_ui            - Composes width, height, and the optional center point in a rect

==========================================================

 5. Shader program files


A shader program file is a simple text file containing the
actual algorithms of a shader fx.

In the current implementation of Toonz Shader Fxs, there are
3 possible shader program files that need to be specified:

  a. The main fragment shader program, responsible of
     executing the code that actually renders the fx

  b. An optional vertex shader program to calculate the 
     geometries of contents required from input ports

  c. An optional vertex shader program to calculate the
     bounding box of the fx output

----------------------------------------------------------

 5.a. The 'MainProgram' Fragment Shader


The main program is in practice a standard GLSL fragment
shader - however, Toonz will provide it a set of additional
uniform input variables that must be addressed to correctly
compute the desired output.


The complete list of additional variables always supplied
by Toonz is:

  uniform mat3    worldToOutput;
  uniform mat3    outputToWorld;

These matrix variables describe the affine transforms mapping
output coordinates to Toonz's world coordinates, and vice-versa.

They include an additional coordinate as an OpenGL version-portable
way to perform translations by natural multiplication - transforming
a point is then done like:

  vec2 worldPoint = (outputToWorld * vec3(outPoint, 1.0)).xy

Fx parameters are typically intended in world coordinates,
and should be adjusted through these transforms - for example,
a camstand-displayed radius value must be multiplied by the
'worldToOutput' scale factors in order to get the corresponding
value in output coordinates.

World/Output transforms may be restricted to a specific sub-class
of affine transforms by specifying so in the Shader Interface File.

Restricting to isotropic transforms may be useful to simplify
cases where angular values are taken into account, since this
transforms class preserves angles by allowing only uniform scales,
rotations and translations. Non-uniform scales and shears are
later applied by Toonz on the produced fx output if necessary.


In case input ports have been specified, we also have:

  uniform sampler2D inputImage[n];
  uniform mat3      outputToInput[n];
  uniform mat3      inputToOutput[n];

The sampler variables correspond to the input content to the
fx. The matrix variables are the reference transforms from
output to input variables, and vice-versa.


Additional uniform variables corresponding to fx parameters
will also be supplied by Toonz. For example, if a "float radius"
parameter was specified, a corresponding

  uniform float radius;

input variable will be provided to the program.


WARNING: Toonz requires that *output* colors must be 
         'premultiplied' - that is, common RGB components
         (in the range [0, 1]) must be stored multiplied
         by their alpha component.

----------------------------------------------------------

 5.b. The optional 'PortsProgram' Vertex Shader


The shader program (b) is required in case an fx specifies
input ports, AND it needs to calculate some input content
in a different region than the required output.
It can be neglected otherwise.

For example, a blur fx requires that input contents outside
the required output rectangle are 'blurred in' it.


The 'PortsProgram' vertex shader is a one-shot shader
run by Toonz on a single dummy vertex - which uses
OpenGL 3.0's "Transform Feedback" extension to return a
set of predefined 'varying' output variables


The complete set of variables supplied by Toonz and required
in output by the program is:

  uniform mat3    worldToOutput;
  uniform mat3    outputToWorld;
  uniform vec4    outputRect;

  varying vec4    inputRect[portsCount];
  varying vec4    worldToInput[portsCount];

The transforms are intended in the same way as (5.a).

The outputRect and inputRect[] variables store the
(left, bottom, right, top) rect components in output
and input coordinates respectively.

Parameter input variables are obviously also supplied.


WARNING: *All* the required output variables must be
	 declared AND filled with values.

         There is no recognized default for them, and the
         fx will (silently) fail to render if some are not
         assigned.

----------------------------------------------------------

 5.c. The optional 'BBoxProgram' Vertex Shader


Some fx may be able to restrict their opaque renderable
area inside a rect.

For example, blurring an image will 'blur out' the image
content by the specified blur radius. Beyond that, the fx
will render full transparent pixels. Thus, the bounding
box of the fx in this case will be calculated as the
input bounding box, enlarged by the blur radius.

The default output bounding box is assumed to be infinite;
if that is the case, the BBoxProgram can be omitted.


Fx writers may want to supply an explicit program to
calculate the bounding box of the fx, given its input
bounding boxes. This is be useful in Toonz's rendering
pipeline because the software is then allowed to
restrict memory allocation (and fxs calculations)
for the output image to said output bounding box, resulting
in less memory consumption and increased speed.


The complete set of variables supplied by Toonz and required
in output by the program is:

  uniform vec4 infiniteRect;
  uniform vec4 inputBBox[portsCount];

  varying vec4 outputBBox;

The infiniteRect variable should be used to identify both
input and output infinite bboxes.

