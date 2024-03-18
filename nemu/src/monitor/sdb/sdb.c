/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include <memory/paddr.h>

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  return -1;
}

static int cmd_help(char *args);

static int cmd_si(char *args){
  char *arg = strtok(NULL, " ");
  int n = 1;
  if (arg != NULL) {
    sscanf(arg, "%d", &n);
  }
  cpu_exec(n);
  return 0;
}

static int cmd_info(char *args){
  char *arg = strtok(NULL, " ");
  if (arg == NULL) {
    printf("Please input r/w\n");
    return 0;
  }
  if (strcmp(arg, "r") == 0) {
    isa_reg_display();
  }else {
    printf("Please input r/w\n");
  }
  return 0;
}


// [x] 规定表达式EXPR中只能是一个十六进制数
// [ ]调用表达式求值函数
static int cmd_x(char *args){
  // read arg: n
  char *arg = strtok(NULL, " ");
  int n = 1;
  if (arg == NULL) {
    printf("Please input n\n");
    return 0;
  }
  sscanf(arg, "%d", &n);
  // read arg: EXPR
  arg = strtok(NULL, " ");
  if (arg == NULL) {
    printf("Please input EXPR\n");
    return 0;
  }
  // EXPR -> addr
  // 使用循环将指定长度的内存数据通过十六进制打印出来
  paddr_t addr = 0;
  sscanf(arg, "%x", &addr);
  
  for(int i=0;i<n;i++){
    word_t value = paddr_read(addr, 4);
    printf("0x%08x: ", addr);
    for (int j = 0; j < 4; j++) {
      printf("%02x ", (value >> (j * 8)) & 0xFF); // 输出每个字节的十六进制形式
    }
    // 输出每个字节的字符形式
    for (int j = 0; j < 4; j++) {
      char ch = (value >> (j * 8)) & 0xFF;
      if (ch >= 32 && ch <= 126) {
        printf("%c", ch);
      } else {
        printf(".");
      }
    }
    printf("\n");
    addr += 4;
  }
  return 0;
}


static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },

  /* TODO: Add more commands */
  /* cmd_table[i].handler(args)*/
  { "si", "[n] Single step execution, dufault n = 1", cmd_si },
  { "info", "r Print register", cmd_info },
  //{ "info", "r/w Print register/watch point state", cmd_info },
  { "x", "[n] Scan memory. Evaluate the expression EXPR and use the result as the starting memory Address, output N consecutive 4 bytes in hexadecimal form", cmd_x },

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
