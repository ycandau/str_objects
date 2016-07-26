/**
*  @file
*  strchr - a Max object to find a character in a string
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
typedef struct _strchr
{
  t_object obj;

  void *inl_proxy;
  long  inl_proxy_ind;
  void *outl_int;
  
  t_dstr i_dstr1;
  t_dstr i_dstr2;
  long   o_pos;

  long  mode;
  long  fprecision;
  char  format[6];

} t_strchr;

/****************************************************************
*  Global class pointer
*/
static t_class *strchr_class = NULL;

/****************************************************************
*  Function declarations
*/
void *strchr_new      (t_symbol *sym, long argc, t_atom *argv);
void  strchr_free     (t_strchr *x);
void  strchr_assist   (t_strchr *x, void *b, long msg, long arg, char *dst);

void  strchr_bang     (t_strchr *x);
void  strchr_int      (t_strchr *x, long n);
void  strchr_float    (t_strchr *x, double f);
void  strchr_list     (t_strchr *x, t_symbol *sym, long argc, t_atom *argv);
void  strchr_anything (t_strchr *x, t_symbol *sym, long argc, t_atom *argv);
void  strchr_set      (t_strchr *x, t_symbol *sym, long argc, t_atom *argv);
void  strchr_post     (t_strchr *x);

void  strchr_action   (t_strchr *x);
void  strchr_output   (t_strchr *x);

t_dstr    str_proxy_to_dstr  (t_strchr *x);
t_dstr    str_cat_atom       (t_strchr *x, t_dstr dstr, t_atom *atom);
t_dstr    str_cat_args       (t_strchr *x, t_dstr dstr, long argc, t_atom *argv);
t_max_err str_mode_set       (t_strchr *x, void *attr, long argc, t_atom *argv);
t_max_err str_fprecision_set (t_strchr *x, void *attr, long argc, t_atom *argv);


/****************************************************************
*  Initialization
*/
void ext_main(void *r)
{
  t_class *c;

  c = class_new("strchr",
    (method)strchr_new,
    (method)strchr_free,
    (long)sizeof(t_strchr),
    (method)NULL,
    A_GIMME, 0);

  class_addmethod(c, (method)strchr_assist,   "assist",    A_CANT,  0);
  class_addmethod(c, (method)strchr_bang,     "bang",               0);
  class_addmethod(c, (method)strchr_int,      "int",       A_LONG,  0);
  class_addmethod(c, (method)strchr_float,    "float",     A_FLOAT, 0);
  class_addmethod(c, (method)strchr_list,     "list",      A_GIMME, 0);
  class_addmethod(c, (method)strchr_anything, "anything",  A_GIMME, 0);
  class_addmethod(c, (method)strchr_set,      "set",       A_GIMME, 0);
  class_addmethod(c, (method)strchr_post,     "post",               0);
  class_addmethod(c, (method)stdinletinfo,    "inletinfo", A_CANT,  0);

  CLASS_ATTR_LONG(c, "mode", 0, t_strchr, mode);
  CLASS_ATTR_ORDER(c, "mode", 0, "1");            // order
  CLASS_ATTR_LABEL(c, "mode", 0, "mode");         // label
  CLASS_ATTR_FILTER_CLIP(c, "mode", 0, 1);        // min-max filter
  CLASS_ATTR_SAVE(c, "mode", 0);                  // save with patcher
  CLASS_ATTR_SELFSAVE(c, "mode", 0);              // display as saved
  CLASS_ATTR_ACCESSORS(c, "mode", NULL, str_mode_set);

  CLASS_ATTR_LONG(c, "fprecision", 0, t_strchr, fprecision);
  CLASS_ATTR_ORDER(c, "fprecision", 0, "2");
  CLASS_ATTR_LABEL(c, "fprecision", 0, "float precision");
  CLASS_ATTR_FILTER_CLIP(c, "fprecision", 0, 10);
  CLASS_ATTR_SAVE(c, "fprecision", 0);
  CLASS_ATTR_SELFSAVE(c, "fprecision", 0);
  CLASS_ATTR_ACCESSORS(c, "fprecision", NULL, str_fprecision_set);

  class_register(CLASS_BOX, c);
  strchr_class = c;
}

