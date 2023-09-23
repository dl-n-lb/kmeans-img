#ifndef CLAP_H_
#define CLAP_H_

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/*//////////////////////////////////////////////////////////////////////////////
Simple command line argument processor in c (inspired by python's argparse)
All named arguments will be extracted first, then unnamed arguments in the order
specified when adding them to the parser.

NOTE:
Parsing is a destructive operation; it will edit argv!!
//////////////////////////////////////////////////////////////////////////////*/

#define CLAP_UNREACHABLE(str) assert(false && "Unreachable:"str)

#define CLAP_INITIAL_ARGS_CAP 4
#define CLAP_ARGS_GROW_RATE 2

#define CLAP_FLAG_TRUE "t"
#define CLAP_FLAG_FALSE NULL

typedef struct {
  const char *name; // required; passed as --`name`
  char alias; // single character alias; passed as -`alias`
  const char *description;
  const char *default_value; // only useful if its an optional argument
  enum {
    CLAP_ARG_OPTIONAL  = 1 << 0, // optional argument, default is required
    CLAP_ARG_HIDDEN    = 1 << 1, // hide from help (why would you want to do this?)
    CLAP_ARG_FLAG      = 1 << 2, // boolean value, set by --flag or -f
    CLAP_ARG_UNNAMED   = 1 << 3, // be parsed in order separately from all named flags
  } options;
} clap_arg;

typedef struct {
  const char *program_name;
  int argc;
  char **argv;
  
  const char *program_desc;

  struct {
    clap_arg *args;
    size_t count;
    size_t cap;
  } arguments;
  
  bool already_parsed;
  struct {
    const char **names;
    const char **values;
    size_t count;    
  } results;

  struct {
    const char **values;
    size_t count;
  } unnamed_results;
  
  struct {
    enum {
      CLAP_NO_ERR,
      CLAP_NO_VALUE,
      CLAP_INVALID_NAME,
      CLAP_MISSING_ARGS,
      CLAP_ALREADY_PARSED,
      CLAP_TOO_MANY_UNNAMED_ARGS,
      CLAP_MISSING_UNNAMED_ARG,
    } kind;
    const char *arg;
  } error;
} clap_parser;

typedef struct {
  const char *desc;
} clap_parser_opts;

clap_parser clap_parser_init(const int argc, char **argv,
			     const clap_parser_opts opts);

void clap_arg_add(clap_parser *p, clap_arg arg);
int clap_find_arg_with_name(const clap_parser *p, const char *name);
int clap_find_arg_with_alias(const clap_parser *p, const char alias);
bool clap_parse(clap_parser *p);
void clap_print_err(clap_parser p);
void clap_print_help(clap_parser p);
const char *clap_get(clap_parser p, const char *arg_name);
const char *clap_get_unnamed(clap_parser p, const size_t idx);
void clap_destroy(clap_parser *p);

#endif // CLAP_H_
#ifndef CLAP_NO_IMPLEMENTATION

clap_parser clap_parser_init(const int argc, char **argv,
			     const clap_parser_opts opts) {
  assert(argc >= 1 && "There must be at least one argument (program name)");
  assert(*argv != NULL && "argv is empty but argc > 0");
  return (clap_parser) {
    .program_name = argv[0],
    .program_desc = opts.desc,
    .argc = argc,
    .argv = argv,
    .arguments.args = malloc(CLAP_INITIAL_ARGS_CAP * sizeof(clap_arg)),
    .arguments.cap = CLAP_INITIAL_ARGS_CAP,
    .arguments.count = 0,
  };
}

void clap_arg_add(clap_parser *p, clap_arg arg) {
  if (p->arguments.count + 1 >= p->arguments.cap) {
    p->arguments.cap *= CLAP_ARGS_GROW_RATE;
    p->arguments.args = realloc(p->arguments.args,
				p->arguments.cap * sizeof(clap_arg));
    assert(p->arguments.args && "Allocation Error: realloc failed");
  }
  p->arguments.args[p->arguments.count++] = arg;
}

int clap_find_arg_with_name(const clap_parser *p, const char *name) {
  for (size_t i = 0; i < p->arguments.count; ++i) {
    clap_arg arg = p->arguments.args[i];
    if (arg.options & CLAP_ARG_UNNAMED) { continue; }
    if (strcmp(arg.name, name) == 0) {
      return i;
    }
  }
  
  return -1;
}

