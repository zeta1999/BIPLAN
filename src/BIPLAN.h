
/* ______     ______           ______   _
  |      | | |      | |              | | \    |
  |_____/  | |______| |        ______| |  \   |
  |     \  | |        |       |      | |   \  |
  |______| | |        |______ |______| |    \_|
  Byte coded Interpreted Programming Language
  Giovanni Blu Mitolo 2017-2020 - gioscarab@gmail.com
      _____              _________________________
     |   | |            |_________________________|
     |   | |_______________||__________   \___||_________ |
   __|___|_|               ||          |__|   ||     |   ||
  /________|_______________||_________________||__   |   |D
    (O)                 |_________________________|__|___/|
                                           \ /            |
                                           (O)
  BIPLAN Copyright (c) 2017-2019, Giovanni Blu Mitolo All rights reserved.
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License. */

#pragma once
#include "BIPLAN_Defines.h"
#include "BIPLAN_Decoder.h"

class BIPLAN_Interpreter {
  public:
  /* TYPES ----------------------------------------------------------------- */
  struct param_type { BP_VAR_TYPE value; uint8_t id = BP_VARIABLES; };
  struct fun_type { char *address; param_type params[BP_PARAMS]; };
  struct def_type { char *address; uint8_t id; uint16_t params[BP_PARAMS]; };

  struct cycle_type {
    char *address;
    bool direction;
    BP_VAR_TYPE var;
    uint8_t var_id = BP_VARIABLES;
    BP_VAR_TYPE to;
  };
  /* BUFFERS --------------------------------------------------------------- */
  BP_VAR_TYPE       variables      [BP_VARIABLES];
  char              string         [BP_STRING_MAX_LENGTH];
  char              strings        [BP_STRINGS][BP_STRING_MAX_LENGTH];
  struct cycle_type cycles         [BP_CYCLE_DEPTH];
  struct fun_type   functions      [BP_FUN_DEPTH];
  struct def_type   definitions    [BP_MAX_FUNCTIONS];
  /* STATE ----------------------------------------------------------------- */
  char             *program_start  = NULL;
  uint8_t           cycle_id       = 0;
  uint8_t           fun_cycle_id   = 0;
  int               fun_id         = 0;
  bool              ended          = false;
  uint8_t           return_type    = 0;
  /* CALLBACKS ------------------------------------------------------------- */
  error_type        error_fun      = NULL;
  BPM_PRINT_TYPE    print_fun      = NULL;
  BPM_INPUT_TYPE    data_in_fun    = NULL;
  BPM_SERIAL_TYPE   serial_fun     = NULL;

  /* FINISHED -------------------------------------------------------------- */
  bool finished() { return ended || decoder_finished(); };

  /* RUN ------------------------------------------------------------------- */
  void run() { if(!decoder_finished()) statement(); };

  /* END PROGRAM ----------------------------------------------------------- */
  void end_call() { expect(BP_END); ended = true; };

  /* ERROR ----------------------------------------------------------------- */
  void error(char *position, const char *string) {
    error_fun(position, string);
    ended = true;
  };

  /* RESTART PROGRAM CALL -------------------------------------------------- */
  void restart_call() { set_default(); decoder_init(program_start); };

  /* INDEX LINES ----------------------------------------------------------- */
  void index_function_definitions(char* program) {
    uint16_t param;
    char *p = program;
    uint16_t l = 0;
    for(; *p && p; p++) {
      if(*p == BP_FUN_DEF) {
        param = 0;
        p++; l++;
        definitions[l].id = *p;
        for(uint8_t i = 0; i < BP_PARAMS; i++)
          definitions[l].params[i] = BP_PARAMS;
        p++;
        while(*p == BP_COMMA || *p == BP_L_RPARENT) {
          p++;
          if(*p == BP_ADDRESS) {
            p++;
            definitions[l].params[param++] = *p;
            p++;
          }
          if(*p == BP_R_RPARENT) break;
        }
        definitions[l].address = p + 2;
      }
    }
  };

