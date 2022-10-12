#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

/*
 * For debug option. If you want to debug, set 1.
 * If not, set 0.
 */
#define DEBUG 1

#define MAX_SYMBOL_TABLE_SIZE 1024
#define MEM_TEXT_START 0x00400000
#define MEM_DATA_START 0x10000000
#define BYTES_PER_WORD 4
#define INST_LIST_LEN 22

/******************************************************
 * Structure Declaration
 *******************************************************/

typedef struct inst_struct
{
    char *name;
    char *op;
    char type;
    char *funct;
} inst_t;

typedef struct symbol_struct
{
    char name[32];
    uint32_t address;
} symbol_t;

enum section
{
    DATA = 0,
    TEXT,
    MAX_SIZE
};

/******************************************************
 * Global Variable Declaration
 *******************************************************/

inst_t inst_list[INST_LIST_LEN] = {
    //  idx
    {"add", "000000", 'R', "100000"},  //    0      rd rs rt
    {"sub", "000000", 'R', "100010"},  //    1      rd rs rt
    {"addiu", "001001", 'I', ""},      //    2      rt rs imm
    {"addu", "000000", 'R', "100001"}, //    3      rd rs rt
    {"and", "000000", 'R', "100100"},  //    4      rd rs rt
    {"andi", "001100", 'I', ""},       //    5      rt rs imm
    {"beq", "000100", 'I', ""},        //    6      rs rt imm
    {"bne", "000101", 'I', ""},        //    7      rs rt imm
    {"j", "000010", 'J', ""},          //    8      address
    {"jal", "000011", 'J', ""},        //    9      address
    {"jr", "000000", 'R', "001000"},   //   10      rs
    {"lui", "001111", 'I', ""},        //   11      rt imm
    {"lw", "100011", 'I', ""},         //   12      rt imm(rs)
    {"nor", "000000", 'R', "100111"},  //   13      rd rs rt
    {"or", "000000", 'R', "100101"},   //   14      rd rs rt
    {"ori", "001101", 'I', ""},        //   15      rt rs imm
    {"sltiu", "001011", 'I', ""},      //   16      rt rs imm
    {"sltu", "000000", 'R', "101011"}, //   17      rd rs rt
    {"sll", "000000", 'R', "000000"},  //   18      rd rt shamt
    {"srl", "000000", 'R', "000010"},  //   19      rd rt shamt
    {"sw", "101011", 'I', ""},         //   20      rt imm(rs)
    {"subu", "000000", 'R', "100011"}  //   21      rd rs rt
};

// r except: jr, srl, sll
// i -> default rt rs imm,
// except lui rt imm / lw, sw rt imm(rs) /
// j default
symbol_t SYMBOL_TABLE[MAX_SYMBOL_TABLE_SIZE]; // Global Symbol Table

uint32_t symbol_table_cur_index = 0; // For indexing of symbol table

/* Temporary file stream pointers */
FILE *data_seg;
FILE *text_seg;

/* Size of each section */
uint32_t data_section_size = 0;
uint32_t text_section_size = 0;

/******************************************************
 * Function Declaration
 *******************************************************/

/* Change file extension from ".s" to ".o" */
char *change_file_ext(char *str)
{
    char *dot = strrchr(str, '.');

    if (!dot || dot == str || (strcmp(dot, ".s") != 0))
        return NULL;

    str[strlen(str) - 1] = 'o';
    return "";
}

/* Add symbol to global symbol table */
void symbol_table_add_entry(symbol_t symbol)
{
    SYMBOL_TABLE[symbol_table_cur_index++] = symbol;
#if DEBUG
    printf("%s: 0x%08x\n", symbol.name, symbol.address);
#endif
}

/* Convert integer number to binary string */
char *num_to_bits(unsigned int num, int len)
{
    char *bits = (char *)malloc(len + 1);
    int idx = len - 1, i;
    while (num > 0 && idx >= 0)
    {
        if (num % 2 == 1)
        {
            bits[idx--] = '1';
        }
        else
        {
            bits[idx--] = '0';
        }
        num /= 2;
    }
    for (i = idx; i >= 0; i--)
    {
        bits[i] = '0';
    }
    bits[len] = '\0';
    return bits;
}

