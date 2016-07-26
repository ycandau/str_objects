/**
*  @file
*  strtok - a Max object to parse a string into tokens
*
*  Originally by Jan Schacher
*
*  Refactored by Yves Candau to use:
*    - a thread safe version of strtok,
*    - the new style Max object,
*    - dynamic strings,
*    - attributes.
*
*  @todo:  - strtok_action: test for NULL strings
*          - dynamic array of atoms, or test for length
*   
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
typedef struct _strtok
{
  t_object obj;

  void *inl_proxy;
  long  inl_proxy_ind;
  void *outl_any;
  
  t_dstr    i_dstr1;
  t_dstr    i_dstr2;
  t_atom    o_tok_arr[256];
  short     o_tok_cnt;
  t_symbol *o_tok_first;

  long  mode;
  long  fprecision;
  char  format[6];

} t_strtok;

/****************************************************************
*  Global class pointer
*/
static t_class *strtok_class = NULL;

/****************************************************************
*  Function declarations
*/
void *strtok_new      (t_symbol *sym, long argc, t_atom *argv);
void  strtok_free     (t_strtok *x);
void  strtok_assist   (t_strtok *x, void *b, long msg, long arg, char *dst);

void  strtok_bang     (t_strtok *x);
void  strtok_int      (t_strtok *x, long n);
void  strtok_float    (t_strtok *x, double f);
void  strtok_list     (t_strtok *x, t_symbol *sym, long argc, t_atom *argv);
void  strtok_anything (t_strtok *x, t_symbol *sym, long argc, t_atom *argv);
void  strtok_set      (t_strtok *x, t_symbol *sym, long argc, t_atom *argv);
void  strtok_post     (t_strtok *x);

void  strtok_action   (t_strtok *x);
void  strtok_output   (t_strtok *x);

t_dstr    str_proxy_to_dstr  (t_strtok *x);
t_dstr    str_cat_atom       (t_strtok *x, t_dstr dstr, t_atom *atom);
t_dstr    str_cat_args       (t_strtok *x, t_dstr dstr, long argc, t_atom *argv);
t_max_err str_mode_set       (t_strtok *x, void *attr, long argc, t_atom *argv);
t_max_err str_fprecision_set (t_strtok *x, void *attr, long argc, t_atom *argv);


/****************************************************************
*  Initialization
*/
void ext_main(void *r)
{
  t_class *c;

  c = class_new("strtok",
    (method)strtok_new,
    (method)strtok_free,
    (long)sizeof(t_strtok),
    (method)NULL,
    A_GIMME, 0);

  class_addmethod(c, (method)strtok_assist,   "assist",    A_CANT,  0);
  class_addmethod(c, (method)strtok_bang,     "bang",               0);
  class_addmethod(c, (method)strtok_int,      "int",       A_LONG,  0);
  class_addmethod(c, (method)strtok_float,    "float",     A_FLOAT, 0);
  class_addmethod(c, (method)strtok_list,     "list",      A_GIMME, 0);
  class_addmethod(c, (method)strtok_anything, "anything",  A_GIMME, 0);
  class_addmethod(c, (method)strtok_set,      "set",       A_GIMME, 0);
  class_addmethod(c, (method)strtok_post,     "post",               0);
  class_addmethod(c, (method)stdinletinfo,    "inletinfo", A_CANT,  0);

  CLASS_ATTR_LONG(c, "mode", 0, t_strtok, mode);
  CLASS_ATTR_ORDER(c, "mode", 0, "1");            // order
  CLASS_ATTR_LABEL(c, "mode", 0, "mode");         // label
  CLASS_ATTR_FILTER_CLIP(c, "mode", 0, 1);        // min-max filter
  CLASS_ATTR_SAVE(c, "mode", 0);                  // save with patcher
  CLASS_ATTR_SELFSAVE(c, "mode", 0);              // display as saved
  CLASS_ATTR_ACCESSORS(c, "mode", NULL, str_mode_set);

  CLASS_ATTR_LONG(c, "fprecision", 0, t_strtok, fprecision);
  CLASS_ATTR_ORDER(c, "fprecision", 0, "2");
  CLASS_ATTR_LABEL(c, "fprecision", 0, "float precision");
  CLASS_ATTR_FILTER_CLIP(c, "fprecision", 0, 10);
  CLASS_ATTR_SAVE(c, "fprecision", 0);
  CLASS_ATTR_SELFSAVE(c, "fprecision", 0);
  CLASS_ATTR_ACCESSORS(c, "fprecision", NULL, str_fprecision_set);

  class_register(CLASS_BOX, c);
  strtok_class = c;
}

/****************************************************************
*  Constructor
*/
void *strtok_new(t_symbol *sym, long argc, t_atom *argv)
{
  t_strtok *x = NULL;

  x = (t_strtok *)object_alloc(strtok_class);

  if (x == NULL) {
    error("strtok:  Allocation failed.");
    return NULL;
  }

  // Set inlets, outlets, and proxy
  x->inl_proxy_ind = 0;
  x->inl_proxy = proxy_new((t_object *)x, 1, &x->inl_proxy_ind);
  x->outl_any = intout((t_object *)x);

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
    strtok_free(x);
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
  x->o_tok_cnt = 0;
  x->o_tok_first = gensym("");

  // Process the attributes
  attr_args_process(x, (short)argc, argv);

  return x;
}

/****************************************************************
*  Destructor
*/
void strtok_free(t_strtok *x)
{
  dstr_free(&x->i_dstr1);
  dstr_free(&x->i_dstr2);
  freeobject((t_object *)x->inl_proxy);
}

