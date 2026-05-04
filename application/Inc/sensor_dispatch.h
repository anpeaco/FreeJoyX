/**
  ******************************************************************************
  * @file           : sensor_dispatch.h
  * @brief          : Cross-board SPI/I2C DMA-completion sensor chain dispatcher.
  *
  * Hoisted from board/f103_bluepill DMA1_Channel2/3/4/5_IRQHandler bodies so
  * the F411 BSP can call the same chain logic from its DMA2 Stream0/3 +
  * DMA1 Stream2/7 IRQ handlers. All four entry points are board-agnostic --
  * the SPI/I2C/sensor calls they make (SPI_HalfDuplex_Receive,
  * TLE5011_StartDMA, AS5600_StartDMA, etc.) already have BSP shims at the
  * board layer.
  *
  * IRQ handler shape (per board):
  *   1. Clear DMA flag, disable DMA channel/stream    (board-specific regs)
  *   2. Call the matching Sensor_On*Complete helper   (board-agnostic)
  *
  * Each helper handles SPI BSY drain / I2C BTF wait / STOP generation /
  * acknowledgement of which sensor just finished, then chains the next
  * sensor in the loop via its StartDMA helper. F103 + F411 thus run
  * identical sensor-chain behaviour from the same source.
  ******************************************************************************
  */

#ifndef SENSOR_DISPATCH_H_
#define SENSOR_DISPATCH_H_

/* SPI1 RX-complete: a sensor's incoming DMA transfer just finished.
 * Calls the sensor's StopDMA helper, then walks forward looking for
 * the next ready-to-start SPI sensor and arms its StartDMA. F103 entry
 * was DMA1_Channel2_IRQHandler; F411 entry is DMA2_Stream0_IRQHandler. */
void Sensor_OnSpiRxComplete(void);

/* SPI1 TX-complete: a sensor's outgoing DMA transfer just finished.
 * For TLE5011/TLE5012 the same physical transfer continues into a
 * receive cycle (half-duplex); other sensors mark tx_complete and
 * wait for the matching RX. F103 entry was DMA1_Channel3_IRQHandler;
 * F411 entry is DMA2_Stream3_IRQHandler. */
void Sensor_OnSpiTxComplete(void);

/* I2C2 TX-complete (ADS1115 mux-set + AS5600 read-prelude). After
 * BTF + STOP, walks forward and starts the next ready I2C sensor.
 * F103 entry was DMA1_Channel4_IRQHandler; F411 entry is
 * DMA1_Stream7_IRQHandler. */
void Sensor_OnI2cTxComplete(void);

/* I2C2 RX-complete. STOP + clear, then if the just-finished sensor
 * was an ADS1115 it advances the mux to the next channel. F103 entry
 * was DMA1_Channel5_IRQHandler; F411 entry is DMA1_Stream2_IRQHandler. */
void Sensor_OnI2cRxComplete(void);

#endif /* SENSOR_DISPATCH_H_ */
