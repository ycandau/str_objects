/**
*  @file
*  strcmp - a Max object to compare two strings
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
typedef struct _strcmp
{
  t_object obj;

  void *inl_proxy;
  long  inl_proxy_ind;
  void *outl_int1;
  void *outl_int2;

  t_dstr i_dstr1;
  t_dstr i_dstr2;
  long   o_int1;
  long   o_int2;

  long  mode;
  long  fprecision;
  char  format[6];

} t_strcmp;

/****************************************************************
*  Global class pointer
*/
static t_class *strcmp_class = NULL;

/****************************************************************
*  Function declarations
*/
void *strcmp_new      (t_symbol *sym, long argc, t_atom *argv);
void  strcmp_free     (t_strcmp *x);
void  strcmp_assist   (t_strcmp *x, void *b, long msg, long arg, char *dst);

void  strcmp_bang     (t_strcmp *x);
void  strcmp_int      (t_strcmp *x, long n);
void  strcmp_float    (t_strcmp *x, double f);
void  strcmp_list     (t_strcmp *x, t_symbol *sym, long argc, t_atom *argv);
void  strcmp_anything (t_strcmp *x, t_symbol *sym, long argc, t_atom *argv);
void  strcmp_set      (t_strcmp *x, t_symbol *sym, long argc, t_atom *argv);
void  strcmp_post     (t_strcmp *x);

void  strcmp_action   (t_strcmp *x);
void  strcmp_output   (t_strcmp *x);

t_dstr    str_proxy_to_dstr  (t_strcmp *x);
t_dstr    str_cat_atom       (t_strcmp *x, t_dstr dstr, t_atom *atom);
t_dstr    str_cat_args       (t_strcmp *x, t_dstr dstr, long argc, t_atom *argv);
t_max_err str_mode_set       (t_strcmp *x, void *attr, long argc, t_atom *argv);
t_max_err str_fprecision_set (t_strcmp *x, void *attr, long argc, t_atom *argv);


/****************************************************************
*  Initialization
*/
void ext_main(void *r)
{
  t_class *c;

  c = class_new("strcmp",
    (method)strcmp_new,
    (method)strcmp_free,
    (long)sizeof(t_strcmp),
    (method)NULL,
    A_GIMME, 0);

  class_addmethod(c, (method)strcmp_assist,   "assist",    A_CANT,  0);
  class_addmethod(c, (method)strcmp_bang,     "bang",               0);
  class_addmethod(c, (method)strcmp_int,      "int",       A_LONG,  0);
  class_addmethod(c, (method)strcmp_float,    "float",     A_FLOAT, 0);
  class_addmethod(c, (method)strcmp_list,     "list",      A_GIMME, 0);
  class_addmethod(c, (method)strcmp_anything, "anything",  A_GIMME, 0);
  class_addmethod(c, (method)strcmp_set,      "set",       A_GIMME, 0);
  class_addmethod(c, (method)strcmp_post,     "post",               0);
  class_addmethod(c, (method)stdinletinfo,    "inletinfo", A_CANT,  0);

  CLASS_ATTR_LONG(c, "mode", 0, t_strcmp, mode);
  CLASS_ATTR_ORDER(c, "mode", 0, "1");            // order
  CLASS_ATTR_LABEL(c, "mode", 0, "mode");         // label
  CLASS_ATTR_FILTER_CLIP(c, "mode", 0, 1);        // min-max filter
  CLASS_ATTR_SAVE(c, "mode", 0);                  // save with patcher
  CLASS_ATTR_SELFSAVE(c, "mode", 0);              // display as saved
  CLASS_ATTR_ACCESSORS(c, "mode", NULL, str_mode_set);

  CLASS_ATTR_LONG(c, "fprecision", 0, t_strcmp, fprecision);
  CLASS_ATTR_ORDER(c, "fprecision", 0, "2");
  CLASS_ATTR_LABEL(c, "fprecision", 0, "float precision");
  CLASS_ATTR_FILTER_CLIP(c, "fprecision", 0, 10);
  CLASS_ATTR_SAVE(c, "fprecision", 0);
  CLASS_ATTR_SELFSAVE(c, "fprecision", 0);
  CLASS_ATTR_ACCESSORS(c, "fprecision", NULL, str_fprecision_set);

  class_register(CLASS_BOX, c);
  strcmp_class = c;
}

/****************************************************************
*  Constructor
*/
void *strcmp_new(t_symbol *sym, long argc, t_atom *argv)
{
  t_strcmp *x = NULL;

  x = (t_strcmp *)object_alloc(strcmp_class);

  if (x == NULL) {
    error("strcmp:  Allocation failed.");
    return NULL;
  }

  // Set inlets, outlets, and proxy
  x->inl_proxy_ind = 0;
  x->inl_proxy = proxy_new((t_object *)x, 1, &x->inl_proxy_ind);
  x->outl_int2 = intout((t_object *)x);
  x->outl_int1 = intout((t_object *)x);

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
    strcmp_free(x);
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
  x->o_int1 = 0;
  x->o_int2 = -1;

  // Process the attributes
  attr_args_process(x, (short)argc, argv);

  return x;
}

