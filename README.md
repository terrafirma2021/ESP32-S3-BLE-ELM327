## ESP32_S3 ELM327 Emulator for Can Bus

**Work in Progress (WIP) - Early Basic Implementation**

This project aims to create an ESP32_S3-based ELM327 emulator that operates using the Can protocol. The goal is to simulate an ELM327 device without the need for a separate physical ELM327 device. This implementation is designed primarily for use with motorcycles, focusing on CAN bus communication. However, it can be adapted for other applications if necessary.

## Features

- Emulates an ELM327 device using ESP32_S3.
- Supports the Can protocol.
- Integration with Torque app to display Multiple PIDS.

## Usage

1. Upload this code to your ESP32_S3 board.
2. Connect your smartphone or Torque app to the ESP32_S3 over Bluetooth.
3. Use compatible KWP2000 commands to interact with your vehicle's data.
4. View vehicle information and data on the Torque app.

## TODO List

- Implement additional AT commands to resolve any existing errors.
- Add a switch to control headers in communication. Done
- Add a switch to remove spaces from responses. Done
- Integrate live data from the L9637D for enhanced functionality.

## Contributing

Contributions to this project are welcome! Feel free to fork the repository, make improvements, and submit pull requests.

## License

This project is licensed under the [MIT License](LICENSE).
