#pragma once

#ifndef __TENV_H__
#define __TENV_H__

#include "toonz4.6\tmacro.h"
#include "toonz4.6\avl.h"

#undef TNZAPI
#ifdef TNZ_IS_COMMONLIB
#define TNZAPI TNZ_EXPORT_API
#else
#define TNZAPI TNZ_IMPORT_API
#endif

/* Prototypes */

/*
 *  Suffissi:
 *
 *    _s : string
 *    _i : int
 *    _c : char
 *    _d : double
 *    _r : rational (stringa;e.s: per l'aspect ratio "4/3")
 *
 * n.b. non bisogna fare free() delle stringhe ritornate da tenv_get_var_s()
 *
 */

/*
 *  La lista completa delle variabili di setup e del tipo a cui sono
 *  associate e' mostrato in fondo al file.
 */

/* Set delle variabili (fanno prima un check)
 * Ritornano FALSE se falliscono
 */

TNZAPI int tenv_set_var_s(char *var_name, char *value);
TNZAPI int tenv_set_var_i(char *var_name, int value);
TNZAPI int tenv_set_var_c(char *var_name, char value);
TNZAPI int tenv_set_var_d(char *var_name, double value);
TNZAPI int tenv_set_var_r(char *var_name, char *value);

/* Get delle variabili
 * Ritornano FALSE se falliscono
 * Non si deve assolutamente liberare nulla !!
 * In fondo a questo file viene spiegato come utilizzare
 * le seguenti funzioni per tutte le variabili di setup.
 */

TNZAPI int tenv_get_var_s(char *var_name, char **value);
TNZAPI int tenv_get_var_i(char *var_name, int *value);
TNZAPI int tenv_get_var_c(char *var_name, char *value);
TNZAPI int tenv_get_var_d(char *var_name, double *value);
TNZAPI int tenv_get_var_r(char *var_name, char **value);

/* Check delle variabili (  range_min <=  value  <= range_max  )
 * Ritornano FALSE se falliscono
 */

TNZAPI int tenv_check_var_s(char *var_name, char *value);
TNZAPI int tenv_check_var_i(char *var_name, int value);
TNZAPI int tenv_check_var_c(char *var_name, char value);
TNZAPI int tenv_check_var_d(char *var_name, double value);
TNZAPI int tenv_check_var_r(char *var_name, char *value);

/* Ritorna il messaggio di errore settato dalla libreria
 *
 */

TNZAPI char *tenv_get_error_string(void);

/* Ritorna il codice di errore settato dalla libreria.
 * I codici di errore hanno il prefisso TENV_ERR_.
 */

int tenv_get_error_code(void);

/* Error Codes */

#define TENV_ERR_VARIABLE_TYPE 1
#define TENV_ERR_OUT_OF_MEMORY 2
#define TENV_ERR_INSERT_VARIABLE 3
#define TENV_ERR_REMOVE_VARIABLE 4
#define TENV_ERR_FIND_VARIABLE 5
#define TENV_ERR_VARIABLE_RANGE 6
#define TENV_ERR_VARIABLE_VALUE 7
#define TENV_ERR_SETUP_NOT_FOUND 8
#define TENV_ERR_SETUP_STAT_FAILED 9
#define TENV_ERR_SETUP_OPEN_FAILED 10
#define TENV_ERR_TOONZROOT_NOT_FOUND 11

#define TENV_ERR_LIST_OPEN_FAILED 20
#define TENV_ERR_LIST_ITEM_EXIST 21
#define TENV_ERR_LIST_UPDATE_FAILED 22
#define TENV_ERR_LIST_DELETE_FAILED 23
#define TENV_ERR_LIST_REMOVE_FAILED 24
#define TENV_ERR_LIST_SET_FAILED 25

/*---------------------------------------------------------------------------*/

/* Enums */