/****************************************************************
*  Assist
*/
void strtok_assist(t_strtok *x, void *b, long msg, long arg, char *dst)
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
      if (x->mode == 0) { sprintf(dst, "tokens from s1 separated by s2 (list)"); }
      else { sprintf(dst, "tokens from s2 separated by s1 (list)"); }
      break;
    default: break;
    }
    break;
  }
}

/****************************************************************
*  Interface functions
*/
void strtok_bang(t_strtok *x)
{
  strtok_output(x);
}

/****************************************************************
*  Process int inputs
*/
void strtok_int(t_strtok *x, long n)
{
  t_dstr dstr = str_proxy_to_dstr(x);

  dstr_cpy_int(dstr, n);
  strtok_action(x);
  if (dstr == x->i_dstr1) { strtok_output(x); }
}

/****************************************************************
*  Process float inputs
*/
void strtok_float(t_strtok *x, double f)
{
  t_dstr dstr = str_proxy_to_dstr(x);

  dstr_cpy_printf(dstr, x->format, f);
  strtok_action(x);
  if (dstr == x->i_dstr1) { strtok_output(x); }
}

/****************************************************************
*  Process list inputs
*/
void strtok_list(t_strtok *x, t_symbol *sym, long argc, t_atom *argv)
{
  t_dstr dstr = str_proxy_to_dstr(x);

  dstr_empty(dstr);
  str_cat_args(x, dstr, argc, argv);
  strtok_action(x);
  if (dstr == x->i_dstr1) { strtok_output(x); }
}

/****************************************************************
*  Process any other inputs
*/
void strtok_anything(t_strtok *x, t_symbol *sym, long argc, t_atom *argv)
{
  t_dstr dstr = str_proxy_to_dstr(x);

  dstr_cpy_cstr(dstr, sym->s_name);
  str_cat_args(x, dstr, argc, argv);
  strtok_action(x);
  if (dstr == x->i_dstr1) { strtok_output(x); }
}

/****************************************************************
*  Set the string in the cold buffer without outputting
*/
void strtok_set(t_strtok *x, t_symbol *sym, long argc, t_atom *argv)
{
  t_dstr dstr = str_proxy_to_dstr(x);

  dstr_empty(dstr);
  str_cat_args(x, dstr, argc, argv);
  strtok_action(x);
}

/****************************************************************
*  Post the object string buffers
*/
void strtok_post(t_strtok *x)
{
  object_post((t_object *)x, "Mode:  %i", x->mode);
  object_post((t_object *)x, "Float precision:  %i", x->fprecision);
  object_post((t_object *)x, "Token count:  %i", x->o_tok_cnt);
  object_post((t_object *)x, "Alloc:  Left: %i - Right: %i",
    DSTR_ALLOC(x->i_dstr1), DSTR_ALLOC(x->i_dstr2));
  object_post((t_object *)x, "Left: %s", DSTR_CSTR(x->i_dstr1));
  object_post((t_object *)x, "Right: %s", DSTR_CSTR(x->i_dstr2));
}

/****************************************************************
*  The specific string action
*/
void strtok_action(t_strtok *x)
{
  t_dstr temp = dstr_new_dstr((x->mode == 0) ? x->i_dstr1 : x->i_dstr2);

  // Test that the t_dstr strings are not NULL
  if (DSTR_IS_NULL(x->i_dstr1) || DSTR_IS_NULL(x->i_dstr2) || DSTR_IS_NULL(temp)) {
    x->o_tok_cnt = 0;
    object_error((t_object *)x, "Allocation error. Reset the external.");
    return;
  }

  short cnt = 0;
  char *token = NULL;
  char *next_token = NULL;
  char *sep = (x->mode == 0) ? DSTR_CSTR(x->i_dstr2) : DSTR_CSTR(x->i_dstr1);

  token = strtok_s(DSTR_CSTR(temp), sep, &next_token);

  if (token) {
    cnt++;
    x->o_tok_first = gensym(token);
    token = strtok_s(NULL, sep, &next_token);

    while (token) {
      atom_setsym(x->o_tok_arr + cnt++ - 1, gensym(token));
      token = strtok_s(NULL, sep, &next_token);
    }
    x->o_tok_cnt = cnt;
  } 

  dstr_free(&temp);
}

/****************************************************************
*  Output the string
*/
void strtok_output(t_strtok *x)
{
  if (x->o_tok_cnt >= 1) {
    outlet_anything(x->outl_any, x->o_tok_first, x->o_tok_cnt - 1, x->o_tok_arr);
  }
}

/****************************************************************
*  Get the destination buffer depending on the proxy
*/
t_dstr str_proxy_to_dstr(t_strtok *x)
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
t_dstr str_cat_atom(t_strtok *x, t_dstr dstr, t_atom *atom)
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
t_dstr str_cat_args(t_strtok *x, t_dstr dstr, long argc, t_atom *argv)
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
t_max_err str_mode_set(t_strtok *x, void *attr, long argc, t_atom *argv)
{
  if (argc && argv) { x->mode = (long)atom_getlong(argv); } else { x->mode = 0; }

  strtok_action(x);
  return MAX_ERR_NONE;
}

/****************************************************************
*  Custom setter for the float precision attribute
*/
t_max_err str_fprecision_set(t_strtok *x, void *attr, long argc, t_atom *argv)
{
  if (argc && argv) { x->fprecision = (long)atom_getlong(argv); } else { x->fprecision = 6; }

  strcpy(x->format, "%.");
  _itoa(x->fprecision, x->format + 2, 10);
  strcat(x->format, "f");

  return MAX_ERR_NONE;
}