int stringToInt(char *num)
{
}

/* Record .text section to output file */
void record_text_section(FILE *output)
{
    uint32_t cur_addr = MEM_TEXT_START;
    char line[1024];

    /* Point to text_seg stream */
    rewind(text_seg);

    /* Print .text section */
    while (fgets(line, 1024, text_seg) != NULL)
    {
        char inst[0x1000] = {0};
        char op[32] = {0};
        char label[32] = {0};
        char type = '0';
        int i, idx = 0;
        int rs, rt, rd, imm, shamt;
        int addr;

        rs = rt = rd = imm = shamt = addr = 0;
#if DEBUG
        printf("0x%08x: ", cur_addr);
#endif

        char inst_bits[33];
        inst_bits[0] = '\0';

        char temp_inst[32];

        sscanf(line, "%s", temp_inst);

        for (i = 0; i < INST_LIST_LEN; i++)
        {
            if (strcmp(temp_inst, inst_list[i].name) == 0)
            {
                idx = i;
                break;
            }
        }

        type = inst_list[idx].type;

        switch (type)
        {
        case 'R':
            // rd rs rt
            // r except: jr, srl, sll
            if (strcmp(inst_list[idx].name, "jr") == 0)
            {
                sscanf(line, "%s $%d", temp_inst, &rs);
            }
            else if (strcmp(inst_list[idx].name, "sll") == 0 || strcmp(inst_list[idx].name, "srl") == 0)
            {
                sscanf(line, "%s $%d, $%d, %d", temp_inst, &rd, &rt, &shamt);
            }
            else
            {
                sscanf(line, "%s $%d, $%d, $%d", temp_inst, &rd, &rs, &rt);
            }

            strcat(inst_bits, inst_list[idx].op);
            strcat(inst_bits, num_to_bits(rs, 5));
            strcat(inst_bits, num_to_bits(rt, 5));
            strcat(inst_bits, num_to_bits(rd, 5));
            strcat(inst_bits, num_to_bits(shamt, 5));
            strcat(inst_bits, inst_list[idx].funct);

#if DEBUG
            printf("op:%s rs:$%d rt:$%d rd:$%d shamt:%d funct:%s\n",
                   op, rs, rt, rd, shamt, inst_list[idx].funct);
#endif
            break;

        case 'I':
            // i -> default rt rs imm,
            // except lui rt imm / lw, sw rt imm(rs) /
            if (strcmp(inst_list[idx].name, "lui") == 0)
            {
                sscanf(line, "%s $%d %d", temp_inst, &rt, &imm);
            }
            else if (strcmp(inst_list[idx].name, "lw") == 0 || strcmp(inst_list[idx].name, "sw") == 0)
            {
                sscanf(line, "%s $%d, %d($%d)", temp_inst, &rt, &imm, &rs);
            }
            else
            {
                sscanf(line, "%s $%d, $%d, %d", temp_inst, &rt, &rs, &imm);
            }

            strcat(inst_bits, inst_list[idx].op);
            strcat(inst_bits, num_to_bits(rs, 5));
            strcat(inst_bits, num_to_bits(rt, 5));
            strcat(inst_bits, num_to_bits(imm, 16));
#if DEBUG
            printf("op:%s rs:$%d rt:$%d imm:0x%x\n",
                   op, rs, rt, imm);
#endif
            break;

        case 'J':
            sscanf(line, "%s %x", temp_inst, &addr);

            strcat(inst_bits, inst_list[idx].op);
            strcat(inst_bits, num_to_bits(addr, 26));
#if DEBUG
            printf("op:%s addr:%i\n", op, addr);
#endif
            break;

        default:
            break;
        }

        fprintf(output, "%s", inst_bits);
        fprintf(output, "\n");
        cur_addr += BYTES_PER_WORD;
    }
}