typedef enum {
  TENV_NONE,
  TENV_CHAR,
  TENV_INT,
  TENV_DOUBLE,
  TENV_STRING,
  TENV_STRINGV,
  TENV_RATIONAL
} TENV_TYPES;

typedef struct {
  char *alias_str;
  char *alias_for;
} TENV_ALIAS;

/* Structures */

typedef struct {
  int id;
  TENV_TYPES type;
  char *var_name;
  char *lbl_name;
  double from, to, step, def, cur;
  int prec;
  char *rational_def;
  char *rational_cur;
  char *string;
  int stringv_num;
  int stringv_cur;
  int stringv_def;
  char **stringv;
  char loaded;
  int n_aliases;
  TENV_ALIAS *alias;
} TENV_INFO;

/* Prototypes */

TNZAPI void tenv_init(void);

TNZAPI TENV_INFO *tenv_get_var_info(char *var_name);
TNZAPI TENV_INFO *tenv_get_first_var(void);
TNZAPI TENV_INFO *tenv_get_next_var(void);
TNZAPI int tenv_update_current_values(char *fname);
TNZAPI int tenv_check_for_save(char *fname);
TNZAPI int tenv_write(char *fname);
TNZAPI int tenv_write_def(char *fname);
TNZAPI int tenv_varcmp(TENV_INFO *ptr1, TENV_INFO *ptr2);

TNZAPI int tenv_setups_set(char *setup_name);
TNZAPI int tenv_setups_add(char *setup_name);
TNZAPI int tenv_setups_scan(int (*func)(char *setup_name, void *usr),
                            void *usr);
TNZAPI int tenv_setups_remove(char *setup_name);
TNZAPI int tenv_delete_setups_file(void);
TNZAPI int tenv_setups_does_exist(char *setup_name);
TNZAPI int tenv_setups_update_list(void);
TNZAPI char *tenv_get_filesetup(void);
TNZAPI int tenv_is_database_enabled(void);
TNZAPI void tenv_set_pt_flag(int mode);
TNZAPI int tenv_get_pt_flag(void);
TNZAPI int tenv_check_cosmo(void);
TNZAPI void tenv_refresh(void);
TNZAPI void tenv_set_default_filesetup(void);
TNZAPI void tenv_rgb_update(char *filesetup);

/* Defines */

#define TENV_ID(p) (p)->id

#define TENV_TYPE(p) (p)->type

#define TENV_VAR_NAME(p) (p)->var_name
#define TENV_LBL_NAME(p) (p)->lbl_name

#define TENV_CHAR_FROM(p) (char)(p)->from
#define TENV_CHAR_TO(p) (char)(p)->to
#define TENV_CHAR_STEP(p) (char)(p)->step
#define TENV_CHAR_DEF(p) (char)(p)->def
#define TENV_CHAR_CURR(p) (char)(p)->cur

#define TENV_INT_FROM(p) (int)(p)->from
#define TENV_INT_TO(p) (int)(p)->to
#define TENV_INT_STEP(p) (int)(p)->step
#define TENV_INT_DEF(p) (int)(p)->def
#define TENV_INT_CURR(p) (int)(p)->cur
#define TENV_INT_PREC(p) (int)(p)->prec

#define TENV_DOUBLE_FROM(p) (p)->from
#define TENV_DOUBLE_TO(p) (p)->to
#define TENV_DOUBLE_STEP(p) (p)->step
#define TENV_DOUBLE_DEF(p) (p)->def
#define TENV_DOUBLE_CURR(p) (p)->cur
#define TENV_DOUBLE_PREC(p) (p)->prec

#define TENV_RATIONAL_FROM(p) (p)->from
#define TENV_RATIONAL_TO(p) (p)->to
#define TENV_RATIONAL_STEP(p) (p)->step
#define TENV_RATIONAL_DEF_N(p) (p)->def
#define TENV_RATIONAL_CUR_N(p) (p)->cur
#define TENV_RATIONAL_DEF_S(p) (p)->rational_def
#define TENV_RATIONAL_CUR_S(p) (p)->rational_cur