/****************************************************************
*  Constructor
*/
void *strchr_new(t_symbol *sym, long argc, t_atom *argv)
{
  t_strchr *x = NULL;

  x = (t_strchr *)object_alloc(strchr_class);

  if (x == NULL) {
    error("strchr:  Allocation failed.");
    return NULL;
  }

  // Set inlets, outlets, and proxy
  x->inl_proxy_ind = 0;
  x->inl_proxy = proxy_new((t_object *)x, 1, &x->inl_proxy_ind);
  x->outl_int = intout((t_object *)x);

  // Set the left string buffer
  x->i_dstr1 = dstr_new();

  // First argument:  right string buffer
  x->i_dstr2 = dstr_new();
  if ((argc >= 1) && (attr_args_offset((short)argc, argv) >= 1)) {
    x->i_dstr2 = str_cat_atom(x, x->i_dstr2, argv);
  }

  // Test the string buffers
  if (DSTR_IS_NULL(x->i_dstr1) || DSTR_IS_NULL(x->i_dstr2)) {
    object_error((t_object *)x, "Allocation error.");
    strchr_free(x);
    return NULL;
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

  // Set the remaining variables
  x->o_pos = -1;

  // Process the attributes
  attr_args_process(x, (short)argc, argv);

  return x;
}

/****************************************************************
*  Destructor
*/
void strchr_free(t_strchr *x)
{
  dstr_free(&x->i_dstr1);
  dstr_free(&x->i_dstr2);
  freeobject((t_object *)x->inl_proxy);
}

/****************************************************************
*  Assist
*/
void strchr_assist(t_strchr *x, void *b, long msg, long arg, char *dst)
{
  switch (msg) {
  case ASSIST_INLET:
    switch (arg) {
    case 0: sprintf(dst, "string s1 (int, float, symbol, list)"); break;
    case 1: sprintf(dst, "string s2 (int, float, symbol, list)"); break;
    default: break;
    }
    break;
  case ASSIST_OUTLET:
    switch (arg) {
    case 0:
      if (x->mode == 0) { sprintf(dst, "position of s2 in s1 (int)"); }
      else { sprintf(dst, "position of s1 in s2 (int)"); }
      break;
    default: break;
    }
    break;
  }
}

/****************************************************************
*  Interface functions
*/
void strchr_bang(t_strchr *x)
{
  strchr_output(x);
}

/****************************************************************
*  Process int inputs
*/
void strchr_int(t_strchr *x, long n)
{
  t_dstr dstr = str_proxy_to_dstr(x);

  dstr_cpy_int(dstr, n);
  strchr_action(x);
  if (dstr == x->i_dstr1) { strchr_output(x); }
}

/****************************************************************
*  Process float inputs
*/
void strchr_float(t_strchr *x, double f)
{
  t_dstr dstr = str_proxy_to_dstr(x);

  dstr_cpy_printf(dstr, x->format, f);
  strchr_action(x);
  if (dstr == x->i_dstr1) { strchr_output(x); }
}

/****************************************************************
*  Process list inputs
*/
void strchr_list(t_strchr *x, t_symbol *sym, long argc, t_atom *argv)
{
  t_dstr dstr = str_proxy_to_dstr(x);

  dstr_empty(dstr);
  str_cat_args(x, dstr, argc, argv);
  strchr_action(x);
  if (dstr == x->i_dstr1) { strchr_output(x); }
}

/****************************************************************
*  Process any other inputs
*/
void strchr_anything(t_strchr *x, t_symbol *sym, long argc, t_atom *argv)
{
  t_dstr dstr = str_proxy_to_dstr(x);

  dstr_cpy_cstr(dstr, sym->s_name);
  str_cat_args(x, dstr, argc, argv);
  strchr_action(x);
  if (dstr == x->i_dstr1) { strchr_output(x); }
}

/****************************************************************
*  Set the string in the cold buffer without outputting
*/
void strchr_set(t_strchr *x, t_symbol *sym, long argc, t_atom *argv)
{
  t_dstr dstr = str_proxy_to_dstr(x);

  dstr_empty(dstr);
  str_cat_args(x, dstr, argc, argv);
  strchr_action(x);
}

/****************************************************************
*  Post the object string buffers
*/
void strchr_post(t_strchr *x)
{
  object_post((t_object *)x, "Mode:  %i", x->mode);
  object_post((t_object *)x, "Float precision:  %i", x->fprecision);
  object_post((t_object *)x, "Alloc:  Left: %i - Right: %i",
    DSTR_ALLOC(x->i_dstr1), DSTR_ALLOC(x->i_dstr2));
  object_post((t_object *)x, "Left: %s", DSTR_CSTR(x->i_dstr1));
  object_post((t_object *)x, "Right: %s", DSTR_CSTR(x->i_dstr2));
}

/****************************************************************
*  The specific string action
*/
void strchr_action(t_strchr *x)
{
  // Test that the t_dstr strings are not NULL
  if (DSTR_IS_NULL(x->i_dstr1) || DSTR_IS_NULL(x->i_dstr2)) {
    x->o_pos = -1;
    object_error((t_object *)x, "Allocation error. Reset the external.");
    return;
  }

  char *cp;
  if (x->mode == 0) {
    cp = memchr(DSTR_CSTR(x->i_dstr1), DSTR_CSTR(x->i_dstr2)[0], DSTR_LENGTH(x->i_dstr1));
    x->o_pos = cp ? (long)(cp - DSTR_CSTR(x->i_dstr1) + 1) : -1;

  } else if (x->mode == 1) {
    cp = memchr(DSTR_CSTR(x->i_dstr2), DSTR_CSTR(x->i_dstr1)[0], DSTR_LENGTH(x->i_dstr2));
    x->o_pos = cp ? (long)(cp - DSTR_CSTR(x->i_dstr2) + 1) : -1;
  }
}

/****************************************************************
*  Output the string
*/
void strchr_output(t_strchr *x)
{
  outlet_int(x->outl_int, x->o_pos);
}

/****************************************************************
*  Get the destination buffer depending on the proxy
*/
t_dstr str_proxy_to_dstr(t_strchr *x)
{
  switch (proxy_getinlet((t_object *)x)) {
  case 0:  return x->i_dstr1;
  case 1:  return x->i_dstr2;
  default: return NULL;
  }
}

/****************************************************************
*  Concatenate the content of an atom to an t_dstr string
*/
t_dstr str_cat_atom(t_strchr *x, t_dstr dstr, t_atom *atom)
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
t_dstr str_cat_args(t_strchr *x, t_dstr dstr, long argc, t_atom *argv)
{
  for (long i = 0; i < argc; i++) {
    if (DSTR_LENGTH(dstr)) { dstr_cat_bin(dstr, " ", 1); }
    str_cat_atom(x, dstr, argv + i);
  }

  return dstr;
}

/****************************************************************
*  Custom setter for the mode attribute
*/
t_max_err str_mode_set(t_strchr *x, void *attr, long argc, t_atom *argv)
{
  if (argc && argv) { x->mode = (long)atom_getlong(argv); } else { x->mode = 0; }

  strchr_action(x);
  return MAX_ERR_NONE;
}

/****************************************************************
*  Custom setter for the float precision attribute
*/
t_max_err str_fprecision_set(t_strchr *x, void *attr, long argc, t_atom *argv)
{
  if (argc && argv) { x->fprecision = (long)atom_getlong(argv); } else { x->fprecision = 6; }

  strcpy(x->format, "%.");
  _itoa(x->fprecision, x->format + 2, 10);
  strcat(x->format, "f");

  return MAX_ERR_NONE;
}