/* Record .data section to output file */
void record_data_section(FILE *output)
{
    uint32_t cur_addr = MEM_DATA_START;
    char line[1024];

    /* Point to data segment stream */
    rewind(data_seg);

    /* Print .data section */
    while (fgets(line, 1024, data_seg) != NULL)
    {
        char temp_name[32];
        int temp_word = 0;

        sscanf(line, "%s %d", temp_name, &temp_word);
        char *data_bits = num_to_bits(temp_word, 32);

#if DEBUG
        printf("0x%08x: ", cur_addr);
        printf("%s", line);
#endif
        fprintf(output, "%s\n", data_bits);
        cur_addr += BYTES_PER_WORD;
    }
}

char *replaceWord(const char *s, const char *oldW,
                  const char *newW)
{
    char *result;
    int i, cnt = 0;
    int newWlen = strlen(newW);
    int oldWlen = strlen(oldW);

    // Counting the number of times old word
    // occur in the string
    for (i = 0; s[i] != '\0'; i++)
    {
        if (strstr(&s[i], oldW) == &s[i])
        {
            cnt++;

            // Jumping to index after the old word.
            i += oldWlen - 1;
        }
    }

    // Making new string of enough length
    result = (char *)malloc(i + cnt * (newWlen - oldWlen) + 1);

    i = 0;
    while (*s)
    {
        // compare the substring with the result
        if (strstr(s, oldW) == s)
        {
            strcpy(&result[i], newW);
            i += newWlen;
            s += oldWlen;
        }
        else
            result[i++] = *s++;
    }

    result[i] = '\0';
    return result;
}

/* Fill the blanks */
void make_binary_file(FILE *output)
{
#if DEBUG
    char line[1024] = {0};
    rewind(text_seg);
    /* Print line of text segment */
    while (fgets(line, 1024, text_seg) != NULL)
    {
        printf("%s", line);
    }
    printf("text section size: %d, data section size: %d\n",
           text_section_size, data_section_size);
#endif

    /* Print text section size and data section size */
    fprintf(output, "%s\n", num_to_bits(text_section_size, 32));
    fprintf(output, "%s\n", num_to_bits(data_section_size, 32));

    /* Print .text section */
    record_text_section(output);

    /* Print .data section */
    record_data_section(output);
}

void replaceVariable(FILE *text_seg)
{
    char line[1024] = {0};
    rewind(text_seg);

    char seg_string[2048] = {0};

    while (fgets(line, 1024, text_seg) != NULL)
    {
        strcat(seg_string, line);
    }

    char *result = seg_string;

    for (int i = 0; i < symbol_table_cur_index; i++)
    {
        char temp_address[32];
        sprintf(temp_address, "0x%x", SYMBOL_TABLE[i].address);
        result = replaceWord(result, SYMBOL_TABLE[i].name, temp_address);
    }

    fclose(text_seg);
    text_seg = tmpfile();
    rewind(text_seg);
    fprintf(text_seg, "%s", result);
}

void laToLuiOri(FILE *text_seg)
{
    char line[1024] = {0};
    rewind(text_seg);

    char seg_string[2048] = {0};

    while (fgets(line, 1024, text_seg) != NULL)
    {
        char wLine[32] = {0};
        if (line[0] == 'l' && line[1] == 'a')
        {
            char temp_line[64];
            int temp_reg = 0;
            int temp_addr = 0;
            sscanf(line, "%s $%d, 0x%8x", temp_line, &temp_reg, &temp_addr);
            sscanf(line, "%s %[^\n]", temp_line, temp_line);

            if (temp_addr >= 0x0001)
            {
                strcpy(wLine, "lui\t");
                strcat(wLine, temp_line);
            }
            else
            {
                strcpy(wLine, "ori\t");
                strcat(wLine, temp_line);
            }
            strcat(wLine, "\n");
        }
        else
        {
            strcpy(wLine, line);
        }

        strcat(seg_string, wLine);
    }

    fclose(text_seg);
    text_seg = tmpfile();
    rewind(text_seg);
    fprintf(text_seg, "%s", seg_string);
}

