// SPIStrategy.cpp
// Gaugemancy
//
// Created by Fred Rodrigues on 02/11/2024.

#include "SPIStrategy.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include "ofLog.h"

SPIStrategy::SPIStrategy() {
    // Initialize SPI communication settings here if necessary
}

SPIStrategy::~SPIStrategy() {
    // Clean up SPI communication here if necessary
}

void SPIStrategy::updateSensorData() {
    // Implement SPI data retrieval
    // This is a placeholder implementation and should be adapted to your specific hardware

    // Open SPI device
    int fd = open("/dev/spidev0.0", O_RDWR);
    if (fd < 0) {
        ofLogError("SPIStrategy") << "Failed to open SPI device";
        return;
    }

    // Set SPI mode, bits per word, speed, etc.
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = 500000;
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) == -1) {
        ofLogError("SPIStrategy") << "Failed to set SPI mode";
        close(fd);
        return;
    }
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1) {
        ofLogError("SPIStrategy") << "Failed to set bits per word";
        close(fd);
        return;
    }
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
        ofLogError("SPIStrategy") << "Failed to set max speed";
        close(fd);
        return;
    }

    // Prepare SPI transfer structures
    uint8_t tx[] = {0x00}; // Command to read sensor data (modify as needed)
    uint8_t rx[2] = {0, 0};

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = sizeof(rx),
        .speed_hz = speed,
        .delay_usecs = 0,
        .bits_per_word = bits,
    };

    // Perform SPI transaction
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 1) {
        ofLogError("SPIStrategy") << "Failed to send SPI message";
        close(fd);
        return;
    }

    // Convert received data to sensor values
    // Assuming we're reading a 10-bit ADC value from the sensor
    uint16_t rawValue = (rx[0] << 8) | rx[1];
    float sensorValue = static_cast<float>(rawValue) / 1023.0f; // Normalize to 0.0 - 1.0

    // Update sensorValues map
    sensorValues["sensor1"].set(sensorValue);

    // Close SPI device
    close(fd);
}

ofParameter<float> SPIStrategy::getSensorValue(const std::string& name) const {
    auto it = sensorValues.find(name);
    if (it != sensorValues.end()) {
        return it->second;
    } else {
        // Return a default value if sensor not found
        ofLogWarning("SPIStrategy") << "Sensor '" << name << "' not found. Returning default value.";
        return ofParameter<float>(0.0f);
    }
}