#define TENV_STRING(p) (p)->string

#define TENV_STRINGV_N(p) (p)->stringv_num
#define TENV_STRINGV_C(p) (p)->stringv_cur
#define TENV_STRINGV_D(p) (p)->stringv_def
#define TENV_STRINGV_I(p, i) (p)->stringv[i]
#define TENV_STRINGV_P(p) (p)->stringv
#define TENV_STRINGV(p) (p)->stringv[TENV_STRINGV_C(p)]

#define TENV_ADD_VAR_CHAR(var_name, lbl_name, from, to, step, def)             \
  tenv_add_node(TENV_CHAR, var_name, lbl_name, from, to, step, def, NULL)

#define TENV_ADD_VAR_INT(var_name, lbl_name, from, to, step, def)              \
  tenv_add_node(TENV_INT, var_name, lbl_name, from, to, step, def, NULL)

#define TENV_ADD_VAR_DOUBLE(var_name, lbl_name, from, to, step, def, prec)     \
  tenv_add_node(TENV_DOUBLE, var_name, lbl_name, from, to, step, def, prec,    \
                NULL)

#define TENV_ADD_VAR_RATIONAL(var_name, lbl_name, from, to, step, def)         \
  tenv_add_node(TENV_RATIONAL, var_name, lbl_name, from, to, step, def, NULL)

#define TENV_ADD_VAR_STRING(var_name, lbl_name, string)                        \
  tenv_add_node(TENV_STRING, var_name, lbl_name, string, NULL)

/* una macro per contare il numero di elementi di un array  */
#define NUMBER_OF_ELEMENTS(T) (sizeof(T) / sizeof(T[0]))

#define TENV_ADD_VAR_STRINGV(var_name, lbl_name, stringv, index_of_def)        \
  tenv_add_node(TENV_STRINGV, var_name, lbl_name, stringv,                     \
                stringv[NUMBER_OF_ELEMENTS(stringv) - 1]                       \
                    ? NUMBER_OF_ELEMENTS(stringv)                              \
                    : NUMBER_OF_ELEMENTS(stringv) - 1,                         \
                index_of_def, NULL)

#define TENV_ADD_VAR_STRINGVV(var_name, lbl_name, stringv, items_num,          \
                              index_of_def)                                    \
  tenv_add_node(TENV_STRINGV, var_name, lbl_name, stringv, items_num,          \
                index_of_def, NULL)

static char Tenv_string_error[2048];
static int Tenv_code_error;
static TREE *Tenv_tree  = NULL;
static int Tenv_pt_flag = FALSE;

static char *Tenv_yes_no[]       = {"yes", "no"};
static char *Tenv_yes_no_u[]     = {"YES", "NO"};
static char *Tenv_status_mode[]  = {"disabled", "enabled"};
static char *Tenv_browser_mode[] = {"show all files", "show significant only"};
static char *Tenv_turn_mode[]    = {"ON", "OFF"};
static char *Tenv_onion_img_mode[] = {"keep loaded", "load when needed",
                                      "disabled"};
static char *Tenv_flash_viewer[] = {"Internal", "System"};
static char *Tenv_work_res[] = {"pal",  "ntsc", "640",      "1K",      "1280",
                                "1.5K", "1840", "1920",     "2K",      "2560",
                                "3K",   "4K",   "cin_half", "cin_full"};
static char *Tenv_camera_prev[] = {"Vertical_Fit(TV)", "Horiz_Fit(Film)"};
static char *Tenv_fc_trasp[]    = {"white", "black"};
static char *Tenv_ddr_connect[] = {"SCSI", "ETHERNET"};
static char *Tenv_default_plt[] = {"32 inks + 128 paints",
                                   "256 inks + 256 paints"};