  /* FIND FUNCTION DEFINITION ---------------------------------------------- */
  uint16_t find_definition(uint8_t d) {
    for(uint16_t i = 0; i < BP_MAX_FUNCTIONS; i++)
      if(definitions[i].id == d) return i;
    error(decoder_position(), BP_ERROR_FUNCTION_CALL);
    return 0;
  };

  /* FIND FUNCTION END ----------------------------------------------------- */
  void find_function_end() {
    uint8_t n = 0;
    while(decoder_get() != BP_R_RPARENT || (n > 1)) {
      if(decoder_get() == BP_ENDOFINPUT)
        error(decoder_position(), BP_ERROR_FUNCTION_END);
      if(decoder_get() == BP_L_RPARENT) n++;
      if(decoder_get() == BP_R_RPARENT) n--;
      decoder_next();
    }
  };

  /* FIND PARAM LIST LENGTH ------------------------------------------------ */
  uint8_t find_param_list_length(uint8_t d) {
    d = find_definition(d);
    uint8_t p = 0;
    while(definitions[d].params[p++] != BP_PARAMS);
    if(p < BP_PARAMS) return p;
    error(decoder_position(), BP_ERROR_PARAMETERS);
    ended = true;
    return 0;
  }

  /* INITIALIZE INTERPRETER ------------------------------------------------ */

  BIPLAN_Interpreter() { set_default(); };

  BIPLAN_Interpreter(
    char *program,
    error_type error,
    BPM_PRINT_TYPE print,
    BPM_INPUT_TYPE data_input,
    BPM_SERIAL_TYPE s
  ) {
    initialize(program, error, print, data_input, s);
  };

  void initialize(
    char *program,
    error_type error,
    BPM_PRINT_TYPE print,
    BPM_INPUT_TYPE data_input,
    BPM_SERIAL_TYPE s
  ) {
    program_start = program;
    set_default();
    index_function_definitions(program);
    decoder_init(program);
    serial_fun = s;
    error_fun = error;
    print_fun = print;
    data_in_fun = data_input;
  };

  void set_default() {
    cycle_id = 0;
    fun_id = 0;
    ended = false;
  };

  /* EXPECT A CERTAIN CODE, OTHERWISE THROW ERROR -------------------------- */
  void expect(uint8_t c) {
    if(c != decoder_get()) error(decoder_position(), BP_ERROR_SYMBOL);
    else decoder_next();
  };

  /* IGNORE A CERTAIN CODE ------------------------------------------------- */
  bool ignore(uint8_t c) {
    if(c == decoder_get()) {
      decoder_next();
      return true;
    } return false;
  };

  /* GET VARIABLE ---------------------------------------------------------- */
  BP_VAR_TYPE get_variable(int n) {
    if(n >= 0 && n <= BP_VARIABLES) return variables[n];
    error(decoder_position(), BP_ERROR_VARIABLE_GET);
    return 0;
  };

  /* SET VARIABLE ---------------------------------------------------------- */
  void set_variable(int n, BP_VAR_TYPE v) {
    if(n >= 0 && n <= BP_VARIABLES) variables[n] = v;
    else error(decoder_position(), BP_ERROR_VARIABLE_SET);
  };

  /* UNARY OPERATOR -------------------------------------------------------- */
  int8_t unary() {
    int8_t u = 0;
    while(decoder_get() == BP_INCREMENT || decoder_get() == BP_DECREMENT) {
      if(decoder_get() == BP_INCREMENT) u++; else u--;
      decoder_next();
    } return u;
  };

  /* NUMERIC VARIABLE: 1234 -------------------------------------------------*/
  BP_VAR_TYPE var_factor() {
    BP_VAR_TYPE v;
    int16_t pre = unary(), post = 0, id = BP_VARIABLES;
    uint8_t type = decoder_get();
    decoder_next();
    id = *(decoder_position() - 1) - BP_ADDRESS_OFFSET;
    if(type == BP_ADDRESS) v = get_variable(id);
    else if(decoder_get() == BP_ACCESS) {
      decoder_next();
      v = strings[id][expression()];
      expect(BP_ACCESS_END);
      return_type = BP_ACCESS;
    } else {
      return_type = BP_S_ADDRESS;
      v = id;
    }
    if(decoder_get() == BP_INCREMENT || decoder_get() == BP_DECREMENT)
      post = unary();
    if((pre != 0) || (post != 0)) set_variable(id, v + pre + post);
    return v + pre;
  };

