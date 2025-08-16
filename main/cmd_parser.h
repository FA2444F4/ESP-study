#ifndef CMD_PARSER_H
#define CMD_PARSER_H

/**
 * @brief 处理从 UART 接收到的一整行命令
 *
 * @param line 指向包含命令的字符串
 */
void cmd_parser_process_line(char *line);

#endif // CMD_PARSER_H