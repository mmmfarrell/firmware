/*
 * Copyright (c) 2017, James Jackson and Daniel Koch, BYU MAGICC Lab
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

#pragma GCC push_options
#pragma GCC optimize ("O0")

#include "f4.h"

namespace rosflight_firmware {

F4Board::F4Board()
{
}

void F4Board::init_board(void)
{
  systemInit();
  led2_.init(LED2_GPIO, LED2_PIN);
  led1_.init(LED1_GPIO, LED1_PIN);

  int_i2c_.init(&i2c_config[MAG_I2C]);
  ext_i2c_.init(&i2c_config[EXTERNAL_I2C]);
  spi1_.init(&spi_config[MPU6000_SPI]);
  spi3_.init(&spi_config[FLASH_SPI]);
}

void F4Board::board_reset(bool bootloader)
{
  (void)bootloader;
  NVIC_SystemReset();
}

// clock
uint32_t F4Board::clock_millis()
{
  return millis();
}

uint64_t F4Board::clock_micros()
{
  return micros();
}

void F4Board::clock_delay(uint32_t milliseconds)
{
  delay(milliseconds);
}

// serial
void F4Board::serial_init(uint32_t baud_rate)
{
  (void)baud_rate;
  vcp_.init();
}

void F4Board::serial_write(const uint8_t *src, size_t len)
{
  vcp_.write(src, len);
}

uint16_t F4Board::serial_bytes_available(void)
{
  return vcp_.rx_bytes_waiting();
}

uint8_t F4Board::serial_read(void)
{
  return vcp_.read_byte();
}

void F4Board::serial_flush()
{
  vcp_.flush();
}

// sensors
void F4Board::sensors_init()
{
  imu_.init(&spi1_);
  mag_.init(&int_i2c_);
  baro_.init(&int_i2c_);
  airspeed_.init(&ext_i2c_);

  while(millis() < 50); // wait for sensors to boot up
}

uint16_t F4Board::num_sensor_errors(void)
{
  return int_i2c_.num_errors();
}

bool F4Board::new_imu_data()
{
  return imu_.new_data();
}

bool F4Board::imu_read(float accel[3], float* temperature, float gyro[3], uint64_t* time_us)
{
  float read_accel[3], read_gyro[3];
  imu_.read(read_accel, read_gyro, temperature, time_us);

  accel[0] = -read_accel[1];
  accel[1] = -read_accel[0];
  accel[2] = -read_accel[2];

  gyro[0] = -read_gyro[1];
  gyro[1] = -read_gyro[0];
  gyro[2] = -read_gyro[2];

  return true;
}

void F4Board::imu_not_responding_error(void)
{
  sensors_init();
}

void F4Board::mag_read(float mag[3])
{
  mag_.update();
  mag_.read(mag);
}

bool F4Board::mag_check(void)
{
  mag_.update();
  return mag_.present();
}

void F4Board::baro_read(float *pressure, float *temperature)
{
  baro_.update();
  baro_.read(pressure, temperature);
}

bool F4Board::baro_check()
{
  baro_.update();
  return baro_.present();
}

bool F4Board::diff_pressure_check(void)
{
  airspeed_.update();
  return airspeed_.present();
}

void F4Board::diff_pressure_read(float *diff_pressure, float *temperature)
{
  airspeed_.update();
  airspeed_.read(diff_pressure, temperature);
}

bool F4Board::sonar_check(void)
{
  return false;
}

float F4Board::sonar_read(void)
{
  return 0.0;
}

// PWM
void F4Board::rc_init(rc_type_t rc_type)
{
  switch (rc_type)
  {
  case RC_TYPE_SBUS:
    sbus_uart_.init(&uart_config[0], 100000, UART::MODE_8E2);
    inv_pin_.init(SBUS_INV_GPIO, SBUS_INV_PIN, GPIO::OUTPUT);
    rc_sbus_.init(&inv_pin_, &sbus_uart_);
    rc_ = &rc_sbus_;
    break;
  case RC_TYPE_PPM:
  default:
    rc_ppm_.init(&pwm_config[RC_PPM_PIN]);
    rc_ = &rc_ppm_;
    break;
  }
}

float F4Board::rc_read(uint8_t channel)
{
  return rc_->read(channel);
}

void F4Board::pwm_init(uint32_t refresh_rate, uint16_t idle_pwm)
{
  for (int i = 0; i < PWM_NUM_OUTPUTS; i++)
  {
    esc_out_[i].init(&pwm_config[i], refresh_rate, 2000, 1000);
    esc_out_[i].writeUs(idle_pwm);
  }
}

void F4Board::pwm_write(uint8_t channel, float value)
{
  esc_out_[channel].write(value);
}

bool F4Board::rc_lost()
{
  return rc_->lost();
}

// non-volatile memory
void F4Board::memory_init(void)
{
  return flash_.init(&spi3_);
}

bool F4Board::memory_read(void * data, size_t len)
{
  return flash_.read_config((uint8_t*)data, len);
}

bool F4Board::memory_write(void * data, size_t len)
{
  return flash_.write_config(reinterpret_cast<uint8_t*>(data), len);
}

// LED
void F4Board::led0_on(void) { led1_.on(); }
void F4Board::led0_off(void) { led1_.off(); }
void F4Board::led0_toggle(void) { led1_.toggle(); }

void F4Board::led1_on(void) { led2_.on(); }
void F4Board::led1_off(void) { led2_.off(); }
void F4Board::led1_toggle(void) { led2_.toggle(); }
}

#pragma GCC pop_options
#pragma GCC diagnostic pop
