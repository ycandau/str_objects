/**
*  @file
*  strlen - a Max object to get the length of a string
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
typedef struct _strlen
{
  t_object obj;

  void *outl_int;

  t_dstr i_dstr;
  long   o_length;

  long  fprecision;
  char  format[6];

} t_strlen;

/****************************************************************
*  Global class pointer
*/
static t_class *strlen_class = NULL;

/****************************************************************
*  Function declarations
*/
void *strlen_new      (t_symbol *sym, long argc, t_atom *argv);
void  strlen_free     (t_strlen *x);
void  strlen_assist   (t_strlen *x, void *b, long msg, long arg, char *dst);

void  strlen_bang     (t_strlen *x);
void  strlen_int      (t_strlen *x, t_atom_long n);
void  strlen_float    (t_strlen *x, double f);
void  strlen_list     (t_strlen *x, t_symbol *sym, long argc, t_atom *argv);
void  strlen_anything (t_strlen *x, t_symbol *sym, long argc, t_atom *argv);
void  strlen_set      (t_strlen *x, t_symbol *sym, long argc, t_atom *argv);
void  strlen_post     (t_strlen *x);

void  strlen_action   (t_strlen *x);
void  strlen_output   (t_strlen *x);

t_dstr    str_cat_atom       (t_strlen *x, t_dstr dstr, t_atom *atom);
t_dstr    str_cat_args       (t_strlen *x, t_dstr dstr, long argc, t_atom *argv);
t_max_err str_fprecision_set (t_strlen *x, void *attr, long argc, t_atom *argv);

/****************************************************************
*  Initialization
*/
void ext_main(void *r)
{
  t_class *c;

  c = class_new("strlen",
    (method)strlen_new,
    (method)strlen_free,
    (long)sizeof(t_strlen),
    (method)NULL,
    A_GIMME, 0);

  class_addmethod(c, (method)strlen_assist,   "assist",    A_CANT,  0);
  class_addmethod(c, (method)strlen_bang,     "bang",               0);
  class_addmethod(c, (method)strlen_int,      "int",       A_LONG,  0);
  class_addmethod(c, (method)strlen_float,    "float",     A_FLOAT, 0);
  class_addmethod(c, (method)strlen_list,     "list",      A_GIMME, 0);
  class_addmethod(c, (method)strlen_anything, "anything",  A_GIMME, 0);
  class_addmethod(c, (method)strlen_set,      "set",       A_GIMME, 0);
  class_addmethod(c, (method)strlen_post,     "post",               0);
  class_addmethod(c, (method)stdinletinfo,    "inletinfo", A_CANT,  0);

  CLASS_ATTR_LONG(c, "fprecision", 0, t_strlen, fprecision);
  CLASS_ATTR_ORDER(c, "fprecision", 0, "1");
  CLASS_ATTR_LABEL(c, "fprecision", 0, "float precision");
  CLASS_ATTR_FILTER_CLIP(c, "fprecision", 0, 10);
  CLASS_ATTR_SAVE(c, "fprecision", 0);
  CLASS_ATTR_SELFSAVE(c, "fprecision", 0);
  CLASS_ATTR_ACCESSORS(c, "fprecision", NULL, str_fprecision_set);

  class_register(CLASS_BOX, c);
  strlen_class = c;
}

/****************************************************************
*  Constructor
*/
void *strlen_new(t_symbol *sym, long argc, t_atom *argv)
{
  t_strlen *x = NULL;

  x = (t_strlen *)object_alloc(strlen_class);

  if (x == NULL) {
    error("strlen:  Object allocation failed.");
    return NULL;
  }

  // Set the inlets and outlets
  x->outl_int = intout((t_object *)x);

  // Set the string buffers
  x->i_dstr = dstr_new();
  if (DSTR_IS_NULL(x->i_dstr)) {
    object_error((t_object *)x, "Allocation error.");
    strlen_free(x);
    return NULL;
  }

  // Set the float precision
  object_attr_setlong(x, gensym("fprecision"), 6);

  // Set the remaining variables
  x->o_length = -1;

  // Process the attributes
  attr_args_process(x, (short)argc, argv);

  return x;
}

