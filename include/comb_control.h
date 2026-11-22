#ifndef COMB_CONTROL_H
#define COMB_CONTROL_H

/* _comb_control's commands and their parameters. */
#define COMB_CTRL_GET          0
#define COMB_CTRL_SET          1
#define COMB_CTRL_RESET        2
#define COMB_CTRL_HANDSHAKE    3
#define COMB_CTRL_SET_CALLBACK 4
#define COMB_CTRL_RESET_GRAPH  5

#define COMB_PARAM_STATUS      0
#define COMB_PARAM_CONTROL     1
#define COMB_PARAM_MODE        2
#define COMB_PARAM_BAUD        3
#define COMB_PARAM_RX_UNIT     4
#define COMB_PARAM_ASYNC_COUNT 5
#define COMB_PARAM_ASYNC_BUSY  6

#define COMB_RESET_ALL         0
#define COMB_RESET_ERROR       1
#define COMB_CANCEL_WRITE      2
#define COMB_CANCEL_READ       3

#define COMB_HANDSHAKE_SET_RTS 0
#define COMB_HANDSHAKE_GET_CTS 1

#define COMB_ASYNC_WRITE       0
#define COMB_ASYNC_READ        1

#endif