/* Fill the blanks */
void make_symbol_table(FILE *input)
{
    char line[1024] = {0};
    uint32_t address = 0;
    enum section cur_section = MAX_SIZE;

    /* Read each section and put the stream */
    while (fgets(line, 1024, input) != NULL)
    {
        char *temp;
        char _line[1024] = {0};
        strcpy(_line, line);
        temp = strtok(_line, "\t\n");

        /* Check section type */
        if (!strcmp(temp, ".data"))
        {
            cur_section = DATA;
            data_seg = tmpfile();
            continue;
        }
        else if (!strcmp(temp, ".text"))
        {
            cur_section = TEXT;
            text_seg = tmpfile();
            continue;
        }

        char name[32];
        char temp_seg[128];

        /* Put the line into each segment stream */
        if (cur_section == DATA)
        {
            if (address < MEM_DATA_START)
            {
                address = MEM_DATA_START;
            }

            if (line[0] != '\t')
            {
                sscanf(line, "%[^:]:\t%[^\n]", name, temp_seg);

                symbol_t new_symbol;
                strcpy(new_symbol.name, name);
                new_symbol.address = address;
                symbol_table_add_entry(new_symbol);
            }
            else
            {
                sscanf(line, "\t%[^\n]", temp_seg);
            }
            fprintf(data_seg, "%s\n", temp_seg);

            data_section_size += BYTES_PER_WORD;
        }
        else if (cur_section == TEXT)
        {
            if (address > MEM_DATA_START)
            {
                address = MEM_TEXT_START;
            }

            if (line[0] != '\t')
            {
                sscanf(line, "%[^:]\n", name);

                symbol_t new_symbol;
                strcpy(new_symbol.name, name);
                new_symbol.address = address;

                symbol_table_add_entry(new_symbol);
            }
            else
            {
                sscanf(line, "\t%[^\n]", temp_seg);
                fprintf(text_seg, "%s\n", temp_seg);
                text_section_size += BYTES_PER_WORD;
            }
        }

        address += BYTES_PER_WORD;
    }

    replaceVariable(text_seg);
    laToLuiOri(text_seg);
}

/******************************************************
 * Function: main
 *
 * Parameters:
 *  int
 *      argc: the number of argument
 *  char*
 *      argv[]: array of a sting argument
 *
 * Return:
 *  return success exit value
 *
 * Info:
 *  The typical main function in C language.
 *  It reads system arguments from terminal (or commands)
 *  and parse an assembly file(*.s).
 *  Then, it converts a certain instruction into
 *  object code which is basically binary code.
 *
 *******************************************************/

int main(int argc, char *argv[])
{
    FILE *input, *output;
    char *filename;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <*.s>\n", argv[0]);
        fprintf(stderr, "Example: %s sample_input/example?.s\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Read the input file */
    input = fopen(argv[1], "r");
    if (input == NULL)
    {
        perror("ERROR");
        exit(EXIT_FAILURE);
    }

    /* Create the output file (*.o) */
    filename = strdup(argv[1]); // strdup() is not a standard C library but fairy used a lot.
    if (change_file_ext(filename) == NULL)
    {
        fprintf(stderr, "'%s' file is not an assembly file.\n", filename);
        exit(EXIT_FAILURE);
    }

    output = fopen(filename, "w");
    if (output == NULL)
    {
        perror("ERROR");
        exit(EXIT_FAILURE);
    }

    /******************************************************
     *  Let's complete the below functions!
     *
     *  make_symbol_table(FILE *input)
     *  make_binary_file(FILE *output)
     *  ├── record_text_section(FILE *output)
     *  └── record_data_section(FILE *output)
     *
     *******************************************************/
    make_symbol_table(input);
    make_binary_file(output);

    fclose(input);
    fclose(output);

    return 0;
}