  /* FACTOR: (n) ------------------------------------------------------------*/
  BP_VAR_TYPE factor() {
    BP_VAR_TYPE v = 0;
    bool bitwise_not = (decoder_get() == BP_BITWISE_NOT);
    ignore(BP_BITWISE_NOT);
    switch(decoder_get()) {
      case BP_VAR_ACCESS:
        decoder_next(); v = variables[expression()];
        expect(BP_ACCESS_END); break;
      case BP_STR_ACCESS:
        decoder_next(); v = (BP_VAR_TYPE)strings[expression()];
        expect(BP_ACCESS_END); break;
      case BP_NUMBER: v = atoi(decoder_position()); expect(BP_NUMBER); break;
      case BP_DREAD: decoder_next(); return BPM_IO_READ(expression());
      case BP_MILLIS: decoder_next(); v = (BPM_MILLIS() % 32767); break;
      case BP_AGET: decoder_next(); v = BPM_AREAD(expression()); break;
      case BP_RND: decoder_next();  v = random_call(); break;
      case BP_SQRT: decoder_next(); v = sqrt(expression()); break;
      case BP_FUNCTION: v = function_call(); decoder_next(); break;
      case BP_SERIAL_RX:
        v = BPM_SERIAL_READ(serial_fun); decoder_next(); break;
      case BP_INPUT: v = BPM_INPUT(data_in_fun); decoder_next(); break;
      case BP_INPUT_AV:
        v = BPM_INPUT_AVAILABLE(serial_fun); decoder_next(); break;
      case BP_SERIAL_AV:
        v = BPM_SERIAL_AVAILABLE(serial_fun); decoder_next(); break;
      case BP_L_RPARENT:
        decoder_next(); v = relation(); expect(BP_R_RPARENT); break;
      case BP_SIZEOF: v = sizeof_call(); break;
      case BP_STOI: v = stoi_call(); break;
      default: v = var_factor();
    }
    return (bitwise_not) ? ~((unsigned)v) : v;
  };

  /* TERM: *, /, % ----------------------------------------------------------*/
  BP_VAR_TYPE term() {
    BP_VAR_TYPE f1 = 0, f2 = 0;
    uint8_t operation;
    f1 = factor();
    operation = decoder_get();
    while(operation == BP_MULT || operation == BP_DIV || operation == BP_MOD) {
      decoder_next();
      f2 = factor();
      switch(operation) {
        case BP_MULT: f1 = f1 * f2; break;
        case BP_DIV:  f1 = f1 / f2; break;
        case BP_MOD:  f1 = f1 % f2; break;
      } operation = decoder_get();
    } return f1;
  };

  /* EXPRESSION +, -, &, | --------------------------------------------------*/
  BP_VAR_TYPE expression() {
    BP_VAR_TYPE t1 = 0, t2 = 0;
    t1 = term();
    uint8_t operation = decoder_get();
    while(
      operation == BP_PLUS || operation == BP_MINUS || operation == BP_AND ||
      operation == BP_OR || operation == BP_XOR || operation == BP_L_SHIFT ||
      operation == BP_R_SHIFT
    ) {
      decoder_next();
      t2 = term();
      switch(operation) {
        case BP_PLUS:    t1 = t1 + t2;  break;
        case BP_MINUS:   t1 = t1 - t2;  break;
        case BP_AND:     t1 = t1 & t2;  break;
        case BP_OR:      t1 = t1 | t2;  break;
        case BP_XOR:     t1 = t1 ^ t2;  break;
        case BP_L_SHIFT: t1 = t1 << t2; break;
        case BP_R_SHIFT: t1 = t1 >> t2; break;
      } operation = decoder_get();
    } return t1;
  };