int clap_find_arg_with_alias(const clap_parser *p, const char alias) {
  for (size_t i = 0; i < p->arguments.count; ++i) {
    clap_arg arg = p->arguments.args[i];
    if (arg.alias == alias) {
      return i;
    }
  }
  
  return -1;
}

// set the first instance of delim to \0
// and then return the next index
// use with null terminated string ONLY
// internal
size_t clap_split_str_on_delim(char *str, char delim) {
  size_t i = 0;
  while (str[i] != '\0') {
    if (str[i] == delim) {
      str[i] = '\0';
      i += 1;
      break;
    }
    i++;
  }
  return i;
}

bool clap_parse(clap_parser *p) {
  if (p->already_parsed) {
    printf("Parser called twice? Skipping...\n");
    p->error.kind = CLAP_ALREADY_PARSED;
    return false;
  }
  p->already_parsed = true;
  // get the results ready
  p->results.count = p->arguments.count;
  p->results.names = malloc(p->results.count * sizeof(const char*));
  p->results.values = malloc(p->results.count * sizeof(const char*));
  p->unnamed_results.values = malloc(p->results.count * sizeof(const char*));
  // copy args names into results names
  size_t unnamed_cnt = 0, req_unnamed_cnt = 0;
  for (size_t i = 0; i < p->results.count; ++i) {
    if (!(p->arguments.args[i].options & CLAP_ARG_UNNAMED)) {
      p->results.names[i] = p->arguments.args[i].name;
      p->results.values[i] = p->arguments.args[i].default_value;
    } else {
      unnamed_cnt++;
      req_unnamed_cnt += !(p->arguments.args[i].options & CLAP_ARG_OPTIONAL);
    }
  }
  // parse
  size_t i = 1;
  while ((int)i < p->argc) {
    const char *arg = p->argv[i];
    const size_t arglen = strlen(arg);
    // is a long name
    if (arglen >= 2 && arg[0] == '-' && arg[1] == '-') {
      const char *name = &arg[2];
      size_t split_idx = clap_split_str_on_delim((char *)name, '=');
      const char *val = name + split_idx;
      // find the corresponding argument
      int idx = clap_find_arg_with_name(p, name);
      if (idx == -1) {
	p->error.kind = CLAP_INVALID_NAME;
	p->error.arg = name;
	return false;
      }
      clap_arg c_arg = p->arguments.args[idx];
      // flags do not need an option, so parsing is done
      if (c_arg.options & CLAP_ARG_FLAG) {
	p->results.values[idx] = CLAP_FLAG_TRUE;
	i++;
	continue;
      }
      // full names could be separated by space or =
      if (split_idx >= strlen(name)) {
	if ((int)i + 1 >= p->argc) {
	  p->error.kind = CLAP_NO_VALUE;
	  p->error.arg = name;
	  return false;
	}
	p->results.values[idx] = p->argv[i+1];
	i += 2;
	continue;
      }
      p->results.values[idx] = val;
      i++;
      continue;
    }
    // is an alias
    else if (arg[0] == '-') {
      if (strlen(arg) < 2) {
	p->error.kind = CLAP_NO_VALUE;
	p->error.arg = arg;
	return false;
      }
      char alias = arg[1];
      int idx = clap_find_arg_with_alias(p, alias);
      if (idx == -1) {
	p->error.kind = CLAP_INVALID_NAME;
	p->error.arg = &arg[1]; // alias will die so cant give ptr to it
	return false;
      }
      clap_arg c_arg = p->arguments.args[idx];
      // if its a flag, they can be grouped
      if (c_arg.options & CLAP_ARG_FLAG) {	
	char *remaining = (char *)&arg[1];
	// keep going until there are no flags left to set
	while (strlen(remaining) > 0) {
	  // get the next alias
	  char a = remaining[0];
	  int idx = clap_find_arg_with_alias(p, a);
	  if (idx == -1) {
	    p->error.kind = CLAP_INVALID_NAME;
	    p->error.arg = &remaining[0]; // a will die so cant give ptr to it
	    return false;
	  }
	  // if its not a flag, this is an error
	  if (!(p->arguments.args[idx].options & CLAP_ARG_FLAG)) {
	    p->error.kind = CLAP_NO_VALUE;
	    p->error.arg = &remaining[0]; // a will die so cant give ptr to it
	    return false;
	  }
	  p->results.values[idx] = CLAP_FLAG_TRUE;
	  remaining++;
	}
	i++;
	continue;
      }
      // not a flag
      if ((int)i + 1 >= p->argc) {
	p->error.kind = CLAP_NO_VALUE;
	p->error.arg = &arg[1]; // a will die so cant give ptr to it
	return false;
      }
      p->results.values[idx] = p->argv[i+1];
      i += 2;
      continue;
    }
    // is an unnamed argument
    if (p->unnamed_results.count >= unnamed_cnt) {
      p->error.kind = CLAP_TOO_MANY_UNNAMED_ARGS;
      p->error.arg = arg;
      return false;
    }
    p->unnamed_results.values[p->unnamed_results.count] = arg;
    p->unnamed_results.count++;
    i++;
  }
  
  if (p->unnamed_results.count < req_unnamed_cnt) {
    p->error.kind = CLAP_MISSING_UNNAMED_ARG;
    return false;
  }
  
  // check for any missed arguments, return the first one
  for (size_t i = 0; i < p->arguments.count; ++i) {
    if (p->arguments.args[i].options && CLAP_ARG_UNNAMED) {
      continue;
    }
    if (!p->results.values[i] && !(p->arguments.args[i].options & CLAP_ARG_OPTIONAL)) {
      p->error.kind = CLAP_MISSING_ARGS;
      p->error.arg = p->results.names[i];
      return false;
    }
  }
  return true;
}

