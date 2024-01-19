## ESP32_S3 ELM327 Emulator for Can Bus

This project aims to create an ESP32_S3 BLE ELM327 emulator that operates using the Can protocol. The goal is to simulate a connection to the ECU sending fake responses to a compatible app.

## Features

- Emulates an ELM327 device using ESP32_S3.
- Supports the Can protocol.
- Integration with Torque app to display Multiple PIDS.
- Fake data sent to simulate connection to ECU
- Uses BLE

## Usage

1. Upload this code to your ESP32_S3 board.
2. Connect your smartphone or Torque app to the ESP32_S3 over Bluetooth.
3. View vehicle information and data on the Torque app.

## Contributing

Contributions to this project are welcome! Feel free to fork the repository, make improvements, and submit pull requests.

## License

This project is licensed under the [MIT License](LICENSE).