  /* RELATION <, >, = ------------------------------------------------------ */
  BP_VAR_TYPE relation() {
    BP_VAR_TYPE r1 = expression(), r2 = 0;
    uint8_t operation = decoder_get();
    while(
      operation == BP_EQ || operation == BP_NOT_EQ || operation == BP_LTOEQ ||
      operation == BP_GTOEQ || operation == BP_LT || operation == BP_GT ||
      operation == BP_LOGIC_OR || operation == BP_LOGIC_AND
    ) {
      decoder_next();
      r2 = expression();
      switch(operation) {
        case BP_NOT_EQ:    r1 = r1 != r2; break;
        case BP_EQ:        r1 = r1 == r2; break;
        case BP_GTOEQ:     r1 = r1 >= r2; break;
        case BP_LTOEQ:     r1 = r1 <= r2; break;
        case BP_LOGIC_OR:  r1 = r1 || r2; break;
        case BP_LOGIC_AND: r1 = r1 && r2; break;
        case BP_LT:        r1 = r1 <  r2; break;
        case BP_GT:        r1 = r1 >  r2; break;
      } operation = decoder_get();
    } return r1;
  };

  /* PRINT ----------------------------------------------------------------- */
  void print_call() {
    decoder_next();
    ignore(BP_L_RPARENT);
    do {
      BP_VAR_TYPE v = 0;
      bool is_char = (decoder_get() == BP_CHAR);
      if(is_char || (decoder_get() == BP_COMMA)) decoder_next();
      if(decoder_get() == BP_STR_ACCESS) {
        decoder_next();
        BPM_PRINT_WRITE(print_fun, strings[relation()]);
        expect(BP_ACCESS_END);
      } else if(decoder_get() == BP_STRING) {
        decoder_string(string, sizeof(string));
        BPM_PRINT_WRITE(print_fun, string);
        decoder_next();
      } else if(decoder_get() == BP_S_ADDRESS) {
        v = var_factor();
        if(return_type == BP_ACCESS) {
          if(is_char) BPM_PRINT_WRITE(print_fun, (char)v);
          else BPM_PRINT_WRITE(print_fun, v);
        } else BPM_PRINT_WRITE(print_fun, strings[v]);
      } else {
        v = relation();
        if(return_type == BP_S_ADDRESS)
          BPM_PRINT_WRITE(print_fun, strings[v]);
        else if(is_char) BPM_PRINT_WRITE(print_fun, (char)v);
        else BPM_PRINT_WRITE(print_fun, v);
      }
    } while(
      decoder_get() != BP_SEMICOLON  && decoder_get() != BP_CR &&
      decoder_get() != BP_R_RPARENT  && decoder_get() != BP_SEMICOLON
    );
    ignore(BP_R_RPARENT);
  };

  /* BLOCK CALL ------------------------------------------------------------ */
  void skip_block() {
    uint16_t id = 1;
    do {
      if(decoder_get() == BP_IF) id++;
      if(decoder_get() == BP_ENDIF) id--;
      if((decoder_get() == BP_ELSE) && (id == 1)) return;
      if(decoder_get() == BP_ENDOFINPUT)
        error(decoder_position(), BP_ERROR_BLOCK);
      decoder_next();
    } while(id >= 1);
  }

  /* ELSE (if arrived here, else should be ignored) ------------------------ */

  void else_call() {
    decoder_next();
    ignore(BP_CR);
    skip_block();
  };

  /* IF -------------------------------------------------------------------- */
  void if_call() {
    decoder_next();
    int r = relation();
    ignore(BP_CR);
    if(!r) skip_block();
    if(decoder_get() == BP_ELSE) {
      decoder_next();
      ignore(BP_CR);
      if(r) skip_block();
    }
  };

  /* ASSIGN VALUE TO VARIABLE ---------------------------------------------- */
  void variable_assignment_call() {
    if(decoder_get() == BP_VAR_ACCESS) {
      decoder_next();
      BP_VAR_TYPE vi = relation();
      expect(BP_ACCESS_END);
      return set_variable(vi, relation());
    } else {
      decoder_next();
      int vi = *(decoder_position() - 1) - BP_ADDRESS_OFFSET;
      set_variable(vi, relation());
    }
  };

