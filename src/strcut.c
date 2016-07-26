/**
*  @file
*  strcut - a Max object to cut a string in two
*  
*  Originally by Jan Schacher
*
*  Refactored by Yves Candau to use:
*    - the new style Max object,
*    - dynamic strings,
*    - attributes.
*/

/****************************************************************
*  Header files
*/
#include "ext.h"
#include "ext_obex.h"
#include "dstring.h"

/****************************************************************
*  Preprocessor
*/

/****************************************************************
*  Max object structure
*/
typedef struct _strcut
{
  t_object obj;

  void *outl_any1;
  void *outl_any2;

  t_dstr i_dstr;
  long   i_pos;

  t_dstr    o_dstr1;
  t_dstr    o_dstr2;
  t_symbol *o_sym1;
  t_symbol *o_sym2;

  long  mode;
  long  fprecision;
  char  format[6];

} t_strcut;

/****************************************************************
*  Global class pointer
*/
static t_class *strcut_class = NULL;

/****************************************************************
*  Function declarations
*/
void *strcut_new      (t_symbol *sym, long argc, t_atom *argv);
void  strcut_free     (t_strcut *x);
void  strcut_assist   (t_strcut *x, void *b, long msg, long arg, char *dst);

void  strcut_bang     (t_strcut *x);
void  strcut_int      (t_strcut *x, t_atom_long n);
void  strcut_in1      (t_strcut *x, t_atom_long n);
void  strcut_float    (t_strcut *x, double f);
void  strcut_list     (t_strcut *x, t_symbol *sym, long argc, t_atom *argv);
void  strcut_anything (t_strcut *x, t_symbol *sym, long argc, t_atom *argv);
void  strcut_set      (t_strcut *x, t_symbol *sym, long argc, t_atom *argv);
void  strcut_post     (t_strcut *x);

void  strcut_action   (t_strcut *x);
void  strcut_output   (t_strcut *x);

t_dstr    str_cat_atom       (t_strcut *x, t_dstr dstr, t_atom *atom);
t_dstr    str_cat_args       (t_strcut *x, t_dstr dstr, long argc, t_atom *argv);
t_max_err str_fprecision_set (t_strcut *x, void *attr, long argc, t_atom *argv);

/****************************************************************
*  Initialization
*/
void ext_main(void *r)
{
  t_class *c;

  c = class_new("strcut",
    (method)strcut_new,
    (method)strcut_free,
    (long)sizeof(t_strcut),
    (method)NULL,
    A_GIMME, 0);

  class_addmethod(c, (method)strcut_assist,   "assist",    A_CANT,  0);
  class_addmethod(c, (method)strcut_bang,     "bang",               0);
  class_addmethod(c, (method)strcut_int,      "int",       A_LONG,  0);
  class_addmethod(c, (method)strcut_in1,      "in1",       A_LONG,  0);
  class_addmethod(c, (method)strcut_float,    "float",     A_FLOAT, 0);
  class_addmethod(c, (method)strcut_list,     "list",      A_GIMME, 0);
  class_addmethod(c, (method)strcut_anything, "anything",  A_GIMME, 0);
  class_addmethod(c, (method)strcut_set,      "set",       A_GIMME, 0);
  class_addmethod(c, (method)strcut_post,     "post",               0);
  class_addmethod(c, (method)stdinletinfo,    "inletinfo", A_CANT,  0);

  CLASS_ATTR_LONG(c, "mode", 0, t_strcut, mode);
  CLASS_ATTR_ORDER(c, "mode", 0, "1");            // order
  CLASS_ATTR_LABEL(c, "mode", 0, "mode");         // label
  CLASS_ATTR_FILTER_CLIP(c, "mode", 0, 1);        // min-max filter
  CLASS_ATTR_SAVE(c, "mode", 0);                  // save with patcher
  CLASS_ATTR_SELFSAVE(c, "mode", 0);              // display as saved

  CLASS_ATTR_LONG(c, "fprecision", 0, t_strcut, fprecision);
  CLASS_ATTR_ORDER(c, "fprecision", 0, "2");
  CLASS_ATTR_LABEL(c, "fprecision", 0, "float precision");
  CLASS_ATTR_FILTER_CLIP(c, "fprecision", 0, 10);
  CLASS_ATTR_SAVE(c, "fprecision", 0);
  CLASS_ATTR_SELFSAVE(c, "fprecision", 0);
  CLASS_ATTR_ACCESSORS(c, "fprecision", NULL, str_fprecision_set);

  class_register(CLASS_BOX, c);
  strcut_class = c;
}

