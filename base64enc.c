#include <stdio.h>  // Standard input and output
#include <errno.h>  // Access to errno and Exxx macros
#include <stdint.h> // Extra fixed-width data types
#include <string.h> // String utilities
#include <err.h>    // Convenience functions for error reporting (non-standard)

static char const b64_alphabet[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz"
  "0123456789"
  "+/";

int main(int argc, char *argv[])
{
    FILE *fp = stdin; /* keeps file pointer in scope of for loop */
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [FILE]\n", argv[0]);
        errx(1, "Too many arguments");
    } else if (argc == 2 && strcmp(argv[1], "-")) {
        fp = fopen(argv[1], "r"); /* open FILE */
        if (fp == NULL) { /* if file not found quit with error message */
          fprintf(stderr, "Usage: %s [FILE]\n", argv[0]);
          errx(1, "Failed to open the file as %s doesn't exist.\n", argv[1]);
        }
    } else {
        fp = stdin; /* use stdin instead */
    }
      int output_count = 0; /* used for keeping count for new line at end of program */
      int wrapping_count = 0; /* used to keep count of wrapping every 76 char */
      for (;;) {
        uint8_t input_bytes[3] = {0}; /* set array of 3 bytes to 0 */
        size_t n_read = fread(input_bytes, sizeof(uint8_t), sizeof(input_bytes), fp); /* reads the 3 bytes at a time from file */
        if (n_read != 0) {
            /* Have data */
            int alph_ind[4] = {0}; /* 3 bytes (24 bits) altogether each index 6 bits meaning there are 4 indices */
            alph_ind[0] = input_bytes[0] >> 2; /* shifts bits right 2 and saves the original 6 bits in index 0 */
            alph_ind[1] = (input_bytes[0] << 4 | input_bytes[1] >> 4) & 0x3Fu; /* gets next 6 bits (0x3Fu = 00111111 with & keeps the last 6 bits) for index 1 */
            alph_ind[2] = ((input_bytes[1] & 0x0Fu) << 2) | (input_bytes[2] >> 6); /* keeps next 6 bits (0x0Fu = 00001111 unsigned with & keeps last 4 bits) from the previous input bytes and 2 first bits from the following input bytes */
            alph_ind[3] = input_bytes[2] & 0x3F; /* keeps the last 6 bits */
            char output[5]; /* the 4 alpha indices plus null terminator */
            /* find character from alphabet array based on each index and save into output */
            output[0] = b64_alphabet[alph_ind[0]];
            output[1] = b64_alphabet[alph_ind[1]];
            output[2] = b64_alphabet[alph_ind[2]];
            output[3] = b64_alphabet[alph_ind[3]];
            if(n_read == 1){ /* if size of bytes read is only 1 requires padding == */
              output[2] = '=';
              output[3] = '=';
            }
            if(n_read == 2){ /* if size of bytes read is 2 instead of 3 bytes requires padding = */
              output[3] = '=';
            }
            wrapping_count += 4;
            if (wrapping_count > 76) { /* adds new line to every 76 character and resets count */
              putchar('\n');
              wrapping_count = 4;
            }

            size_t n_write = fwrite(output,sizeof(char), 4, stdout);
            if (n_write < 4) {
            if (ferror(stdout)) err(1, "Write error"); /* Write error */
            }
            output_count += n_write;
        }
            size_t num_requested = sizeof(input_bytes);
            if (n_read < num_requested) {
                /* Got less than expected */
              if (feof(fp)) break; /* End of file */
              if (ferror(fp)) err(1, "Read error"); /* Read error */
            }
        }
        /* add new line at end of program when necessary */
        if (output_count > 0 && wrapping_count > 0) { 
        putchar('\n');
    }
    if (fp != stdin) fclose(fp); /* close opened files; */

    return 0;
    }