  /* ASSIGN VALUE TO STRING ------------------------------------------------ */
  void string_assignment_call() {
    int ci = BP_STRING_MAX_LENGTH, si;
    bool str_acc = (decoder_get() == BP_STR_ACCESS);
    decoder_next();
    if(str_acc) {
      si = expression();
      expect(BP_ACCESS_END);
    } else si = *(decoder_position() - 1) - BP_ADDRESS_OFFSET;
    if(decoder_get() == BP_ACCESS) {
      decoder_next();
      ci = expression();
      expect(BP_ACCESS_END);
    }
    if(ci == BP_STRING_MAX_LENGTH) {
      if(decoder_get() == BP_STRING) {
        decoder_string(strings[si], sizeof(strings[si]));
        expect(BP_STRING);
        decoder_next();
      } else if(decoder_get() == BP_S_ADDRESS) {
        decoder_next();
        ci = *(decoder_position() - 1) - BP_ADDRESS_OFFSET;
        for(uint16_t i = 0; i < sizeof(strings[ci]); i++)
          strings[si][i] = strings[ci][i];
        decoder_next();
      }
    } else {
      if(decoder_get() == BP_STRING) {
        expect(BP_STRING);
        strings[si][ci] = (char)(*(decoder_position() - 2));
        decoder_next();
      } else {
        strings[si][ci] = (uint8_t)expression();
        decoder_next();
      }
    }
  };

  /* RETURN ---------------------------------------------------------------- */
  BP_VAR_TYPE return_call() {
    BP_VAR_TYPE rel = 0;
    decoder_next();
    if(fun_id > 0) {
      if(decoder_get() != BP_CR) rel = relation();
      fun_id--;
      for(int i = 0; i < BP_PARAMS; i++)
        if(functions[fun_id].params[i].id != BP_VARIABLES) {
          set_variable( // Set back global variables;
            functions[fun_id].params[i].id,
            functions[fun_id].params[i].value
          );
          functions[fun_id].params[i].id = BP_VARIABLES;
          functions[fun_id].params[i].value = 0;
        }
      decoder_goto(functions[fun_id].address);
      cycle_id = fun_cycle_id;
      return rel;
    } else {
      error(decoder_position(), BP_ERROR_RETURN);
      return 0;
    }
  };

  /* FUNCTION -------------------------------------------------------------- */
  BP_VAR_TYPE function_call() {
    fun_cycle_id = cycle_id;
    int16_t i = 0;
    char *start = decoder_position();
    find_function_end();
    char *end = decoder_position();
    decoder_goto(start);
    expect(BP_FUNCTION);
    uint16_t f = *(decoder_position() - 1), v = BP_VARIABLES;
    uint8_t  p = find_param_list_length(f);
    if((*(decoder_position() + 1) == BP_R_RPARENT))
      expect(BP_L_RPARENT); // If call with no params
    else if(decoder_get() == BP_L_RPARENT)
      do {
        v = definitions[find_definition(f)].params[i] - BP_ADDRESS_OFFSET;
        decoder_next();
        functions[fun_id].params[i].id = v;
        functions[fun_id].params[i].value = get_variable(v);
        set_variable(v, relation()); // Set the value of local variable
        i++;
      } while((i < BP_PARAMS) && (i < p - 1));
    expect(BP_R_RPARENT);
    ignore(BP_CR);
    if(fun_id < BP_FUN_DEPTH) {
      functions[fun_id++].address = end;
      decoder_goto(definitions[find_definition(f)].address);
      while(decoder_get() != BP_RETURN) statement();
      return return_call();
    } else {
      error(decoder_position(), BP_ERROR_FUNCTION_CALL);
      return 0;
    }
  };

  /* CONTINUE -------------------------------------------------------------- */
  void continue_call() {
    uint16_t id = cycle_id;
    while(cycle_id >= id) {
      if(decoder_get() == BP_NEXT || decoder_get() == BP_REDO)
        if(cycle_id == id--) break;
      if(decoder_get() == BP_WHILE || decoder_get() == BP_FOR) id++;
      decoder_next();
    }
  };