static char *Tenv_scanner_dpi[]    = {"LOW", "MEDIUM", "HIGH"};
static char *Tenv_scanner_tone[]   = {"B&W", "GREYTONES", "RGB"};
static char *Tenv_scanner_driver[] = {"INTERNAL"
#ifdef WIN32
                                      ,
                                      "TWAIN"
#endif
};
static char *Tenv_north_south[] = {"NORTH", "SOUTH", "NONE"};
static char *Tenv_east_west[]   = {"EAST", "WEST", "NONE"};
static char *Tenv_top_bottom[]  = {"TOP", "BOTTOM"};
static char *Tenv_autocenter[]  = {"using pegholes", "taped pegbar", "OFF"};
static char *Tenv_adjust_mode[] = {"BLACK EQ", "HISTOGRAM", "HISTO-L", "NONE"};
static char *Tenv_compr_mode[]  = {"compressed", "uncompressed"};
static char *Tenv_tz_compr_mode[] = {"Toonz RLE", "LZW (Toonz 4.2 compatible)"};
static char *Tenv_matte_chan[]    = {"write", "do_not_write"};
static char *Tenv_colorstyle[]    = {"gray scale", "full color"};
static char *Tenv_tga_subtypes[] = {"gray scale", "color mapped", "full color"};
static char *Tenv_bmp_subtypes[] = {"black and white", "gray scale",
                                    "color mapped", "full color"};
static char *Tenv_orientation[] = {"RIGHT BOTTOM", "RIGHT TOP",  "LEFT BOTTOM",
                                   "LEFT TOP",     "TOP RIGHT",  "TOP LEFT",
                                   "BOTTOM RIGHT", "BOTTOM LEFT"};
static char *Tenv_rgb_bpp[] = {"8 (gray scale)", "24 (RGB)", "32 (RGBM)",
                               "48 (RGB)", "64 (RGBM)"};
static char *Tenv_mm_rgb_bpp[] = {"32 (RGBM)", "64 (RGBM)"};
static char *Tenv_tga_bpp[]    = {"8 (gray scale)",
                               "8 (fast cmapped-16)",
                               "8 (fast cmapped-24)",
                               "8 (fast cmapped-32)",
                               "8 (smart cmapped-16)",
                               "8 (smart cmapped-24)",
                               "8 (smart cmapped-32)",
                               "16 (RGB)",
                               "24 (RGB)",
                               "32 (RGBM)"};
static char *Tenv_mm_tga_bpp[] = {"8 (fast cmapped-32)", "8 (smart cmapped-32)",
                                  "32 (RGBM)"};
static char *Tenv_tga_bpp_bgt[] = {
    "8 (gray scale)", "8 (cmapped-16)", "8 (cmapped-24)", "8 (cmapped-32)",
    "16 (RGB)",       "24 (RGB)",       "32 (RGBM)"};
static char *Tenv_bmp_bpp[] = {"1 (black and white)", "4 (gray scale)",
                               "8 (gray scale)",      "8 (fast cmapped)",
                               "8 (smart cmapped)",   "24 (RGB)"};
static char *Tenv_bmp_bpp_bgt[] = {"1 (black and white)", "4 (gray scale)",
                                   "8 (gray scale)", "8 (cmapped)", "24 (RGB)"};
static char *Tenv_jpg_bpp[]      = {"8 (gray scale)", "24 (RGB)"};
static char *Tenv_tif_compress[] = {"NONE", "CCITTFAX3", "CCITTFAX4",
                                    "PACKBITS", "LZW"};
static char *Tenv_mm_tif_compress[] = {"NONE", "PACKBITS", "LZW"};
static char *Tenv_tif_bpp[]         = {"1 (black and white)",
                               "1 (white and black)",
                               "8 (gray scale)",
                               "8 (fast cmapped)",
                               "8 (smart cmapped)",
                               "24 (RGB)",
                               "32 (RGBM)",
                               "48 (RGB)",
                               "64 (RGBM)"};