/****************************************************************
*  Constructor
*/
void *strcut_new(t_symbol *sym, long argc, t_atom *argv)
{
  t_strcut *x = NULL;

  x = (t_strcut *)object_alloc(strcut_class);

  if (x == NULL) {
    error("strcut:  Object allocation failed.");
    return NULL;
  }

  // Set inlets and outlets
  intin(x, 1);
  x->outl_any2 = outlet_new((t_object *)x, NULL);
  x->outl_any1 = outlet_new((t_object *)x, NULL);

  // Set the string buffers
  x->i_dstr    = dstr_new();
  x->o_dstr1  = dstr_new();
  x->o_dstr2 = dstr_new();
  x->o_sym1 = gensym("");
  x->o_sym2 = gensym("");
  if (DSTR_IS_NULL(x->i_dstr) || DSTR_IS_NULL(x->o_dstr1) || DSTR_IS_NULL(x->o_dstr2)) {
    object_error((t_object *)x, "Allocation error.");
    strcut_free(x);
    return NULL;
  }

  // First argument:  cutting index
  x->i_pos = 0;
  if ((argc >= 1) && (attr_args_offset((short)argc, argv) >= 1)) {
    if ((atom_gettype(argv) == A_LONG) && (atom_getlong(argv) >= 0)) {
      x->i_pos = (long)atom_getlong(argv);
    } else {
      object_error((t_object *)x, "Arg 1:  Cutting index:  Positive int expected");
    }
  }

  // Second argument:  mode
  long mode = 0;
  if ((argc >= 2) && (attr_args_offset((short)argc, argv) >= 2)) {
    if ((atom_gettype(argv + 1) == A_LONG) && (atom_getlong(argv + 1) >= 0) && (atom_getlong(argv + 1) <= 1)) {
      mode = (long)atom_getlong(argv + 1);
    } else {
      object_error((t_object *)x, "Arg 2:  Mode:  0 or 1 expected");
    }
  }
  object_attr_setlong(x, gensym("mode"), mode);

  // Set the float precision
  object_attr_setlong(x, gensym("fprecision"), 6);

  // Process the attributes
  attr_args_process(x, (short)argc, argv);

  return x;
}

/****************************************************************
*  Destructor
*/
void strcut_free(t_strcut *x)
{
  dstr_free(&x->i_dstr);
  dstr_free(&x->o_dstr1);
  dstr_free(&x->o_dstr2);
}

/****************************************************************
*  Assist
*/
void strcut_assist(t_strcut *x, void *b, long msg, long arg, char *dst)
{
  switch (msg) {
  case ASSIST_INLET:
    switch (arg) {
    case 0: sprintf(dst, "string to cut (int, float, symbol, list)"); break;
    case 1: sprintf(dst, "cutting position (int)"); break;
    default: break;
    }
    break;
  case ASSIST_OUTLET:
    if (x->mode == arg) { sprintf(dst, "left portion of cut string (symbol)"); }
    else { sprintf(dst, "right portion of cut string (symbol)"); }
    break;
  }
}

/****************************************************************
*  Interface functions
*/
void strcut_bang(t_strcut *x)
{
  strcut_output(x);
}

/****************************************************************
*  Process int inputs
*/
void strcut_int(t_strcut *x, t_atom_long n)
{
  dstr_cpy_int(x->i_dstr, n);
  strcut_action(x);
  strcut_output(x);
}

/****************************************************************
*  Process int inputs
*/
void strcut_in1(t_strcut *x, t_atom_long n)
{
  x->i_pos = (long)((n >= 0) ? n : 0);
  strcut_action(x);
}

/****************************************************************
*  Process float inputs
*/
void strcut_float(t_strcut *x, double f)
{
  dstr_cpy_printf(x->i_dstr, x->format, f);
  strcut_action(x);
  strcut_output(x);
}

/****************************************************************
*  Process list inputs
*/
void strcut_list(t_strcut *x, t_symbol *sym, long argc, t_atom *argv)
{
  dstr_empty(x->i_dstr);
  str_cat_args(x, x->i_dstr, argc, argv);
  strcut_action(x);
  strcut_output(x);
}

/****************************************************************
*  Process any other inputs
*/
void strcut_anything(t_strcut *x, t_symbol *sym, long argc, t_atom *argv)
{
  dstr_cpy_cstr(x->i_dstr, sym->s_name);
  str_cat_args(x, x->i_dstr, argc, argv);
  strcut_action(x);
  strcut_output(x);
}