  /* BREAK ----------------------------------------------------------------- */
  void break_call() { continue_call(); decoder_next(); cycle_id--; };

  /* CYCLE ----------------------------------------------------------------- */
  void for_call() {
    decoder_next();
    ignore(BP_L_RPARENT);
    expect(BP_ADDRESS);
    uint8_t vi = *(decoder_position() - 1) - BP_ADDRESS_OFFSET;
    BP_VAR_TYPE e;
    if(cycle_id < BP_CYCLE_DEPTH) // temporarily stash global variable
      cycles[cycle_id].var = get_variable(vi);
    set_variable(vi, expression());
    expect(BP_COMMA);
    e = expression();
    ignore(BP_R_RPARENT);
    ignore(BP_CR);
    if(cycle_id < BP_CYCLE_DEPTH) {
      cycles[cycle_id].var_id    = vi;
      cycles[cycle_id].address   = decoder_position();
      cycles[cycle_id].direction = (get_variable(vi) < e) ? 1 : 0;
      cycles[cycle_id++].to      = e;
    } else error_fun(decoder_position(), BP_ERROR_CYCLE_MAX);
  };

  /* NEXT ------------------------------------------------------------------ */
  void next_call() {
    decoder_next();
    if(cycle_id) {
      int vi = cycles[cycle_id - 1].var_id;
      BP_VAR_TYPE v = get_variable(vi);
      bool d = cycles[cycle_id - 1].direction;
      if(
        ((d)  && v < cycles[cycle_id - 1].to) ||
        ((!d) && v > cycles[cycle_id - 1].to)
      ) {
        decoder_goto(cycles[cycle_id - 1].address);
        set_variable(vi, (d) ? ++v : --v);
      } else { // Set back global variable and reset cycle variable buffer
        if(vi != BP_VARIABLES) set_variable(vi, cycles[cycle_id - 1].var);
        cycles[--cycle_id].var_id = BP_VARIABLES;
      }
    } else error(decoder_position(), BP_ERROR_CYCLE_NEXT);
  };

  /* WHILE ----------------------------------------------------------------- */
  void while_call() {
    char *start = decoder_position();
    decoder_next();
    if(relation()) {
      if(cycle_id < BP_CYCLE_DEPTH) cycles[cycle_id++].address = start;
      else error(decoder_position(), BP_ERROR_WHILE_MAX);
    } else {
      uint16_t c = 1;
      while(decoder_get() != BP_REDO && (c > 0)) {
        if(decoder_get() == BP_WHILE) c++;
        if(decoder_get() == BP_REDO) c--;
        decoder_next();
      }
    }
  };

  /* REDO ------------------------------------------------------------------ */
  void redo_call() {
    decoder_next();
    char *end = decoder_position();
    if(cycle_id) {
      decoder_goto(cycles[cycle_id - 1].address);
      decoder_next();
      if(relation()) decoder_next();
      else {
        decoder_goto(end);
        cycle_id--;
      }
    } else error(decoder_position(), BP_ERROR_REDO);
  };

  /* DIGITAL WRITE --------------------------------------------------------- */
  void digitalWrite_call() {
    decoder_next();
    ignore(BP_L_RPARENT);
    BP_VAR_TYPE pin = expression();
    expect(BP_COMMA);
    BPM_IO_WRITE(pin, expression());
    ignore(BP_R_RPARENT);
  };

  /* PINMODE --------------------------------------------------------------- */
  void pinMode_call() {
    decoder_next();
    ignore(BP_L_RPARENT);
    BP_VAR_TYPE pin = expression();
    expect(BP_COMMA);
    BPM_IO_MODE(pin, expression());
    ignore(BP_R_RPARENT);
  }

  /* WAIT/DELAY ------------------------------------------------------------ */
  void delay_call() {
    decoder_next();
    ignore(BP_L_RPARENT);
    BPM_DELAY(expression());
    ignore(BP_R_RPARENT);
  };