// TODO
void clap_print_err(clap_parser p) {
  switch (p.error.kind) {
  case CLAP_NO_VALUE: { printf("ERROR: No value provided at %s\n", p.error.arg); break; }
  case CLAP_INVALID_NAME: { printf("ERROR: Invalid name at %s\n", p.error.arg); break; }
  case CLAP_MISSING_ARGS: {
    printf("ERROR: Arg(s) Missing, including `%s`\n", p.error.arg);
    break;
  }
  case CLAP_ALREADY_PARSED: {
    printf("ERROR: Parser called twice on the same inputs. \
	   Parsing is destructive.\n");
    break;
  }
  case CLAP_MISSING_UNNAMED_ARG: {
    printf("ERROR: Missing at least one required unnamed arg\n");
    break;
  }
  case CLAP_TOO_MANY_UNNAMED_ARGS: {
    printf("ERROR: Too many unnamed args at `%s`\n", p.error.arg);
    break;
  }
  case CLAP_NO_ERR: break;
  default: assert(false && "Unreachable");
  }
  clap_print_help(p);
}

void clap_print_help(clap_parser p) {
  if (p.program_desc) { printf("%s: %s\n", p.program_name, p.program_desc); }
  printf("Usage: %s <args>\n", p.program_name);
  for (size_t i = 0; i < p.arguments.count; ++i) {
    clap_arg arg = p.arguments.args[i];
    if (!(arg.options & CLAP_ARG_HIDDEN)) {
      if (arg.options & CLAP_ARG_UNNAMED) {
	printf("<unnamed>: %s\n", arg.description);
      } else if (arg.name && arg.alias && arg.description) {
	printf("%s (%c): %s\n", arg.name, arg.alias, arg.description);
      }
    }
  }
}

const char *clap_get(clap_parser p, const char *arg_name) {
  int idx = clap_find_arg_with_name(&p, arg_name);
  if (idx == -1) {
    assert(false && "Invalid name passed to clap_get");
  }
  return p.results.values[idx];
}

const char *clap_get_unnamed(clap_parser p, const size_t idx) {
  if (idx >= p.unnamed_results.count) return NULL;
  return p.unnamed_results.values[idx];
}


void clap_destroy(clap_parser *p) {
  p->arguments.count = 0;
  p->arguments.cap = 0;
  free(p->arguments.args);
  p->results.count = 0;
  free(p->results.names);
  free(p->results.values);
  p->unnamed_results.count = 0;
  free(p->unnamed_results.values);
}

#endif