/****************************************************************
*  Set the input string in the left inlet without outputting
*/
void strcut_set(t_strcut *x, t_symbol *sym, long argc, t_atom *argv)
{
  dstr_empty(x->i_dstr);
  str_cat_args(x, x->i_dstr, argc, argv);
  strcut_action(x);
}

/****************************************************************
*  Post the object string buffers
*/
void strcut_post(t_strcut *x)
{
  object_post((t_object *)x, "Mode:  %i", x->mode);
  object_post((t_object *)x, "Float precision:  %i", x->fprecision);
  object_post((t_object *)x, "Alloc:  In: %i - Left: %i - Right: %i",
    DSTR_ALLOC(x->i_dstr), DSTR_ALLOC(x->o_dstr1), DSTR_ALLOC(x->o_dstr2));
  object_post((t_object *)x, "In: %s", DSTR_CSTR(x->i_dstr));
  object_post((t_object *)x, "Left: %s", DSTR_CSTR(x->o_dstr1));
  object_post((t_object *)x, "Right: %s", DSTR_CSTR(x->o_dstr2));
}

/****************************************************************
*  The specific string action
*/
void strcut_action(t_strcut *x)
{
  if (x->i_pos <= 0) {
    dstr_empty(x->o_dstr1);
    dstr_cpy_dstr(x->o_dstr2, x->i_dstr);
  }
  else if (x->i_pos < (long)DSTR_LENGTH(x->i_dstr)) {
    dstr_rcpy_dstr(x->o_dstr1, x->i_dstr, 0, x->i_pos);
    dstr_rcpy_dstr(x->o_dstr2, x->i_dstr, x->i_pos, DSTR_LENGTH(x->i_dstr) - x->i_pos);

  } else {
    dstr_cpy_dstr(x->o_dstr1, x->i_dstr);
    dstr_empty(x->o_dstr2);
  }

  // Test that the t_dstr strings are not NULL
  if (!DSTR_IS_NULL(x->i_dstr) && !DSTR_IS_NULL(x->o_dstr1) && !DSTR_IS_NULL(x->o_dstr2)) {
    x->o_sym1 = gensym(DSTR_CSTR(x->o_dstr1));
    x->o_sym2 = gensym(DSTR_CSTR(x->o_dstr2));
  
  } else {
    x->o_sym1 = gensym("<error>");
    x->o_sym2 = gensym("<error>");
    object_error((t_object *)x, "Allocation error. Reset the external.");
  }
}

/****************************************************************
*  Output the strings
*/
void strcut_output(t_strcut *x)
{
  switch(x->mode) {
  case 0:
    outlet_anything(x->outl_any2, x->o_sym2, 0, NULL);
    outlet_anything(x->outl_any1, x->o_sym1, 0, NULL);
    break;
  case 1:
    outlet_anything(x->outl_any2, x->o_sym1, 0, NULL);
    outlet_anything(x->outl_any1, x->o_sym2, 0, NULL);
    break;
  default: return;
  }
}

/****************************************************************
*  Concatenate the content of an atom to an t_dstr string
*/
t_dstr str_cat_atom(t_strcut *x, t_dstr dstr, t_atom *atom)
{
  switch (atom_gettype(atom)) {
  case A_LONG:  dstr_cat_int(dstr, atom_getlong(atom)); break;
  case A_FLOAT: dstr_cat_printf(dstr, x->format, atom_getfloat(atom)); break;
  case A_SYM:   dstr_cat_cstr(dstr, atom_getsym(atom)->s_name); break;
  }

  return dstr;
}

/****************************************************************
*  Helper function to process list inputs
*/
t_dstr str_cat_args(t_strcut *x, t_dstr dstr, long argc, t_atom *argv)
{
  for (long i = 0; i < argc; i++) {
    if (DSTR_LENGTH(dstr)) { dstr_cat_bin(dstr, " ", 1); }
    str_cat_atom(x, dstr, argv + i);
  }

  return dstr;
}

/****************************************************************
*  Custom setter for the float precision attribute
*/
t_max_err str_fprecision_set(t_strcut *x, void *attr, long argc, t_atom *argv)
{
  if (argc && argv) { x->fprecision = (long)atom_getlong(argv); }
  else { x->fprecision = 6; }

  strcpy(x->format, "%.");
  _itoa(x->fprecision, x->format + 2, 10);
  strcat(x->format, "f");

  return MAX_ERR_NONE;
}