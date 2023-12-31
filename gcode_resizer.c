/**
 * A simple utility to resize a gcode file and move it to the origin of 0.0.
 * It is used to process output from DrawingBot or Inkscape and was created because
 * the existing mostly Python tools are very slow.
 *
 * @author ppet36
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define M_X 0x01
#define M_Y 0x02
#define M_Z 0x04
#define M_F 0x08
#define M_I 0x10
#define M_J 0x20
#define M_K 0x40

#define DMAX 99999999

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })


// The file is processed in two passes.
// The first evaluates the size and displacement, and the second performs the transformation and writes the modified gcode.
enum Mode {
  PARSE,
  RESIZE,
  MAX
} mode = PARSE;

// Parser state.
enum State {
  NORMAL,
  COMMENT,
  CMD,
  X,
  Y,
  Z,
  I,
  J,
  K,
  F
};

// Structure filled for single command.
struct GCodeCommand {
  char kw;
  char cmd [16];
  uint8_t val_mask;
  double x;
  double y;
  double z;
  double i;
  double j;
  double k;
  double f;
};

double max_x = 0;
double max_y = 0;
double min_x = DMAX;
double min_y = DMAX;

double last_move_x = 0;
double last_move_y = 0;
uint8_t last_move_mask = 0x00;

double multiplier = 1;
double offset_x = 0;
double offset_y = 0;

bool rotate = false;
bool resize = false;

char buffer [1024];
int buffer_pos = 0;


// Emits command to standard out.
void emit_command (struct GCodeCommand *cmd) {
  printf ("%c%s ", cmd->kw, cmd->cmd);

  if (cmd->val_mask & M_X) {
    printf ("X%.5f ", cmd->x);
  }

  if (cmd->val_mask & M_Y) {
    printf ("Y%.5f ", cmd->y);
  }

  if (cmd->val_mask & M_Z) {
    printf ("Z%.5f ", cmd->z);
  }

  if (cmd->val_mask & M_I) {
    printf ("I%.5f ", cmd->i);
  }

  if (cmd->val_mask & M_J) {
    printf ("J%.5f ", cmd->j);
  }

  if (cmd->val_mask & M_K) {
    printf ("K%.5f ", cmd->k);
  }

  if (cmd->val_mask & M_F) {
    printf ("F%.2f ", cmd->f);
  }

  puts("");
}

// Updates min/max coordinates
void update_min_max (double x, double y, uint8_t mask) {
  if (mask & M_X) {
    max_x = max (x, max_x);
    min_x = min (x, min_x);
  }
 
  if (mask & M_Y) {
    max_y = max (y, max_y);
    min_y = min (y, min_y);
  }
}

// Processes command according to current mode.
void process_command (struct GCodeCommand *cmd) {
  double pom, x_ofs, x, y;
  int g;

  switch (mode) {
    case PARSE :
      if (cmd->kw == 'G') {
        g = atoi(cmd->cmd);

        if ((g != 1) && (g != 2) && (g != 3)) {
          last_move_x = cmd->x;
          last_move_y = cmd->y;
          last_move_mask = cmd->val_mask;

          break;
        } else {
          update_min_max (last_move_x, last_move_y, last_move_mask);
          last_move_mask = 0x00;
        }
      } else {
        break;
      }

      update_min_max (cmd->x, cmd->y, cmd->val_mask);
    break;
    case RESIZE :
      if (resize) {
        x = (cmd->x - offset_x) * multiplier;
        y = (cmd->y - offset_y) * multiplier;
 
        if (cmd->kw == 'G') {
          if (atoi (cmd->cmd) == 0) {
            x = max(0, x);
            y = max(0, y);
          }        
        }
 
        if (cmd->val_mask & M_X) {
          cmd->x = x;
        }
 
        if (cmd->val_mask & M_Y) {
          cmd->y = y;
        }
 
        if (cmd->val_mask & M_I) {
          cmd->i *= multiplier;
        }
 
        if (cmd->val_mask & M_J) {
          cmd->j *= multiplier;
        }
      }

      if (rotate) {
        x_ofs = (max_y - min_y) * multiplier;

        if ((cmd->val_mask & M_X) && (cmd->val_mask & M_Y)) {
          pom = cmd->x;
          cmd->x = (cmd->y * -1.0) + x_ofs;
          cmd->y = pom;
        } else if (cmd->val_mask & M_X) {
          cmd->y = cmd->x;
          cmd->val_mask -= M_X;
          cmd->val_mask |= M_Y;
        } else if (cmd->val_mask & M_Y) {
          cmd->x = (cmd->y * -1.0) + x_ofs;
          cmd->val_mask -= M_Y;
          cmd->val_mask |= M_X;
        }

        if ((cmd->val_mask & M_I) && (cmd->val_mask & M_J)) {
          pom = cmd->i;
          cmd->i = cmd->j * -1.0;
          cmd->j = pom;
        } else if (cmd->val_mask & M_I) {
          cmd->j = cmd->i;
          cmd->val_mask -= M_I;
          cmd->val_mask |= M_J;
        } else if (cmd->val_mask & M_J) {
          cmd->i = cmd->j * -1.0;
          cmd->val_mask -= M_J;
          cmd->val_mask |= M_I;
        }
      }

      emit_command (cmd);
    break;
    case MAX : break;
  }
}

// Parse double. Adds character to buffer and if value is completed
// returns true and fill value to val at pointer.
bool add_char_dbuffer (char ch, double *val) {
   if ((buffer_pos < 1) && isspace (ch)) {
     return false;
   }

   if ((ch == '-') || (ch == '.') || ((ch >= '0') && (ch <= '9'))) {
     buffer[buffer_pos++] = ch;
     return false;
   } else {
     buffer[buffer_pos] = 0;
     (* val) = atof (buffer);
     buffer_pos = 0;
     return true;
   }
}

// Parses gcode line and process it.
void parse_line (char *line) {
  int i, n;
  char ch;
  enum State state;
  struct GCodeCommand gcmd;

  n = strlen(line);
  state = NORMAL;
  memset (&gcmd, 0, sizeof(struct GCodeCommand));

  for (i = 0; i < n; i++) {
    ch = line[i];

    switch (state) {
      case NORMAL :
        switch (ch) {
          case 'G' : case 'g' : case 'M' : case 'm' : case 'T' : case 't' : 
            gcmd.kw = ch;
            state = CMD;
          break;
          case 'X' : case 'x' :
            state = X;
          break;
          case 'Y' : case 'y' :
            state = Y;
          break;
          case 'Z' : case 'z' :
            state = Z;
          break;
          case 'F' : case 'f' :
            state = F;
          break;
          case 'I' : case 'i' :
            state = I;
          break;
          case 'J' : case 'j' :
            state = J;
          break;
          case 'K' : case 'k' :
            state = K;
          break;
          case ';' : case '(' : case '%' :
            state = COMMENT;
          break;
          default :
            if (!isspace (ch)) {
              fprintf (stderr, "Unknown command \"%c\" in \"%s\"!", ch, line);
              exit (1);
            }
          break;
        }
      break;
      case CMD :
        if ((ch == '.') || ((ch >= '0') && (ch <= '9'))) {
          buffer[buffer_pos++] = ch;
        } else {
          memcpy (&gcmd.cmd, buffer, buffer_pos);
          buffer_pos = 0;
          state = NORMAL;
          i--;
        }
      break;
      case X :
        if (add_char_dbuffer (ch, &gcmd.x)) {
          state = NORMAL;
          gcmd.val_mask |= M_X;
          i--;
        }
      break;
      case Y :
        if (add_char_dbuffer (ch, &gcmd.y)) {
          state = NORMAL;
          gcmd.val_mask |= M_Y;
          i--;
        }
      break;
      case Z :
        if (add_char_dbuffer (ch, &gcmd.z)) {
          state = NORMAL;
          gcmd.val_mask |= M_Z;
          i--;
        }
      break;
      case I :
        if (add_char_dbuffer (ch, &gcmd.i)) {
          state = NORMAL;
          gcmd.val_mask |= M_I;
          i--;
        }
      break;
      case J :
        if (add_char_dbuffer (ch, &gcmd.j)) {
          state = NORMAL;
          gcmd.val_mask |= M_J;
          i--;
        }
      break;
      case K :
        if (add_char_dbuffer (ch, &gcmd.k)) {
          state = NORMAL;
          gcmd.val_mask |= M_K;
          i--;
        }
      break;
      case F :
        if (add_char_dbuffer (ch, &gcmd.f)) {
          state = NORMAL;
          gcmd.val_mask |= M_F;
          i--;
        }
      break;
      case COMMENT : break;
    }
  }

  if (strlen(gcmd.cmd) > 0) {
    process_command (&gcmd);
  }
}


void print_usage (char* command) {
  fputs ("A simple utility to resize the passed gcode to fill the requested dimensions.\nGcode is also shifted to origin 0,0. The modified gcode is written to standard output.\n\n", stderr);
  fprintf (stderr, "Usage: %s <gcode_file> [--resize <<reqSizeX[%%]>x[reqSizeY]>] [--rotate] >out.gcode\n\n", command);
}

// main()
int main (int argc, char** argv) {
  char *file_name = NULL, *req_size = NULL, *token, *delimiter="x", *line = NULL;
  int i;
  size_t len;
  ssize_t readed;
  double req_x = 0, req_y = 0, req = 0, d1, d2;
  bool percent = false;
  FILE *fp;

  if (argc < 2) {
    print_usage (argv[0]);
    return 1;
  }

  for (i = 1; i < argc; i++) {
    if (!strcmp (argv[i], "--rotate")) {
      rotate = true;
    } else if (!strcmp (argv[i], "--resize")) {
      if (i < argc - 1) {
        req_size = argv[++i];
        resize = true;
      } else {
        fputs ("Parameter --resize requires size specification!\n", stderr);
        print_usage (argv[0]);
        return 1;
      }
    } else if (!strcmp (argv[i], "--help")) {
      print_usage (argv[0]);
      return 0;
    } else if (file_name == NULL) {
      file_name = argv[i];
    } else {
      fprintf (stderr, "Unexpected parameter %s!\n\n", argv[i]);
      print_usage (argv[0]);
      return 1;
    }
  }

  if (!file_name) {
    fputs ("No input file provided!\n\n", stderr);
    print_usage (argv[0]);
    return 1;
  }

  if (resize) {
    token = strtok (req_size, delimiter);
    while (token) {
      if (req_x < 1) {
        if (strchr (token, '%')) {
          percent = true;
        }
        req_x = atof (token);
      } else if (req_y < 1) {
        req_y = atof (token);
      }
      token = strtok (NULL, delimiter);
    }
  
    if ((req_x < 1) && (req_y < 1)) {
      fprintf (stderr, "Invalid size \"%s\" requested!\n", req_size);
      return 1;
    }

    if (req_y < 1) {
      req = req_x;
      req_x = 0;
      req_y = 0;
    }
  }


  for (mode = PARSE; mode < MAX; mode++) {

    fp = fopen (file_name, "r");
    if (fp == NULL) {
      fprintf (stderr, "File %s not found!\n", file_name);
      return 1;
    }

    if (mode == PARSE) {
      fprintf (stderr, "Processing file %s...\n", file_name);
    }

    while ((readed = getline (&line, &len, fp)) != -1) {
      parse_line (line);
    }
  
    fclose (fp);

    if (mode == PARSE) {
      fprintf (stderr, "GCode:\tMinX=%.5f, MinY=%.5f\n", min_x, min_y);
      fprintf (stderr, "GCode:\tMaxX=%.5f, MaxY=%.5f\n\n", max_x, max_y);

      if ((max_x < 1) || (max_y < 1) || (min_x > DMAX - 1) || (min_y > DMAX - 1)) {
        fprintf (stderr, "No moves in gcode file %s!\n", file_name);
        return 1;
      } 

      if (resize) {
        if (req > 0) {
          if (percent) {
            fprintf (stderr, "Req:\tResize %.2f%%\n", req);
  
            d1 = d2 = req / 100.0;
          } else {
            fprintf (stderr, "Req:\tResize MaxSide=%.5f\n", req);
         
            d1 = req / (max_x - min_x);
            d2 = req / (max_y - min_y);
          }
        } else {
          fprintf (stderr, "Req:\tResize MaxX=%.5f, MaxY=%.5f\n", req_x, req_y);
  
          d1 = req_x / (max_x - min_x);
          d2 = req_y / (max_y - min_y);
        }

        multiplier = (d1 > d2) ? d2 : d1;
        offset_x = min_x;
        offset_y = min_y;
 
        fprintf (stderr, "\t> Mult:  %.10f\n", multiplier);
        fprintf (stderr, "\t> Offs:  %.5fx%.5f\n", offset_x, offset_y);
      }

      if (rotate) {
        fputs ("Req:    Rotate\n", stderr);
      }
    } else {
      fputs ("\nFile successfully processed...\n", stderr);
    }
  }

  return 0;
}
