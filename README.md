# ROIP Gateway

This project demonstrates how to use the Linphone library to create a Radio over IP (ROIP) gateway. It manages VoIP calls, codec configurations, and audio streams, enabling communication between radio systems and IP networks.

## Features

- **Drop Call**: Drop an ongoing call by extension using HTTP requests.
- **Registration**: Register and unregister with a SIP server.
- **Codec Management**: Enable, disable, and list audio and video codecs.
- **Sound Device Configuration**: Configure sound devices for playback and capture.
- **Call State Monitoring**: Track call state changes and detect voice activity.

## Dependencies

- Linphone core library
- CURL library
- pthread library

## Key Functions

- **DropCallByExtension**: Sends an HTTP request to drop a call by extension.
- **Register**: Registers the user agent (UA) with the SIP server.
- **Unregister**: Unregisters the UA from the SIP server.
- **TestCall**: Initiates a call to a specified destination.
- **RoipCall**: Specialized function for handling ROIP gateway calls.
- **ListCodecs**: Lists available audio or video codecs.
- **VoiceActivityDetection**: Detects voice activity based on bandwidth.

## Usage

1. **Build**: Compile the code using a C compiler with necessary libraries linked.
2. **Configure**: Update the `identity` and `password` variables with your SIP credentials.
3. **Run**: Execute the compiled binary to start the ROIP gateway.

```bash
gcc -o linphone_roip_gateway main.c -l linphone -l curl -lpthread
./linphone_roip_gateway
```

## Example

To make a call through the ROIP gateway:
```c
RoipCall(lc, "*100@10.10.10.20:5070");
```

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