  /* RANDOM CALL (min optional, max optional) ------------------------------ */
  BP_VAR_TYPE random_call() {
    ignore(BP_L_RPARENT);
    BP_VAR_TYPE a = expression(), b;
    if(decoder_get() == BP_COMMA) {
      decoder_next();
      b = BPM_RANDOM(a, expression());
    } else b = BPM_RANDOM(a);
    ignore(BP_R_RPARENT);
    return b;
  };

  /* SERIAL TX CALL -------------------------------------------------------- */
  void serial_tx_call() {
    decoder_next();
    ignore(BP_L_RPARENT);
    if(decoder_get() == BP_STRING) {
      decoder_string(string, sizeof(string));
      for(uint16_t i = 0; i < BP_STRING_MAX_LENGTH; i++)
        BPM_SERIAL_WRITE(serial_fun, string[i]);
      decoder_next();
    } else if(decoder_get() == BP_S_ADDRESS) {
      decoder_next();
      uint8_t id = *(decoder_position() - 1) - BP_ADDRESS_OFFSET;
      for(uint16_t i = 0; i < sizeof(strings[id]); i++)
        BPM_SERIAL_WRITE(serial_fun, strings[id][i]);
    } else BPM_SERIAL_WRITE(serial_fun, relation());
    ignore(BP_R_RPARENT);
  };

  /* STRING LENGTH CALL ---------------------------------------------------- */
  BP_VAR_TYPE sizeof_call() {
    decoder_next();
    ignore(BP_L_RPARENT);
    if(decoder_get() == BP_S_ADDRESS) {
      decoder_next();
      BP_VAR_TYPE l =
        strlen(strings[*(decoder_position() - 1) - BP_ADDRESS_OFFSET]);
      ignore(BP_R_RPARENT);
      return l;
    } else if(decoder_get() == BP_ADDRESS) {
      decoder_next();
      ignore(BP_R_RPARENT);
      return sizeof(BP_VAR_TYPE);
    } return 0;
  };

  /* STOI - CONVERTS STRINGS TO NUMBER ------------------------------------- */
  BP_VAR_TYPE stoi_call() {
    BP_VAR_TYPE v = 0;
    decoder_next();
    ignore(BP_L_RPARENT);
    if(decoder_get() == BP_S_ADDRESS) {
      decoder_next();
      v = BPM_STOI(strings[*(decoder_position() - 1) - BP_ADDRESS_OFFSET]);
    }
    if(decoder_get() == BP_STRING) {
      decoder_next();
      v = BPM_STOI(string);
    }
    ignore(BP_R_RPARENT);
    return v;
  };

  /* STATEMENTS: (print, if, return, for, while...) ------------------------ */
  void statement() {
    return_type = 0;
    switch(decoder_get()) {
      case BP_SEMICOLON:  ; // Same as BP_CR
      case BP_CR:         ; // Same as BP_ENDIF
      case BP_ENDIF:      decoder_next(); return;
      case BP_FUNCTION:   function_call(); expect(BP_R_RPARENT);
                          ignore(BP_CR); return;
      case BP_VAR_ACCESS: ; // assignment by reference
      case BP_ADDRESS:    return variable_assignment_call();
      case BP_STR_ACCESS: ; // assignment by reference
      case BP_S_ADDRESS:  return string_assignment_call();
      case BP_INCREMENT:  var_factor(); ignore(BP_CR); return;
      case BP_DECREMENT:  var_factor(); ignore(BP_CR); return;
      case BP_RETURN:     return_call(); return;
      case BP_IF:         return if_call();
      case BP_ELSE:       return else_call();
      case BP_FOR:        return for_call();
      case BP_WHILE:      return while_call();
      case BP_REDO:       return redo_call();
      case BP_NEXT:       return next_call();
      case BP_BREAK:      return break_call();
      case BP_CONTINUE:   return continue_call();
      case BP_PRINT:      return print_call();
      case BP_END:        return end_call();
      case BP_DWRITE:     return digitalWrite_call();
      case BP_PINMODE:    return pinMode_call();
      case BP_DELAY:      return delay_call();
      case BP_RESTART:    return restart_call();
      case BP_SERIAL_TX:  return serial_tx_call();
      default: error(decoder_position(), BP_ERROR_STATEMENT);
    }
  };
};