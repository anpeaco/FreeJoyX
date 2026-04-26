/**
  ******************************************************************************
  * @file           : board.h
  * @brief          : Top-level BSP API.
  *
  * Application code includes this single header to get access to all BSP
  * abstractions. The board-specific implementation lives under
  * board/<board_name>/Src/ and gets pulled into the build by
  * armgcc/target_<chip>.mk.
  *
  * This header is intentionally light right now -- it just pulls in the
  * board identity. Later phases of the F411 port will add wrappers for
  * GPIO / flash / DFU / tick / USB so the application code becomes fully
  * board-agnostic.
  ******************************************************************************
  */

#ifndef BOARD_H_
#define BOARD_H_

#include "board_ids.h"

#endif /* BOARD_H_ */
