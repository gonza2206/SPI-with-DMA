#include <cstdio>
#include <cstdlib>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#define TEST_SIZE 256

static uint8_t txbuf[TEST_SIZE];
static uint8_t rxbuf[TEST_SIZE];

// Global variables for DMA channels
static uint dma_tx;
static uint dma_rx;
static bool transmission_ended = false;

// Function to fill the transmission buffer with random data
void fill_tx_buffer() {
    for (uint i = 0; i < TEST_SIZE; ++i) {
        txbuf[i] = rand() % 256;
    }
}

// DMA interrupt handler
void dma_handler() {
    // Check if the RX channel has completed the transfer
    if (dma_hw->ints0 & (1u << dma_rx)) {
        dma_hw->ints0 = 1u << dma_rx; // Clear the interrupt flag

        // Check if the TX channel has also finished
        if (!dma_channel_is_busy(dma_tx)) {
            printf("DMA Transfer Complete\n");
            transmission_ended = true;
        }
    }
}

// SPI initialization function
void spi_init() {
    // Enable SPI at 1 MHz and connect to GPIO pins
    spi_init(spi_default, 1000 * 1000);
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

    // Force loopback for testing
    hw_set_bits(&spi_get_hw(spi_default)->cr1, SPI_SSPCR1_LBM_BITS);
}

// Function to configure DMA channels
void setup_dma_channels() {
    // Fill the transmission buffer with random data
    fill_tx_buffer();

/************ Configure the TX DMA channel *************/

    printf("Configure TX DMA\n");

// Get the default configuration for the DMA channel and store it in the variable 'config'
    dma_channel_config config = dma_channel_get_default_config(dma_tx);

// Set the size of the data to be transferred to 8 bits (1 byte)
    channel_config_set_transfer_data_size(&config, DMA_SIZE_8);

// Set the Data Request (DREQ) signal for the DMA channel using the SPI DREQ signal
// 'spi_get_dreq(spi_default, true)' returns the DREQ value for the SPI peripheral
//This ensures that the DMA transfer is triggered by the SPI peripheral's DREQ signal when it's ready to transmit data.
    channel_config_set_dreq(&config, spi_get_dreq(spi_default, true));

// Configure the DMA channel 'dma_tx' with the specified parameters:
// - Write data to the SPI data register (DR), as it's the destination for TX data
// - Read data from the memory buffer 'txbuf', as it's the source of TX data
// - 'TEST_SIZE' indicates the number of elements (bytes) to transfer from 'txbuf'
// - Set 'false' to indicate that the DMA transfer should not start immediately
    dma_channel_configure(dma_tx, &config,
                          &spi_get_hw(spi_default)->dr, // Write address: SPI data register
                          txbuf,                         // Read address: Memory buffer
                          TEST_SIZE,                     // Number of elements (bytes)
                          false);                        // Do not start yet

/************ Configure the RX DMA channel *************/

    printf("Configure RX DMA\n");

// Get the default configuration for the DMA channel and store it in the variable 'config'
    config = dma_channel_get_default_config(dma_rx);

// Set the size of the data to be transferred to 8 bits (1 byte)
    channel_config_set_transfer_data_size(&config, DMA_SIZE_8);

// Set the Data Request (DREQ) signal for the DMA channel using the SPI DREQ signal
// 'spi_get_dreq(spi_default, false)' returns the DREQ value for the SPI peripheral
//This ensures that the DMA transfer is triggered by the SPI peripheral's DREQ signal when it's ready to transmit data.
    channel_config_set_dreq(&config, spi_get_dreq(spi_default, false));

// Set the increment mode for the read and write addresses
// In this case, the read address (SPI data register) will not be incremented, In SPI the incoming data is in one
// single register.
// While the written address (memory buffer 'rxbuf') will be incremented after each transfer, this is because before
//we are using an external buffer, so after each data trasfer we need to increase the register to transmit the next
//value.
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);

// Configure the DMA channel 'dma_rx' with the specified parameters:
// - Write data to the memory buffer 'rxbuf', as it's the destination for RX data
// - Read data from the SPI data register, as it's the source of RX data
// - 'TEST_SIZE' indicates the number of elements (bytes) to transfer to 'rxbuf'
// - Set 'false' to indicate that the DMA transfer should not start immediately
    dma_channel_configure(dma_rx, &config,
                          rxbuf,                         // Write address: Memory buffer
                          &spi_get_hw(spi_default)->dr, // Read address: SPI data register
                          TEST_SIZE,                     // Number of elements (bytes)
                          false);                        // Do not start yet


    // Enable DMA interrupts
    dma_channel_set_irq0_enabled(dma_rx, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

// Setup function
void setup() {
    stdio_init_all();

    sleep_ms(5000);
    printf("SPI DMA example\n");

    spi_init();

    // Claim unused DMA channels
    dma_tx = dma_claim_unused_channel(true);
    dma_rx = dma_claim_unused_channel(true);
    setup_dma_channels();

    printf("Starting DMAs...\n");
    // Start DMA transfers simultaneously
    dma_start_channel_mask((1u << dma_tx) | (1u << dma_rx));
}

// Function to check and print transferred data
bool check_and_print_data() {
    // Check transferred data
    printf("Checking...\n");
    printf(" [ ");
    for (uint i = 0; i < TEST_SIZE; ++i) {
        printf(" %i ", rxbuf[i]);
        if (rxbuf[i] != txbuf[i]) {
            return false;
        }
    }
    printf(" ]\n");
    return true;
}

// Loop function
void loop() {
    if (transmission_ended) {
        transmission_ended = false;
        if (check_and_print_data()) {
            printf("All good\n");
            // Fill the transmission buffer with new data
            fill_tx_buffer();
            sleep_ms(1000);
        } else {
            printf("Communication FAIL, mismatch information\n");
        }
        // Restart DMA transfers
        dma_channel_set_read_addr(dma_tx, txbuf, false);
        dma_channel_set_write_addr(dma_rx, rxbuf, false);
        dma_start_channel_mask((1u << dma_tx) | (1u << dma_rx));
    }
}

// Main function
int main() {
    setup();

    while (true) {
        loop();
    }
}