/****************************************************************
*  Destructor
*/
void strlen_free(t_strlen *x)
{
  dstr_free(&x->i_dstr);
}

/****************************************************************
*  Assist
*/
void strlen_assist(t_strlen *x, void *b, long msg, long arg, char *dst)
{
  switch (msg) {
  case ASSIST_INLET:
    switch (arg) {
    case 0: sprintf(dst, "string to measure (int, float, symbol, list)"); break;
    default: break;
    }
    break;
  case ASSIST_OUTLET:
    switch(arg) {
    case 0: sprintf(dst, "lenght of the string (int)"); break;
    default: break;
    }
    break;
  }
}

/****************************************************************
*  Interface functions
*/
void strlen_bang(t_strlen *x)
{
  strlen_output(x);
}

/****************************************************************
*  Process int inputs
*/
void strlen_int(t_strlen *x, t_atom_long n)
{
  dstr_cpy_int(x->i_dstr, n);
  strlen_action(x);
  strlen_output(x);
}

/****************************************************************
*  Process float inputs
*/
void strlen_float(t_strlen *x, double f)
{
  dstr_cpy_printf(x->i_dstr, x->format, f);
  strlen_action(x);
  strlen_output(x);
}

/****************************************************************
*  Process list inputs
*/
void strlen_list(t_strlen *x, t_symbol *sym, long argc, t_atom *argv)
{
  dstr_empty(x->i_dstr);
  str_cat_args(x, x->i_dstr, argc, argv);
  strlen_action(x);
  strlen_output(x);
}

/****************************************************************
*  Process any other inputs
*/
void strlen_anything(t_strlen *x, t_symbol *sym, long argc, t_atom *argv)
{
  dstr_cpy_cstr(x->i_dstr, sym->s_name);
  str_cat_args(x, x->i_dstr, argc, argv);
  strlen_action(x);
  strlen_output(x);
}

/****************************************************************
*  Set the input string in the left inlet without outputting
*/
void strlen_set(t_strlen *x, t_symbol *sym, long argc, t_atom *argv)
{
  dstr_empty(x->i_dstr);
  str_cat_args(x, x->i_dstr, argc, argv);
  strlen_action(x);
}

/****************************************************************
*  Post the object string buffers
*/
void strlen_post(t_strlen *x)
{
  object_post((t_object *)x, "Float precision:  %i", x->fprecision);
  object_post((t_object *)x, "Alloc:  In: %i", DSTR_ALLOC(x->i_dstr));
  object_post((t_object *)x, "In: %s", DSTR_CSTR(x->i_dstr));
}

/****************************************************************
*  The specific string action
*/
void strlen_action(t_strlen *x)
{
  if (!DSTR_IS_NULL(x->i_dstr)) {
    x->o_length = (long)DSTR_LENGTH(x->i_dstr);
  } else {
    x->o_length = -1;
    object_error((t_object *)x, "Allocation error. Reset the external.");
  }
}

/****************************************************************
*  Output the strings
*/
void strlen_output(t_strlen *x)
{
    outlet_int(x->outl_int, x->o_length);
}

/****************************************************************
*  Concatenate the content of an atom to an t_dstr string
*/
t_dstr str_cat_atom(t_strlen *x, t_dstr dstr, t_atom *atom)
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
t_dstr str_cat_args(t_strlen *x, t_dstr dstr, long argc, t_atom *argv)
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
t_max_err str_fprecision_set(t_strlen *x, void *attr, long argc, t_atom *argv)
{
  if (argc && argv) { x->fprecision = (long)atom_getlong(argv); }
  else { x->fprecision = 6; }

  strcpy(x->format, "%.");
  _itoa(x->fprecision, x->format + 2, 10);
  strcat(x->format, "f");

  return MAX_ERR_NONE;
}