/****************************************************************
*  Destructor
*/
void strcmp_free(t_strcmp *x)
{
  dstr_free(&x->i_dstr1);
  dstr_free(&x->i_dstr2);
  freeobject((t_object *)x->inl_proxy);
}

/****************************************************************
*  Assist
*/
void strcmp_assist(t_strcmp *x, void *b, long msg, long arg, char *dst)
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
    case 0: sprintf(dst, "match s1 and s2 (0 / 1)"); break;
    case 1:
      if (x->mode == 0) { sprintf(dst, "compare s1 to s2 (-1 / 0 / 1)"); }
      else { sprintf(dst, "compare s2 to s1 (-1 / 0 / 1)"); }
      break;
    default: break;
    }
    break;
  }
}

/****************************************************************
*  Interface functions
*/
void strcmp_bang(t_strcmp *x)
{
  strcmp_output(x);
}

/****************************************************************
*  Process int inputs
*/
void strcmp_int(t_strcmp *x, long n)
{
  t_dstr dstr = str_proxy_to_dstr(x);

  dstr_cpy_int(dstr, n);
  strcmp_action(x);
  if (dstr == x->i_dstr1) { strcmp_output(x); }
}

/****************************************************************
*  Process float inputs
*/
void strcmp_float(t_strcmp *x, double f)
{
  t_dstr dstr = str_proxy_to_dstr(x);

  dstr_cpy_printf(dstr, x->format, f);
  strcmp_action(x);
  if (dstr == x->i_dstr1) { strcmp_output(x); }
}

/****************************************************************
*  Process list inputs
*/
void strcmp_list(t_strcmp *x, t_symbol *sym, long argc, t_atom *argv)
{
  t_dstr dstr = str_proxy_to_dstr(x);

  dstr_empty(dstr);
  str_cat_args(x, dstr, argc, argv);
  strcmp_action(x);
  if (dstr == x->i_dstr1) { strcmp_output(x); }
}

/****************************************************************
*  Process any other inputs
*/
void strcmp_anything(t_strcmp *x, t_symbol *sym, long argc, t_atom *argv)
{
  t_dstr dstr = str_proxy_to_dstr(x);

  dstr_cpy_cstr(dstr, sym->s_name);
  str_cat_args(x, dstr, argc, argv);
  strcmp_action(x);
  if (dstr == x->i_dstr1) { strcmp_output(x); }
}

/****************************************************************
*  Set the string in the cold buffer without outputting
*/
void strcmp_set(t_strcmp *x, t_symbol *sym, long argc, t_atom *argv)
{
  t_dstr dstr = str_proxy_to_dstr(x);

  dstr_empty(dstr);
  str_cat_args(x, dstr, argc, argv);
  strcmp_action(x);
}

/****************************************************************
*  Post the object string buffers
*/
void strcmp_post(t_strcmp *x)
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
void strcmp_action(t_strcmp *x)
{
  int cmp;

  // Test that the t_dstr strings are not NULL
  if (DSTR_IS_NULL(x->i_dstr1) || DSTR_IS_NULL(x->i_dstr2)) {
    x->o_int1 = 0;
    x->o_int2 = -1;
    object_error((t_object *)x, "Allocation error. Reset the external.");
    return;
  }

  if (x->mode == 0) { cmp = memcmp(DSTR_CSTR(x->i_dstr1), DSTR_CSTR(x->i_dstr2),
    min(DSTR_LENGTH(x->i_dstr1), DSTR_LENGTH(x->i_dstr2)) + 1); }
  else { cmp = memcmp(DSTR_CSTR(x->i_dstr2), DSTR_CSTR(x->i_dstr1),
    min(DSTR_LENGTH(x->i_dstr1), DSTR_LENGTH(x->i_dstr2)) + 1); }

  x->o_int1 = cmp ? 0 : 1;
  x->o_int2 = (cmp < 0) ? -1 : ((cmp > 0) ? +1 : 0);
}

/****************************************************************
*  Output the string
*/
void strcmp_output(t_strcmp *x)
{
  outlet_int(x->outl_int2, x->o_int2);
  outlet_int(x->outl_int1, x->o_int1);
}

/****************************************************************
*  Get the destination buffer depending on the proxy
*/
t_dstr str_proxy_to_dstr(t_strcmp *x)
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
t_dstr str_cat_atom(t_strcmp *x, t_dstr dstr, t_atom *atom)
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
t_dstr str_cat_args(t_strcmp *x, t_dstr dstr, long argc, t_atom *argv)
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
t_max_err str_mode_set(t_strcmp *x, void *attr, long argc, t_atom *argv)
{
  if (argc && argv) { x->mode = (long)atom_getlong(argv); } else { x->mode = 0; }

  strcmp_action(x);
  return MAX_ERR_NONE;
}

/****************************************************************
*  Custom setter for the float precision attribute
*/
t_max_err str_fprecision_set(t_strcmp *x, void *attr, long argc, t_atom *argv)
{
  if (argc && argv) { x->fprecision = (long)atom_getlong(argv); } else { x->fprecision = 6; }

  strcpy(x->format, "%.");
  _itoa(x->fprecision, x->format + 2, 10);
  strcat(x->format, "f");

  return MAX_ERR_NONE;
}