static char *Tenv_mm_tif_bpp[]  = {"32 (RGBM)", "64 (RGBM)"};
static char *Tenv_tif_bpp_bgt[] = {"1 (black and white)",
                                   "1 (white and black)",
                                   "8 (gray scale)",
                                   "8 (cmapped)",
                                   "24 (RGB)",
                                   "32 (RGBM)",
                                   "48 (RGB)",
                                   "64 (RGBM)"};
static char *Tenv_screen_buf[]  = {"singlebuffer", "doublebuffer"};
static char *Tenv_loading_pol[] = {"whole level", "first only", "one every 10",
                                   "one every 4", "one every 2"};
static char *Tenv_unit_system[] = {"inches/degrees", "pulses", "cm/degrees",
                                   "mm/degrees"};
static char *Tenv_field_guide[] = {"cinemascope", "vistavision", "normal",
                                   "hdtv"};
static char *Tenv_frame_count[] = {"by footage", "by number", "by timecode"};

/*
static char *Tenv_col_order   [] = { "right to left", "left to right" };
*/

static char *Tenv_levcol_move[] = {"click&drag", "ALT+click&drag"};

static char *Tenv_numcol_move[] = {"click&drag", "ALT+click&drag"};

static char *Tenv_xsh_pegbar_look[]  = {"detailed", "bounding box"};
static char *Tenv_xsh_load_at_once[] = {"yes", "no"};
static char *Tenv_xsh_column_icon_loading_policy[] = {"immediately",
                                                      "on demand"};
static char *Tenv_xsh_level_names[]    = {"repeat at marks", "first cell only"};
static char *Tenv_xsh_cuts_and_holds[] = {"yes", "no"};
static char *Tenv_subpix_move[]        = {"all", "none", "only_rgb"};
static char *Tenv_bpc_prec[]           = {"32 bit", "64 bit"};
static char *Tenv_resample_t[] = {"standard_quality", "improved_quality",
                                  "high_quality"};
static char *Tenv_ident_img_h[] = {"link_to_previous",
                                   "do_not_link_to_previous"};
static char *Tenv_cosmo_compr[] = {/*"BLACK&WHITE", "GREYTONES",
                                     "256 COLORS", "4096 COLORS", */
                                   "COSMO_PAL", "COSMO_NTSC"};
static char *Tenv_screen_pix[] = {"16 bit", "32 bit"};

static char *Tenv_axis_frame[] = {"frame_on_vertical", "frame_on_horizontal"};

static char *Tenv_transition[] = {"linear", "speedin/out", "easein/out"};

static char *Tenv_buttons_style[] = {"classic", "modern"};

static char *Tenv_color_res[] = {"1 bit", "8 bit", "full color"};
/*---------------------------------------------------------------------------


  Per le seguenti variabili di setup fare:


           {
             char *value;

             tenv_get_var_s(var_name, &value);

             .
             .
             .
           }


    -- General --

    TOONZ_DATA_BASE
    TOONZ_DATA_BASE_STATUS
    TOONZ_STUDIO_PLT
    TOONZ_WORKING_RES
    TOONZ_CAMERA_PREVALENCE
    TOONZ_SCREEN_PIXEL       (solo per WINDOWS NT)
    FULL_COLOR_TRANSPARENCY
    DDR_CONNECTION
    TOONZ_DEFAULT_PLT
    TOONZ_TZUP_COMPRESSION


    -- Input --

    INPUT_PAP_FILE
    INPUT_SCANNER_DPI
    INPUT_SCANNER_TONE
    INPUT_PAPER_FEEDER
    INPUT_FDG_FILE
    INPUT_PEGS
    INPUT_NORTH_SOUTH
    INPUT_EAST_WEST
    INPUT_AUTOPOS
    INPUT_AUTOADJUST
    INPUT_AUTOADJUST_MODE
    INPUT_AUTOCLOSE


    -- Bgtiler --

    BGTILER_RGB_COMPRESSION
    BGTILER_TGA_COMPRESSION
    BGTILER_TGA_SUBTYPES
    BGTILER_TGA_BITS_PER_PIXEL
    BGTILER_TIFF_COMPRESSION
    BGTILER_TIFF_BITS_PER_PIXEL


    -- Pltedit --

    PLT_SCREEN_BUFFERS
    PLTEDIT_COLOR_CHIPS_NUM


    -- Inknpaint --

    INKNPAINT_UNDO
    INKNP_COLOR_CHIPS_NUM
    INKNPAINT_ONION_SKIN_IMAGE


    -- Xsheet --

    XSHEET_DEFAULT_LOADING_POLICY
    XSHEET_DEFAULT_OUTPUT_IMF
    XSHEET_UNITS_SYSTEM
    XSHEET_FIELD_GUIDE_TYPE
    XSHEET_GRAPH_AXIS_FRAME
    FRAME_COUNTER
    COLUMN_ORDER


    -- Casm --

    CASM_SUBPIXEL_MOVE
    CASM_RESAMPLE_TYPE
    IDENTICAL_IMAGES_HANDLER
    CASM_RGB_SUBTYPE
    CASM_RGB_WRITE_MATTE
    CASM_TGA_COMPRESSION
    CASM_TGA_SUBTYPES
    CASM_TGA_BITS_PER_PIXEL
    CASM_TIFF_ORIENTATION
    CASM_TIFF_COMPRESSION
    CASM_TIFF_BITS_PER_PIXEL


    -- Eth rec --

    TOONZ_RECDEV_NET
    TOONZ_REMOTE_DDR_USER


    -- Flip --

    AUTO_SCREEN_FIT
    COSMO_CAMERA_DEFAULT
    COMPRESSION_DEFAULT
    LOAD_FULL_COLOR_LEVELS
    FILL_UNPAINTED_LEVELS





                ****************************************




  Per le seguenti variabili di setup fare:


           {
             int value;

             tenv_get_var_i(var_name, &value);

             .
             .
             .
           }



   -- General --

   TOONZ_FRAMERATE

   -- Input

   INPUT_SCANNER_BRIGHTNESS
   INPUT_SCANNER_CONTRAST
   INPUT_SCANNER_THRESHOLD
   INPUT_SHARPNESS
   INPUT_PROCESS_BRIGHTNESS
   INPUT_PROCESS_CONTRAST

   -- Inknpaint --

   TOONZ_FILL_DEPTH
   TOONZ_SHIFT_FILL_DEPTH
   RECTACLOSE_DISTANCE
   RECTACLOSE_ANGLE
   RECTACLOSE_USED_INK
   RECTACLOSE_INK_OPACITY


   -- Xsheet --

   XSHEET_DEFAULT_CAMERA_FIELD
   XSHEET_DEFAULT_FIELD_GUIDE_SIZE
   XSHEET_DEFAULT_CAMERA_XRES
   XSHEET_DEFAULT_CLC_RED
   XSHEET_DEFAULT_CLC_GREEN
   XSHEET_DEFAULT_CLC_BLUE
   XSHEET_DEFAULT_CLC_MATTE
   XSHEET_DEFAULT_SUBSAMPLING
   XSHEET_DEFAULT_BG_SUBSAMPLING
   XSHEET_MARKER




                ****************************************




  Per le seguenti variabili di setup fare:


           {
             double value;

             tenv_get_var_d(var_name, &value);

             .
             .
             .
           }




  -- Input --

  INPUT_ASPECT_RATIO
  INPUT_FIELD_SIZE
  INPUT_NORTH_SOUTH_SHIFT
  INPUT_EAST_WEST_SHIFT


  -- Xsheet --

  XSHEET_DEFAULT_CAMERA_AR


  -- Casm --

  CASM_RENDER_TILE
  CASM_MEMORY_CHUNK




---------------------------------------------------------------------------*/

#endif /* __TENV_H__ */
